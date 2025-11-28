/**
 * @file xslot.h
 * @brief X-Slot 协议栈 C API 主入口
 *
 * X-Slot 是专为 DDC 楼宇控制器设计的无线互联协议 SDK，
 * 支持 TP1107 Mesh 无线模式和 HMI 串口直连模式。
 */
#ifndef XSLOT_H
#define XSLOT_H

#include "xslot_error.h"
#include "xslot_types.h"


#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * 初始化与控制
 * =============================================================================
 */

/**
 * @brief 初始化协议栈
 * @param config 配置参数
 * @return 句柄，失败返回 NULL
 */
xslot_handle_t xslot_init(const xslot_config_t *config);

/**
 * @brief 释放协议栈资源
 * @param handle 句柄
 */
void xslot_deinit(xslot_handle_t handle);

/**
 * @brief 启动协议栈 (自动检测模式并应用配置)
 * @param handle 句柄
 * @return 错误码
 */
int xslot_start(xslot_handle_t handle);

/**
 * @brief 停止协议栈
 * @param handle 句柄
 */
void xslot_stop(xslot_handle_t handle);

/**
 * @brief 获取当前运行模式 (在 start 成功后调用)
 * @param handle 句柄
 * @return 运行模式
 */
xslot_run_mode_t xslot_get_run_mode(xslot_handle_t handle);

/**
 * @brief 获取版本信息
 * @return 版本字符串 (如 "1.0.0")
 */
const char *xslot_get_version(void);

/* =============================================================================
 * 业务数据操作
 * =============================================================================
 */

/**
 * @brief 上报 BACnet 对象数据 (边缘节点使用)
 * @param handle 句柄
 * @param objects 对象数组
 * @param count 对象数量
 * @return 错误码
 */
int xslot_report_objects(xslot_handle_t handle,
                         const xslot_bacnet_object_t *objects, uint8_t count);

/**
 * @brief 远程写入 BACnet 对象 (汇聚节点使用)
 * @param handle 句柄
 * @param target 目标节点地址
 * @param obj 对象数据
 * @return 错误码
 */
int xslot_write_object(xslot_handle_t handle, uint16_t target,
                       const xslot_bacnet_object_t *obj);

/**
 * @brief 查询节点对象数据 (HMI 使用)
 * @param handle 句柄
 * @param target 目标节点地址
 * @param object_ids 对象 ID 数组
 * @param count 对象数量
 * @return 错误码
 */
int xslot_query_objects(xslot_handle_t handle, uint16_t target,
                        const uint16_t *object_ids, uint8_t count);

/* =============================================================================
 * 节点管理
 * =============================================================================
 */

/**
 * @brief 发送心跳
 * @param handle 句柄
 * @param target 目标地址
 * @return 错误码
 */
int xslot_send_ping(xslot_handle_t handle, uint16_t target);

/**
 * @brief 获取节点列表
 * @param handle 句柄
 * @param nodes 节点信息数组 (输出)
 * @param max_count 数组最大容量
 * @return 实际节点数量，失败返回负数错误码
 */
int xslot_get_nodes(xslot_handle_t handle, xslot_node_info_t *nodes,
                    int max_count);

/**
 * @brief 检查节点是否在线
 * @param handle 句柄
 * @param addr 节点地址
 * @return true=在线, false=离线或未知
 */
bool xslot_is_node_online(xslot_handle_t handle, uint16_t addr);

/* =============================================================================
 * 回调注册
 * =============================================================================
 */

/**
 * @brief 设置原始数据接收回调
 * @param handle 句柄
 * @param callback 回调函数
 */
void xslot_set_data_callback(xslot_handle_t handle,
                             xslot_data_received_cb callback);

/**
 * @brief 设置节点上下线回调
 * @param handle 句柄
 * @param callback 回调函数
 */
void xslot_set_node_callback(xslot_handle_t handle,
                             xslot_node_online_cb callback);

/**
 * @brief 设置写入请求回调 (边缘节点使用)
 * @param handle 句柄
 * @param callback 回调函数
 */
void xslot_set_write_callback(xslot_handle_t handle,
                              xslot_write_request_cb callback);

/**
 * @brief 设置对象上报回调 (汇聚节点使用)
 * @param handle 句柄
 * @param callback 回调函数
 */
void xslot_set_report_callback(xslot_handle_t handle,
                               xslot_report_received_cb callback);

/* =============================================================================
 * 运行时配置 (可选)
 * =============================================================================
 */

/**
 * @brief 动态修改无线参数
 * @param handle 句柄
 * @param cell_id 小区 ID
 * @param power_dbm 发射功率
 * @return 错误码
 */
int xslot_update_wireless_config(xslot_handle_t handle, uint8_t cell_id,
                                 int8_t power_dbm);

/* =============================================================================
 * 工具函数
 * =============================================================================
 */

/**
 * @brief 反序列化 BACnet 对象 (从原始数据)
 * @param data 原始数据
 * @param len 数据长度
 * @param objects 对象数组 (输出)
 * @param max_count 数组最大容量
 * @return 解析的对象数量，失败返回负数错误码
 */
int xslot_deserialize_objects(const uint8_t *data, uint8_t len,
                              xslot_bacnet_object_t *objects,
                              uint8_t max_count);

#ifdef __cplusplus
}
#endif

#endif /* XSLOT_H */
