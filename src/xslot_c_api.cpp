/**
 * @file xslot_c_api.cpp
 * @brief X-Slot C API 实现
 *
 * 薄封装层，内部使用 C++ Manager 类实现。
 */
#include "core/manager.h"
#include <xslot/xslot.h>
#include <xslot/xslot_error.h>

/* 版本字符串 */
static const char VERSION_STRING[] = "2.0.0";

/* ============================================================================
 * 内部辅助函数
 * ============================================================================
 */

static inline xslot::Manager *to_manager(xslot_handle_t handle) {
  return static_cast<xslot::Manager *>(handle);
}

static inline int to_c_error(const xslot::VoidResult &result) {
  if (result) {
    return XSLOT_OK;
  }
  return static_cast<int>(result.error());
}

/* ============================================================================
 * 初始化与控制
 * ============================================================================
 */

xslot_handle_t xslot_init(const xslot_config_t *config) {
  if (!config) {
    return nullptr;
  }

  try {
    return new xslot::Manager(*config);
  } catch (...) {
    return nullptr;
  }
}

void xslot_deinit(xslot_handle_t handle) {
  if (handle) {
    delete to_manager(handle);
  }
}

int xslot_start(xslot_handle_t handle) {
  if (!handle) {
    return XSLOT_ERR_PARAM;
  }

  return to_c_error(to_manager(handle)->start());
}

void xslot_stop(xslot_handle_t handle) {
  if (handle) {
    to_manager(handle)->stop();
  }
}

xslot_run_mode_t xslot_get_run_mode(xslot_handle_t handle) {
  if (!handle) {
    return XSLOT_MODE_NONE;
  }

  return static_cast<xslot_run_mode_t>(to_manager(handle)->get_mode());
}

const char *xslot_get_version(void) { return VERSION_STRING; }

/* ============================================================================
 * 业务数据操作
 * ============================================================================
 */

int xslot_report_objects(xslot_handle_t handle,
                         const xslot_bacnet_object_t *objects, uint8_t count) {
  if (!handle || !objects || count == 0) {
    return XSLOT_ERR_PARAM;
  }

  return to_c_error(to_manager(handle)->report({objects, count}));
}

int xslot_write_object(xslot_handle_t handle, uint16_t target,
                       const xslot_bacnet_object_t *obj) {
  if (!handle || !obj) {
    return XSLOT_ERR_PARAM;
  }

  return to_c_error(to_manager(handle)->write(target, *obj));
}

int xslot_query_objects(xslot_handle_t handle, uint16_t target,
                        const uint16_t *object_ids, uint8_t count) {
  if (!handle || !object_ids || count == 0) {
    return XSLOT_ERR_PARAM;
  }

  return to_c_error(to_manager(handle)->query(target, {object_ids, count}));
}

/* ============================================================================
 * 节点管理
 * ============================================================================
 */

int xslot_send_ping(xslot_handle_t handle, uint16_t target) {
  if (!handle) {
    return XSLOT_ERR_PARAM;
  }

  return to_c_error(to_manager(handle)->ping(target));
}

int xslot_get_nodes(xslot_handle_t handle, xslot_node_info_t *nodes,
                    int max_count) {
  if (!handle || !nodes || max_count <= 0) {
    return XSLOT_ERR_PARAM;
  }

  const auto &table = to_manager(handle)->node_table();
  return static_cast<int>(
      table.get_all_c({nodes, static_cast<size_t>(max_count)}));
}

bool xslot_is_node_online(xslot_handle_t handle, uint16_t addr) {
  if (!handle) {
    return false;
  }

  return to_manager(handle)->node_table().is_online(addr);
}

/* ============================================================================
 * 回调注册
 * ============================================================================
 */

void xslot_set_data_callback(xslot_handle_t handle,
                             xslot_data_received_cb callback) {
  if (handle) {
    to_manager(handle)->set_data_callback_c(callback);
  }
}

void xslot_set_node_callback(xslot_handle_t handle,
                             xslot_node_online_cb callback) {
  if (handle) {
    to_manager(handle)->set_node_callback_c(callback);
  }
}

void xslot_set_write_callback(xslot_handle_t handle,
                              xslot_write_request_cb callback) {
  if (handle) {
    to_manager(handle)->set_write_callback_c(callback);
  }
}

void xslot_set_report_callback(xslot_handle_t handle,
                               xslot_report_received_cb callback) {
  if (handle) {
    to_manager(handle)->set_report_callback_c(callback);
  }
}

/* ============================================================================
 * 运行时配置
 * ============================================================================
 */

int xslot_update_wireless_config(xslot_handle_t handle, uint8_t cell_id,
                                 int8_t power_dbm) {
  if (!handle) {
    return XSLOT_ERR_PARAM;
  }

  return to_c_error(to_manager(handle)->update_config(cell_id, power_dbm));
}

/* ============================================================================
 * 工具函数
 * ============================================================================
 */

int xslot_deserialize_objects(const uint8_t *data, uint8_t len,
                              xslot_bacnet_object_t *objects,
                              uint8_t max_count) {
  if (!data || !objects || len == 0 || max_count == 0) {
    return XSLOT_ERR_PARAM;
  }

  // 使用 MessageBuilder 的解析功能
  // TODO: 实现反序列化
  return 0;
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
