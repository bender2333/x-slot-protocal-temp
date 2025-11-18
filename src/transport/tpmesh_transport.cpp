// =============================================================================
// src/transport/tpmesh_transport.cpp - TPMesh传输层实现
// =============================================================================

#include "tpmesh_transport.h"
#include "../core/xslot_protocol.h"
#include <iostream>
#include <sstream>

namespace xslot {

// =============================================================================
// 构造/析构
// =============================================================================

TPMeshTransport::TPMeshTransport(const char *port, uint32_t baudrate)
    : local_addr_(0) {
  at_driver_ = std::make_unique<TPMeshATDriver>(port, baudrate);
}

TPMeshTransport::~TPMeshTransport() { close(); }

// =============================================================================
// ITransport接口实现
// =============================================================================

bool TPMeshTransport::open() {
  if (!at_driver_->open()) {
    return false;
  }

  // 注册URC回调
  at_driver_->setURCCallback(
      [this](const URCEvent &event) { this->onURCEvent(event); });

  return true;
}

void TPMeshTransport::close() {
  if (at_driver_) {
    at_driver_->close();
  }
}

bool TPMeshTransport::send(uint16_t dest, const uint8_t *data, size_t len) {
  if (!at_driver_ || !at_driver_->isOpen()) {
    return false;
  }

  // 使用UM模式（不需要确认）
  return at_driver_->sendData(dest, data, len, 0);
}

int TPMeshTransport::receive(uint8_t *buffer, size_t max_len,
                             uint32_t timeout_ms) {
  std::unique_lock<hal::Mutex> lock(rx_mutex_);

  // 等待队列非空或超时
  bool got_data = rx_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                  [this]() { return !rx_queue_.empty(); });

  if (!got_data) {
    return 0; // 超时
  }

  // 取出一帧
  Frame frame = rx_queue_.front();
  rx_queue_.pop();
  lock.unlock();

  // 编码为字节流
  std::vector<uint8_t> frame_bytes = MessageCodec::encode(frame);
  size_t copy_len = std::min(frame_bytes.size(), max_len);
  std::memcpy(buffer, frame_bytes.data(), copy_len);

  return static_cast<int>(copy_len);
}

xslot_slot_mode_t TPMeshTransport::detect() {
  if (at_driver_ && at_driver_->isOpen()) {
    return XSLOT_MODE_WIRELESS;
  }
  return XSLOT_MODE_EMPTY;
}

// =============================================================================
// 配置
// =============================================================================

bool TPMeshTransport::configure(uint16_t addr, uint8_t cell_id,
                                int8_t power_dbm) {
  if (!at_driver_ || !at_driver_->isOpen()) {
    std::cerr << "[TPMesh] Driver not open" << std::endl;
    return false;
  }

  local_addr_ = addr;

  // 1. 配置地址
  if (!at_driver_->configAddress(addr)) {
    std::cerr << "[TPMesh] Failed to configure address" << std::endl;
    return false;
  }

  // 2. 配置小区ID
  if (cell_id != 0xFF) {
    if (!at_driver_->configCell(cell_id)) {
      std::cerr << "[TPMesh] Failed to configure cell" << std::endl;
      return false;
    }
  }

  // 3. 配置发射功率
  if (!at_driver_->configPower(power_dbm)) {
    std::cerr << "[TPMesh] Failed to configure power" << std::endl;
    return false;
  }

  // 4. 配置为常接收模式 (TypeD)
  if (!at_driver_->configLowPower(3)) {
    std::cerr << "[TPMesh] Failed to configure low power mode" << std::endl;
    return false;
  }

  // 5. 配置空口速率为76.8Kbps
  if (!at_driver_->configBaudrate(76800, 76800)) {
    std::cerr << "[TPMesh] Failed to configure air baudrate" << std::endl;
    return false;
  }

  std::cout << "[TPMesh] Configuration completed:" << std::endl;
  std::cout << "  Address: 0x" << std::hex << addr << std::endl;
  std::cout << "  Cell ID: " << std::dec << static_cast<int>(cell_id)
            << std::endl;
  std::cout << "  Power: " << static_cast<int>(power_dbm) << " dBm"
            << std::endl;

  return true;
}

