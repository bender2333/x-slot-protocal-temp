/**
 * @file message_builder.cpp
 * @brief X-Slot 消息构建器实现
 */
#include "message_builder.h"
#include <cstring>

namespace xslot {

// ============================================================================
// BacnetSerializer 实现
// ============================================================================

Result<size_t> BacnetSerializer::serialize(const xslot_bacnet_object_t &obj,
                                           std::span<uint8_t> buffer) noexcept {
  BufferWriter writer(buffer);

  // OBJ_ID (2 bytes)
  if (!writer.write(obj.object_id)) {
    return Result<size_t>::error(Error::NoMemory);
  }

  // OBJ_TYPE (1 byte)
  if (!writer.write(obj.object_type)) {
    return Result<size_t>::error(Error::NoMemory);
  }

  // FLAGS (1 byte)
  if (!writer.write(obj.flags)) {
    return Result<size_t>::error(Error::NoMemory);
  }

  // VALUE (变长)
  if (is_analog_type(obj.object_type)) {
    if (!writer.write(obj.present_value.analog)) {
      return Result<size_t>::error(Error::NoMemory);
    }
  } else if (is_binary_type(obj.object_type)) {
    if (!writer.write(obj.present_value.binary)) {
      return Result<size_t>::error(Error::NoMemory);
    }
  } else {
    if (!writer.write_bytes({obj.present_value.raw, 16})) {
      return Result<size_t>::error(Error::NoMemory);
    }
  }

  return Result<size_t>::ok(writer.offset());
}

Result<size_t> BacnetSerializer::serialize_batch(
    std::span<const xslot_bacnet_object_t> objects,
    std::span<uint8_t> buffer) noexcept {
  if (objects.empty()) {
    return Result<size_t>::error(Error::InvalidParam);
  }

  BufferWriter writer(buffer);

  // COUNT (1 byte)
  if (!writer.write(static_cast<uint8_t>(objects.size()))) {
    return Result<size_t>::error(Error::NoMemory);
  }

  // 序列化每个对象
  for (const auto &obj : objects) {
    if (!writer.write(obj.object_id)) {
      return Result<size_t>::error(Error::NoMemory);
    }
    if (!writer.write(obj.object_type)) {
      return Result<size_t>::error(Error::NoMemory);
    }
    if (!writer.write(obj.flags)) {
      return Result<size_t>::error(Error::NoMemory);
    }

    if (is_analog_type(obj.object_type)) {
      if (!writer.write(obj.present_value.analog)) {
        return Result<size_t>::error(Error::NoMemory);
      }
    } else if (is_binary_type(obj.object_type)) {
      if (!writer.write(obj.present_value.binary)) {
        return Result<size_t>::error(Error::NoMemory);
      }
    } else {
      if (!writer.write_bytes({obj.present_value.raw, 16})) {
        return Result<size_t>::error(Error::NoMemory);
      }
    }
  }

  return Result<size_t>::ok(writer.offset());
}

Result<size_t>
BacnetSerializer::deserialize(std::span<const uint8_t> buffer,
                              xslot_bacnet_object_t &obj) noexcept {
  BufferReader reader(buffer);

  // OBJ_ID
  auto id = reader.read<uint16_t>();
  if (!id) {
    return Result<size_t>::error(Error::InvalidParam);
  }
  obj.object_id = *id;

  // OBJ_TYPE
  auto type = reader.read<uint8_t>();
  if (!type) {
    return Result<size_t>::error(Error::InvalidParam);
  }
  obj.object_type = *type;

  // FLAGS
  auto flags = reader.read<uint8_t>();
  if (!flags) {
    return Result<size_t>::error(Error::InvalidParam);
  }
  obj.flags = *flags;

  // VALUE
  if (is_analog_type(obj.object_type)) {
    auto val = reader.read<float>();
    if (!val) {
      return Result<size_t>::error(Error::InvalidParam);
    }
    obj.present_value.analog = *val;
  } else if (is_binary_type(obj.object_type)) {
    auto val = reader.read<uint8_t>();
    if (!val) {
      return Result<size_t>::error(Error::InvalidParam);
    }
    obj.present_value.binary = *val;
  } else {
    if (!reader.read_bytes({obj.present_value.raw, 16})) {
      return Result<size_t>::error(Error::InvalidParam);
    }
  }

  return Result<size_t>::ok(reader.offset());
}

Result<size_t> BacnetSerializer::deserialize_batch(
    std::span<const uint8_t> buffer,
    std::span<xslot_bacnet_object_t> objects) noexcept {
  if (buffer.empty() || objects.empty()) {
    return Result<size_t>::error(Error::InvalidParam);
  }

  BufferReader reader(buffer);

  // COUNT
  auto count_opt = reader.read<uint8_t>();
  if (!count_opt) {
    return Result<size_t>::error(Error::InvalidParam);
  }

  uint8_t count = *count_opt;
  if (count > objects.size()) {
    count = static_cast<uint8_t>(objects.size());
  }

  for (uint8_t i = 0; i < count; i++) {
    auto &obj = objects[i];

    auto id = reader.read<uint16_t>();
    if (!id) {
      return Result<size_t>::error(Error::InvalidParam);
    }
    obj.object_id = *id;

    auto type = reader.read<uint8_t>();
    if (!type) {
      return Result<size_t>::error(Error::InvalidParam);
    }
    obj.object_type = *type;

    auto flags = reader.read<uint8_t>();
    if (!flags) {
      return Result<size_t>::error(Error::InvalidParam);
    }
    obj.flags = *flags;

    if (is_analog_type(obj.object_type)) {
      auto val = reader.read<float>();
      if (!val) {
        return Result<size_t>::error(Error::InvalidParam);
      }
      obj.present_value.analog = *val;
    } else if (is_binary_type(obj.object_type)) {
      auto val = reader.read<uint8_t>();
      if (!val) {
        return Result<size_t>::error(Error::InvalidParam);
      }
      obj.present_value.binary = *val;
    } else {
      if (!reader.read_bytes({obj.present_value.raw, 16})) {
        return Result<size_t>::error(Error::InvalidParam);
      }
    }
  }

  return Result<size_t>::ok(count);
}

// ============================================================================
// 增量格式序列化
// ============================================================================

static uint8_t get_type_hint(uint8_t obj_type) noexcept {
  uint8_t hint = 0x80; // INCREMENTAL_FLAG

  if (obj_type == XSLOT_OBJ_ANALOG_INPUT ||
      obj_type == XSLOT_OBJ_ANALOG_OUTPUT ||
      obj_type == XSLOT_OBJ_ANALOG_VALUE) {
    hint |= 0x00; // VALUE_TYPE_ANALOG
  } else if (obj_type == XSLOT_OBJ_BINARY_INPUT ||
             obj_type == XSLOT_OBJ_BINARY_OUTPUT ||
             obj_type == XSLOT_OBJ_BINARY_VALUE) {
    hint |= 0x01; // VALUE_TYPE_BINARY
  } else {
    hint |= 0x02; // VALUE_TYPE_OTHER
  }

  return hint;
}

static uint8_t infer_object_type(uint8_t type_hint) noexcept {
  uint8_t value_type = type_hint & 0x0F;
  switch (value_type) {
  case 0x00:
    return XSLOT_OBJ_ANALOG_INPUT;
  case 0x01:
    return XSLOT_OBJ_BINARY_INPUT;
  default:
    return XSLOT_OBJ_ANALOG_VALUE;
  }
}

Result<size_t>
BacnetSerializer::serialize_incremental(const xslot_bacnet_object_t &obj,
                                        std::span<uint8_t> buffer) noexcept {
  BufferWriter writer(buffer);

  // OBJ_ID (2 bytes)
  if (!writer.write(obj.object_id)) {
    return Result<size_t>::error(Error::NoMemory);
  }

  // TYPE_HINT (1 byte)
  if (!writer.write(get_type_hint(obj.object_type))) {
    return Result<size_t>::error(Error::NoMemory);
  }

  // VALUE (变长)
  if (is_analog_type(obj.object_type)) {
    if (!writer.write(obj.present_value.analog)) {
      return Result<size_t>::error(Error::NoMemory);
    }
  } else if (is_binary_type(obj.object_type)) {
    if (!writer.write(obj.present_value.binary)) {
      return Result<size_t>::error(Error::NoMemory);
    }
  } else {
    if (!writer.write_bytes({obj.present_value.raw, 16})) {
      return Result<size_t>::error(Error::NoMemory);
    }
  }

  return Result<size_t>::ok(writer.offset());
}

Result<size_t> BacnetSerializer::serialize_incremental_batch(
    std::span<const xslot_bacnet_object_t> objects,
    std::span<uint8_t> buffer) noexcept {
  if (objects.empty()) {
    return Result<size_t>::error(Error::InvalidParam);
  }

  BufferWriter writer(buffer);

  // COUNT (1 byte)
  if (!writer.write(static_cast<uint8_t>(objects.size()))) {
    return Result<size_t>::error(Error::NoMemory);
  }

  // 序列化每个对象
  for (const auto &obj : objects) {
    if (!writer.write(obj.object_id)) {
      return Result<size_t>::error(Error::NoMemory);
    }
    if (!writer.write(get_type_hint(obj.object_type))) {
      return Result<size_t>::error(Error::NoMemory);
    }

    if (is_analog_type(obj.object_type)) {
      if (!writer.write(obj.present_value.analog)) {
        return Result<size_t>::error(Error::NoMemory);
      }
    } else if (is_binary_type(obj.object_type)) {
      if (!writer.write(obj.present_value.binary)) {
        return Result<size_t>::error(Error::NoMemory);
      }
    } else {
      if (!writer.write_bytes({obj.present_value.raw, 16})) {
        return Result<size_t>::error(Error::NoMemory);
      }
    }
  }

  return Result<size_t>::ok(writer.offset());
}

Result<size_t> BacnetSerializer::deserialize_incremental_batch(
    std::span<const uint8_t> buffer,
    std::span<xslot_bacnet_object_t> objects) noexcept {
  if (buffer.empty() || objects.empty()) {
    return Result<size_t>::error(Error::InvalidParam);
  }

  BufferReader reader(buffer);

  // COUNT
  auto count_opt = reader.read<uint8_t>();
  if (!count_opt) {
    return Result<size_t>::error(Error::InvalidParam);
  }

  uint8_t count = *count_opt;
  if (count > objects.size()) {
    count = static_cast<uint8_t>(objects.size());
  }

  for (uint8_t i = 0; i < count; i++) {
    auto &obj = objects[i];

    auto id = reader.read<uint16_t>();
    if (!id) {
      return Result<size_t>::error(Error::InvalidParam);
    }
    obj.object_id = *id;

    auto hint = reader.read<uint8_t>();
    if (!hint) {
      return Result<size_t>::error(Error::InvalidParam);
    }
    uint8_t type_hint = *hint;

    obj.object_type = infer_object_type(type_hint);
    obj.flags = 0;

    uint8_t value_type = type_hint & 0x0F;
    if (value_type == 0x00) { // ANALOG
      auto val = reader.read<float>();
      if (!val) {
        return Result<size_t>::error(Error::InvalidParam);
      }
      obj.present_value.analog = *val;
    } else if (value_type == 0x01) { // BINARY
      auto val = reader.read<uint8_t>();
      if (!val) {
        return Result<size_t>::error(Error::InvalidParam);
      }
      obj.present_value.binary = *val;
    } else {
      if (!reader.read_bytes({obj.present_value.raw, 16})) {
        return Result<size_t>::error(Error::InvalidParam);
      }
    }
  }

  return Result<size_t>::ok(count);
}

} // namespace xslot
