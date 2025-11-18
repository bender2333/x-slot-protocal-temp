// =============================================================================
// src/bacnet/xslot_bacnet.h - BACnet序列化
// =============================================================================

#ifndef XSLOT_BACNET_H
#define XSLOT_BACNET_H

#include "xslot.h"
#include <vector>

namespace xslot {

// BACnet对象序列化
class BACnetSerializer {
public:
  // 序列化单个对象到字节流
  static std::vector<uint8_t> serialize(const xslot_bacnet_object_t &obj);

  // 序列化多个对象到字节流
  static std::vector<uint8_t>
  serializeMultiple(const xslot_bacnet_object_t *objects, uint8_t count);

  // 反序列化字节流到对象
  static bool deserialize(const uint8_t *data, size_t len,
                          xslot_bacnet_object_t &obj);

  // 反序列化字节流到多个对象
  static int deserializeMultiple(const uint8_t *data, size_t len,
                                 xslot_bacnet_object_t *objects,
                                 uint8_t max_count);

  // 计算序列化后的大小
  static size_t getSerializedSize(const xslot_bacnet_object_t &obj);

private:
  // 内部序列化格式：
  // [OBJ_ID:2][OBJ_TYPE:1][FLAGS:1][VALUE:变长]
  static constexpr size_t MIN_OBJ_SIZE = 4;
};

} // namespace xslot

#endif // XSLOT_BACNET_H