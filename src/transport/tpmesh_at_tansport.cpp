// =============================================================================
// src/transport/xslot_at_transport.cpp - AT指令传输层 (V2.0 异步重构)
// =============================================================================

#include "xslot_at_transport.h"
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace xslot {

// =============================================================================
// ATCommandHandler 实现 (V2.0)
// =============================================================================

ATCommandHandler::ATCommandHandler(const char *port, uint32_t baudrate)
    : port_(port), baudrate_(baudrate), running_(false), state_(State::IDLE),
      sync_cmd_done_(false), sync_cmd_success_(false) {}

ATCommandHandler::~ATCommandHandler() { close(); }

bool ATCommandHandler::open() {
  serial_ = std::make_unique<hal::SerialPort>(port_.c_str(), baudrate_);
  if (!serial_->open()) {
    std::cerr << "[AT] Failed to open serial port: " << port_ << std::endl;
    serial_.reset();
    return false;
  }
  std::cout << "[AT] Serial port opened: " << port_ << " @ " << baudrate_
            << " bps" << std::endl;

  running_ = true;
  rx_thread_ = std::make_unique<hal::Thread>(
      std::bind(&ATCommandHandler::rxThreadFunc, this));
  rx_thread_->start();

  // 等待模块启动
  hal::sleep(100);

  // 测试AT连接
  std::vector<std::string> response;
  if (!sendCommandSync("AT", response, 1000)) {
    std::cerr << "[AT] Module not responding" << std::endl;
    close();
    return false;
  }

  std::cout << "[AT] Module connection OK." << std::endl;
  return true;
}

void ATCommandHandler::close() {
  running_ = false;
  if (rx_thread_ && rx_thread_->isRunning()) {
    rx_thread_->join();
  }
  if (serial_ && serial_->isOpen()) {
    serial_->close();
  }
  serial_.reset();
}

void ATCommandHandler::setURCCallback(URCCallback cb) {
  hal::LockGuard lock(urc_cb_mutex_);
  urc_callback_ = cb;
}

// 线程函数，唯一的串口读者
void ATCommandHandler::rxThreadFunc() {
  while (running_) {
    std::string line;
    // readLine 应该有一个合理的超时，以便循环可以检查 running_
    if (readLine(line, 200)) {
      std::cout << "[AT RX] " << line << std::endl;
      processLine(line);
    }

    // 检查是否有异步命令要发送
    std::string async_cmd;
    {
      hal::LockGuard queue_lock(async_queue_mutex_);
      if (!async_cmd_queue_.empty()) {
        async_cmd = async_cmd_queue_.front();
        async_cmd_queue_.pop();
      }
    }
    if (!async_cmd.empty()) {
      // 异步命令是"即发即忘"，不需要进入WAITING_RESPONSE
      hal::LockGuard sync_lock(sync_mutex_);
      if (state_ == State::IDLE) {
        writeSerial(async_cmd);
      } else {
        // 忙，稍后重试
        hal::LockGuard queue_lock(async_queue_mutex_);
        async_cmd_queue_.push(async_cmd);
      }
    }
  }
}

// 核心状态机
void ATCommandHandler::processLine(const std::string &line) {
  hal::LockGuard lock(sync_mutex_);

  if (state_ == State::WAITING_RESPONSE) {
    if (line.find("OK") != std::string::npos) {
      sync_cmd_success_ = true;
      sync_cmd_done_ = true;
      state_ = State::IDLE;
      sync_cv_.notify_all(); // 唤醒等待的同步命令
    } else if (line.find("ERROR") != std::string::npos) {
      sync_cmd_success_ = false;
      sync_cmd_done_ = true;
      state_ = State::IDLE;
      sync_cv_.notify_all(); // 唤醒等待的同步命令
    } else {
      // 这是响应数据，加入缓冲区
      response_buffer_.push_back(line);
    }
  } else {
    // IDLE 状态，这必须是 URC
    if (line.rfind("+", 0) == 0) {
      hal::LockGuard cb_lock(urc_cb_mutex_);
      if (urc_callback_) {
        urc_callback_(line);
      }
    }
  }
}

// 异步发送 (用于 AT+SEND)
bool ATCommandHandler::sendCommandAsync(const std::string &cmd) {
  hal::LockGuard lock(async_queue_mutex_);
  async_cmd_queue_.push(cmd);
  return true;
}

// 同步发送 (用于 AT+ADDR, AT+PWR 等)
bool ATCommandHandler::sendCommandSync(const std::string &cmd,
                                       std::vector<std::string> &response_lines,
                                       uint32_t timeout_ms) {
  std::unique_lock<hal::Mutex> lock(sync_mutex_);

  if (state_ != State::IDLE) {
    std::cerr << "[AT] Error: Handler is busy" << std::endl;
    return false; // 忙，无法发送
  }

  // 准备状态
  state_ = State::WAITING_RESPONSE;
  sync_cmd_done_ = false;
  response_buffer_.clear();

  if (!writeSerial(cmd)) {
    state_ = State::IDLE;
    return false;
  }

  // 等待响应
  if (sync_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                        [this] { return sync_cmd_done_; })) {
    // 成功或失败
    if (sync_cmd_success_) {
      response_lines = response_buffer_;
    }
    return sync_cmd_success_;
  } else {
    // 超时
    std::cerr << "[AT] Command timeout: " << cmd << std::endl;
    state_ = State::IDLE; // 重置状态机
    return false;
  }
}

