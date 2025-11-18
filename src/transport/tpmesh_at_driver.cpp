// =============================================================================
// src/transport/tpmesh_at_driver.cpp - TPMesh AT命令驱动层实现
// =============================================================================

#include "tpmesh_at_driver.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace xslot {

// =============================================================================
// 构造/析构
// =============================================================================

TPMeshATDriver::TPMeshATDriver(const char *port, uint32_t baudrate)
    : port_(port), baudrate_(baudrate), running_(false), state_(State::IDLE),
      response_ok_(false) {}

TPMeshATDriver::~TPMeshATDriver() { close(); }

// =============================================================================
// 打开/关闭
// =============================================================================

bool TPMeshATDriver::open() {
  if (serial_ && serial_->isOpen()) {
    return true;
  }

  // 打开串口
  serial_ = std::make_unique<hal::SerialPort>(port_.c_str(), baudrate_);
  if (!serial_->open()) {
    std::cerr << "[TPMesh] Failed to open serial port: " << port_ << std::endl;
    serial_.reset();
    return false;
  }

  std::cout << "[TPMesh] Serial opened: " << port_ << " @ " << baudrate_
            << " bps" << std::endl;

  // 启动接收线程
  running_ = true;
  rx_thread_ =
      std::make_unique<hal::Thread>([this]() { this->rxThreadFunc(); });
  rx_thread_->start();

  // 等待模块稳定
  hal::sleep(100);

  // 测试连接
  if (!testConnection()) {
    std::cerr << "[TPMesh] Module not responding to AT" << std::endl;
    close();
    return false;
  }

  std::cout << "[TPMesh] Module is ready" << std::endl;
  return true;
}

void TPMeshATDriver::close() {
  running_ = false;

  // 等待接收线程退出
  if (rx_thread_ && rx_thread_->isRunning()) {
    rx_thread_->join();
  }
  rx_thread_.reset();

  // 关闭串口
  if (serial_ && serial_->isOpen()) {
    serial_->close();
  }
  serial_.reset();
}

bool TPMeshATDriver::isOpen() const { return serial_ && serial_->isOpen(); }

// =============================================================================
// 同步AT命令
// =============================================================================

bool TPMeshATDriver::sendCommand(const std::string &cmd,
                                 std::vector<std::string> &response_lines,
                                 uint32_t timeout_ms) {
  std::unique_lock<hal::Mutex> lock(sync_mutex_);

  // 检查状态
  if (state_ != State::IDLE) {
    std::cerr << "[TPMesh] AT driver is busy" << std::endl;
    return false;
  }

  // 准备状态
  state_ = State::WAITING_RESPONSE;
  response_buffer_.clear();
  response_ok_ = false;

  // 发送命令
  if (!writeLine(cmd)) {
    state_ = State::IDLE;
    return false;
  }

  // 等待响应（OK或ERROR）
  bool got_response =
      sync_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                        [this]() { return state_ == State::IDLE; });

  if (!got_response) {
    std::cerr << "[TPMesh] Command timeout: " << cmd << std::endl;
    state_ = State::IDLE;
    return false;
  }

  // 复制响应
  response_lines = response_buffer_;
  return response_ok_;
}

bool TPMeshATDriver::sendCommand(const std::string &cmd, uint32_t timeout_ms) {
  std::vector<std::string> dummy;
  return sendCommand(cmd, dummy, timeout_ms);
}

// =============================================================================
// URC回调设置
// =============================================================================

void TPMeshATDriver::setURCCallback(URCCallback callback) {
  hal::LockGuard lock(urc_mutex_);
  urc_callback_ = callback;
}

// =============================================================================
// 常用AT命令封装
// =============================================================================

bool TPMeshATDriver::testConnection() { return sendCommand("AT", 1000); }

bool TPMeshATDriver::queryVersion(std::string &version) {
  std::vector<std::string> response;
  if (!sendCommand("AT+VER?", response)) {
    return false;
  }
  if (!response.empty()) {
    version = response[0];
  }
  return true;
}

bool TPMeshATDriver::queryESN(std::string &esn) {
  std::vector<std::string> response;
  if (!sendCommand("AT+ESN?", response)) {
    return false;
  }
  if (!response.empty()) {
    esn = response[0];
  }
  return true;
}

bool TPMeshATDriver::reboot() { return sendCommand("AT+REBOOT"); }

