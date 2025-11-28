/**
 * @file tpmesh_at_driver.h
 * @brief TPMesh AT 驱动层
 *
 * 纯 AT 命令抽象，处理同步响应和异步 URC。
 */
#ifndef TPMESH_AT_DRIVER_H
#define TPMESH_AT_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief URC 类型
 */
typedef enum {
  URC_NNMI,  /**< 数据接收 */
  URC_SEND,  /**< 发送状态 */
  URC_ROUTE, /**< 路由变化 */
  URC_ACK,   /**< 送达确认 */
  URC_BOOT,  /**< 模组重启 */
  URC_READY, /**< AT 就绪 */
} tpmesh_urc_type_t;

/**
 * @brief URC 数据
 */
typedef struct {
  tpmesh_urc_type_t type;
  uint16_t src_addr;
  uint16_t dest_addr;
  int8_t rssi;
  uint8_t sn;
  uint8_t data[256];
  uint16_t data_len;
  char result[32];
} tpmesh_urc_t;

/**
 * @brief URC 回调
 */
typedef void (*tpmesh_urc_cb)(void *ctx, const tpmesh_urc_t *urc);

/**
 * @brief AT 驱动句柄
 */
typedef struct tpmesh_at_driver *tpmesh_at_driver_t;

/**
 * @brief 创建 AT 驱动
 * @param port 串口名
 * @param baudrate 波特率
 * @return 驱动句柄
 */
tpmesh_at_driver_t tpmesh_at_create(const char *port, uint32_t baudrate);

/**
 * @brief 销毁 AT 驱动
 */
void tpmesh_at_destroy(tpmesh_at_driver_t drv);

/**
 * @brief 启动驱动
 */
int tpmesh_at_start(tpmesh_at_driver_t drv);

/**
 * @brief 停止驱动
 */
void tpmesh_at_stop(tpmesh_at_driver_t drv);

/**
 * @brief 设置 URC 回调
 */
void tpmesh_at_set_urc_callback(tpmesh_at_driver_t drv, tpmesh_urc_cb cb,
                                void *ctx);

/**
 * @brief 发送 AT 命令 (同步等待响应)
 * @param drv 驱动
 * @param cmd 命令 (不含 AT+ 前缀和 \r\n)
 * @param timeout_ms 超时时间
 * @return 0=OK, <0=错误
 */
int tpmesh_at_send_cmd(tpmesh_at_driver_t drv, const char *cmd,
                       uint32_t timeout_ms);

/**
 * @brief 发送 AT 命令并获取响应
 * @param drv 驱动
 * @param cmd 命令
 * @param response 响应缓冲区
 * @param resp_size 缓冲区大小
 * @param timeout_ms 超时时间
 * @return 响应长度, <0=错误
 */
int tpmesh_at_send_cmd_resp(tpmesh_at_driver_t drv, const char *cmd,
                            char *response, uint16_t resp_size,
                            uint32_t timeout_ms);

/**
 * @brief 探测模组 (发送 AT 测试)
 * @return 0=成功, <0=失败
 */
int tpmesh_at_probe(tpmesh_at_driver_t drv);

/**
 * @brief 配置地址
 */
int tpmesh_at_set_addr(tpmesh_at_driver_t drv, uint16_t addr);

/**
 * @brief 配置小区 ID
 */
int tpmesh_at_set_cell(tpmesh_at_driver_t drv, uint8_t cell_id);

/**
 * @brief 配置发射功率
 */
int tpmesh_at_set_power(tpmesh_at_driver_t drv, int8_t power_dbm);

/**
 * @brief 发送数据
 * @param drv 驱动
 * @param addr 目标地址
 * @param data 数据
 * @param len 长度
 * @param type 消息类型 (0=UM, 1=AM, 2=FAST, 3=FLOOD)
 * @return 0=成功, <0=失败
 */
int tpmesh_at_send_data(tpmesh_at_driver_t drv, uint16_t addr,
                        const uint8_t *data, uint16_t len, uint8_t type);

#ifdef __cplusplus
}
#endif

#endif /* TPMESH_AT_DRIVER_H */
