/**
 * @file direct_transport.cpp
 * @brief HMI 直连传输层实现
 *
 * 纯串口透传，直接解析 X-Slot 帧。
 */
#include "direct_transport.h"
#include "../core/xslot_protocol.h"
#include <cstdlib>
#include <cstring>
#include <xslot/xslot_error.h>

/* HAL 函数声明 */
extern "C" {
void *hal_serial_open(const char *port, uint32_t baudrate);
void hal_serial_close(void *handle);
int hal_serial_write(void *handle, const uint8_t *data, uint16_t len);
int hal_serial_read(void *handle, uint8_t *data, uint16_t max_len,
                    uint32_t timeout_ms);
uint32_t hal_get_timestamp_ms(void);
}

#define RX_BUFFER_SIZE 256

struct direct_transport_impl {
  i_transport_t base;
  void *serial;
  xslot_config_t config;
  bool running;

  /* 接收回调 */
  transport_receive_cb recv_cb;
  void *recv_ctx;

  /* 接收缓冲区 */
  uint8_t rx_buffer[RX_BUFFER_SIZE];
  uint16_t rx_len;
};

/* 前向声明 */
static int direct_start(void *impl);
static void direct_stop(void *impl);
static int direct_send(void *impl, const uint8_t *data, uint16_t len);
static int direct_probe(void *impl);
static int direct_configure(void *impl, uint8_t cell_id, int8_t power_dbm);
static void direct_set_recv_cb(void *impl, transport_receive_cb cb, void *ctx);
static void direct_destroy(void *impl);

static const i_transport_vtable_t direct_vtable = {
    .start = direct_start,
    .stop = direct_stop,
    .send = direct_send,
    .probe = direct_probe,
    .configure = direct_configure,
    .set_receive_cb = direct_set_recv_cb,
    .destroy = direct_destroy,
};

/**
 * @brief 尝试从缓冲区解析帧
 */
static void try_parse_frame(direct_transport_impl *impl) {
  while (impl->rx_len >= XSLOT_FRAME_MIN_SIZE) {
    /* 查找同步字节 */
    uint16_t sync_pos = 0;
    while (sync_pos < impl->rx_len &&
           impl->rx_buffer[sync_pos] != XSLOT_SYNC_BYTE) {
      sync_pos++;
    }

    /* 移除同步字节之前的数据 */
    if (sync_pos > 0) {
      std::memmove(impl->rx_buffer, impl->rx_buffer + sync_pos,
                   impl->rx_len - sync_pos);
      impl->rx_len -= sync_pos;
    }

    if (impl->rx_len < XSLOT_FRAME_MIN_SIZE) {
      break;
    }

    /* 获取数据长度 */
    uint8_t data_len = impl->rx_buffer[XSLOT_OFFSET_LEN];
    if (data_len > XSLOT_MAX_DATA_LEN) {
      /* 无效长度，跳过同步字节 */
      std::memmove(impl->rx_buffer, impl->rx_buffer + 1, impl->rx_len - 1);
      impl->rx_len--;
      continue;
    }

    uint16_t frame_size = xslot_frame_total_size(data_len);
    if (impl->rx_len < frame_size) {
      /* 数据不完整，等待更多数据 */
      break;
    }

    /* 验证 CRC */
    if (xslot_frame_verify_crc(impl->rx_buffer, frame_size)) {
      /* 帧有效，回调上层 */
      if (impl->recv_cb) {
        impl->recv_cb(impl->recv_ctx, impl->rx_buffer, frame_size);
      }

      /* 移除已处理的帧 */
      std::memmove(impl->rx_buffer, impl->rx_buffer + frame_size,
                   impl->rx_len - frame_size);
      impl->rx_len -= frame_size;
    } else {
      /* CRC 错误，跳过同步字节 */
      std::memmove(impl->rx_buffer, impl->rx_buffer + 1, impl->rx_len - 1);
      impl->rx_len--;
    }
  }
}

