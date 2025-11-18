// =============================================================================
// src/bacnet/xslot_bacnet.cpp - BACnet序列化实现
// =============================================================================

#include "xslot_bacnet.h"
#include <cstring>
#include <iostream>

namespace xslot {

std::vector<uint8_t>
BACnetSerializer::serialize(const xslot_bacnet_object_t &obj) {
  std::vector<uint8_t> buffer;

  // [OBJ_ID:2][OBJ_TYPE:1][FLAGS:1][VALUE:变长]
  buffer.push_back(obj.object_id & 0xFF);
  buffer.push_back((obj.object_id >> 8) & 0xFF);
  buffer.push_back(obj.object_type);
  buffer.push_back(obj.flags);

  // 根据类型序列化值
  switch (obj.object_type) {
  case BACNET_OBJ_ANALOG_INPUT:
  case BACNET_OBJ_ANALOG_OUTPUT:
  case BACNET_OBJ_ANALOG_VALUE: {
    // 浮点数使用4字节
    uint32_t float_bits;
    memcpy(&float_bits, &obj.value.analog_value, 4);
    buffer.push_back(float_bits & 0xFF);
    buffer.push_back((float_bits >> 8) & 0xFF);
    buffer.push_back((float_bits >> 16) & 0xFF);
    buffer.push_back((float_bits >> 24) & 0xFF);
    break;
  }

  case BACNET_OBJ_BINARY_INPUT:
  case BACNET_OBJ_BINARY_OUTPUT:
  case BACNET_OBJ_BINARY_VALUE: {
    // 二进制值使用1字节
    buffer.push_back(obj.value.binary_value);
    break;
  }

  default:
    // 未知类型，使用原始数据
    buffer.insert(buffer.end(), obj.value.raw_data, obj.value.raw_data + 16);
    break;
  }

  return buffer;
}

std::vector<uint8_t>
BACnetSerializer::serializeMultiple(const xslot_bacnet_object_t *objects,
                                    uint8_t count) {

  std::vector<uint8_t> buffer;

  // 头部：[COUNT:1]
  buffer.push_back(count);

  // 逐个序列化对象
  for (uint8_t i = 0; i < count; i++) {
    auto obj_data = serialize(objects[i]);
    buffer.insert(buffer.end(), obj_data.begin(), obj_data.end());
  }

  return buffer;
}

bool BACnetSerializer::deserialize(const uint8_t *data, size_t len,
                                   xslot_bacnet_object_t &obj) {
  if (len < MIN_OBJ_SIZE)
    return false;

  // 解析头部
  obj.object_id = data[0] | (data[1] << 8);
  obj.object_type = data[2];
  obj.flags = data[3];

  // 解析值
  switch (obj.object_type) {
  case BACNET_OBJ_ANALOG_INPUT:
  case BACNET_OBJ_ANALOG_OUTPUT:
  case BACNET_OBJ_ANALOG_VALUE: {
    if (len < MIN_OBJ_SIZE + 4)
      return false;
    uint32_t float_bits =
        data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
    memcpy(&obj.value.analog_value, &float_bits, 4);
    break;
  }

  case BACNET_OBJ_BINARY_INPUT:
  case BACNET_OBJ_BINARY_OUTPUT:
  case BACNET_OBJ_BINARY_VALUE: {
    if (len < MIN_OBJ_SIZE + 1)
      return false;
    obj.value.binary_value = data[4];
    break;
  }

  default:
    if (len < MIN_OBJ_SIZE + 16)
      return false;
    memcpy(obj.value.raw_data, data + 4, 16);
    break;
  }

  return true;
}

int BACnetSerializer::deserializeMultiple(const uint8_t *data, size_t len,
                                          xslot_bacnet_object_t *objects,
                                          uint8_t max_count) {
  if (len < 1)
    return -1;

  uint8_t count = data[0];
  if (count > max_count)
    count = max_count;

  size_t offset = 1;
  int parsed = 0;

  for (uint8_t i = 0; i < count && offset < len; i++) {
    if (deserialize(data + offset, len - offset, objects[i])) {
      offset += getSerializedSize(objects[i]);
      parsed++;
    } else {
      break;
    }
  }

  return parsed;
}

size_t BACnetSerializer::getSerializedSize(const xslot_bacnet_object_t &obj) {
  size_t size = MIN_OBJ_SIZE;

  switch (obj.object_type) {
  case BACNET_OBJ_ANALOG_INPUT:
  case BACNET_OBJ_ANALOG_OUTPUT:
  case BACNET_OBJ_ANALOG_VALUE:
    size += 4;
    break;

  case BACNET_OBJ_BINARY_INPUT:
  case BACNET_OBJ_BINARY_OUTPUT:
  case BACNET_OBJ_BINARY_VALUE:
    size += 1;
    break;

  default:
    size += 16;
    break;
  }

  return size;
}

} // namespace xslot