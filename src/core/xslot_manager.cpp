/**
 * @file xslot_manager.cpp
 * @brief X-Slot 协议栈核心管理器实现
 */
#include "xslot_manager.h"
#include "../transport/i_transport.h"
#include "message_codec.h"
#include <cstdlib>
#include <cstring>
#include <xslot/xslot_error.h>

/* HAL 函数声明 */
extern "C" {
uint32_t hal_get_timestamp_ms(void);
void hal_sleep_ms(uint32_t ms);
}

struct xslot_manager {
  xslot_config_t config;
  xslot_run_mode_t mode;
  node_table_t node_table;
  i_transport_t *transport;
  bool running;
  uint8_t seq;

  /* 回调 */
  xslot_data_received_cb data_cb;
  xslot_node_online_cb node_cb;
  xslot_write_request_cb write_cb;
  xslot_report_received_cb report_cb;
};

/* 前向声明 */
static void on_frame_received(void *ctx, const uint8_t *data, uint16_t len);
static i_transport_t *detect_and_create_transport(xslot_manager_t *mgr);

xslot_manager_t *xslot_manager_create(const xslot_config_t *config) {
  if (!config)
    return nullptr;

  xslot_manager_t *mgr =
      (xslot_manager_t *)std::calloc(1, sizeof(xslot_manager_t));
  if (!mgr)
    return nullptr;

  std::memcpy(&mgr->config, config, sizeof(xslot_config_t));
  mgr->mode = XSLOT_MODE_NONE;
  mgr->running = false;
  mgr->seq = 0;

  /* 创建节点表 */
  mgr->node_table = node_table_create(XSLOT_MAX_NODES);
  if (!mgr->node_table) {
    std::free(mgr);
    return nullptr;
  }

  return mgr;
}

void xslot_manager_destroy(xslot_manager_t *mgr) {
  if (!mgr)
    return;

  xslot_manager_stop(mgr);

  if (mgr->node_table) {
    node_table_destroy(mgr->node_table);
  }

  std::free(mgr);
}

int xslot_manager_start(xslot_manager_t *mgr) {
  if (!mgr)
    return XSLOT_ERR_PARAM;
  if (mgr->running)
    return XSLOT_OK;

  /* 检测并创建传输层 */
  mgr->transport = detect_and_create_transport(mgr);
  if (!mgr->transport) {
    mgr->mode = XSLOT_MODE_NONE;
    return XSLOT_ERR_NO_DEVICE;
  }

  /* 设置接收回调 */
  transport_set_receive_callback(mgr->transport, on_frame_received, mgr);

  /* 启动传输层 */
  int ret = transport_start(mgr->transport);
  if (ret != XSLOT_OK) {
    transport_destroy(mgr->transport);
    mgr->transport = nullptr;
    mgr->mode = XSLOT_MODE_NONE;
    return ret;
  }

  mgr->running = true;
  return XSLOT_OK;
}

void xslot_manager_stop(xslot_manager_t *mgr) {
  if (!mgr || !mgr->running)
    return;

  mgr->running = false;

  if (mgr->transport) {
    transport_stop(mgr->transport);
    transport_destroy(mgr->transport);
    mgr->transport = nullptr;
  }
}

xslot_run_mode_t xslot_manager_get_mode(xslot_manager_t *mgr) {
  return mgr ? mgr->mode : XSLOT_MODE_NONE;
}

int xslot_manager_send_frame(xslot_manager_t *mgr, const xslot_frame_t *frame) {
  if (!mgr || !frame || !mgr->transport)
    return XSLOT_ERR_PARAM;
  if (!mgr->running)
    return XSLOT_ERR_NOT_INIT;

  uint8_t buffer[XSLOT_FRAME_MAX_SIZE];
  int len = xslot_frame_encode(frame, buffer, sizeof(buffer));
  if (len < 0)
    return len;

  return transport_send(mgr->transport, buffer, len);
}

int xslot_manager_report(xslot_manager_t *mgr,
                         const xslot_bacnet_object_t *objects, uint8_t count) {
  if (!mgr || !objects || count == 0)
    return XSLOT_ERR_PARAM;

  xslot_frame_t frame;
  int ret =
      message_build_report(&frame, mgr->config.local_addr, XSLOT_ADDR_HUB,
                           mgr->seq++, objects, count, true /* 使用增量格式 */);
  if (ret != XSLOT_OK)
    return ret;

  return xslot_manager_send_frame(mgr, &frame);
}

int xslot_manager_write(xslot_manager_t *mgr, uint16_t target,
                        const xslot_bacnet_object_t *obj) {
  if (!mgr || !obj)
    return XSLOT_ERR_PARAM;

  xslot_frame_t frame;
  int ret = message_build_write(&frame, mgr->config.local_addr, target,
                                mgr->seq++, obj);
  if (ret != XSLOT_OK)
    return ret;

  return xslot_manager_send_frame(mgr, &frame);
}

int xslot_manager_query(xslot_manager_t *mgr, uint16_t target,
                        const uint16_t *object_ids, uint8_t count) {
  if (!mgr || !object_ids || count == 0)
    return XSLOT_ERR_PARAM;

  xslot_frame_t frame;
  int ret = message_build_query(&frame, mgr->config.local_addr, target,
                                mgr->seq++, object_ids, count);
  if (ret != XSLOT_OK)
    return ret;

  return xslot_manager_send_frame(mgr, &frame);
}