// =============================================================================
// 发送状态查询
// =============================================================================

SendStatus TPMeshTransport::getSendStatus(uint8_t sn) {
  if (sn == 0 || sn > 63) {
    return SendStatus::UNKNOWN;
  }

  hal::LockGuard lock(send_mutex_);
  return send_trackers_[sn - 1].status;
}

void TPMeshTransport::setRouteCallback(RouteCallback callback) {
  hal::LockGuard lock(route_mutex_);
  route_callback_ = callback;
}

// =============================================================================
// URC事件处理（核心）
// =============================================================================

void TPMeshTransport::onURCEvent(const URCEvent &event) {
  switch (event.type) {
  case URCType::NNMI: {
    // 数据接收
    Frame frame;
    if (parseNNMI(event.raw_line, frame)) {
      hal::LockGuard lock(rx_mutex_);
      rx_queue_.push(frame);
      rx_cv_.notify_one();
    }
    break;
  }

  case URCType::SEND: {
    // 发送状态
    uint8_t sn;
    SendStatus status;
    if (parseSEND(event.raw_line, sn, status)) {
      hal::LockGuard lock(send_mutex_);
      if (sn > 0 && sn <= 63) {
        send_trackers_[sn - 1].sn = sn;
        send_trackers_[sn - 1].status = status;
        send_trackers_[sn - 1].timestamp = hal::getTimestamp();
      }
    }
    break;
  }

  case URCType::ROUTE: {
    // 路由变化
    bool created;
    uint16_t addr;
    if (parseROUTE(event.raw_line, created, addr)) {
      hal::LockGuard lock(route_mutex_);
      if (route_callback_) {
        route_callback_(created, addr);
      }
    }
    break;
  }

  case URCType::ACK: {
    // 送达确认
    uint16_t src;
    int8_t rssi;
    uint8_t sn;
    if (parseACK(event.raw_line, src, rssi, sn)) {
      // TODO: 可以通知上层AM模式的送达确认
      std::cout << "[TPMesh] ACK from 0x" << std::hex << src
                << " RSSI=" << std::dec << static_cast<int>(rssi)
                << " SN=" << static_cast<int>(sn) << std::endl;
    }
    break;
  }

  case URCType::BOOT:
    std::cout << "[TPMesh] Module rebooted" << std::endl;
    break;

  case URCType::READY:
    std::cout << "[TPMesh] Module ready" << std::endl;
    break;

  case URCType::FLOOD: {
    // 泛洪数据接收
    // TODO: 根据需要处理
    std::cout << "[TPMesh] Flood data: " << event.raw_line << std::endl;
    break;
  }

  default:
    // 未知URC
    std::cout << "[TPMesh] Unknown URC: " << event.raw_line << std::endl;
    break;
  }
}

// =============================================================================
// URC解析器
// =============================================================================

bool TPMeshTransport::parseNNMI(const std::string &urc, Frame &frame) {
  // 格式: +NNMI:<SRC>,<DEST>,<RSSI>,<LEN>,<DATA>
  // 示例: +NNMI:FFFE,0001,-80,10,AA00010001010A05...

  if (urc.rfind("+NNMI:", 0) != 0) {
    return false;
  }

  try {
    std::string data_part = urc.substr(6); // 去掉"+NNMI:"
    std::istringstream iss(data_part);
    std::string token;
    std::vector<std::string> parts;

    while (std::getline(iss, token, ',')) {
      parts.push_back(token);
    }

    if (parts.size() != 5) {
      std::cerr << "[TPMesh] Invalid NNMI format" << std::endl;
      return false;
    }

    // 解析字段
    // uint16_t src = std::stoul(parts[0], nullptr, 16);
    // uint16_t dest = std::stoul(parts[1], nullptr, 16);
    // int8_t rssi = std::stoi(parts[2]);
    // size_t len = std::stoul(parts[3]);
    std::string data_hex = parts[4];

    // 将HEX字符串转为字节流
    std::vector<uint8_t> frame_bytes = hexToBytes(data_hex);

    // 解码X-Slot帧
    if (!MessageCodec::decode(frame_bytes.data(), frame_bytes.size(), frame)) {
      std::cerr << "[TPMesh] Failed to decode X-Slot frame" << std::endl;
      return false;
    }

    return true;

  } catch (const std::exception &e) {
    std::cerr << "[TPMesh] Exception parsing NNMI: " << e.what() << std::endl;
    return false;
  }
}

