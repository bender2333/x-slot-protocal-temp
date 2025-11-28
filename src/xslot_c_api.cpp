/**
 * @file xslot_c_api.cpp
 * @brief X-Slot C API 实现
 */
#include "bacnet/bacnet_serializer.h"
#include "core/xslot_manager.h"
#include <xslot/xslot.h>

/* 版本字符串 */
static const char VERSION_STRING[] = "1.0.0";

/* ============================================================================
 * 初始化与控制
 * ============================================================================
 */

xslot_handle_t xslot_init(const xslot_config_t *config) {
  if (!config)
    return nullptr;

  return xslot_manager_create(config);
}

void xslot_deinit(xslot_handle_t handle) {
  if (handle) {
    xslot_manager_destroy((xslot_manager_t *)handle);
  }
}

int xslot_start(xslot_handle_t handle) {
  if (!handle)
    return XSLOT_ERR_PARAM;

  return xslot_manager_start((xslot_manager_t *)handle);
}

void xslot_stop(xslot_handle_t handle) {
  if (handle) {
    xslot_manager_stop((xslot_manager_t *)handle);
  }
}

xslot_run_mode_t xslot_get_run_mode(xslot_handle_t handle) {
  if (!handle)
    return XSLOT_MODE_NONE;

  return xslot_manager_get_mode((xslot_manager_t *)handle);
}

const char *xslot_get_version(void) { return VERSION_STRING; }

/* ============================================================================
 * 业务数据操作
 * ============================================================================
 */

int xslot_report_objects(xslot_handle_t handle,
                         const xslot_bacnet_object_t *objects, uint8_t count) {
  if (!handle || !objects || count == 0)
    return XSLOT_ERR_PARAM;

  return xslot_manager_report((xslot_manager_t *)handle, objects, count);
}

int xslot_write_object(xslot_handle_t handle, uint16_t target,
                       const xslot_bacnet_object_t *obj) {
  if (!handle || !obj)
    return XSLOT_ERR_PARAM;

  return xslot_manager_write((xslot_manager_t *)handle, target, obj);
}

int xslot_query_objects(xslot_handle_t handle, uint16_t target,
                        const uint16_t *object_ids, uint8_t count) {
  if (!handle || !object_ids || count == 0)
    return XSLOT_ERR_PARAM;

  return xslot_manager_query((xslot_manager_t *)handle, target, object_ids,
                             count);
}

/* ============================================================================
 * 节点管理
 * ============================================================================
 */

int xslot_send_ping(xslot_handle_t handle, uint16_t target) {
  if (!handle)
    return XSLOT_ERR_PARAM;

  return xslot_manager_ping((xslot_manager_t *)handle, target);
}

int xslot_get_nodes(xslot_handle_t handle, xslot_node_info_t *nodes,
                    int max_count) {
  if (!handle || !nodes || max_count <= 0)
    return XSLOT_ERR_PARAM;

  node_table_t table = xslot_manager_get_node_table((xslot_manager_t *)handle);
  if (!table)
    return 0;

  return node_table_get_all(table, nodes, max_count);
}

bool xslot_is_node_online(xslot_handle_t handle, uint16_t addr) {
  if (!handle)
    return false;

  node_table_t table = xslot_manager_get_node_table((xslot_manager_t *)handle);
  if (!table)
    return false;

  return node_table_is_online(table, addr);
}

/* ============================================================================
 * 回调注册
 * ============================================================================
 */

void xslot_set_data_callback(xslot_handle_t handle,
                             xslot_data_received_cb callback) {
  if (handle) {
    xslot_manager_set_data_cb((xslot_manager_t *)handle, callback);
  }
}

void xslot_set_node_callback(xslot_handle_t handle,
                             xslot_node_online_cb callback) {
  if (handle) {
    xslot_manager_set_node_cb((xslot_manager_t *)handle, callback);
  }
}

void xslot_set_write_callback(xslot_handle_t handle,
                              xslot_write_request_cb callback) {
  if (handle) {
    xslot_manager_set_write_cb((xslot_manager_t *)handle, callback);
  }
}

void xslot_set_report_callback(xslot_handle_t handle,
                               xslot_report_received_cb callback) {
  if (handle) {
    xslot_manager_set_report_cb((xslot_manager_t *)handle, callback);
  }
}

/* ============================================================================
 * 运行时配置
 * ============================================================================
 */

int xslot_update_wireless_config(xslot_handle_t handle, uint8_t cell_id,
                                 int8_t power_dbm) {
  if (!handle)
    return XSLOT_ERR_PARAM;

  return xslot_manager_update_config((xslot_manager_t *)handle, cell_id,
                                     power_dbm);
}

/* ============================================================================
 * 工具函数
 * ============================================================================
 */

int xslot_deserialize_objects(const uint8_t *data, uint8_t len,
                              xslot_bacnet_object_t *objects,
                              uint8_t max_count) {
  if (!data || !objects || len == 0 || max_count == 0)
    return XSLOT_ERR_PARAM;

  return bacnet_deserialize_objects(data, len, objects, max_count);
}

/* ============================================================================
 * 错误码描述
 * ============================================================================
 */

const char *xslot_strerror(xslot_error_t err) {
  switch (err) {
  case XSLOT_OK:
    return "Success";
  case XSLOT_ERR_PARAM:
    return "Invalid parameter";
  case XSLOT_ERR_TIMEOUT:
    return "Timeout";
  case XSLOT_ERR_CRC:
    return "CRC check failed";
  case XSLOT_ERR_NO_MEM:
    return "Out of memory";
  case XSLOT_ERR_BUSY:
    return "System busy";
  case XSLOT_ERR_OFFLINE:
    return "Node offline";
  case XSLOT_ERR_NO_DEVICE:
    return "No device detected";
  case XSLOT_ERR_NOT_INIT:
    return "Not initialized";
  case XSLOT_ERR_SEND_FAIL:
    return "Send failed";
  default:
    return "Unknown error";
  }
}
