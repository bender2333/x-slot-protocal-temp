// =============================================================================
// src/transport/tpmesh_at_driver.h - TPMesh AT命令驱动层 (纯AT抽象)
// =============================================================================
//
// 设计原则：
// 1. 只关心AT命令的收发，不关心业务逻辑
// 2. 区分"同步响应"和"URC异步事件"
// 3. 与串口层解耦，与传输层解耦
// 4. 不追踪发送状态，不维护业务队列
//
// =============================================================================

#ifndef TPMESH_AT_DRIVER_H
#define TPMESH_AT_DRIVER_H

#include "../hal/xslot_hal.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace xslot {

// =============================================================================
// URC事件类型枚举
// =============================================================================
enum class URCType {
  NNMI,    // +NNMI:<SRC>,<DEST>,<RSSI>,<LEN>,<DATA>  - 数据接收
  SEND,    // +SEND:<SN>,<RESULT>                     - 发送状态
  ROUTE,   // +ROUTE:CREATE/DELETE ADDR[<addr>]       - 路由变化
  ACK,     // +ACK:<SRC>,<RSSI>,<SN>                  - 送达确认
  FLOOD,   // +FLOOD:<SRC>,<LEN>,<DATA>               - 泛洪数据
  BOOT,    // +BOOT                                   - 模块重启
  READY,   // +READY                                  - AT接口就绪
  SUSPEND, // +SUSPEND                                - 进入休眠
  RESUME,  // +RESUME                                 - 退出休眠
  UNKNOWN  // 未知URC
};

// =============================================================================
// URC事件结构
// =============================================================================
struct URCEvent {
  URCType type;
  std::string raw_line; // 原始URC字符串

  URCEvent() : type(URCType::UNKNOWN) {}
  URCEvent(URCType t, const std::string &line) : type(t), raw_line(line) {}
};

// =============================================================================
// TPMesh AT命令驱动器 (纯AT抽象层)
// =============================================================================
class TPMeshATDriver {
public:
  // URC事件回调类型
  using URCCallback = std::function<void(const URCEvent &event)>;

  TPMeshATDriver(const char *port, uint32_t baudrate);
  ~TPMeshATDriver();

  // 打开/关闭串口
  bool open();
  void close();
  bool isOpen() const;

  // -------------------------------------------------------------------------
  // 同步AT命令 (阻塞等待OK/ERROR)
  // -------------------------------------------------------------------------

  // 发送AT命令，等待OK/ERROR，返回响应行
  // @param cmd: AT命令字符串（不含\r\n）
  // @param response_lines: 输出参数，存放响应行（不含OK/ERROR）
  // @param timeout_ms: 超时时间
  // @return true=成功(OK), false=失败(ERROR或超时)
  bool sendCommand(const std::string &cmd,
                   std::vector<std::string> &response_lines,
                   uint32_t timeout_ms = 2000);

  // 便捷方法：发送命令但不关心响应内容
  bool sendCommand(const std::string &cmd, uint32_t timeout_ms = 2000);

  // -------------------------------------------------------------------------
  // URC事件处理
  // -------------------------------------------------------------------------

  // 设置URC事件回调（在接收线程中调用）
  void setURCCallback(URCCallback callback);

  // -------------------------------------------------------------------------
  // 常用AT命令封装 (可选，方便上层使用)
  // -------------------------------------------------------------------------

  // 基础命令
  bool testConnection();                   // AT
  bool queryVersion(std::string &version); // AT+VER?
  bool queryESN(std::string &esn);         // AT+ESN?
  bool reboot();                           // AT+REBOOT

  // 配置命令
  bool configAddress(uint16_t addr, uint16_t group_addr = 0); // AT+ADDR=
  bool configCell(uint8_t cell_id);                           // AT+CELL=
  bool configPower(int8_t dbm);                               // AT+PWR=
  bool configWakeup(uint16_t period_ms);                      // AT+WOR=
  bool configBaudrate(uint32_t ctrl_bps, uint32_t data_bps);  // AT+BPS=
  bool configLowPower(uint8_t mode);   // AT+LP= (2=TypeC, 3=TypeD)
  bool configAwake(uint16_t awake_ms); // AT+AWAKE=

  // 查询命令
  bool queryAddress(uint16_t &addr, uint16_t &group_addr, bool &is_hub);
  bool queryCell(uint8_t &cell_id);
  bool queryPower(int8_t &dbm);

  // 数据发送命令 (立即返回OK，结果通过+SEND: URC异步通知)
  // @param dest: 目标地址
  // @param data: 数据
  // @param len: 数据长度
  // @param type: 消息类型 (0=UM, 1=AM, 2=FAST, 3=FLOOD)
  bool sendData(uint16_t dest, const uint8_t *data, size_t len, uint8_t type);

private:
  // 串口
  std::unique_ptr<hal::SerialPort> serial_;
  std::string port_;
  uint32_t baudrate_;

  // 接收线程
  std::unique_ptr<hal::Thread> rx_thread_;
  std::atomic<bool> running_;

  // 状态机
  enum class State {
    IDLE,            // 空闲，等待命令或URC
    WAITING_RESPONSE // 等待同步命令的响应
  };
  State state_;

  // 同步命令响应缓冲
  std::vector<std::string> response_buffer_;
  bool response_ok_; // true=OK, false=ERROR
  hal::Mutex sync_mutex_;
  std::condition_variable_any sync_cv_;

  // URC回调
  URCCallback urc_callback_;
  hal::Mutex urc_mutex_;

  // 线程函数
  void rxThreadFunc();

  // 行处理
  void processLine(const std::string &line);

  // 串口读写
  bool readLine(std::string &line, uint32_t timeout_ms);
  bool writeLine(const std::string &cmd);

  // URC识别
  static URCType identifyURC(const std::string &line);

  // 辅助函数
  std::string bytesToHex(const uint8_t *data, size_t len);
};

} // namespace xslot

#endif // TPMESH_AT_DRIVER_H
