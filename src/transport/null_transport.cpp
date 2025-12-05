/**
 * @file null_transport.cpp
 * @brief 空传输层实现 (Modern C++)
 *
 * 所有操作返回 NoDevice 错误。用于无设备检测到时的占位。
 */
#include "transport.h"

namespace xslot {

/**
 * @brief 空传输层实现
 */
class NullTransport final : public ITransport {
public:
  NullTransport() = default;
  ~NullTransport() override = default;

  VoidResult start() override { return VoidResult::ok(); }

  void stop() override {}

  VoidResult send(std::span<const uint8_t> /*data*/) override {
    return VoidResult::error(Error::NoDevice);
  }

  VoidResult probe() override { return VoidResult::error(Error::NoDevice); }

  VoidResult configure(uint8_t /*cell_id*/, int8_t /*power_dbm*/) override {
    return VoidResult::error(Error::NoDevice);
  }

  void set_receive_callback(TransportReceiveCallback /*callback*/) override {}

  bool is_running() const override { return false; }
};

std::unique_ptr<ITransport> create_null_transport() {
  return std::make_unique<NullTransport>();
}

} // namespace xslot