int xslot_manager_ping(xslot_manager_t *mgr, uint16_t target) {
  if (!mgr)
    return XSLOT_ERR_PARAM;

  xslot_frame_t frame;
  int ret =
      message_build_ping(&frame, mgr->config.local_addr, target, mgr->seq++);
  if (ret != XSLOT_OK)
    return ret;

  return xslot_manager_send_frame(mgr, &frame);
}

node_table_t xslot_manager_get_node_table(xslot_manager_t *mgr) {
  return mgr ? mgr->node_table : nullptr;
}

void xslot_manager_set_data_cb(xslot_manager_t *mgr,
                               xslot_data_received_cb cb) {
  if (mgr)
    mgr->data_cb = cb;
}

void xslot_manager_set_node_cb(xslot_manager_t *mgr, xslot_node_online_cb cb) {
  if (mgr)
    mgr->node_cb = cb;
}

void xslot_manager_set_write_cb(xslot_manager_t *mgr,
                                xslot_write_request_cb cb) {
  if (mgr)
    mgr->write_cb = cb;
}

void xslot_manager_set_report_cb(xslot_manager_t *mgr,
                                 xslot_report_received_cb cb) {
  if (mgr)
    mgr->report_cb = cb;
}

int xslot_manager_update_config(xslot_manager_t *mgr, uint8_t cell_id,
                                int8_t power_dbm) {
  if (!mgr)
    return XSLOT_ERR_PARAM;

  mgr->config.cell_id = cell_id;
  mgr->config.power_dbm = power_dbm;

  if (mgr->transport && mgr->mode == XSLOT_MODE_WIRELESS) {
    return transport_configure(mgr->transport, cell_id, power_dbm);
  }

  return XSLOT_OK;
}

/* ============================================================================
 * 内部实现
 * ============================================================================
 */

/**
 * @brief 处理接收到的帧
 */
static void handle_frame(xslot_manager_t *mgr, const xslot_frame_t *frame) {
  /* 更新节点表 */
  bool is_new = node_table_update(mgr->node_table, frame->from, 0);
  if (is_new && mgr->node_cb) {
    mgr->node_cb(frame->from, true);
  }

  /* 根据命令类型处理 */
  switch (frame->cmd) {
  case XSLOT_CMD_PING: {
    /* 回复 PONG */
    xslot_frame_t pong;
    message_build_pong(&pong, mgr->config.local_addr, frame->from, frame->seq);
    xslot_manager_send_frame(mgr, &pong);
    break;
  }

  case XSLOT_CMD_PONG:
    /* 心跳响应，节点表已更新 */
    break;

  case XSLOT_CMD_REPORT: {
    /* 数据上报 (汇聚节点接收) */
    if (mgr->report_cb) {
      xslot_bacnet_object_t objects[16];
      int count = message_parse_report(frame, objects, 16);
      if (count > 0) {
        mgr->report_cb(frame->from, objects, count);
      }
    }
    break;
  }

  case XSLOT_CMD_WRITE: {
    /* 写入请求 (边缘节点接收) */
    if (mgr->write_cb) {
      xslot_bacnet_object_t obj;
      if (message_parse_write(frame, &obj) > 0) {
        mgr->write_cb(frame->from, &obj);
      }
    }
    /* 回复 ACK */
    xslot_frame_t ack;
    message_build_write_ack(&ack, mgr->config.local_addr, frame->from,
                            frame->seq, XSLOT_OK);
    xslot_manager_send_frame(mgr, &ack);
    break;
  }

  case XSLOT_CMD_RESPONSE:
  case XSLOT_CMD_QUERY:
    /* 原始数据回调 */
    if (mgr->data_cb) {
      mgr->data_cb(frame->from, frame->data, frame->len);
    }
    break;

  default:
    break;
  }
}

/**
 * @brief 传输层接收回调
 */
static void on_frame_received(void *ctx, const uint8_t *data, uint16_t len) {
  xslot_manager_t *mgr = (xslot_manager_t *)ctx;
  if (!mgr || !data)
    return;

  xslot_frame_t frame;
  if (xslot_frame_decode(data, len, &frame) == XSLOT_OK) { 
    /* 检查目标地址 */
    if (frame.to == mgr->config.local_addr ||
        frame.to == XSLOT_ADDR_BROADCAST) {
      handle_frame(mgr, &frame);
    }
  }
}

/**
 * @brief 检测并创建传输层
 */
static i_transport_t *detect_and_create_transport(xslot_manager_t *mgr) {
  /* 尝试创建 TPMesh 传输层 */
  i_transport_t *transport = tpmesh_transport_create(&mgr->config);
  if (transport) {
    /* 尝试探测模组 */
    if (transport_probe(transport) == XSLOT_OK) {
      mgr->mode = XSLOT_MODE_WIRELESS;
      return transport;
    }
    transport_destroy(transport);
  }

  /* 尝试创建 Direct 传输层 */
  transport = direct_transport_create(&mgr->config);
  if (transport) {
    if (transport_probe(transport) == XSLOT_OK) {
      mgr->mode = XSLOT_MODE_HMI;
      return transport;
    }
    transport_destroy(transport);
  }

  /* 创建 Null 传输层 */
  mgr->mode = XSLOT_MODE_NONE;
  return null_transport_create();
}
