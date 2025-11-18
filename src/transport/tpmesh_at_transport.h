// =============================================================================
// src/transport/xslot_at_transport.h - AT指令传输层 (V2.0 异步重构)
// =============================================================================

#ifndef XSLOT_AT_TRANSPORT_H
#define XSLOT_AT_TRANSPORT_H

#include "../core/xslot_protocol.h" // 引入Frame
#include "../hal/xslot_hal.h"
#include "xslot_transport.h"
#include <atomic>
#include <condition_variable> // C++标准库(如果HAL没有提供)
#include <functional>
#include <memory>
#include <mutex> // C++标准库
#include <queue>
#include <string>
#include <vector>

namespace xslot {

// 内部AT命令处理器 (核心)
class ATCommandHandler {
public:
  // 回调类型
  using ResponseCallback =
      std::function<void(bool success, const std::vector<std::string> &lines)>;
  using URCCallback = std::function<void(const std::string &urc)>;

  ATCommandHandler(const char *port, uint32_t baudrate);
  ~ATCommandHandler();

  bool open();
  void close();

  // 异步发送命令 (用于 AT+SEND)
  // 这是"即发即忘" (fire-and-forget)，结果通过 URC (+SEND:...) 异步返回
  bool sendCommandAsync(const std::string &cmd);

  // 同步发送命令 (用于配置指令)
  bool sendCommandSync(const std::string &cmd,
                       std::vector<std::string> &response_lines,
                       uint32_t timeout_ms = 2000);

  // 设置URC回调
  void setURCCallback(URCCallback cb);

private:
  // 串口和HAL
  std::unique_ptr<hal::SerialPort> serial_;
  std::string port_;
  uint32_t baudrate_;

  // 线程和状态
  std::unique_ptr<hal::Thread> rx_thread_;
  std::atomic<bool> running_;

  // 状态机
  enum class State { IDLE, WAITING_RESPONSE };
  State state_;
  std::vector<std::string> response_buffer_;

  // 同步命令的同步机制
  hal::Mutex sync_mutex_; // 保护同步命令的执行
  std::condition_variable_any sync_cv_;
  bool sync_cmd_done_;
  bool sync_cmd_success_;

  // 异步命令（AT+SEND）队列
  hal::Mutex async_queue_mutex_;
  std::queue<std::string> async_cmd_queue_;

  // URC回调
  URCCallback urc_callback_;
  hal::Mutex urc_cb_mutex_;

  // 线程函数
  void rxThreadFunc();
  void processLine(const std::string &line);

  // 串口读写
  bool readLine(std::string &line, uint32_t timeout_ms);
  bool writeSerial(const std::string &cmd);
};

// AT传输层实现 (适配器)
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
  // 当ATHandler解析到URC时调用此函数
  void onURCReceived(const std::string &urc);
  std::string bytesToHex(const uint8_t *data, size_t len);
  std::vector<uint8_t> hexToBytes(const std::string &hex);

  std::unique_ptr<ATCommandHandler> at_handler_;
  uint16_t local_addr_;

  // 线程安全的接收队列 (由URC线程填充, 由XSlotManager线程消耗)
  std::queue<Frame> rx_queue_;
  hal::Mutex rx_queue_mutex_;
  std::condition_variable_any rx_queue_cv_; // 用于唤醒 receive()
};

} // namespace xslot

#endif // XSLOT_AT_TRANSPORT_H