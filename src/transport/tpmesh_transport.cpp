/**
 * @file tpmesh_transport.cpp
 * @brief TPMesh 传输层实现 (Modern C++)
 */
#include "tpmesh_at_driver.h"
#include "transport.h"
#include <cstring>

namespace xslot {

/**
 * @brief TPMesh 传输层实现
 */
class TpmeshTransport final : public ITransport {
public:
  explicit TpmeshTransport(const xslot_config_t &config)
      : config_(config), at_driver_(nullptr), running_(false) {}

  ~TpmeshTransport() override {
    stop();
    if (at_driver_) {
      tpmesh_at_destroy(at_driver_);
      at_driver_ = nullptr;
    }
  }

  VoidResult start() override {
    if (running_) {
      return VoidResult::ok();
    }

    /* 创建 AT 驱动（如果尚未创建） */
    if (!at_driver_) {
      at_driver_ = tpmesh_at_create(
          config_.uart_port,
          config_.uart_baudrate ? config_.uart_baudrate : 115200);
      if (!at_driver_) {
        return VoidResult::error(Error::NoDevice);
      }

      /* 设置 URC 回调 */
      tpmesh_at_set_urc_callback(at_driver_, &TpmeshTransport::on_urc_static,
                                 this);
    }

    /* 启动 AT 驱动 */
    int ret = tpmesh_at_start(at_driver_);
    if (ret != 0) {
      return VoidResult::from_c_error(ret);
    }

    /* 配置模组 */
    ret = tpmesh_at_set_addr(at_driver_, config_.local_addr);
    if (ret != 0) {
      return VoidResult::from_c_error(ret);
    }

    if (config_.cell_id > 0) {
      tpmesh_at_set_cell(at_driver_, config_.cell_id);
    }

    if (config_.power_dbm != 0) {
      tpmesh_at_set_power(at_driver_, config_.power_dbm);
    }

    if (config_.power_mode == XSLOT_POWER_MODE_LOW ||
        config_.power_mode == XSLOT_POWER_MODE_NORMAL) {
      tpmesh_at_set_power_mode(at_driver_, config_.power_mode);
    }

    running_ = true;
    return VoidResult::ok();
  }

  void stop() override {
    if (!running_) {
      return;
    }

    running_ = false;

    if (at_driver_) {
      tpmesh_at_stop(at_driver_);
    }
  }

  VoidResult send(std::span<const uint8_t> data) override {
    if (!at_driver_ || data.empty()) {
      return VoidResult::error(Error::InvalidParam);
    }

    /* 从帧中提取目标地址 (TO 字段在偏移 3-4) */
    if (data.size() < 5) {
      return VoidResult::error(Error::InvalidParam);
    }

    uint16_t dest_addr =
        static_cast<uint16_t>(data[3]) | (static_cast<uint16_t>(data[4]) << 8);

    /* 使用 Type 0 (UM) 发送 */
    int ret = tpmesh_at_send_data(at_driver_, dest_addr, data.data(),
                                  static_cast<uint16_t>(data.size()), 0);
    return VoidResult::from_c_error(ret);
  }

  VoidResult probe() override {
    /* 创建临时 AT 驱动进行探测 */
    tpmesh_at_driver_t driver = tpmesh_at_create(
        config_.uart_port,
        config_.uart_baudrate ? config_.uart_baudrate : 115200);
    if (!driver) {
      return VoidResult::error(Error::NoDevice);
    }

    /* 启动串口 */
    int ret = tpmesh_at_start(driver);
    if (ret != 0) {
      tpmesh_at_destroy(driver);
      return VoidResult::from_c_error(ret);
    }

    /* 探测模组 */
    ret = tpmesh_at_probe(driver);

    /* 清理 */
    tpmesh_at_stop(driver);
    tpmesh_at_destroy(driver);

    return VoidResult::from_c_error(ret);
  }

  VoidResult configure(uint8_t cell_id, int8_t power_dbm) override {
    if (!at_driver_) {
      return VoidResult::error(Error::NotInitialized);
    }

    int ret = 0;

    if (cell_id > 0) {
      ret = tpmesh_at_set_cell(at_driver_, cell_id);
      if (ret != 0) {
        return VoidResult::from_c_error(ret);
      }
    }

    if (power_dbm != 0) {
      ret = tpmesh_at_set_power(at_driver_, power_dbm);
    }

    return VoidResult::from_c_error(ret);
  }

  void set_receive_callback(TransportReceiveCallback callback) override {
    recv_cb_ = std::move(callback);
  }

  bool is_running() const override { return running_; }

private:
  static void on_urc_static(void *ctx, const tpmesh_urc_t *urc) {
    auto *self = static_cast<TpmeshTransport *>(ctx);
    self->on_urc(urc);
  }

  void on_urc(const tpmesh_urc_t *urc) {
    if (!urc) {
      return;
    }

    switch (urc->type) {
    case URC_NNMI:
      /* 数据接收，转发给上层 */
      if (recv_cb_ && urc->data_len > 0) {
        recv_cb_({urc->data, urc->data_len});
      }
      break;

    case URC_SEND:
      /* 发送状态，可用于追踪 */
      break;

    case URC_ROUTE:
      /* 路由变化 */
      break;

    case URC_ACK:
      /* 送达确认 */
      break;

    default:
      break;
    }
  }

  xslot_config_t config_;
  tpmesh_at_driver_t at_driver_;
  bool running_;

  TransportReceiveCallback recv_cb_;
};

std::unique_ptr<ITransport>
create_tpmesh_transport(const xslot_config_t &config) {
  return std::make_unique<TpmeshTransport>(config);
}

} // namespace xslot
