// =============================================================================
// src/bacnet/xslot_bacnet_incremental.cpp - BACnet增量序列化器实现
// =============================================================================

#include "xslot_bacnet_incremental.h"
#include <cstring>
#include <iostream>

namespace xslot {

// =============================================================================
// 序列化 - 完整格式
// =============================================================================

std::vector<uint8_t>
BACnetIncrementalSerializer::serializeFull(const xslot_bacnet_object_t &obj) {

  std::vector<uint8_t> buffer;

  // [OBJ_ID:2] [OBJ_TYPE:1] [FLAGS:1] [VALUE:变长]
  buffer.push_back(obj.object_id & 0xFF);
  buffer.push_back((obj.object_id >> 8) & 0xFF);
  buffer.push_back(obj.object_type);

  // FLAGS: bit7=0表示完整格式
  uint8_t flags = obj.flags & 0x7F; // 确保bit7=0
  buffer.push_back(flags);

  // 序列化值
  switch (obj.object_type) {
  case BACNET_OBJ_ANALOG_INPUT:
  case BACNET_OBJ_ANALOG_OUTPUT:
  case BACNET_OBJ_ANALOG_VALUE: {
    // 4字节IEEE 754浮点数
    uint32_t float_bits;
    std::memcpy(&float_bits, &obj.value.analog_value, 4);
    buffer.push_back(float_bits & 0xFF);
    buffer.push_back((float_bits >> 8) & 0xFF);
    buffer.push_back((float_bits >> 16) & 0xFF);
    buffer.push_back((float_bits >> 24) & 0xFF);
    break;
  }

  case BACNET_OBJ_BINARY_INPUT:
  case BACNET_OBJ_BINARY_OUTPUT:
  case BACNET_OBJ_BINARY_VALUE: {
    // 1字节二进制值
    buffer.push_back(obj.value.binary_value);
    break;
  }

  default: {
    // 16字节原始数据
    buffer.insert(buffer.end(), obj.value.raw_data, obj.value.raw_data + 16);
    break;
  }
  }

  return buffer;
}

// =============================================================================
// 序列化 - 增量格式
// =============================================================================

std::vector<uint8_t> BACnetIncrementalSerializer::serializeIncremental(
    const xslot_bacnet_object_t &obj) {

  std::vector<uint8_t> buffer;

  // [OBJ_ID:2] [TYPE_HINT:1] [VALUE:变长]
  buffer.push_back(obj.object_id & 0xFF);
  buffer.push_back((obj.object_id >> 8) & 0xFF);

  // TYPE_HINT: bit7=1表示增量格式，bit3-0=值类型
  uint8_t type_hint = FLAG_INCREMENTAL | getTypeHint(obj.object_type);
  buffer.push_back(type_hint);

  // 序列化值（只传Present_Value）
  std::vector<uint8_t> value_bytes = serializeValue(obj, type_hint);
  buffer.insert(buffer.end(), value_bytes.begin(), value_bytes.end());

  return buffer;
}

// =============================================================================
// 批量序列化
// =============================================================================

std::vector<uint8_t> BACnetIncrementalSerializer::serializeFullBatch(
    const xslot_bacnet_object_t *objects, uint8_t count) {

  std::vector<uint8_t> buffer;

  // [COUNT:1] [OBJ1] [OBJ2] ...
  buffer.push_back(count);

  for (uint8_t i = 0; i < count; i++) {
    std::vector<uint8_t> obj_bytes = serializeFull(objects[i]);
    buffer.insert(buffer.end(), obj_bytes.begin(), obj_bytes.end());
  }

  return buffer;
}

std::vector<uint8_t> BACnetIncrementalSerializer::serializeIncrementalBatch(
    const xslot_bacnet_object_t *objects, uint8_t count) {

  std::vector<uint8_t> buffer;

  // [COUNT:1] [OBJ1] [OBJ2] ...
  buffer.push_back(count);

  for (uint8_t i = 0; i < count; i++) {
    std::vector<uint8_t> obj_bytes = serializeIncremental(objects[i]);
    buffer.insert(buffer.end(), obj_bytes.begin(), obj_bytes.end());
  }

  return buffer;
}

// =============================================================================
// 反序列化（自动识别格式）
// =============================================================================

bool BACnetIncrementalSerializer::deserialize(const uint8_t *data, size_t len,
                                              xslot_bacnet_object_t &obj,
                                              bool &is_incremental) {

  if (len < MIN_INCREMENTAL_SIZE) {
    return false;
  }

  // 解析OBJ_ID
  obj.object_id = data[0] | (data[1] << 8);

  // 检查第3字节的bit7判断格式
  uint8_t third_byte = data[2];
  is_incremental = (third_byte & FLAG_INCREMENTAL) != 0;

  if (is_incremental) {
    // 增量格式: [OBJ_ID:2] [TYPE_HINT:1] [VALUE:变长]
    if (len < MIN_INCREMENTAL_SIZE) {
      return false;
    }

    uint8_t type_hint = third_byte;
    obj.object_type = inferObjectType(type_hint);
    obj.flags = 0; // 增量格式没有FLAGS

    // 反序列化值
    return deserializeValue(data + 3, len - 3, obj, type_hint);

  } else {
    // 完整格式: [OBJ_ID:2] [OBJ_TYPE:1] [FLAGS:1] [VALUE:变长]
    if (len < MIN_FULL_SIZE) {
      return false;
    }

    obj.object_type = data[2];
    obj.flags = data[3];

    // 反序列化值
    switch (obj.object_type) {
    case BACNET_OBJ_ANALOG_INPUT:
    case BACNET_OBJ_ANALOG_OUTPUT:
    case BACNET_OBJ_ANALOG_VALUE: {
      if (len < MIN_FULL_SIZE + 4) {
        return false;
      }
      uint32_t float_bits =
          data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
      std::memcpy(&obj.value.analog_value, &float_bits, 4);
      break;
    }

    case BACNET_OBJ_BINARY_INPUT:
    case BACNET_OBJ_BINARY_OUTPUT:
    case BACNET_OBJ_BINARY_VALUE: {
      if (len < MIN_FULL_SIZE + 1) {
        return false;
      }
      obj.value.binary_value = data[4];
      break;
    }

    default: {
      if (len < MIN_FULL_SIZE + 16) {
        return false;
      }
      std::memcpy(obj.value.raw_data, data + 4, 16);
      break;
    }
    }

    return true;
  }
}

// =============================================================================
// 批量反序列化
// =============================================================================

int BACnetIncrementalSerializer::deserializeBatch(
    const uint8_t *data, size_t len, xslot_bacnet_object_t *objects,
    uint8_t max_count) {

  if (len < 1) {
    return -1;
  }

  uint8_t count = data[0];
  if (count > max_count) {
    count = max_count;
  }

  size_t offset = 1;
  int parsed = 0;

  for (uint8_t i = 0; i < count && offset < len; i++) {
    bool is_incremental;
    if (deserialize(data + offset, len - offset, objects[i], is_incremental)) {
      size_t obj_size = getSerializedSize(objects[i], is_incremental);
      offset += obj_size;
      parsed++;
    } else {
      break;
    }
  }

  return parsed;
}

// =============================================================================
// 辅助函数
// =============================================================================

bool BACnetIncrementalSerializer::isIncremental(const uint8_t *data,
                                                size_t len) {
  if (len < 3) {
    return false;
  }
  return (data[2] & FLAG_INCREMENTAL) != 0;
}

size_t
BACnetIncrementalSerializer::getSerializedSize(const xslot_bacnet_object_t &obj,
                                               bool incremental) {

  if (incremental) {
    // [OBJ_ID:2] [TYPE_HINT:1] [VALUE:变长]
    switch (obj.object_type) {
    case BACNET_OBJ_ANALOG_INPUT:
    case BACNET_OBJ_ANALOG_OUTPUT:
    case BACNET_OBJ_ANALOG_VALUE:
      return 2 + 1 + 4; // 7字节

    case BACNET_OBJ_BINARY_INPUT:
    case BACNET_OBJ_BINARY_OUTPUT:
    case BACNET_OBJ_BINARY_VALUE:
      return 2 + 1 + 1; // 4字节

    default:
      return 2 + 1 + 16; // 19字节
    }

  } else {
    // [OBJ_ID:2] [OBJ_TYPE:1] [FLAGS:1] [VALUE:变长]
    switch (obj.object_type) {
    case BACNET_OBJ_ANALOG_INPUT:
    case BACNET_OBJ_ANALOG_OUTPUT:
    case BACNET_OBJ_ANALOG_VALUE:
      return 2 + 1 + 1 + 4; // 8字节

    case BACNET_OBJ_BINARY_INPUT:
    case BACNET_OBJ_BINARY_OUTPUT:
    case BACNET_OBJ_BINARY_VALUE:
      return 2 + 1 + 1 + 1; // 5字节

    default:
      return 2 + 1 + 1 + 16; // 20字节
    }
  }
}

size_t BACnetIncrementalSerializer::estimateBatchSize(
    const xslot_bacnet_object_t *objects, uint8_t count, bool incremental) {

  size_t total = 1; // COUNT字节

  for (uint8_t i = 0; i < count; i++) {
    total += getSerializedSize(objects[i], incremental);
  }

  return total;
}

// =============================================================================
// 私有辅助函数
// =============================================================================

uint8_t BACnetIncrementalSerializer::getTypeHint(uint8_t obj_type) {
  switch (obj_type) {
  case BACNET_OBJ_ANALOG_INPUT:
  case BACNET_OBJ_ANALOG_OUTPUT:
  case BACNET_OBJ_ANALOG_VALUE:
    return TYPE_ANALOG;

  case BACNET_OBJ_BINARY_INPUT:
  case BACNET_OBJ_BINARY_OUTPUT:
  case BACNET_OBJ_BINARY_VALUE:
    return TYPE_BINARY;

  default:
    return TYPE_OTHER;
  }
}

uint8_t BACnetIncrementalSerializer::inferObjectType(uint8_t type_hint) {
  uint8_t value_type = type_hint & 0x0F;

  switch (value_type) {
  case TYPE_ANALOG:
    return BACNET_OBJ_ANALOG_VALUE; // 默认为AV
  case TYPE_BINARY:
    return BACNET_OBJ_BINARY_VALUE; // 默认为BV
  default:
    return BACNET_OBJ_ANALOG_VALUE;
  }
}

std::vector<uint8_t>
BACnetIncrementalSerializer::serializeValue(const xslot_bacnet_object_t &obj,
                                            uint8_t type_hint) {

  std::vector<uint8_t> buffer;
  uint8_t value_type = type_hint & 0x0F;

  switch (value_type) {
  case TYPE_ANALOG: {
    // 4字节IEEE 754浮点数
    uint32_t float_bits;
    std::memcpy(&float_bits, &obj.value.analog_value, 4);
    buffer.push_back(float_bits & 0xFF);
    buffer.push_back((float_bits >> 8) & 0xFF);
    buffer.push_back((float_bits >> 16) & 0xFF);
    buffer.push_back((float_bits >> 24) & 0xFF);
    break;
  }

  case TYPE_BINARY: {
    // 1字节二进制值
    buffer.push_back(obj.value.binary_value);
    break;
  }

  default: {
    // 16字节原始数据
    buffer.insert(buffer.end(), obj.value.raw_data, obj.value.raw_data + 16);
    break;
  }
  }

  return buffer;
}

bool BACnetIncrementalSerializer::deserializeValue(const uint8_t *data,
                                                   size_t len,
                                                   xslot_bacnet_object_t &obj,
                                                   uint8_t type_hint) {

  uint8_t value_type = type_hint & 0x0F;

  switch (value_type) {
  case TYPE_ANALOG: {
    if (len < 4) {
      return false;
    }
    uint32_t float_bits =
        data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
    std::memcpy(&obj.value.analog_value, &float_bits, 4);
    break;
  }

  case TYPE_BINARY: {
    if (len < 1) {
      return false;
    }
    obj.value.binary_value = data[0];
    break;
  }

  default: {
    if (len < 16) {
      return false;
    }
    std::memcpy(obj.value.raw_data, data, 16);
    break;
  }
  }

  return true;
}

} // namespace xslot