i_transport_t *direct_transport_create(const xslot_config_t *config) {
  if (!config)
    return nullptr;

  direct_transport_impl *impl =
      (direct_transport_impl *)std::calloc(1, sizeof(direct_transport_impl));
  if (!impl)
    return nullptr;

  std::memcpy(&impl->config, config, sizeof(xslot_config_t));
  impl->running = false;

  /* 设置虚表 */
  impl->base.vtable = &direct_vtable;
  impl->base.impl = impl;

  return &impl->base;
}

static int direct_start(void *impl_ptr) {
  direct_transport_impl *impl = (direct_transport_impl *)impl_ptr;
  if (!impl)
    return XSLOT_ERR_PARAM;
  if (impl->running)
    return XSLOT_OK;

  impl->serial = hal_serial_open(
      impl->config.uart_port,
      impl->config.uart_baudrate ? impl->config.uart_baudrate : 115200);
  if (!impl->serial) {
    return XSLOT_ERR_NO_DEVICE;
  }

  impl->running = true;
  impl->rx_len = 0;

  return XSLOT_OK;
}

static void direct_stop(void *impl_ptr) {
  direct_transport_impl *impl = (direct_transport_impl *)impl_ptr;
  if (!impl || !impl->running)
    return;

  impl->running = false;

  if (impl->serial) {
    hal_serial_close(impl->serial);
    impl->serial = nullptr;
  }
}

static int direct_send(void *impl_ptr, const uint8_t *data, uint16_t len) {
  direct_transport_impl *impl = (direct_transport_impl *)impl_ptr;
  if (!impl || !impl->serial || !data || len == 0)
    return XSLOT_ERR_PARAM;

  int ret = hal_serial_write(impl->serial, data, len);
  return (ret == len) ? XSLOT_OK : XSLOT_ERR_SEND_FAIL;
}

static int direct_probe(void *impl_ptr) {
  direct_transport_impl *impl = (direct_transport_impl *)impl_ptr;
  if (!impl)
    return XSLOT_ERR_PARAM;

  /* 打开串口 */
  impl->serial = hal_serial_open(
      impl->config.uart_port,
      impl->config.uart_baudrate ? impl->config.uart_baudrate : 115200);
  if (!impl->serial) {
    return XSLOT_ERR_NO_DEVICE;
  }

  /* 等待并尝试接收 X-Slot 帧 */
  uint8_t buffer[32];
  uint32_t start = hal_get_timestamp_ms();
  bool found_sync = false;

  while (hal_get_timestamp_ms() - start < 500) {
    int ret = hal_serial_read(impl->serial, buffer, sizeof(buffer), 50);
    if (ret > 0) {
      for (int i = 0; i < ret; i++) {
        if (buffer[i] == XSLOT_SYNC_BYTE) {
          found_sync = true;
          break;
        }
      }
      if (found_sync)
        break;
    }
  }

  hal_serial_close(impl->serial);
  impl->serial = nullptr;

  /* 如果收到同步字节，认为是 HMI 模式 */
  return found_sync ? XSLOT_OK : XSLOT_ERR_NO_DEVICE;
}

static int direct_configure(void *impl_ptr, uint8_t cell_id, int8_t power_dbm) {
  (void)impl_ptr;
  (void)cell_id;
  (void)power_dbm;
  /* Direct 模式不支持无线配置 */
  return XSLOT_OK;
}

static void direct_set_recv_cb(void *impl_ptr, transport_receive_cb cb,
                               void *ctx) {
  direct_transport_impl *impl = (direct_transport_impl *)impl_ptr;
  if (impl) {
    impl->recv_cb = cb;
    impl->recv_ctx = ctx;
  }
}

static void direct_destroy(void *impl_ptr) {
  direct_transport_impl *impl = (direct_transport_impl *)impl_ptr;
  if (!impl)
    return;

  direct_stop(impl);
  std::free(impl);
}
