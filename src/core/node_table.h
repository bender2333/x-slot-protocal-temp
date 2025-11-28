/**
 * @file node_table.h
 * @brief 节点表管理
 */
#ifndef NODE_TABLE_H
#define NODE_TABLE_H

#include <stdbool.h>
#include <stdint.h>
#include <xslot/xslot_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 节点表句柄
 */
typedef struct node_table *node_table_t;

/**
 * @brief 创建节点表
 * @param max_nodes 最大节点数
 * @return 节点表句柄
 */
node_table_t node_table_create(uint8_t max_nodes);

/**
 * @brief 销毁节点表
 */
void node_table_destroy(node_table_t table);

/**
 * @brief 更新节点 (收到心跳或数据时调用)
 * @param table 节点表
 * @param addr 节点地址
 * @param rssi 信号强度
 * @return true=新节点上线, false=已存在节点
 */
bool node_table_update(node_table_t table, uint16_t addr, int8_t rssi);

/**
 * @brief 检查并标记超时节点
 * @param table 节点表
 * @param timeout_ms 超时时间
 * @param offline_cb 离线回调 (可为 NULL)
 */
void node_table_check_timeout(node_table_t table, uint32_t timeout_ms,
                              xslot_node_online_cb offline_cb);

/**
 * @brief 检查节点是否在线
 */
bool node_table_is_online(node_table_t table, uint16_t addr);

/**
 * @brief 获取节点信息
 * @return true=找到, false=未找到
 */
bool node_table_get_node(node_table_t table, uint16_t addr,
                         xslot_node_info_t *info);

/**
 * @brief 获取所有节点
 * @return 节点数量
 */
int node_table_get_all(node_table_t table, xslot_node_info_t *nodes,
                       int max_count);

/**
 * @brief 获取在线节点数量
 */
int node_table_online_count(node_table_t table);

/**
 * @brief 移除节点
 */
void node_table_remove(node_table_t table, uint16_t addr);

/**
 * @brief 清空节点表
 */
void node_table_clear(node_table_t table);

#ifdef __cplusplus
}
#endif

#endif /* NODE_TABLE_H */