bool TPMeshATDriver::configAddress(uint16_t addr, uint16_t group_addr) {
  std::ostringstream oss;
  oss << "AT+ADDR=" << std::hex << std::uppercase << std::setw(4)
      << std::setfill('0') << addr;

  if (group_addr != 0) {
    oss << "," << std::setw(4) << std::setfill('0') << group_addr;
  }

  return sendCommand(oss.str());
}

bool TPMeshATDriver::configCell(uint8_t cell_id) {
  std::ostringstream oss;
  oss << "AT+CELL=" << static_cast<int>(cell_id);
  return sendCommand(oss.str());
}

bool TPMeshATDriver::configPower(int8_t dbm) {
  std::ostringstream oss;
  oss << "AT+PWR=" << static_cast<int>(dbm);
  return sendCommand(oss.str());
}

bool TPMeshATDriver::configWakeup(uint16_t period_ms) {
  std::ostringstream oss;
  oss << "AT+WOR=" << period_ms;
  return sendCommand(oss.str());
}

bool TPMeshATDriver::configBaudrate(uint32_t ctrl_bps, uint32_t data_bps) {
  std::ostringstream oss;

  // 配置控制信道速率
  oss << "AT+BPS=CTRL," << ctrl_bps;
  if (!sendCommand(oss.str())) {
    return false;
  }

  // 配置数据信道速率
  oss.str("");
  oss << "AT+BPS=DATA," << data_bps;
  return sendCommand(oss.str());
}

bool TPMeshATDriver::configLowPower(uint8_t mode) {
  std::ostringstream oss;
  oss << "AT+LP=" << static_cast<int>(mode);
  return sendCommand(oss.str());
}

bool TPMeshATDriver::configAwake(uint16_t awake_ms) {
  std::ostringstream oss;
  oss << "AT+AWAKE=" << awake_ms;
  return sendCommand(oss.str());
}

bool TPMeshATDriver::queryAddress(uint16_t &addr, uint16_t &group_addr,
                                  bool &is_hub) {
  std::vector<std::string> response;
  if (!sendCommand("AT+ADDR?", response)) {
    return false;
  }

  // 解析响应
  // ROOT[1]
  // ADDR[0x0001]
  // GROUP_ADDR[0xFFAA]
  for (const auto &line : response) {
    if (line.find("ROOT[") != std::string::npos) {
      size_t pos = line.find("[");
      if (pos != std::string::npos) {
        is_hub = (line[pos + 1] == '1');
      }
    } else if (line.find("ADDR[0x") != std::string::npos) {
      size_t pos = line.find("[0x");
      if (pos != std::string::npos) {
        std::string addr_str = line.substr(pos + 3, 4);
        addr = static_cast<uint16_t>(std::stoul(addr_str, nullptr, 16));
      }
    } else if (line.find("GROUP_ADDR[0x") != std::string::npos) {
      size_t pos = line.find("[0x");
      if (pos != std::string::npos) {
        std::string gaddr_str = line.substr(pos + 3, 4);
        group_addr = static_cast<uint16_t>(std::stoul(gaddr_str, nullptr, 16));
      }
    }
  }

  return true;
}

bool TPMeshATDriver::queryCell(uint8_t &cell_id) {
  std::vector<std::string> response;
  if (!sendCommand("AT+CELL?", response)) {
    return false;
  }

  if (!response.empty()) {
    // 响应格式: +CELL:255
    size_t pos = response[0].find(":");
    if (pos != std::string::npos) {
      cell_id = static_cast<uint8_t>(std::stoi(response[0].substr(pos + 1)));
    }
  }

  return true;
}

bool TPMeshATDriver::queryPower(int8_t &dbm) {
  std::vector<std::string> response;
  if (!sendCommand("AT+PWR?", response)) {
    return false;
  }

  if (!response.empty()) {
    // 响应格式: +PWR:15
    size_t pos = response[0].find(":");
    if (pos != std::string::npos) {
      dbm = static_cast<int8_t>(std::stoi(response[0].substr(pos + 1)));
    }
  }

  return true;
}

bool TPMeshATDriver::sendData(uint16_t dest, const uint8_t *data, size_t len,
                              uint8_t type) {
  if (len > 400) {
    std::cerr << "[TPMesh] Data too long: " << len << " (max 400)" << std::endl;
    return false;
  }

  std::ostringstream oss;
  oss << "AT+SEND=" << std::hex << std::uppercase << std::setw(4)
      << std::setfill('0') << dest << "," << std::dec << len << ","
      << bytesToHex(data, len) << "," << static_cast<int>(type);

  // AT+SEND 会立即返回OK，实际发送状态通过+SEND: URC异步通知
  return sendCommand(oss.str(), 5000);
}

