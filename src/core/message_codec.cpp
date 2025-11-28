/**
 * @file message_codec.cpp
 * @brief 消息编解码器实现
 */
#include "message_codec.h"
#include "../bacnet/bacnet_incremental.h"
#include "../bacnet/bacnet_serializer.h"
#include <cstring>
#include <xslot/xslot_error.h>

int message_build_ping(xslot_frame_t *frame, uint16_t from, uint16_t to,
                       uint8_t seq) {
  if (!frame)
    return XSLOT_ERR_PARAM;

  xslot_frame_init(frame);
  frame->from = from;
  frame->to = to;
  frame->seq = seq;
  frame->cmd = XSLOT_CMD_PING;
  frame->len = 0;

  return XSLOT_OK;
}

int message_build_pong(xslot_frame_t *frame, uint16_t from, uint16_t to,
                       uint8_t seq) {
  if (!frame)
    return XSLOT_ERR_PARAM;

  xslot_frame_init(frame);
  frame->from = from;
  frame->to = to;
  frame->seq = seq;
  frame->cmd = XSLOT_CMD_PONG;
  frame->len = 0;

  return XSLOT_OK;
}

int message_build_report(xslot_frame_t *frame, uint16_t from, uint16_t to,
                         uint8_t seq, const xslot_bacnet_object_t *objects,
                         uint8_t count, bool incremental) {
  if (!frame || !objects || count == 0) {
    return XSLOT_ERR_PARAM;
  }

  xslot_frame_init(frame);
  frame->from = from;
  frame->to = to;
  frame->seq = seq;
  frame->cmd = XSLOT_CMD_REPORT;

  int len;
  if (incremental) {
    len = bacnet_incremental_serialize_batch(objects, count, frame->data,
                                             XSLOT_MAX_DATA_LEN);
  } else {
    len = bacnet_serialize_objects(objects, count, frame->data,
                                   XSLOT_MAX_DATA_LEN);
  }

  if (len < 0) {
    return len;
  }

  frame->len = (uint8_t)len;
  return XSLOT_OK;
}

int message_build_query(xslot_frame_t *frame, uint16_t from, uint16_t to,
                        uint8_t seq, const uint16_t *object_ids,
                        uint8_t count) {
  if (!frame || !object_ids || count == 0) {
    return XSLOT_ERR_PARAM;
  }

  // 检查长度: COUNT(1) + IDs(2*count)
  if (1 + count * 2 > XSLOT_MAX_DATA_LEN) {
    return XSLOT_ERR_NO_MEM;
  }

  xslot_frame_init(frame);
  frame->from = from;
  frame->to = to;
  frame->seq = seq;
  frame->cmd = XSLOT_CMD_QUERY;

  uint8_t *p = frame->data;
  *p++ = count;

  for (uint8_t i = 0; i < count; i++) {
    *p++ = (uint8_t)(object_ids[i] & 0xFF);
    *p++ = (uint8_t)((object_ids[i] >> 8) & 0xFF);
  }

  frame->len = 1 + count * 2;
  return XSLOT_OK;
}

int message_build_response(xslot_frame_t *frame, uint16_t from, uint16_t to,
                           uint8_t seq, const xslot_bacnet_object_t *objects,
                           uint8_t count) {
  if (!frame || !objects || count == 0) {
    return XSLOT_ERR_PARAM;
  }

  xslot_frame_init(frame);
  frame->from = from;
  frame->to = to;
  frame->seq = seq;
  frame->cmd = XSLOT_CMD_RESPONSE;

  // RESPONSE 使用完整格式
  int len =
      bacnet_serialize_objects(objects, count, frame->data, XSLOT_MAX_DATA_LEN);
  if (len < 0) {
    return len;
  }

  frame->len = (uint8_t)len;
  return XSLOT_OK;
}

int message_build_write(xslot_frame_t *frame, uint16_t from, uint16_t to,
                        uint8_t seq, const xslot_bacnet_object_t *obj) {
  if (!frame || !obj) {
    return XSLOT_ERR_PARAM;
  }

  xslot_frame_init(frame);
  frame->from = from;
  frame->to = to;
  frame->seq = seq;
  frame->cmd = XSLOT_CMD_WRITE;

  // WRITE 使用完整格式 (单个对象)
  int len = bacnet_serialize_object(obj, frame->data, XSLOT_MAX_DATA_LEN);
  if (len < 0) {
    return len;
  }

  frame->len = (uint8_t)len;
  return XSLOT_OK;
}

int message_build_write_ack(xslot_frame_t *frame, uint16_t from, uint16_t to,
                            uint8_t seq, uint8_t result) {
  if (!frame)
    return XSLOT_ERR_PARAM;

  xslot_frame_init(frame);
  frame->from = from;
  frame->to = to;
  frame->seq = seq;
  frame->cmd = XSLOT_CMD_WRITE_ACK;
  frame->data[0] = result;
  frame->len = 1;

  return XSLOT_OK;
}

int message_parse_report(const xslot_frame_t *frame,
                         xslot_bacnet_object_t *objects, uint8_t max_count) {
  if (!frame || !objects || frame->cmd != XSLOT_CMD_REPORT) {
    return XSLOT_ERR_PARAM;
  }

  if (frame->len < 1) {
    return XSLOT_ERR_PARAM;
  }

  // 检查是否为增量格式 (通过第三个字节的 bit7)
  // 格式: [COUNT][OBJ_ID_L][OBJ_ID_H][TYPE_HINT/OBJ_TYPE]...
  if (frame->len >= 4) {
    uint8_t type_byte = frame->data[3]; // 第一个对象的类型字节
    if (type_byte & 0x80) {
      // 增量格式
      return bacnet_incremental_deserialize_batch(frame->data, frame->len,
                                                  objects, max_count);
    }
  }

  // 完整格式
  return bacnet_deserialize_objects(frame->data, frame->len, objects,
                                    max_count);
}

int message_parse_query(const xslot_frame_t *frame, uint16_t *object_ids,
                        uint8_t max_count) {
  if (!frame || !object_ids || frame->cmd != XSLOT_CMD_QUERY) {
    return XSLOT_ERR_PARAM;
  }

  if (frame->len < 1) {
    return XSLOT_ERR_PARAM;
  }

  const uint8_t *p = frame->data;
  uint8_t count = *p++;

  if (count > max_count) {
    count = max_count;
  }

  // 检查长度
  if (frame->len < 1 + count * 2) {
    return XSLOT_ERR_PARAM;
  }

  for (uint8_t i = 0; i < count; i++) {
    object_ids[i] = p[0] | (p[1] << 8);
    p += 2;
  }

  return count;
}

int message_parse_write(const xslot_frame_t *frame,
                        xslot_bacnet_object_t *obj) {
  if (!frame || !obj || frame->cmd != XSLOT_CMD_WRITE) {
    return XSLOT_ERR_PARAM;
  }

  return bacnet_deserialize_object(frame->data, frame->len, obj);
}
