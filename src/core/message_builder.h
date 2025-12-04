/**
 * @file message_builder.h
 * @brief X-Slot 消息构建器 (C++20)
 *
 * 提供类型安全的消息构建和解析接口。
 */
#ifndef XSLOT_MESSAGE_BUILDER_H
#define XSLOT_MESSAGE_BUILDER_H

#include "frame.h"
#include "result.h"
#include <span>
#include <xslot/xslot_types.h>

namespace xslot {

/**
 * @brief 消息构建器命名空间
 *
 * 提供构建各种协议消息的静态方法。
 */
namespace message {

/**
 * @brief 构建 PING 帧
 */
[[nodiscard]] inline Frame build_ping(uint16_t from, uint16_t to,
                                      uint8_t seq) noexcept {
  return FrameBuilder(from).to(to).seq(seq).cmd(Command::Ping).build();
}

/**
 * @brief 构建 PONG 帧
 */
[[nodiscard]] inline Frame build_pong(uint16_t from, uint16_t to,
                                      uint8_t seq) noexcept {
  return FrameBuilder(from).to(to).seq(seq).cmd(Command::Pong).build();
}

/**
 * @brief 构建 WRITE_ACK 帧
 */
[[nodiscard]] inline Frame build_write_ack(uint16_t from, uint16_t to,
                                           uint8_t seq,
                                           uint8_t result) noexcept {
  return FrameBuilder(from)
      .to(to)
      .seq(seq)
      .cmd(Command::WriteAck)
      .write(result)
      .build();
}

/**
 * @brief 构建 QUERY 帧
 */
[[nodiscard]] inline Result<Frame>
build_query(uint16_t from, uint16_t to, uint8_t seq,
            std::span<const uint16_t> object_ids) noexcept {
  if (object_ids.empty()) {
    return Result<Frame>::error(Error::InvalidParam);
  }

  // 检查长度: COUNT(1) + IDs(2*count)
  if (1 + object_ids.size() * 2 > frame_const::MAX_DATA_LEN) {
    return Result<Frame>::error(Error::NoMemory);
  }

  FrameBuilder builder(from);
  builder.to(to).seq(seq).cmd(Command::Query);

  // 写入数量
  builder.write(static_cast<uint8_t>(object_ids.size()));

  // 写入对象 ID 列表
  for (uint16_t id : object_ids) {
    builder.write(id);
  }

  return Result<Frame>::ok(builder.build());
}

/**
 * @brief 解析 QUERY 帧载荷
 * @param frame 帧
 * @param object_ids 输出对象 ID 数组
 * @return 解析的对象数量
 */
[[nodiscard]] inline Result<size_t>
parse_query(const Frame &frame, std::span<uint16_t> object_ids) noexcept {
  if (frame.command() != Command::Query) {
    return Result<size_t>::error(Error::InvalidParam);
  }

  if (frame.len() < 1) {
    return Result<size_t>::error(Error::InvalidParam);
  }

  BufferReader reader(frame.data_span());

  auto count_opt = reader.read<uint8_t>();
  if (!count_opt) {
    return Result<size_t>::error(Error::InvalidParam);
  }

  uint8_t count = *count_opt;
  if (count > object_ids.size()) {
    count = static_cast<uint8_t>(object_ids.size());
  }

  // 检查数据长度
  if (frame.len() < 1 + count * 2) {
    return Result<size_t>::error(Error::InvalidParam);
  }

  for (uint8_t i = 0; i < count; i++) {
    auto id = reader.read<uint16_t>();
    if (!id) {
      return Result<size_t>::error(Error::InvalidParam);
    }
    object_ids[i] = *id;
  }

  return Result<size_t>::ok(count);
}

} // namespace message

/**
 * @brief BACnet 对象序列化器
 *
 * 提供 BACnet 对象的序列化和反序列化功能。
 */
class BacnetSerializer {
public:
  /**
   * @brief 序列化单个对象 (完整格式)
   * 格式: [OBJ_ID:2B][OBJ_TYPE:1B][FLAGS:1B][VALUE:变长]
   */
  static Result<size_t> serialize(const xslot_bacnet_object_t &obj,
                                  std::span<uint8_t> buffer) noexcept;

  /**
   * @brief 序列化多个对象 (完整格式)
   * 格式: [COUNT:1B][OBJ1][OBJ2]...
   */
  static Result<size_t>
  serialize_batch(std::span<const xslot_bacnet_object_t> objects,
                  std::span<uint8_t> buffer) noexcept;

  /**
   * @brief 反序列化单个对象
   * @return 消耗的字节数
   */
  static Result<size_t> deserialize(std::span<const uint8_t> buffer,
                                    xslot_bacnet_object_t &obj) noexcept;

  /**
   * @brief 反序列化多个对象
   * @return 解析的对象数量
   */
  static Result<size_t>
  deserialize_batch(std::span<const uint8_t> buffer,
                    std::span<xslot_bacnet_object_t> objects) noexcept;

  /**
   * @brief 序列化单个对象 (增量格式)
   * 格式: [OBJ_ID:2B][TYPE_HINT:1B][VALUE:变长]
   */
  static Result<size_t>
  serialize_incremental(const xslot_bacnet_object_t &obj,
                        std::span<uint8_t> buffer) noexcept;

