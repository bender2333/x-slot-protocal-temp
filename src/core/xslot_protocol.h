/**
 * @file xslot_protocol.h
 * @brief X-Slot 协议帧格式定义
 */
#ifndef XSLOT_PROTOCOL_H
#define XSLOT_PROTOCOL_H

#include <stdint.h>
#include <xslot/xslot_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * 帧格式常量
 * =============================================================================
 */

#define XSLOT_FRAME_HEADER_SIZE                                                \
  8 /**< 帧头大小: SYNC(1)+FROM(2)+TO(2)+SEQ(1)+CMD(1)+LEN(1) */
#define XSLOT_FRAME_CRC_SIZE 2 /**< CRC 大小 */
#define XSLOT_FRAME_MIN_SIZE (XSLOT_FRAME_HEADER_SIZE + XSLOT_FRAME_CRC_SIZE)
#define XSLOT_FRAME_MAX_SIZE                                                   \
  (XSLOT_FRAME_HEADER_SIZE + XSLOT_MAX_DATA_LEN + XSLOT_FRAME_CRC_SIZE)

/* 帧字段偏移 */
#define XSLOT_OFFSET_SYNC 0
#define XSLOT_OFFSET_FROM 1
#define XSLOT_OFFSET_TO 3
#define XSLOT_OFFSET_SEQ 5
#define XSLOT_OFFSET_CMD 6
#define XSLOT_OFFSET_LEN 7
#define XSLOT_OFFSET_DATA 8

/* =============================================================================
 * 帧结构
 * =============================================================================
 */

/**
 * @brief X-Slot 协议帧
 */
typedef struct {
  uint8_t sync;                     /**< 同步字节 (0xAA) */
  uint16_t from;                    /**< 源地址 */
  uint16_t to;                      /**< 目标地址 */
  uint8_t seq;                      /**< 序列号 (0-255) */
  uint8_t cmd;                      /**< 命令类型 (xslot_cmd_t) */
  uint8_t len;                      /**< 数据长度 */
  uint8_t data[XSLOT_MAX_DATA_LEN]; /**< 载荷数据 */
  uint16_t crc;                     /**< CRC16 校验 */
} xslot_frame_t;

/* =============================================================================
 * CRC 计算
 * =============================================================================
 */

/**
 * @brief 计算 CRC16-CCITT
 * @param data 数据
 * @param len 长度
 * @return CRC16 值
 */
uint16_t xslot_crc16(const uint8_t *data, uint16_t len);

/* =============================================================================
 * 帧操作
 * =============================================================================
 */

/**
 * @brief 初始化帧
 * @param frame 帧指针
 */
void xslot_frame_init(xslot_frame_t *frame);

/**
 * @brief 编码帧到缓冲区
 * @param frame 帧
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 编码后的字节数，失败返回负数
 */
int xslot_frame_encode(const xslot_frame_t *frame, uint8_t *buffer,
                       uint16_t buffer_size);

/**
 * @brief 从缓冲区解码帧
 * @param buffer 输入缓冲区
 * @param len 缓冲区长度
 * @param frame 输出帧
 * @return 成功返回 XSLOT_OK，失败返回错误码
 */
int xslot_frame_decode(const uint8_t *buffer, uint16_t len,
                       xslot_frame_t *frame);

/**
 * @brief 验证帧 CRC
 * @param buffer 帧数据
 * @param len 帧长度
 * @return true=校验通过, false=校验失败
 */
bool xslot_frame_verify_crc(const uint8_t *buffer, uint16_t len);

/**
 * @brief 获取帧总长度
 * @param data_len 数据长度
 * @return 帧总字节数
 */
static inline uint16_t xslot_frame_total_size(uint8_t data_len) {
  return XSLOT_FRAME_HEADER_SIZE + data_len + XSLOT_FRAME_CRC_SIZE;
}

#ifdef __cplusplus
}
#endif

#endif /* XSLOT_PROTOCOL_H */
