// =============================================================================
// src/transport/xslot_at_transport.h - AT指令传输层
// =============================================================================

#ifndef XSLOT_AT_TRANSPORT_H
#define XSLOT_AT_TRANSPORT_H

#include "xslot_transport.h"
#include <mutex>
#include <queue>
#include <string>


namespace xslot {

// AT命令处理器
class ATCommandHandler {
public:
  ATCommandHandler(const char *port, uint32_t baudrate);
  ~ATCommandHandler();

  bool open();
  void close();

  // 基础AT命令
  bool sendCommand(const char *cmd, std::string &response,
                   uint32_t timeout_ms = 1000);
  bool test(); // AT

  // 配置命令
  bool configAddress(uint16_t addr, uint16_t group_addr = 0);
  bool configCell(uint8_t cell_id);
  bool configPower(int8_t dbm);
  bool configWakeup(uint16_t period_ms);
  bool configBaudrate(uint32_t ctrl_bps, uint32_t data_bps);
  bool configLowPower(uint8_t mode); // 2=TypeC, 3=TypeD

  // 查询命令
  bool queryAddress(uint16_t &addr, uint16_t &group_addr, bool &is_hub);
  bool queryVersion(std::string &version);
  bool queryESN(std::string &esn);

  // 数据发送命令 (AT+SEND)
  bool sendData(uint16_t dest, const uint8_t *data, size_t len, uint8_t type);

  // 解析URC
  bool parseURC(const std::string &urc, Frame &frame);

private:
  void *serial_handle_; // 串口句柄
  std::string port_;
  uint32_t baudrate_;
  std::mutex mutex_;
  std::queue<std::string> urc_queue_;

  bool readLine(std::string &line, uint32_t timeout_ms);
  bool waitForResponse(const char *expected, uint32_t timeout_ms);
  std::string hexToString(const uint8_t *data, size_t len);
  bool stringToHex(const std::string &str, uint8_t *data, size_t &len);
};

// AT传输层实现
class ATTransport : public ITransport {
public:
  ATTransport(const char *port, uint32_t baudrate);
  ~ATTransport() override;

  bool send(uint16_t dest, const uint8_t *data, size_t len) override;
  int receive(uint8_t *buffer, size_t max_len, uint32_t timeout_ms) override;
  xslot_slot_mode_t detect() override;
  bool open() override;
  void close() override;

  // 配置接口
  bool configure(uint16_t addr, uint8_t cell_id, int8_t power_dbm);

private:
  std::unique_ptr<ATCommandHandler> at_handler_;
  uint16_t local_addr_;
};

} // namespace xslot

#endif // XSLOT_AT_TRANSPORT_H