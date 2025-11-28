/**
 * @file tpmesh_transport.cpp
 * @brief TPMesh 传输层实现
 */
#include "tpmesh_transport.h"
#include "tpmesh_at_driver.h"
#include <cstdlib>
#include <cstring>
#include <xslot/xslot_error.h>

struct tpmesh_transport_impl {
  i_transport_t base;
  tpmesh_at_driver_t at_driver;
  xslot_config_t config;

  /* 接收回调 */
  transport_receive_cb recv_cb;
  void *recv_ctx;
};

/* 前向声明 */
static int tpmesh_start(void *impl);
static void tpmesh_stop(void *impl);
static int tpmesh_send(void *impl, const uint8_t *data, uint16_t len);
static int tpmesh_probe(void *impl);
static int tpmesh_configure(void *impl, uint8_t cell_id, int8_t power_dbm);
static void tpmesh_set_recv_cb(void *impl, transport_receive_cb cb, void *ctx);
static void tpmesh_destroy(void *impl);

static const i_transport_vtable_t tpmesh_vtable = {
    .start = tpmesh_start,
    .stop = tpmesh_stop,
    .send = tpmesh_send,
    .probe = tpmesh_probe,
    .configure = tpmesh_configure,
    .set_receive_cb = tpmesh_set_recv_cb,
    .destroy = tpmesh_destroy,
};

/**
 * @brief URC 回调处理
 */
static void on_urc_received(void *ctx, const tpmesh_urc_t *urc) {
  tpmesh_transport_impl *impl = (tpmesh_transport_impl *)ctx;
  if (!impl || !urc)
    return;

  switch (urc->type) {
  case URC_NNMI:
    /* 数据接收，转发给上层 */
    if (impl->recv_cb && urc->data_len > 0) {
      impl->recv_cb(impl->recv_ctx, urc->data, urc->data_len);
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

i_transport_t *tpmesh_transport_create(const xslot_config_t *config) {
  if (!config)
    return nullptr;

  tpmesh_transport_impl *impl =
      (tpmesh_transport_impl *)std::calloc(1, sizeof(tpmesh_transport_impl));
  if (!impl)
    return nullptr;

  std::memcpy(&impl->config, config, sizeof(xslot_config_t));

  /* 创建 AT 驱动 */
  impl->at_driver =
      tpmesh_at_create(config->uart_port,
                       config->uart_baudrate ? config->uart_baudrate : 115200);
  if (!impl->at_driver) {
    std::free(impl);
    return nullptr;
  }

  /* 设置 URC 回调 */
  tpmesh_at_set_urc_callback(impl->at_driver, on_urc_received, impl);

  /* 设置虚表 */
  impl->base.vtable = &tpmesh_vtable;
  impl->base.impl = impl;

  return &impl->base;
}

static int tpmesh_start(void *impl_ptr) {
  tpmesh_transport_impl *impl = (tpmesh_transport_impl *)impl_ptr;
  if (!impl)
    return XSLOT_ERR_PARAM;

  /* 启动 AT 驱动 */
  int ret = tpmesh_at_start(impl->at_driver);
  if (ret != XSLOT_OK)
    return ret;

  /* 配置模组 */
  ret = tpmesh_at_set_addr(impl->at_driver, impl->config.local_addr);
  if (ret != XSLOT_OK)
    return ret;

  if (impl->config.cell_id > 0) {
    tpmesh_at_set_cell(impl->at_driver, impl->config.cell_id);
  }

  if (impl->config.power_dbm != 0) {
    tpmesh_at_set_power(impl->at_driver, impl->config.power_dbm);
  }

  return XSLOT_OK;
}

static void tpmesh_stop(void *impl_ptr) {
  tpmesh_transport_impl *impl = (tpmesh_transport_impl *)impl_ptr;
  if (impl && impl->at_driver) {
    tpmesh_at_stop(impl->at_driver);
  }
}

static int tpmesh_send(void *impl_ptr, const uint8_t *data, uint16_t len) {
  tpmesh_transport_impl *impl = (tpmesh_transport_impl *)impl_ptr;
  if (!impl || !data || len == 0)
    return XSLOT_ERR_PARAM;

  /* 从帧中提取目标地址 (TO 字段在偏移 3-4) */
  if (len < 5)
    return XSLOT_ERR_PARAM;

  uint16_t dest_addr = data[3] | (data[4] << 8);

  /* 使用 Type 0 (UM) 发送 */
  return tpmesh_at_send_data(impl->at_driver, dest_addr, data, len, 0);
}

static int tpmesh_probe(void *impl_ptr) {
  tpmesh_transport_impl *impl = (tpmesh_transport_impl *)impl_ptr;
  if (!impl)
    return XSLOT_ERR_PARAM;

  /* 启动串口 */
  int ret = tpmesh_at_start(impl->at_driver);
  if (ret != XSLOT_OK)
    return ret;

  /* 探测模组 */
  ret = tpmesh_at_probe(impl->at_driver);

  /* 停止串口 (start 时会重新打开) */
  tpmesh_at_stop(impl->at_driver);

  return ret;
}

static int tpmesh_configure(void *impl_ptr, uint8_t cell_id, int8_t power_dbm) {
  tpmesh_transport_impl *impl = (tpmesh_transport_impl *)impl_ptr;
  if (!impl)
    return XSLOT_ERR_PARAM;

  int ret = XSLOT_OK;

  if (cell_id > 0) {
    ret = tpmesh_at_set_cell(impl->at_driver, cell_id);
    if (ret != XSLOT_OK)
      return ret;
  }

  if (power_dbm != 0) {
    ret = tpmesh_at_set_power(impl->at_driver, power_dbm);
  }

  return ret;
}

static void tpmesh_set_recv_cb(void *impl_ptr, transport_receive_cb cb,
                               void *ctx) {
  tpmesh_transport_impl *impl = (tpmesh_transport_impl *)impl_ptr;
  if (impl) {
    impl->recv_cb = cb;
    impl->recv_ctx = ctx;
  }
}

static void tpmesh_destroy(void *impl_ptr) {
  tpmesh_transport_impl *impl = (tpmesh_transport_impl *)impl_ptr;
  if (!impl)
    return;

  if (impl->at_driver) {
    tpmesh_at_destroy(impl->at_driver);
  }

  std::free(impl);
}
