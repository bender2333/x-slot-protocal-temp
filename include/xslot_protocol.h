// =============================================================================
// src/core/xslot_protocol.h - 协议核心定义
// =============================================================================

#ifndef XSLOT_PROTOCOL_H
#define XSLOT_PROTOCOL_H

#include <cstdint>
#include <string>
#include <vector>

namespace xslot {

// 协议常量
constexpr uint8_t SYNC_BYTE = 0xAA;
constexpr size_t HEADER_SIZE = 7;
constexpr size_t CRC_SIZE = 2;
constexpr size_t MAX_FRAME_SIZE = HEADER_SIZE + XSLOT_MAX_DATA_LEN + CRC_SIZE;

// 帧结构
struct Frame {
  uint8_t sync;
  uint16_t from;
  uint16_t to;
  uint8_t seq;
  uint8_t cmd;
  uint8_t len;
  uint8_t data[XSLOT_MAX_DATA_LEN];
  uint16_t crc;

  Frame();
  void clear();
};

// CRC计算
class CRC16 {
public:
  static uint16_t calculate(const uint8_t *data, size_t len);
};

// 消息编解码
class MessageCodec {
public:
  static std::vector<uint8_t> encode(const Frame &frame);
  static bool decode(const uint8_t *buffer, size_t len, Frame &frame);
  static void dumpFrame(const Frame &frame, const char *prefix = "");
};

} // namespace xslot

#endif // XSLOT_PROTOCOL_H