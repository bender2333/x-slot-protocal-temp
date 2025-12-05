/**
 * @file transport.h
 * @brief X-Slot 传输层抽象接口 (Modern C++)
 *
 * 定义传输层的抽象基类，支持多种传输方式。
 */
#ifndef XSLOT_TRANSPORT_H
#define XSLOT_TRANSPORT_H

#include "../core/function_ref.h"
#include "../core/result.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <xslot/xslot_types.h>

namespace xslot {

/**
 * @brief 传输层接收回调类型
 */
using TransportReceiveCallback = std::function<void(std::span<const uint8_t>)>;

/**
 * @brief 传输层抽象基类
 *
 * 定义传输层的统一接口，支持 TPMesh、Direct、Null 等多种实现。
 */
class ITransport {
public:
  virtual ~ITransport() = default;

  // 禁用拷贝
  ITransport(const ITransport &) = delete;
  ITransport &operator=(const ITransport &) = delete;

  // 允许移动
  ITransport(ITransport &&) = default;
  ITransport &operator=(ITransport &&) = default;

  /**
   * @brief 启动传输层
   * @return 成功返回 Ok，失败返回错误码
   */
  [[nodiscard]] virtual VoidResult start() = 0;

  /**
   * @brief 停止传输层
   */
  virtual void stop() = 0;

  /**
   * @brief 发送数据
   * @param data 要发送的数据
   * @return 成功返回 Ok，失败返回错误码
   */
  [[nodiscard]] virtual VoidResult send(std::span<const uint8_t> data) = 0;

  /**
   * @brief 探测设备是否存在
   * @return 成功返回 Ok，失败返回错误码
   */
  [[nodiscard]] virtual VoidResult probe() = 0;

  /**
   * @brief 配置传输参数
   * @param cell_id 小区 ID
   * @param power_dbm 发射功率
   * @return 成功返回 Ok，失败返回错误码
   */
  [[nodiscard]] virtual VoidResult configure(uint8_t cell_id,
                                             int8_t power_dbm) = 0;

  /**
   * @brief 设置接收回调
   * @param callback 回调函数，数据到达时调用
   */
  virtual void set_receive_callback(TransportReceiveCallback callback) = 0;

  /**
   * @brief 检查是否正在运行
   */
  [[nodiscard]] virtual bool is_running() const = 0;

protected:
  ITransport() = default;
};

/* ============================================================================
 * 工厂函数
 * ============================================================================
 */

/**
 * @brief 创建 TPMesh 传输层
 * @param config 配置参数
 * @return 传输层实例，失败返回 nullptr
 */
[[nodiscard]] std::unique_ptr<ITransport>
create_tpmesh_transport(const xslot_config_t &config);

/**
 * @brief 创建 Direct 传输层 (HMI 直连)
 * @param config 配置参数
 * @return 传输层实例，失败返回 nullptr
 */
[[nodiscard]] std::unique_ptr<ITransport>
create_direct_transport(const xslot_config_t &config);

/**
 * @brief 创建 Null 传输层 (空实现)
 * @return 传输层实例
 */
[[nodiscard]] std::unique_ptr<ITransport> create_null_transport();

} // namespace xslot

#endif // XSLOT_TRANSPORT_H
