/**
 * @file bacnet_incremental.cpp
 * @brief BACnet 对象增量格式序列化实现 (C++20 Refactored)
 */
#include "bacnet_incremental.h"
#include "../core/buffer_utils.h"
#include "bacnet_object_def.h"
#include <xslot/xslot_error.h>

/* 增量格式头部大小: OBJ_ID(2) + TYPE_HINT(1) = 3 */
constexpr uint8_t INCR_HEADER_SIZE = 3;

/**
 * @brief 根据对象类型获取 TYPE_HINT 值
 */
static uint8_t get_type_hint(uint8_t obj_type) {
  uint8_t hint = INCREMENTAL_FLAG; // bit7=1 标记增量格式

  if (xslot_is_analog_type(obj_type)) {
    hint |= VALUE_TYPE_ANALOG;
  } else if (xslot_is_binary_type(obj_type)) {
    hint |= VALUE_TYPE_BINARY;
  } else {
    hint |= VALUE_TYPE_OTHER;
  }

  return hint;
}

/**
 * @brief 根据 TYPE_HINT 获取值大小
 */
static uint8_t get_value_size_from_hint(uint8_t type_hint) {
  uint8_t value_type = type_hint & 0x0F;
  switch (value_type) {
  case VALUE_TYPE_ANALOG:
    return sizeof(float);
  case VALUE_TYPE_BINARY:
    return 1;
  default:
    return 16;
  }
}

/**
 * @brief 根据 TYPE_HINT 推断对象类型 (仅用于反序列化)
 * 注意: 无法区分 AI/AO/AV 或 BI/BO/BV，默认使用 Input 类型
 */
static uint8_t infer_object_type(uint8_t type_hint) {
  uint8_t value_type = type_hint & 0x0F;
  switch (value_type) {
  case VALUE_TYPE_ANALOG:
    return XSLOT_OBJ_ANALOG_INPUT;
  case VALUE_TYPE_BINARY:
    return XSLOT_OBJ_BINARY_INPUT;
  default:
    return XSLOT_OBJ_ANALOG_VALUE;
  }
}

uint8_t bacnet_incremental_size(const xslot_bacnet_object_t *obj) {
  if (!obj)
    return 0;

  uint8_t value_size;
  if (xslot_is_analog_type(obj->object_type)) {
    value_size = sizeof(float);
  } else if (xslot_is_binary_type(obj->object_type)) {
    value_size = 1;
  } else {
    value_size = 16;
  }

  return INCR_HEADER_SIZE + value_size;
}

int bacnet_incremental_serialize(const xslot_bacnet_object_t *obj,
                                 uint8_t *buffer, uint8_t buffer_size) {
  if (!obj || !buffer) {
    return XSLOT_ERR_PARAM;
  }

  xslot::BufferWriter writer({buffer, buffer_size});

  // OBJ_ID (2 bytes)
  if (!writer.write(obj->object_id))
    return XSLOT_ERR_NO_MEM;

  // TYPE_HINT (1 byte)
  if (!writer.write(get_type_hint(obj->object_type)))
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

int bacnet_incremental_serialize_batch(const xslot_bacnet_object_t *objects,
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
    const auto &obj = objects[i];

    if (!writer.write(obj.object_id))
      return XSLOT_ERR_NO_MEM;
    if (!writer.write(get_type_hint(obj.object_type)))
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

int bacnet_incremental_deserialize(const uint8_t *buffer, uint8_t len,
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

  // TYPE_HINT
  auto hint = reader.read<uint8_t>();
  if (!hint)
    return XSLOT_ERR_PARAM;
  uint8_t type_hint = *hint;

  obj->object_type = infer_object_type(type_hint);
  obj->flags = 0;

  // VALUE
  uint8_t value_type = type_hint & 0x0F;
  if (value_type == VALUE_TYPE_ANALOG) {
    auto val = reader.read<float>();
    if (!val)
      return XSLOT_ERR_PARAM;
    obj->present_value.analog = *val;
  } else if (value_type == VALUE_TYPE_BINARY) {
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

int bacnet_incremental_deserialize_batch(const uint8_t *buffer, uint8_t len,
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
    count = max_count;
  }

  for (uint8_t i = 0; i < count; i++) {
    auto &obj = objects[i];

    auto id = reader.read<uint16_t>();
    if (!id)
      return XSLOT_ERR_PARAM;
    obj.object_id = *id;

    auto hint = reader.read<uint8_t>();
    if (!hint)
      return XSLOT_ERR_PARAM;
    uint8_t type_hint = *hint;

    obj.object_type = infer_object_type(type_hint);
    obj.flags = 0;

    uint8_t value_type = type_hint & 0x0F;
    if (value_type == VALUE_TYPE_ANALOG) {
      auto val = reader.read<float>();
      if (!val)
        return XSLOT_ERR_PARAM;
      obj.present_value.analog = *val;
    } else if (value_type == VALUE_TYPE_BINARY) {
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
