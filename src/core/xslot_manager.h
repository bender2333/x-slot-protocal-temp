/**
 * @file xslot_manager.h
 * @brief X-Slot 协议栈核心管理器
 */
#ifndef XSLOT_MANAGER_H
#define XSLOT_MANAGER_H

#include "node_table.h"
#include "xslot_protocol.h"
#include <xslot/xslot_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 管理器内部结构 (前向声明)
 */
typedef struct xslot_manager xslot_manager_t;

/**
 * @brief 创建管理器
 */
xslot_manager_t *xslot_manager_create(const xslot_config_t *config);

/**
 * @brief 销毁管理器
 */
void xslot_manager_destroy(xslot_manager_t *mgr);

/**
 * @brief 启动管理器
 */
int xslot_manager_start(xslot_manager_t *mgr);

/**
 * @brief 停止管理器
 */
void xslot_manager_stop(xslot_manager_t *mgr);

/**
 * @brief 获取运行模式
 */
xslot_run_mode_t xslot_manager_get_mode(xslot_manager_t *mgr);

/**
 * @brief 发送帧
 */
int xslot_manager_send_frame(xslot_manager_t *mgr, const xslot_frame_t *frame);

/**
 * @brief 上报对象
 */
int xslot_manager_report(xslot_manager_t *mgr,
                         const xslot_bacnet_object_t *objects, uint8_t count);

/**
 * @brief 发送写入命令
 */
int xslot_manager_write(xslot_manager_t *mgr, uint16_t target,
                        const xslot_bacnet_object_t *obj);

/**
 * @brief 发送查询命令
 */
int xslot_manager_query(xslot_manager_t *mgr, uint16_t target,
                        const uint16_t *object_ids, uint8_t count);

/**
 * @brief 发送心跳
 */
int xslot_manager_ping(xslot_manager_t *mgr, uint16_t target);

/**
 * @brief 获取节点表
 */
node_table_t xslot_manager_get_node_table(xslot_manager_t *mgr);

/**
 * @brief 设置回调
 */
void xslot_manager_set_data_cb(xslot_manager_t *mgr, xslot_data_received_cb cb);
void xslot_manager_set_node_cb(xslot_manager_t *mgr, xslot_node_online_cb cb);
void xslot_manager_set_write_cb(xslot_manager_t *mgr,
                                xslot_write_request_cb cb);
void xslot_manager_set_report_cb(xslot_manager_t *mgr,
                                 xslot_report_received_cb cb);

/**
 * @brief 更新无线配置
 */
int xslot_manager_update_config(xslot_manager_t *mgr, uint8_t cell_id,
                                int8_t power_dbm);

#ifdef __cplusplus
}
#endif

#endif /* XSLOT_MANAGER_H */
