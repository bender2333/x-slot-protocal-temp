#ifndef XSLOT_H
#define XSLOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>


// 版本信息
#define XSLOT_VERSION_MAJOR 1
#define XSLOT_VERSION_MINOR 0
#define XSLOT_VERSION_PATCH 0

// 协议常量
#define XSLOT_MAX_DATA_LEN 128
#define XSLOT_MAX_NODES 64
#define XSLOT_ADDR_HUB 0xFFFE
#define XSLOT_ADDR_HMI 0xFF00
#define XSLOT_ADDR_NODE_MIN 0xFFBE
#define XSLOT_ADDR_NODE_MAX 0xFFFD

// 消息类型
typedef enum {
  XSLOT_CMD_PING = 0x01,
  XSLOT_CMD_PONG = 0x02,
  XSLOT_CMD_REPORT = 0x10,
  XSLOT_CMD_QUERY = 0x11,
  XSLOT_CMD_RESPONSE = 0x12,
  XSLOT_CMD_WRITE = 0x20,
  XSLOT_CMD_WRITE_ACK = 0x21
} xslot_command_t;

// 插槽模式
typedef enum {
  XSLOT_MODE_EMPTY = 0,
  XSLOT_MODE_WIRELESS = 1,
  XSLOT_MODE_HMI = 2
} xslot_slot_mode_t;

// 错误码
typedef enum {
  XSLOT_OK = 0,
  XSLOT_ERR_PARAM = -1,
  XSLOT_ERR_TIMEOUT = -2,
  XSLOT_ERR_CRC = -3,
  XSLOT_ERR_NO_MEM = -4,
  XSLOT_ERR_BUSY = -5,
  XSLOT_ERR_OFFLINE = -6
} xslot_error_t;

// BACnet对象类型
typedef enum {
  BACNET_OBJ_ANALOG_INPUT = 0,
  BACNET_OBJ_ANALOG_OUTPUT = 1,
  BACNET_OBJ_ANALOG_VALUE = 2,
  BACNET_OBJ_BINARY_INPUT = 3,
  BACNET_OBJ_BINARY_OUTPUT = 4,
  BACNET_OBJ_BINARY_VALUE = 5
} bacnet_object_type_t;

// BACnet对象数据
typedef struct {
  uint16_t object_id;  // 对象ID
  uint8_t object_type; // 对象类型
  uint8_t flags;       // 标志位
  union {
    float analog_value;   // 模拟值
    uint8_t binary_value; // 二进制值
    uint8_t raw_data[16]; // 原始数据
  } value;
} xslot_bacnet_object_t;

// 节点信息
typedef struct {
  uint16_t addr;        // 节点地址
  uint32_t last_seen;   // 最后心跳时间(ms)
  uint8_t rssi;         // 信号强度
  bool online;          // 在线状态
  uint8_t object_count; // 对象数量
} xslot_node_info_t;

// 配置结构
typedef struct {
  uint16_t local_addr;            // 本地地址
  uint8_t cell_id;                // 小区ID
  uint8_t power_dbm;              // 发射功率(dBm)
  uint16_t wakeup_period_ms;      // 唤醒周期(ms)
  uint32_t uart_baudrate;         // 串口波特率
  uint32_t heartbeat_interval_ms; // 心跳间隔(ms)
  uint32_t heartbeat_timeout_ms;  // 心跳超时(ms)
} xslot_config_t;

// 回调函数类型
typedef void (*xslot_data_received_cb)(uint16_t from, const uint8_t *data,
                                       uint8_t len);
typedef void (*xslot_node_online_cb)(uint16_t addr, bool online);
typedef void (*xslot_write_request_cb)(uint16_t from,
                                       const xslot_bacnet_object_t *obj);

// 句柄类型
typedef void *xslot_handle_t;

// =============================================================================
// API函数
// =============================================================================

/**
 * @brief 初始化X-Slot协议栈
 * @param config 配置参数
 * @return 句柄，失败返回NULL
 */
xslot_handle_t xslot_init(const xslot_config_t *config);

/**
 * @brief 释放X-Slot协议栈
 * @param handle 句柄
 */
void xslot_deinit(xslot_handle_t handle);

/**
 * @brief 启动协议栈
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
 * @brief 探测插槽模式
 * @param handle 句柄
 * @return 插槽模式
 */
xslot_slot_mode_t xslot_detect_mode(xslot_handle_t handle);

/**
 * @brief 发送心跳
 * @param handle 句柄
 * @param target 目标地址
 * @return 错误码
 */
int xslot_send_ping(xslot_handle_t handle, uint16_t target);

/**
 * @brief 上报BACnet对象数据
 * @param handle 句柄
 * @param objects 对象数组
 * @param count 对象数量
 * @return 错误码
 */
int xslot_report_objects(xslot_handle_t handle,
                         const xslot_bacnet_object_t *objects, uint8_t count);

/**
 * @brief 远程写入BACnet对象
 * @param handle 句柄
 * @param target 目标节点地址
 * @param obj 对象数据
 * @return 错误码
 */
int xslot_write_object(xslot_handle_t handle, uint16_t target,
                       const xslot_bacnet_object_t *obj);

/**
 * @brief 查询节点对象数据
 * @param handle 句柄
 * @param target 目标节点地址
 * @param object_ids 对象ID数组
 * @param count 对象数量
 * @return 错误码
 */
int xslot_query_objects(xslot_handle_t handle, uint16_t target,
                        const uint16_t *object_ids, uint8_t count);

/**
 * @brief 获取节点列表
 * @param handle 句柄
 * @param nodes 节点信息数组
 * @param max_count 最大数量
 * @return 实际节点数量
 */
int xslot_get_nodes(xslot_handle_t handle, xslot_node_info_t *nodes,
                    int max_count);

/**
 * @brief 设置数据接收回调
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
 * @brief 设置写入请求回调
 * @param handle 句柄
 * @param callback 回调函数
 */
void xslot_set_write_callback(xslot_handle_t handle,
                              xslot_write_request_cb callback);

/**
 * @brief 配置TP1107模块参数
 * @param handle 句柄
 * @param addr 通信地址
 * @param cell_id 小区ID
 * @param power_dbm 发射功率
 * @return 错误码
 */
int xslot_configure_tp1107(xslot_handle_t handle, uint16_t addr,
                           uint8_t cell_id, int8_t power_dbm);

/**
 * @brief 获取版本信息
 * @return 版本字符串
 */
const char *xslot_get_version(void);

#ifdef __cplusplus
}
#endif

#endif // XSLOT_H