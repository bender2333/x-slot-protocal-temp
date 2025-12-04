/**
 * @file transport.h
 * @brief X-Slot 传输层抽象接口 (C++20)
 *
 * 定义传输层的抽象基类，支持多种传输方式。
 */
#ifndef XSLOT_TRANSPORT_H
#define XSLOT_TRANSPORT_H

#include "../core/function_ref.h"
#include "../core/result.h"
#include <cstdint>
#include <memory>
#include <span>
#include <xslot/xslot_types.h>

// 前向声明 C 传输层类型 (全局命名空间)
struct i_transport;
typedef struct i_transport i_transport_t;

namespace xslot {

/**
 * @brief 传输层接收回调类型
 */
using TransportReceiveCallback = function_ref<void(std::span<const uint8_t>)>;

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
   * @param ctx 用户上下文指针
   */
  virtual void set_receive_callback(TransportReceiveCallback callback,
                                    void *ctx) = 0;

  /**
   * @brief 检查是否正在运行
   */
  [[nodiscard]] virtual bool is_running() const = 0;

protected:
  ITransport() = default;
};

/**
 * @brief 传输层工厂接口
 */
class ITransportFactory {
public:
  virtual ~ITransportFactory() = default;

  /**
   * @brief 创建 TPMesh 传输层
   */
  [[nodiscard]] virtual std::unique_ptr<ITransport>
  create_tpmesh(const xslot_config_t &config) = 0;

  /**
   * @brief 创建 Direct 传输层
   */
  [[nodiscard]] virtual std::unique_ptr<ITransport>
  create_direct(const xslot_config_t &config) = 0;

  /**
   * @brief 创建 Null 传输层
   */
  [[nodiscard]] virtual std::unique_ptr<ITransport> create_null() = 0;
};

/**
 * @brief C 风格传输层适配器
 *
 * 将新的 C++ ITransport 接口适配到旧的 C 风格接口。
 */
class CTransportAdapter {
public:
  /**
   * @brief 从 C 传输层句柄创建适配器
   */
  explicit CTransportAdapter(i_transport_t *c_transport) noexcept;

  ~CTransportAdapter();

  // 禁用拷贝
  CTransportAdapter(const CTransportAdapter &) = delete;
  CTransportAdapter &operator=(const CTransportAdapter &) = delete;

  // 允许移动
  CTransportAdapter(CTransportAdapter &&other) noexcept;
  CTransportAdapter &operator=(CTransportAdapter &&other) noexcept;

  [[nodiscard]] VoidResult start();
  void stop();
  [[nodiscard]] VoidResult send(std::span<const uint8_t> data);
  [[nodiscard]] VoidResult probe();
  [[nodiscard]] VoidResult configure(uint8_t cell_id, int8_t power_dbm);

  /**
   * @brief 设置接收回调 (C 风格)
   */
  void set_receive_callback(void (*callback)(void *, const uint8_t *, uint16_t),
                            void *ctx);

  /**
   * @brief 获取底层 C 传输层句柄
   */
  [[nodiscard]] i_transport_t *get() const noexcept { return transport_; }

  /**
   * @brief 释放所有权
   */
  [[nodiscard]] i_transport_t *release() noexcept;

private:
  i_transport_t *transport_;
};

} // namespace xslot

#endif // XSLOT_TRANSPORT_H
