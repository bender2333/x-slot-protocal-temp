/**
 * @file message_codec.h
 * @brief 消息编解码器
 */
#ifndef MESSAGE_CODEC_H
#define MESSAGE_CODEC_H

#include "xslot_protocol.h"
#include <xslot/xslot_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 构建 PING 帧
 */
int message_build_ping(xslot_frame_t *frame, uint16_t from, uint16_t to,
                       uint8_t seq);

/**
 * @brief 构建 PONG 帧
 */
int message_build_pong(xslot_frame_t *frame, uint16_t from, uint16_t to,
                       uint8_t seq);

/**
 * @brief 构建 REPORT 帧 (数据上报)
 * @param frame 输出帧
 * @param from 源地址
 * @param to 目标地址
 * @param seq 序列号
 * @param objects 对象数组
 * @param count 对象数量
 * @param incremental true=增量格式, false=完整格式
 * @return 错误码
 */
int message_build_report(xslot_frame_t *frame, uint16_t from, uint16_t to,
                         uint8_t seq, const xslot_bacnet_object_t *objects,
                         uint8_t count, bool incremental);

/**
 * @brief 构建 QUERY 帧 (数据查询)
 */
int message_build_query(xslot_frame_t *frame, uint16_t from, uint16_t to,
                        uint8_t seq, const uint16_t *object_ids, uint8_t count);

/**
 * @brief 构建 RESPONSE 帧 (查询响应)
 */
int message_build_response(xslot_frame_t *frame, uint16_t from, uint16_t to,
                           uint8_t seq, const xslot_bacnet_object_t *objects,
                           uint8_t count);

/**
 * @brief 构建 WRITE 帧 (远程写入)
 */
int message_build_write(xslot_frame_t *frame, uint16_t from, uint16_t to,
                        uint8_t seq, const xslot_bacnet_object_t *obj);

/**
 * @brief 构建 WRITE_ACK 帧 (写入确认)
 */
int message_build_write_ack(xslot_frame_t *frame, uint16_t from, uint16_t to,
                            uint8_t seq, uint8_t result);

/**
 * @brief 解析 REPORT 帧载荷
 */
int message_parse_report(const xslot_frame_t *frame,
                         xslot_bacnet_object_t *objects, uint8_t max_count);

/**
 * @brief 解析 QUERY 帧载荷
 */
int message_parse_query(const xslot_frame_t *frame, uint16_t *object_ids,
                        uint8_t max_count);

/**
 * @brief 解析 WRITE 帧载荷
 */
int message_parse_write(const xslot_frame_t *frame, xslot_bacnet_object_t *obj);

#ifdef __cplusplus
}
#endif

#endif /* MESSAGE_CODEC_H */
