// =============================================================================
// src/transport/xslot_at_transport.cpp - AT指令传输层完整实现
// =============================================================================

#include "xslot_at_transport.h"
#include "../hal/xslot_hal.h"
#include "xslot_protocol.h"
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>


namespace xslot {

// =============================================================================
// ATCommandHandler 实现
// =============================================================================

ATCommandHandler::ATCommandHandler(const char *port, uint32_t baudrate)
    : serial_handle_(nullptr), port_(port), baudrate_(baudrate) {}

ATCommandHandler::~ATCommandHandler() { close(); }

bool ATCommandHandler::open() {
  if (serial_handle_)
    return true;

  auto serial = new hal::SerialPort(port_.c_str(), baudrate_);
  if (!serial->open()) {
    delete serial;
    std::cerr << "[AT] Failed to open serial port: " << port_ << std::endl;
    return false;
  }

  serial_handle_ = serial;
  std::cout << "[AT] Serial port opened: " << port_ << " @ " << baudrate_
            << " bps" << std::endl;

  // 等待模块启动
  hal::sleep(100);

  // 测试AT连接
  if (!test()) {
    std::cerr << "[AT] Module not responding" << std::endl;
    close();
    return false;
  }

  return true;
}

void ATCommandHandler::close() {
  if (serial_handle_) {
    auto serial = static_cast<hal::SerialPort *>(serial_handle_);
    serial->close();
    delete serial;
    serial_handle_ = nullptr;
  }
}

bool ATCommandHandler::sendCommand(const char *cmd, std::string &response,
                                   uint32_t timeout_ms) {
  if (!serial_handle_)
    return false;

  std::lock_guard<std::mutex> lock(mutex_);
  auto serial = static_cast<hal::SerialPort *>(serial_handle_);

  // 清空缓冲区
  serial->flush();

  // 发送命令
  std::string full_cmd = std::string(cmd) + "\r\n";
  serial->write(reinterpret_cast<const uint8_t *>(full_cmd.c_str()),
                full_cmd.length());

  std::cout << "[AT TX] " << cmd << std::endl;

  // 读取响应
  uint32_t start = hal::getTimestamp();
  std::string buffer;

  while (hal::getTimestamp() - start < timeout_ms) {
    std::string line;
    if (readLine(line, 100)) {
      std::cout << "[AT RX] " << line << std::endl;

      // 检查是否是命令回显（跳过）
      if (line.find("AT+") == 0)
        continue;

      // 检查结果
      if (line.find("OK") != std::string::npos) {
        response = buffer;
        return true;
      }
      if (line.find("ERROR") != std::string::npos) {
        response = line;
        return false;
      }

      // URC或数据行
      buffer += line + "\n";
    }
  }

  std::cerr << "[AT] Command timeout: " << cmd << std::endl;
  return false;
}

bool ATCommandHandler::test() {
  std::string response;
  return sendCommand("AT", response, 1000);
}

bool ATCommandHandler::configAddress(uint16_t addr, uint16_t group_addr) {
  std::stringstream ss;
  ss << "AT+ADDR=" << std::hex << std::uppercase << std::setw(4)
     << std::setfill('0') << addr;

  if (group_addr != 0) {
    ss << "," << std::setw(4) << std::setfill('0') << group_addr;
  }

  std::string response;
  return sendCommand(ss.str().c_str(), response);
}

bool ATCommandHandler::configCell(uint8_t cell_id) {
  std::stringstream ss;
  ss << "AT+CELL=" << (int)cell_id;

  std::string response;
  return sendCommand(ss.str().c_str(), response);
}

bool ATCommandHandler::configPower(int8_t dbm) {
  std::stringstream ss;
  ss << "AT+PWR=" << (int)dbm;

  std::string response;
  return sendCommand(ss.str().c_str(), response);
}

bool ATCommandHandler::configWakeup(uint16_t period_ms) {
  std::stringstream ss;
  ss << "AT+WOR=" << period_ms;

  std::string response;
  return sendCommand(ss.str().c_str(), response);
}

bool ATCommandHandler::configBaudrate(uint32_t ctrl_bps, uint32_t data_bps) {
  std::stringstream ss;
  ss << "AT+BPS=CTRL," << ctrl_bps;

  std::string response;
  if (!sendCommand(ss.str().c_str(), response))
    return false;

  ss.str("");
  ss << "AT+BPS=DATA," << data_bps;
  return sendCommand(ss.str().c_str(), response);
}

bool ATCommandHandler::configLowPower(uint8_t mode) {
  std::stringstream ss;
  ss << "AT+LP=" << (int)mode;

  std::string response;
  return sendCommand(ss.str().c_str(), response);
}

bool ATCommandHandler::queryAddress(uint16_t &addr, uint16_t &group_addr,
                                    bool &is_hub) {
  std::string response;
  if (!sendCommand("AT+ADDR?", response))
    return false;

  // 解析响应：
  // ROOT[1]
  // ADDR[0x0001]
  // GROUP_ADDR[0xFFAA]

  size_t pos;

  // 解析ROOT
  pos = response.find("ROOT[");
  if (pos != std::string::npos) {
    is_hub = (response[pos + 5] == '1');
  }

  // 解析ADDR
  pos = response.find("ADDR[0x");
  if (pos != std::string::npos) {
    std::string addr_str = response.substr(pos + 7, 4);
    addr = std::stoi(addr_str, nullptr, 16);
  }

  // 解析GROUP_ADDR
  pos = response.find("GROUP_ADDR[0x");
  if (pos != std::string::npos) {
    std::string gaddr_str = response.substr(pos + 13, 4);
    group_addr = std::stoi(gaddr_str, nullptr, 16);
  }

  return true;
}

bool ATCommandHandler::queryVersion(std::string &version) {
  std::string response;
  if (!sendCommand("AT+VER?", response))
    return false;

  version = response;
  return true;
}

bool ATCommandHandler::queryESN(std::string &esn) {
  std::string response;
  if (!sendCommand("AT+ESN?", response))
    return false;

  esn = response;
  return true;
}

bool ATCommandHandler::sendData(uint16_t dest, const uint8_t *data, size_t len,
                                uint8_t type) {
  if (len > 400)
    return false; // TP1107限制

  // AT+SEND=<ADDR>,<LEN>,<DATA>,<TYPE>
  std::stringstream ss;
  ss << "AT+SEND=" << std::hex << std::uppercase << std::setw(4)
     << std::setfill('0') << dest << "," << std::dec << len << ","
     << hexToString(data, len) << "," << (int)type;

  std::string response;
  return sendCommand(ss.str().c_str(), response, 5000); // 5秒超时
}

bool ATCommandHandler::parseURC(const std::string &urc, Frame &frame) {
  // 解析 +NNMI:<SRC>,<DEST>,<RSSI>,<LEN>,<DATA>
  if (urc.find("+NNMI:") == 0) {
    std::istringstream ss(urc.substr(6));
    std::string token;

    // 解析SRC
    if (!std::getline(ss, token, ','))
      return false;
    frame.from = std::stoi(token, nullptr, 16);

    // 解析DEST
    if (!std::getline(ss, token, ','))
      return false;
    frame.to = std::stoi(token, nullptr, 16);

    // 解析RSSI（跳过）
    if (!std::getline(ss, token, ','))
      return false;

    // 解析LEN
    if (!std::getline(ss, token, ','))
      return false;
    int data_len = std::stoi(token);

    // 解析DATA
    if (!std::getline(ss, token))
      return false;
    size_t hex_len = token.length();
    if (!stringToHex(token, frame.data, hex_len))
      return false;

    frame.len = hex_len;
    frame.sync = SYNC_BYTE;
    frame.cmd = XSLOT_CMD_REPORT; // 默认为REPORT

    return true;
  }

  return false;
}

bool ATCommandHandler::readLine(std::string &line, uint32_t timeout_ms) {
  if (!serial_handle_)
    return false;

  auto serial = static_cast<hal::SerialPort *>(serial_handle_);
  uint32_t start = hal::getTimestamp();
  line.clear();

  while (hal::getTimestamp() - start < timeout_ms) {
    uint8_t ch;
    int ret = serial->read(&ch, 1, 10);

    if (ret == 1) {
      if (ch == '\n') {
        // 移除末尾的\r
        if (!line.empty() && line.back() == '\r') {
          line.pop_back();
        }
        return !line.empty();
      } else if (ch != '\r') {
        line += (char)ch;
      }
    }
  }

  return false;
}

std::string ATCommandHandler::hexToString(const uint8_t *data, size_t len) {
  std::stringstream ss;
  ss << std::hex << std::uppercase << std::setfill('0');
  for (size_t i = 0; i < len; i++) {
    ss << std::setw(2) << (int)data[i];
  }
  return ss.str();
}

bool ATCommandHandler::stringToHex(const std::string &str, uint8_t *data,
                                   size_t &len) {
  if (str.length() % 2 != 0)
    return false;

  len = str.length() / 2;
  for (size_t i = 0; i < len; i++) {
    std::string byte_str = str.substr(i * 2, 2);
    data[i] = std::stoi(byte_str, nullptr, 16);
  }
  return true;
}

// =============================================================================
// ATTransport 实现
// =============================================================================

ATTransport::ATTransport(const char *port, uint32_t baudrate) : local_addr_(0) {
  at_handler_ = std::make_unique<ATCommandHandler>(port, baudrate);
}

ATTransport::~ATTransport() { close(); }

bool ATTransport::open() { return at_handler_->open(); }

void ATTransport::close() { at_handler_->close(); }

bool ATTransport::send(uint16_t dest, const uint8_t *data, size_t len) {
  // 使用UM类型（不需要确认）快速发送
  return at_handler_->sendData(dest, data, len, 0);
}

int ATTransport::receive(uint8_t *buffer, size_t max_len, uint32_t timeout_ms) {
  // 注意：AT传输层的接收需要通过URC异步处理
  // 这里简化实现，实际应该有URC处理线程
  hal::sleep(timeout_ms);
  return 0;
}

xslot_slot_mode_t ATTransport::detect() {
  // 发送AT命令测试
  if (at_handler_->test()) {
    return XSLOT_MODE_WIRELESS;
  }
  return XSLOT_MODE_EMPTY;
}

bool ATTransport::configure(uint16_t addr, uint8_t cell_id, int8_t power_dbm) {
  local_addr_ = addr;

  // 配置地址
  if (!at_handler_->configAddress(addr)) {
    std::cerr << "[AT] Failed to configure address" << std::endl;
    return false;
  }

  // 配置小区ID
  if (cell_id != 0xFF) {
    if (!at_handler_->configCell(cell_id)) {
      std::cerr << "[AT] Failed to configure cell" << std::endl;
      return false;
    }
  }

  // 配置发射功率
  if (!at_handler_->configPower(power_dbm)) {
    std::cerr << "[AT] Failed to configure power" << std::endl;
    return false;
  }

  // 配置为常接收模式(LP=3, TypeD)
  if (!at_handler_->configLowPower(3)) {
    std::cerr << "[AT] Failed to configure low power mode" << std::endl;
    return false;
  }

  // 配置通信速率为76800bps
  if (!at_handler_->configBaudrate(76800, 76800)) {
    std::cerr << "[AT] Failed to configure baudrate" << std::endl;
    return false;
  }

  std::cout << "[AT] Configuration completed:" << std::endl;
  std::cout << "  Address: 0x" << std::hex << addr << std::endl;
  std::cout << "  Cell ID: " << std::dec << (int)cell_id << std::endl;
  std::cout << "  Power: " << (int)power_dbm << " dBm" << std::endl;

  return true;
}
