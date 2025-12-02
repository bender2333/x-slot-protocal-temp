/**
 * @file xslot_types.h
 * @brief X-Slot 公共类型定义
 */
#ifndef XSLOT_TYPES_H
#define XSLOT_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * 协议常量
 * =============================================================================
 */
#define XSLOT_VERSION_MAJOR 1
#define XSLOT_VERSION_MINOR 0
#define XSLOT_VERSION_PATCH 0

#define XSLOT_MAX_DATA_LEN 128 /**< 最大数据长度 */
#define XSLOT_MAX_NODES 64     /**< 最大节点数 */
#define XSLOT_SYNC_BYTE 0xAA   /**< 同步字节 */

/* 地址定义 */
#define XSLOT_ADDR_HUB 0xFFFE       /**< 汇聚节点地址 */
#define XSLOT_ADDR_HMI 0xFF00       /**< HMI 固定地址 */
#define XSLOT_ADDR_EDGE_MIN 0xFFBE  /**< 边缘节点最小地址 */
#define XSLOT_ADDR_EDGE_MAX 0xFFFD  /**< 边缘节点最大地址 */
#define XSLOT_ADDR_BROADCAST 0x0000 /**< 广播地址 */

/* =============================================================================
 * 枚举类型
 * =============================================================================
 */

/**
 * @brief 运行模式
 */
typedef enum {
  XSLOT_MODE_NONE = 0,     /**< 未检测到设备 (空闲/异常) */
  XSLOT_MODE_WIRELESS = 1, /**< TP1107 Mesh 无线模式 */
  XSLOT_MODE_HMI = 2       /**< HMI 串口直连模式 */
} xslot_run_mode_t;

/**
 * @brief 命令类型
 */
typedef enum {
  XSLOT_CMD_PING = 0x01,     /**< 心跳请求 */
  XSLOT_CMD_PONG = 0x02,     /**< 心跳响应 */
  XSLOT_CMD_REPORT = 0x10,   /**< 数据上报 (边缘→汇聚) */
  XSLOT_CMD_QUERY = 0x11,    /**< 数据查询 (HMI→汇聚) */
  XSLOT_CMD_RESPONSE = 0x12, /**< 查询响应 (汇聚→HMI) */
  XSLOT_CMD_WRITE = 0x20,    /**< 远程写入 (汇聚→边缘) */
  XSLOT_CMD_WRITE_ACK = 0x21 /**< 写入确认 (边缘→汇聚) */
} xslot_cmd_t;

/**
 * @brief BACnet 对象类型
 */
typedef enum {
  XSLOT_OBJ_ANALOG_INPUT = 0,  /**< AI */
  XSLOT_OBJ_ANALOG_OUTPUT = 1, /**< AO */
  XSLOT_OBJ_ANALOG_VALUE = 2,  /**< AV */
  XSLOT_OBJ_BINARY_INPUT = 3,  /**< BI */
  XSLOT_OBJ_BINARY_OUTPUT = 4, /**< BO */
  XSLOT_OBJ_BINARY_VALUE = 5   /**< BV */
} xslot_object_type_t;

/**
 * @brief 模组功耗模式
 */
typedef enum {
  XSLOT_POWER_MODE_LOW = 2,   /**< Type C 低功耗模式 (默认) */
  XSLOT_POWER_MODE_NORMAL = 3 /**< Type D 非低功耗模式 */
} xslot_power_mode_t;

/* =============================================================================
 * 数据结构
 * =============================================================================
 */

/**
 * @brief BACnet 对象数据 (用于无线传输)
 */
typedef struct {
  uint16_t object_id;  /**< 对象实例号 */
  uint8_t object_type; /**< 对象类型 (xslot_object_type_t) */
  uint8_t flags;       /**< 标志位: bit0=Changed, bit1=OutOfService */
  union {
    float analog;    /**< 模拟值 (AI/AO/AV) */
    uint8_t binary;  /**< 二进制值 (BI/BO/BV): 0 或 1 */
    uint8_t raw[16]; /**< 原始数据 */
  } present_value;
} xslot_bacnet_object_t;

/** 标志位定义 */
#define XSLOT_FLAG_CHANGED 0x01
#define XSLOT_FLAG_OUT_OF_SERVICE 0x02

/**
 * @brief 节点信息
 */
typedef struct {
  uint16_t addr;        /**< 节点地址 */
  uint32_t last_seen;   /**< 最后心跳时间 (ms) */
  int8_t rssi;          /**< 信号强度 (dBm) */
  bool online;          /**< 在线状态 */
  uint8_t object_count; /**< 对象数量 */
} xslot_node_info_t;

/**
 * @brief 配置结构
 */
typedef struct {
  uint16_t local_addr;            /**< 本地地址 */
  uint8_t cell_id;                /**< 小区 ID (0-255) */
  int8_t power_dbm;               /**< 发射功率 (-30-36 dBm) */
  uint16_t wakeup_period_ms;      /**< 唤醒周期 (ms) */
  uint32_t uart_baudrate;         /**< 串口波特率 (默认 115200) */
  uint32_t heartbeat_interval_ms; /**< 心跳间隔 (建议 30000-60000 ms) */
  uint32_t heartbeat_timeout_ms;  /**< 心跳超时 (ms) */
  char uart_port[64]; /**< 串口设备名 (如 "COM3" 或 "/dev/ttyUSB0") */
  xslot_power_mode_t power_mode; /**< 功耗模式 (默认 LOW) */
} xslot_config_t;

/**
 * @brief 协议栈句柄
 */
typedef void *xslot_handle_t;

/* =============================================================================
 * 回调函数类型
 * =============================================================================
 */

/**
 * @brief 数据接收回调
 * @param from 源地址
 * @param data 数据指针
 * @param len 数据长度
 */
typedef void (*xslot_data_received_cb)(uint16_t from, const uint8_t *data,
                                       uint8_t len);

/**
 * @brief 节点上下线回调
 * @param addr 节点地址
 * @param online true=上线, false=离线
 */
typedef void (*xslot_node_online_cb)(uint16_t addr, bool online);

/**
 * @brief 写入请求回调
 * @param from 源地址
 * @param obj 对象数据
 */
typedef void (*xslot_write_request_cb)(uint16_t from,
                                       const xslot_bacnet_object_t *obj);

/**
 * @brief 对象上报回调 (汇聚节点使用)
 * @param from 源地址
 * @param objects 对象数组
 * @param count 对象数量
 */
typedef void (*xslot_report_received_cb)(uint16_t from,
                                         const xslot_bacnet_object_t *objects,
                                         uint8_t count);

#ifdef __cplusplus
}
#endif

#endif /* XSLOT_TYPES_H */
