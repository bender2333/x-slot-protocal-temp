/**
 * @file bacnet_serializer.cpp
 * @brief BACnet 对象完整格式序列化实现 (C++20 Refactored)
 */
#include "bacnet_serializer.h"
#include "../core/buffer_utils.h"
#include "bacnet_object_def.h"
#include <xslot/xslot_error.h>

/* 完整格式头部大小: OBJ_ID(2) + OBJ_TYPE(1) + FLAGS(1) = 4 */
constexpr uint8_t FULL_HEADER_SIZE = 4;

uint8_t bacnet_object_serialized_size(const xslot_bacnet_object_t *obj) {
  if (!obj)
    return 0;
  return FULL_HEADER_SIZE + xslot_get_value_size(obj->object_type);
}

int bacnet_serialize_object(const xslot_bacnet_object_t *obj, uint8_t *buffer,
                            uint8_t buffer_size) {
  if (!obj || !buffer) {
    return XSLOT_ERR_PARAM;
  }

  xslot::BufferWriter writer({buffer, buffer_size});

  // OBJ_ID (2 bytes)
  if (!writer.write(obj->object_id))
    return XSLOT_ERR_NO_MEM;

  // OBJ_TYPE (1 byte)
  if (!writer.write(obj->object_type))
    return XSLOT_ERR_NO_MEM;

  // FLAGS (1 byte)
  if (!writer.write(obj->flags))
    return XSLOT_ERR_NO_MEM;

  // VALUE (变长)
  if (xslot_is_analog_type(obj->object_type)) {
    if (!writer.write(obj->present_value.analog))
      return XSLOT_ERR_NO_MEM;
  } else if (xslot_is_binary_type(obj->object_type)) {
    if (!writer.write(obj->present_value.binary))
      return XSLOT_ERR_NO_MEM;
  } else {
    if (!writer.write_bytes({obj->present_value.raw, 16}))
      return XSLOT_ERR_NO_MEM;
  }

  return static_cast<int>(writer.offset());
}

int bacnet_serialize_objects(const xslot_bacnet_object_t *objects,
                             uint8_t count, uint8_t *buffer,
                             uint8_t buffer_size) {
  if (!objects || !buffer || count == 0) {
    return XSLOT_ERR_PARAM;
  }

  xslot::BufferWriter writer({buffer, buffer_size});

  // COUNT (1 byte)
  if (!writer.write(count))
    return XSLOT_ERR_NO_MEM;

  // 序列化每个对象
  for (uint8_t i = 0; i < count; i++) {
    // 递归调用单个序列化不太高效，因为我们要重新构造 writer
    // 但这里为了代码复用先这样，或者我们直接内联逻辑
    // 更好的方式是让 serialize_object 接受 writer，但这需要改 API
    // 我们这里直接操作 writer

    const auto &obj = objects[i];
    if (!writer.write(obj.object_id))
      return XSLOT_ERR_NO_MEM;
    if (!writer.write(obj.object_type))
      return XSLOT_ERR_NO_MEM;
    if (!writer.write(obj.flags))
      return XSLOT_ERR_NO_MEM;

    if (xslot_is_analog_type(obj.object_type)) {
      if (!writer.write(obj.present_value.analog))
        return XSLOT_ERR_NO_MEM;
    } else if (xslot_is_binary_type(obj.object_type)) {
      if (!writer.write(obj.present_value.binary))
        return XSLOT_ERR_NO_MEM;
    } else {
      if (!writer.write_bytes({obj.present_value.raw, 16}))
        return XSLOT_ERR_NO_MEM;
    }
  }

  return static_cast<int>(writer.offset());
}

int bacnet_deserialize_object(const uint8_t *buffer, uint8_t len,
                              xslot_bacnet_object_t *obj) {
  if (!buffer || !obj) {
    return XSLOT_ERR_PARAM;
  }

  xslot::BufferReader reader({buffer, len});

  // OBJ_ID
  auto id = reader.read<uint16_t>();
  if (!id)
    return XSLOT_ERR_PARAM;
  obj->object_id = *id;

  // OBJ_TYPE
  auto type = reader.read<uint8_t>();
  if (!type)
    return XSLOT_ERR_PARAM;
  obj->object_type = *type;

  // FLAGS
  auto flags = reader.read<uint8_t>();
  if (!flags)
    return XSLOT_ERR_PARAM;
  obj->flags = *flags;

  // VALUE
  if (xslot_is_analog_type(obj->object_type)) {
    auto val = reader.read<float>();
    if (!val)
      return XSLOT_ERR_PARAM;
    obj->present_value.analog = *val;
  } else if (xslot_is_binary_type(obj->object_type)) {
    auto val = reader.read<uint8_t>();
    if (!val)
      return XSLOT_ERR_PARAM;
    obj->present_value.binary = *val;
  } else {
    if (!reader.read_bytes({obj->present_value.raw, 16}))
      return XSLOT_ERR_PARAM;
  }

  return static_cast<int>(reader.offset());
}

int bacnet_deserialize_objects(const uint8_t *buffer, uint8_t len,
                               xslot_bacnet_object_t *objects,
                               uint8_t max_count) {
  if (!buffer || !objects || len < 1) {
    return XSLOT_ERR_PARAM;
  }

  xslot::BufferReader reader({buffer, len});

  // COUNT
  auto count_opt = reader.read<uint8_t>();
  if (!count_opt)
    return XSLOT_ERR_PARAM;

  uint8_t count = *count_opt;
  if (count > max_count) {
    count = max_count; // 截断
  }

  for (uint8_t i = 0; i < count; i++) {
    auto &obj = objects[i];

    auto id = reader.read<uint16_t>();
    if (!id)
      return XSLOT_ERR_PARAM;
    obj.object_id = *id;

    auto type = reader.read<uint8_t>();
    if (!type)
      return XSLOT_ERR_PARAM;
    obj.object_type = *type;

    auto flags = reader.read<uint8_t>();
    if (!flags)
      return XSLOT_ERR_PARAM;
    obj.flags = *flags;

    if (xslot_is_analog_type(obj.object_type)) {
      auto val = reader.read<float>();
      if (!val)
        return XSLOT_ERR_PARAM;
      obj.present_value.analog = *val;
    } else if (xslot_is_binary_type(obj.object_type)) {
      auto val = reader.read<uint8_t>();
      if (!val)
        return XSLOT_ERR_PARAM;
      obj.present_value.binary = *val;
    } else {
      if (!reader.read_bytes({obj.present_value.raw, 16}))
        return XSLOT_ERR_PARAM;
    }
  }

  return count;
}
