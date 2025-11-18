// =============================================================================
// src/transport/tpmesh_transport.h - TPMesh传输层（处理URC事件）
// =============================================================================
//
// 职责：
// 1. 使用TPMeshATDriver发送数据
// 2. 接收并解析URC事件
// 3. 维护接收队列（+NNMI数据）
// 4. （可选）追踪发送状态（+SEND事件）
// 5. 通知路由变化（+ROUTE事件）
//
// =============================================================================

#ifndef TPMESH_TRANSPORT_H
#define TPMESH_TRANSPORT_H

#include "tpmesh_at_driver.h"
#include "xslot_transport.h"
#include <condition_variable>
#include <memory>
#include <queue>

namespace xslot {

// 前向声明
struct Frame;

// =============================================================================
// 发送状态追踪（可选功能）
// =============================================================================
enum class SendStatus {
  HANDLE_OK,    // 成功添加到队列
  HANDLE_ERROR, // 队列已满
  PREPARE,      // 开始发送
  SEND_OK,      // 发送成功
  SEND_ERROR,   // 发送失败
  JOINING,      // 正在建立路由
  ROUTE_FULL,   // 路由表已满
  UNKNOWN       // 未知状态
};

struct SendTracker {
  uint8_t sn;         // 序列号 (1-63)
  SendStatus status;  // 当前状态
  uint32_t timestamp; // 最后更新时间

  SendTracker() : sn(0), status(SendStatus::UNKNOWN), timestamp(0) {}
};

// =============================================================================
// TPMesh传输层
// =============================================================================
class TPMeshTransport : public ITransport {
public:
  TPMeshTransport(const char *port, uint32_t baudrate);
  ~TPMeshTransport() override;

  // ITransport接口实现
  bool open() override;
  void close() override;
  bool send(uint16_t dest, const uint8_t *data, size_t len) override;
  int receive(uint8_t *buffer, size_t max_len, uint32_t timeout_ms) override;
  xslot_slot_mode_t detect() override;

  // 配置接口
  bool configure(uint16_t addr, uint8_t cell_id, int8_t power_dbm);

  // （可选）发送状态查询
  SendStatus getSendStatus(uint8_t sn);

  // 路由变化回调
  using RouteCallback = std::function<void(bool created, uint16_t addr)>;
  void setRouteCallback(RouteCallback callback);

private:
  // AT驱动器
  std::unique_ptr<TPMeshATDriver> at_driver_;
  uint16_t local_addr_;

  // 接收队列（存放解析后的X-Slot帧）
  std::queue<Frame> rx_queue_;
  hal::Mutex rx_mutex_;
  std::condition_variable_any rx_cv_;

  // 发送状态追踪表 (SN: 1-63循环)
  SendTracker send_trackers_[63];
  hal::Mutex send_mutex_;

  // 路由变化回调
  RouteCallback route_callback_;
  hal::Mutex route_mutex_;

  // URC事件处理
  void onURCEvent(const URCEvent &event);

  // URC解析器
  bool parseNNMI(const std::string &urc, Frame &frame);
  bool parseSEND(const std::string &urc, uint8_t &sn, SendStatus &status);
  bool parseROUTE(const std::string &urc, bool &created, uint16_t &addr);
  bool parseACK(const std::string &urc, uint16_t &src, int8_t &rssi,
                uint8_t &sn);

  // 辅助函数
  std::vector<uint8_t> hexToBytes(const std::string &hex);
  SendStatus parseStatus(const std::string &status_str);
};

} // namespace xslot

#endif // TPMESH_TRANSPORT_H