bool ATCommandHandler::writeSerial(const std::string &cmd) {
  std::string full_cmd = cmd + "\r\n";
  std::cout << "[AT TX] " << cmd << std::endl;
  if (serial_->write(reinterpret_cast<const uint8_t *>(full_cmd.c_str()),
                     full_cmd.length()) != (int)full_cmd.length()) {
    std::cerr << "[AT] Serial write error" << std::endl;
    return false;
  }
  return true;
}

// 串口读取一行
bool ATCommandHandler::readLine(std::string &line, uint32_t timeout_ms) {
  line.clear();
  std::string buffer;
  uint32_t start = hal::getTimestamp();
  uint8_t ch;

  while (hal::getTimestamp() - start < timeout_ms) {
    int ret = serial_->read(&ch, 1, 10);
    if (ret == 1) {
      if (ch == '\n') {
        if (!buffer.empty() && buffer.back() == '\r') {
          buffer.pop_back();
        }
        if (!buffer.empty()) {
          line = buffer;
          return true;
        }
      } else {
        buffer += (char)ch;
      }
    }
  }
  return false;
}

// =============================================================================
// ATTransport 实现 (V2.0)
// =============================================================================

ATTransport::ATTransport(const char *port, uint32_t baudrate) : local_addr_(0) {
  at_handler_ = std::make_unique<ATCommandHandler>(port, baudrate);
}

ATTransport::~ATTransport() { close(); }

bool ATTransport::open() {
  if (!at_handler_->open()) {
    return false;
  }
  // 注册URC回调
  at_handler_->setURCCallback(
      std::bind(&ATTransport::onURCReceived, this, std::placeholders::_1));
  return true;
}

void ATTransport::close() { at_handler_->close(); }

xslot_slot_mode_t ATTransport::detect() {
  std::vector<std::string> response;
  if (at_handler_->sendCommandSync("AT", response, 1000)) {
    return XSLOT_MODE_WIRELESS;
  }
  // TODO: 增加HMI探测
  return XSLOT_MODE_EMPTY;
}

// [XSlotManager 线程调用]
bool ATTransport::send(uint16_t dest, const uint8_t *data, size_t len) {
  if (len > XSLOT_MAX_DATA_LEN)
    return false;

  // 格式化 AT+SEND 命令
  std::string hex_data = bytesToHex(data, len);
  std::stringstream ss;
  ss << "AT+SEND=" << std::hex << std::uppercase << std::setw(4)
     << std::setfill('0') << dest << "," << std::dec << len << "," << hex_data
     << ",0"; // 0 = UM模式

  // 异步推入队列
  return at_handler_->sendCommandAsync(ss.str());
}

// [XSlotManager 线程调用]
int ATTransport::receive(uint8_t *buffer, size_t max_len, uint32_t timeout_ms) {
  std::unique_lock<hal::Mutex> lock(rx_queue_mutex_);

  // 等待队列非空，或超时
  if (rx_queue_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                            [this] { return !rx_queue_.empty(); })) {
    // 成功被唤醒
    Frame frame = rx_queue_.front();
    rx_queue_.pop();
    lock.unlock();

    // 将Frame的X-Slot协议数据复制到缓冲区
    // 注意：我们的X-Slot协议帧是在AT命令的DATA荷载中传输的
    // URC回调中解析的应该是X-Slot帧，而不是原始AT数据
    size_t copy_len = std::min(
        static_cast<size_t>(frame.len) + HEADER_SIZE + CRC_SIZE, max_len);
    // TODO: 假设buffer需要的是完整的X-Slot帧
    // 这里需要一个Frame -> 原始字节流的函数
    // 假设 `onURCReceived` 中推入的就是 `XSlotFrame`

    // 重新审视: `onURCReceived` 收到的 `+NNMI` 的DATA部分 *就是* X-Slot帧
    // 所以我们应该在 `onURCReceived` 中解析X-Slot帧
    // `rx_queue_` 中存放的应该是 `XSlotFrame`

    // 假设 `MessageCodec::encode` 可以将 frame 转为字节流
    std::vector<uint8_t> frame_bytes = MessageCodec::encode(frame);
    size_t copy_bytes = std::min(frame_bytes.size(), max_len);
    memcpy(buffer, frame_bytes.data(), copy_bytes);
    return (int)copy_bytes;

  } else {
    // 超时
    return 0;
  }
}