// =============================================================================
// 接收线程
// =============================================================================

void TPMeshATDriver::rxThreadFunc() {
  while (running_) {
    std::string line;
    if (readLine(line, 200)) {
      std::cout << "[TPMesh RX] " << line << std::endl;
      processLine(line);
    }
  }
}

// =============================================================================
// 行处理逻辑（核心状态机）
// =============================================================================

void TPMeshATDriver::processLine(const std::string &line) {
  if (line.empty()) {
    return;
  }

  // 根据当前状态处理
  hal::LockGuard lock(sync_mutex_);

  if (state_ == State::WAITING_RESPONSE) {
    // 等待同步命令响应

    if (line.find("OK") != std::string::npos) {
      // 命令成功
      response_ok_ = true;
      state_ = State::IDLE;
      sync_cv_.notify_one();

    } else if (line.find("ERROR") != std::string::npos) {
      // 命令失败
      response_ok_ = false;
      state_ = State::IDLE;
      sync_cv_.notify_one();

    } else {
      // 响应数据行（加入缓冲区）
      response_buffer_.push_back(line);
    }

  } else {
    // IDLE状态：识别URC事件

    if (line[0] == '+') {
      // 这是一个URC
      URCType type = identifyURC(line);
      URCEvent event(type, line);

      // 通知回调
      hal::LockGuard urc_lock(urc_mutex_);
      if (urc_callback_) {
        urc_callback_(event);
      }
    }
    // 其他行忽略（可能是echo、空行等）
  }
}

// =============================================================================
// URC识别
// =============================================================================

URCType TPMeshATDriver::identifyURC(const std::string &line) {
  if (line.rfind("+NNMI:", 0) == 0) {
    return URCType::NNMI;
  } else if (line.rfind("+SEND:", 0) == 0) {
    return URCType::SEND;
  } else if (line.rfind("+ROUTE:", 0) == 0) {
    return URCType::ROUTE;
  } else if (line.rfind("+ACK:", 0) == 0) {
    return URCType::ACK;
  } else if (line.rfind("+FLOOD:", 0) == 0) {
    return URCType::FLOOD;
  } else if (line.rfind("+BOOT", 0) == 0) {
    return URCType::BOOT;
  } else if (line.rfind("+READY", 0) == 0) {
    return URCType::READY;
  } else if (line.rfind("+SUSPEND", 0) == 0) {
    return URCType::SUSPEND;
  } else if (line.rfind("+RESUME", 0) == 0) {
    return URCType::RESUME;
  } else {
    return URCType::UNKNOWN;
  }
}

// =============================================================================
// 串口读写
// =============================================================================

bool TPMeshATDriver::readLine(std::string &line, uint32_t timeout_ms) {
  line.clear();

  if (!serial_ || !serial_->isOpen()) {
    return false;
  }

  std::string buffer;
  uint32_t start = hal::getTimestamp();

  while (hal::getTimestamp() - start < timeout_ms) {
    uint8_t ch;
    int ret = serial_->read(&ch, 1, 10);

    if (ret == 1) {
      if (ch == '\n') {
        // 行结束
        if (!buffer.empty() && buffer.back() == '\r') {
          buffer.pop_back();
        }
        if (!buffer.empty()) {
          line = buffer;
          return true;
        }
      } else {
        buffer += static_cast<char>(ch);
      }
    }
  }

  return false;
}

bool TPMeshATDriver::writeLine(const std::string &cmd) {
  if (!serial_ || !serial_->isOpen()) {
    return false;
  }

  std::string full_cmd = cmd + "\r\n";
  std::cout << "[TPMesh TX] " << cmd << std::endl;

  int written = serial_->write(
      reinterpret_cast<const uint8_t *>(full_cmd.c_str()), full_cmd.length());

  return (written == static_cast<int>(full_cmd.length()));
}

// =============================================================================
// 辅助函数
// =============================================================================

std::string TPMeshATDriver::bytesToHex(const uint8_t *data, size_t len) {
  std::ostringstream oss;
  oss << std::hex << std::uppercase << std::setfill('0');
  for (size_t i = 0; i < len; i++) {
    oss << std::setw(2) << static_cast<int>(data[i]);
  }
  return oss.str();
}

} // namespace xslot
