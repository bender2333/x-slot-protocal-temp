/**
 * @file transport.cpp
 * @brief X-Slot 传输层实现
 */
#include "transport.h"
#include "i_transport.h"
#include <xslot/xslot_error.h>

namespace xslot {

// ============================================================================
// CTransportAdapter 实现
// ============================================================================

CTransportAdapter::CTransportAdapter(i_transport_t *c_transport) noexcept
    : transport_(c_transport) {}

CTransportAdapter::~CTransportAdapter() {
  if (transport_) {
    transport_destroy(transport_);
    transport_ = nullptr;
  }
}

CTransportAdapter::CTransportAdapter(CTransportAdapter &&other) noexcept
    : transport_(other.transport_) {
  other.transport_ = nullptr;
}

CTransportAdapter &
CTransportAdapter::operator=(CTransportAdapter &&other) noexcept {
  if (this != &other) {
    if (transport_) {
      transport_destroy(transport_);
    }
    transport_ = other.transport_;
    other.transport_ = nullptr;
  }
  return *this;
}

VoidResult CTransportAdapter::start() {
  if (!transport_) {
    return VoidResult::error(Error::InvalidParam);
  }
  int ret = transport_start(transport_);
  return VoidResult::from_c_error(ret);
}

void CTransportAdapter::stop() {
  if (transport_) {
    transport_stop(transport_);
  }
}

VoidResult CTransportAdapter::send(std::span<const uint8_t> data) {
  if (!transport_ || data.empty()) {
    return VoidResult::error(Error::InvalidParam);
  }
  int ret = transport_send(transport_, data.data(),
                           static_cast<uint16_t>(data.size()));
  return VoidResult::from_c_error(ret);
}

VoidResult CTransportAdapter::probe() {
  if (!transport_) {
    return VoidResult::error(Error::InvalidParam);
  }
  int ret = transport_probe(transport_);
  return VoidResult::from_c_error(ret);
}

VoidResult CTransportAdapter::configure(uint8_t cell_id, int8_t power_dbm) {
  if (!transport_) {
    return VoidResult::error(Error::InvalidParam);
  }
  int ret = transport_configure(transport_, cell_id, power_dbm);
  return VoidResult::from_c_error(ret);
}

void CTransportAdapter::set_receive_callback(
    void (*callback)(void *, const uint8_t *, uint16_t), void *ctx) {
  if (transport_) {
    transport_set_receive_callback(transport_, callback, ctx);
  }
}

i_transport_t *CTransportAdapter::release() noexcept {
  auto *t = transport_;
  transport_ = nullptr;
  return t;
}

} // namespace xslot
