/**
 * @file null_transport.cpp
 * @brief 空传输层实现
 *
 * 所有操作返回 XSLOT_ERR_NO_DEVICE。
 */
#include "null_transport.h"
#include <cstdlib>
#include <xslot/xslot_error.h>

struct null_transport_impl {
  i_transport_t base;
};

static int null_start(void *impl) {
  (void)impl;
  return XSLOT_OK;
}

static void null_stop(void *impl) { (void)impl; }

static int null_send(void *impl, const uint8_t *data, uint16_t len) {
  (void)impl;
  (void)data;
  (void)len;
  return XSLOT_ERR_NO_DEVICE;
}

static int null_probe(void *impl) {
  (void)impl;
  return XSLOT_ERR_NO_DEVICE;
}

static int null_configure(void *impl, uint8_t cell_id, int8_t power_dbm) {
  (void)impl;
  (void)cell_id;
  (void)power_dbm;
  return XSLOT_ERR_NO_DEVICE;
}

static void null_set_recv_cb(void *impl, transport_receive_cb cb, void *ctx) {
  (void)impl;
  (void)cb;
  (void)ctx;
}

static void null_destroy(void *impl) {
  if (impl) {
    std::free(impl);
  }
}

static const i_transport_vtable_t null_vtable = {
    .start = null_start,
    .stop = null_stop,
    .send = null_send,
    .probe = null_probe,
    .configure = null_configure,
    .set_receive_cb = null_set_recv_cb,
    .destroy = null_destroy,
};

i_transport_t *null_transport_create(void) {
  null_transport_impl *impl =
      (null_transport_impl *)std::calloc(1, sizeof(null_transport_impl));
  if (!impl)
    return nullptr;

  impl->base.vtable = &null_vtable;
  impl->base.impl = impl;

  return &impl->base;
}
