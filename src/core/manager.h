/**
 * @file manager.h
 * @brief X-Slot 协议栈管理器类 (C++20)
 *
 * 提供现代 C++ 风格的协议栈管理接口。
 */
#ifndef XSLOT_MANAGER_H_NEW
#define XSLOT_MANAGER_H_NEW

#include "../transport/transport.h"
#include "frame.h"
#include "function_ref.h"
#include "message_builder.h"
#include "node_table.h"
#include "result.h"
#include <cstdint>
#include <memory>
#include <span>
#include <xslot/xslot_types.h>

namespace xslot {

/**
 * @brief 运行模式枚举
 */
enum class RunMode : uint8_t {
  None = XSLOT_MODE_NONE,
  Wireless = XSLOT_MODE_WIRELESS,
  Hmi = XSLOT_MODE_HMI,
};

/**
 * @brief 回调类型定义
 */
using DataReceivedCallback =
    function_ref<void(uint16_t from, std::span<const uint8_t> data)>;
using NodeStatusCallback = function_ref<void(uint16_t addr, bool online)>;
using WriteRequestCallback =
    function_ref<void(uint16_t from, const xslot_bacnet_object_t &obj)>;
using ReportReceivedCallback = function_ref<void(
    uint16_t from, std::span<const xslot_bacnet_object_t> objects)>;

/**
 * @brief X-Slot 协议栈管理器
 *
 * 核心管理类，负责：
 * - 节点生命周期管理
 * - 数据收发调度
 * - 模式检测与传输层切换
 * - 用户回调分发
 */
class Manager {
public:
  /**
   * @brief 构造管理器
   * @param config 配置参数
   */
  explicit Manager(const xslot_config_t &config);

  /**
   * @brief 析构管理器
   */
  ~Manager();

  // 禁用拷贝
  Manager(const Manager &) = delete;
  Manager &operator=(const Manager &) = delete;

  // 禁用移动 (包含回调状态)
  Manager(Manager &&) = delete;
  Manager &operator=(Manager &&) = delete;

  /**
   * @brief 启动协议栈
   * @return 成功返回 Ok，失败返回错误码
   */
  [[nodiscard]] VoidResult start();

  /**
   * @brief 停止协议栈
   */
  void stop();

  /**
   * @brief 获取运行模式
   */
  [[nodiscard]] RunMode get_mode() const noexcept { return mode_; }

  /**
   * @brief 检查是否正在运行
   */
  [[nodiscard]] bool is_running() const noexcept { return running_; }

  // ========================================================================
  // 业务操作
  // ========================================================================

  /**
   * @brief 发送帧
   */
  [[nodiscard]] VoidResult send_frame(const Frame &frame);

  /**
   * @brief 上报对象数据
   */
  [[nodiscard]] VoidResult
  report(std::span<const xslot_bacnet_object_t> objects);

  /**
   * @brief 远程写入对象
   */
  [[nodiscard]] VoidResult write(uint16_t target,
                                 const xslot_bacnet_object_t &obj);

  /**
   * @brief 查询对象
   */
  [[nodiscard]] VoidResult query(uint16_t target,
                                 std::span<const uint16_t> object_ids);

  /**
   * @brief 发送心跳
   */
  [[nodiscard]] VoidResult ping(uint16_t target);

  // ========================================================================
  // 节点管理
  // ========================================================================

  /**
   * @brief 获取节点表
   */
  [[nodiscard]] const DefaultNodeTable &node_table() const noexcept {
    return node_table_;
  }

  /**
   * @brief 检查超时节点
   */
  void check_node_timeout(uint32_t timeout_ms);

  // ========================================================================
  // 回调设置 (C++ 风格)
  // ========================================================================

  void set_data_callback(DataReceivedCallback cb) { data_cb_ = cb; }
  void set_node_callback(NodeStatusCallback cb) { node_cb_ = cb; }
  void set_write_callback(WriteRequestCallback cb) { write_cb_ = cb; }
  void set_report_callback(ReportReceivedCallback cb) { report_cb_ = cb; }

  // ========================================================================
  // 回调设置 (C 风格，用于 C API 兼容)
  // ========================================================================

  void set_data_callback_c(xslot_data_received_cb cb) { c_data_cb_ = cb; }
  void set_node_callback_c(xslot_node_online_cb cb) { c_node_cb_ = cb; }
  void set_write_callback_c(xslot_write_request_cb cb) { c_write_cb_ = cb; }
  void set_report_callback_c(xslot_report_received_cb cb) { c_report_cb_ = cb; }

  // ========================================================================
  // 运行时配置
  // ========================================================================

  /**
   * @brief 更新无线配置
   */
  [[nodiscard]] VoidResult update_config(uint8_t cell_id, int8_t power_dbm);

  /**
   * @brief 获取配置
   */
  [[nodiscard]] const xslot_config_t &config() const noexcept {
    return config_;
  }

private:
  // 内部方法
  void handle_frame(const Frame &frame);
  void on_frame_received(std::span<const uint8_t> data);
  CTransportAdapter detect_and_create_transport();
  uint8_t next_seq() noexcept { return seq_++; }

  // 配置
  xslot_config_t config_;
  RunMode mode_ = RunMode::None;
  bool running_ = false;
  uint8_t seq_ = 0;

  // 组件
  DefaultNodeTable node_table_;
  CTransportAdapter transport_{nullptr};

  // C++ 回调
  DataReceivedCallback data_cb_;
  NodeStatusCallback node_cb_;
  WriteRequestCallback write_cb_;
  ReportReceivedCallback report_cb_;

  // C 回调 (兼容层)
  xslot_data_received_cb c_data_cb_ = nullptr;
  xslot_node_online_cb c_node_cb_ = nullptr;
  xslot_write_request_cb c_write_cb_ = nullptr;
  xslot_report_received_cb c_report_cb_ = nullptr;
};

} // namespace xslot

#endif // XSLOT_MANAGER_H_NEW