bool TPMeshTransport::parseSEND(const std::string &urc, uint8_t &sn,
                                SendStatus &status) {
  // 格式: +SEND:<SN>,<RESULT>
  // 示例: +SEND:10,SEND OK

  if (urc.rfind("+SEND:", 0) != 0) {
    return false;
  }

  try {
    std::string data_part = urc.substr(6);
    size_t comma_pos = data_part.find(',');
    if (comma_pos == std::string::npos) {
      return false;
    }

    sn = std::stoi(data_part.substr(0, comma_pos));
    std::string status_str = data_part.substr(comma_pos + 1);
    status = parseStatus(status_str);

    return true;

  } catch (const std::exception &e) {
    std::cerr << "[TPMesh] Exception parsing SEND: " << e.what() << std::endl;
    return false;
  }
}

bool TPMeshTransport::parseROUTE(const std::string &urc, bool &created,
                                 uint16_t &addr) {
  // 格式: +ROUTE:CREATE ADDR[0xFFFE]
  //       +ROUTE:DELETE ADDR[0xFFFE]

  if (urc.rfind("+ROUTE:", 0) != 0) {
    return false;
  }

  try {
    created = (urc.find("CREATE") != std::string::npos);

    size_t addr_pos = urc.find("ADDR[0x");
    if (addr_pos == std::string::npos) {
      return false;
    }

    std::string addr_str = urc.substr(addr_pos + 7, 4);
    addr = std::stoul(addr_str, nullptr, 16);

    return true;

  } catch (const std::exception &e) {
    std::cerr << "[TPMesh] Exception parsing ROUTE: " << e.what() << std::endl;
    return false;
  }
}

bool TPMeshTransport::parseACK(const std::string &urc, uint16_t &src,
                               int8_t &rssi, uint8_t &sn) {
  // 格式: +ACK:<SRC>,<RSSI>,<SN>
  // 示例: +ACK:FFFE,-80,10

  if (urc.rfind("+ACK:", 0) != 0) {
    return false;
  }

  try {
    std::string data_part = urc.substr(5);
    std::istringstream iss(data_part);
    std::string token;
    std::vector<std::string> parts;

    while (std::getline(iss, token, ',')) {
      parts.push_back(token);
    }

    if (parts.size() != 3) {
      return false;
    }

    src = std::stoul(parts[0], nullptr, 16);
    rssi = std::stoi(parts[1]);
    sn = std::stoi(parts[2]);

    return true;

  } catch (const std::exception &e) {
    std::cerr << "[TPMesh] Exception parsing ACK: " << e.what() << std::endl;
    return false;
  }
}

// =============================================================================
// 辅助函数
// =============================================================================

std::vector<uint8_t> TPMeshTransport::hexToBytes(const std::string &hex) {
  std::vector<uint8_t> bytes;
  for (size_t i = 0; i < hex.length(); i += 2) {
    if (i + 1 < hex.length()) {
      std::string byte_str = hex.substr(i, 2);
      bytes.push_back(static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16)));
    }
  }
  return bytes;
}

SendStatus TPMeshTransport::parseStatus(const std::string &status_str) {
  if (status_str == "HANDLE OK") {
    return SendStatus::HANDLE_OK;
  } else if (status_str == "HANDLE ERROR") {
    return SendStatus::HANDLE_ERROR;
  } else if (status_str == "PREPARE") {
    return SendStatus::PREPARE;
  } else if (status_str == "SEND OK") {
    return SendStatus::SEND_OK;
  } else if (status_str == "SEND ERROR") {
    return SendStatus::SEND_ERROR;
  } else if (status_str == "JOINING") {
    return SendStatus::JOINING;
  } else if (status_str == "ROUTE FULL") {
    return SendStatus::ROUTE_FULL;
  } else {
    return SendStatus::UNKNOWN;
  }
}

} // namespace xslot