bool ATTransport::configure(uint16_t addr, uint8_t cell_id, int8_t power_dbm) {
  local_addr_ = addr;
  std::vector<std::string> response;

  std::stringstream ss;
  // 1. 配置地址
  ss << "AT+ADDR=" << std::hex << std::uppercase << std::setw(4)
     << std::setfill('0') << addr;
  if (!at_handler_->sendCommandSync(ss.str(), response, 2000)) {
    std::cerr << "[AT] Failed to configure address" << std::endl;
    return false;
  }

  // 2. 配置小区ID
  ss.str("");
  ss << "AT+CELL=" << (int)cell_id;
  if (!at_handler_->sendCommandSync(ss.str(), response, 2000)) {
    std::cerr << "[AT] Failed to configure cell" << std::endl;
    return false;
  }

  // 3. 配置功率
  ss.str("");
  ss << "AT+PWR=" << (int)power_dbm;
  if (!at_handler_->sendCommandSync(ss.str(), response, 2000)) {
    std::cerr << "[AT] Failed to configure power" << std::endl;
    return false;
  }

  // 4. 配置为常接收模式(LP=3, TypeD)
  ss.str("");
  ss << "AT+LP=3";
  if (!at_handler_->sendCommandSync(ss.str(), response, 2000)) {
    std::cerr << "[AT] Failed to configure low power mode" << std::endl;
    return false;
  }

  // 5. 配置速率为76800
  ss.str("");
  ss << "AT+BPS=CTRL,76800";
  if (!at_handler_->sendCommandSync(ss.str(), response, 2000))
    return false;
  ss.str("");
  ss << "AT+BPS=DATA,76800";
  if (!at_handler_->sendCommandSync(ss.str(), response, 2000))
    return false;

  std::cout << "[AT] Configuration completed" << std::endl;
  return true;
}

// [ATHandler RX 线程调用]
void ATTransport::onURCReceived(const std::string &urc) {
  // 解析 +NNMI:<SRC>,<DEST>,<RSSI>,<LEN>,<DATA>
  if (urc.rfind("+NNMI:", 0) == 0) {
    try {
      std::string data_part = urc.substr(6);
      std::string segment;
      std::stringstream ss(data_part);
      std::vector<std::string> parts;
      while (std::getline(ss, segment, ',')) {
        parts.push_back(segment);
      }

      if (parts.size() == 5) {
        std::string data_hex = parts[4];
        std::vector<uint8_t> frame_bytes = hexToBytes(data_hex);

        // DATA部分就是我们的X-Slot帧
        Frame frame;
        if (MessageCodec::decode(frame_bytes.data(), frame_bytes.size(),
                                 frame)) {
          // 解码成功，推入接收队列
          hal::LockGuard lock(rx_queue_mutex_);
          rx_queue_.push(frame);
          rx_queue_cv_.notify_one(); // 唤醒等待的 receive()
        } else {
          std::cerr << "[AT] URC: Received frame with bad CRC" << std::endl;
        }
      }
    } catch (...) {
      std::cerr << "[AT] URC: Failed to parse +NNMI" << std::endl;
    }
  }
  // 还可以处理 +SEND:, +ROUTE: 等 URC
}

// --- 序列化辅助函数 ---

std::string ATTransport::bytesToHex(const uint8_t *data, size_t len) {
  std::stringstream ss;
  ss << std::hex << std::uppercase << std::setfill('0');
  for (size_t i = 0; i < len; i++) {
    ss << std::setw(2) << (int)data[i];
  }
  return ss.str();
}

std::vector<uint8_t> ATTransport::hexToBytes(const std::string &hex) {
  std::vector<uint8_t> bytes;
  for (size_t i = 0; i < hex.length(); i += 2) {
    std::string byteString = hex.substr(i, 2);
    bytes.push_back((uint8_t)strtol(byteString.c_str(), nullptr, 16));
  }
  return bytes;
}

} // namespace xslot