  /**
   * @brief 序列化多个对象 (增量格式)
   */
  static Result<size_t>
  serialize_incremental_batch(std::span<const xslot_bacnet_object_t> objects,
                              std::span<uint8_t> buffer) noexcept;

  /**
   * @brief 反序列化增量格式
   */
  static Result<size_t> deserialize_incremental_batch(
      std::span<const uint8_t> buffer,
      std::span<xslot_bacnet_object_t> objects) noexcept;

  /**
   * @brief 判断数据是否为增量格式
   */
  static bool is_incremental_format(uint8_t type_hint) noexcept {
    return (type_hint & 0x80) != 0;
  }

private:
  // 类型判断辅助函数
  static bool is_analog_type(uint8_t obj_type) noexcept {
    return obj_type == XSLOT_OBJ_ANALOG_INPUT ||
           obj_type == XSLOT_OBJ_ANALOG_OUTPUT ||
           obj_type == XSLOT_OBJ_ANALOG_VALUE;
  }

  static bool is_binary_type(uint8_t obj_type) noexcept {
    return obj_type == XSLOT_OBJ_BINARY_INPUT ||
           obj_type == XSLOT_OBJ_BINARY_OUTPUT ||
           obj_type == XSLOT_OBJ_BINARY_VALUE;
  }

  static uint8_t get_value_size(uint8_t obj_type) noexcept {
    if (is_analog_type(obj_type))
      return sizeof(float);
    if (is_binary_type(obj_type))
      return 1;
    return 16;
  }

  // 增量格式常量
  static constexpr uint8_t INCREMENTAL_FLAG = 0x80;
  static constexpr uint8_t VALUE_TYPE_ANALOG = 0x00;
  static constexpr uint8_t VALUE_TYPE_BINARY = 0x01;
  static constexpr uint8_t VALUE_TYPE_OTHER = 0x02;
};

/**
 * @brief 消息构建辅助函数
 */
namespace message {

/**
 * @brief 构建 REPORT 帧
 */
[[nodiscard]] inline Result<Frame>
build_report(uint16_t from, uint16_t to, uint8_t seq,
             std::span<const xslot_bacnet_object_t> objects,
             bool incremental = true) noexcept {
  if (objects.empty()) {
    return Result<Frame>::error(Error::InvalidParam);
  }

  Frame frame(from, to, seq, Command::Report);

  auto result = incremental
                    ? BacnetSerializer::serialize_incremental_batch(
                          objects, {frame.data(), frame_const::MAX_DATA_LEN})
                    : BacnetSerializer::serialize_batch(
                          objects, {frame.data(), frame_const::MAX_DATA_LEN});

  if (!result) {
    return Result<Frame>::error(result.error());
  }

  frame.set_len(static_cast<uint8_t>(result.value()));
  return Result<Frame>::ok(std::move(frame));
}

/**
 * @brief 构建 RESPONSE 帧
 */
[[nodiscard]] inline Result<Frame>
build_response(uint16_t from, uint16_t to, uint8_t seq,
               std::span<const xslot_bacnet_object_t> objects) noexcept {
  if (objects.empty()) {
    return Result<Frame>::error(Error::InvalidParam);
  }

  Frame frame(from, to, seq, Command::Response);

  auto result = BacnetSerializer::serialize_batch(
      objects, {frame.data(), frame_const::MAX_DATA_LEN});
  if (!result) {
    return Result<Frame>::error(result.error());
  }

  frame.set_len(static_cast<uint8_t>(result.value()));
  return Result<Frame>::ok(std::move(frame));
}

/**
 * @brief 构建 WRITE 帧
 */
[[nodiscard]] inline Result<Frame>
build_write(uint16_t from, uint16_t to, uint8_t seq,
            const xslot_bacnet_object_t &obj) noexcept {
  Frame frame(from, to, seq, Command::Write);

  auto result = BacnetSerializer::serialize(
      obj, {frame.data(), frame_const::MAX_DATA_LEN});
  if (!result) {
    return Result<Frame>::error(result.error());
  }

  frame.set_len(static_cast<uint8_t>(result.value()));
  return Result<Frame>::ok(std::move(frame));
}

/**
 * @brief 解析 REPORT 帧
 */
[[nodiscard]] inline Result<size_t>
parse_report(const Frame &frame,
             std::span<xslot_bacnet_object_t> objects) noexcept {
  if (frame.command() != Command::Report) {
    return Result<size_t>::error(Error::InvalidParam);
  }

  if (frame.len() < 1) {
    return Result<size_t>::error(Error::InvalidParam);
  }

  // 检查是否为增量格式
  if (frame.len() >= 4) {
    uint8_t type_byte = frame.data()[3];
    if (BacnetSerializer::is_incremental_format(type_byte)) {
      return BacnetSerializer::deserialize_incremental_batch(frame.data_span(),
                                                             objects);
    }
  }

  return BacnetSerializer::deserialize_batch(frame.data_span(), objects);
}

/**
 * @brief 解析 WRITE 帧
 */
[[nodiscard]] inline Result<size_t>
parse_write(const Frame &frame, xslot_bacnet_object_t &obj) noexcept {
  if (frame.command() != Command::Write) {
    return Result<size_t>::error(Error::InvalidParam);
  }

  return BacnetSerializer::deserialize(frame.data_span(), obj);
}

} // namespace message

} // namespace xslot

#endif // XSLOT_MESSAGE_BUILDER_H
