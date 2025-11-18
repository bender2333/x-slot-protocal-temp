// =============================================================================
// src/bacnet/xslot_bacnet_incremental.h - BACnet增量序列化器
// =============================================================================
//
// 设计目标：
// 1. 减少无线传输数据量
// 2. 支持Present_Value增量上报
// 3. 兼容完整对象格式
// 4. 自动识别格式类型
//
// 格式定义：
//
// 【完整格式】（用于初次上报、属性读写）
// [OBJ_ID:2] [OBJ_TYPE:1] [FLAGS:1] [VALUE:变长]
// - FLAGS bit7 = 0 表示完整格式
//
// 【增量格式】（用于COV上报）
// [OBJ_ID:2] [TYPE_HINT:1] [VALUE:变长]
// - TYPE_HINT bit7 = 1 表示增量格式
// - TYPE_HINT bit3-0 = 值类型 (0=ANALOG, 1=BINARY, 2=OTHER)
//
// =============================================================================

#ifndef XSLOT_BACNET_INCREMENTAL_H
#define XSLOT_BACNET_INCREMENTAL_H

#include "xslot.h"
#include <vector>

namespace xslot {

// =============================================================================
// 增量序列化器
// =============================================================================
class BACnetIncrementalSerializer {
public:
  // 序列化标志位定义
  static constexpr uint8_t FLAG_INCREMENTAL = 0x80; // bit7: 1=增量格式
  static constexpr uint8_t TYPE_ANALOG = 0x00;      // 模拟值
  static constexpr uint8_t TYPE_BINARY = 0x01;      // 二进制值
  static constexpr uint8_t TYPE_OTHER = 0x02;       // 其他类型

  // -------------------------------------------------------------------------
  // 序列化（编码）
  // -------------------------------------------------------------------------

  // 序列化单个对象 - 完整格式
  static std::vector<uint8_t> serializeFull(const xslot_bacnet_object_t &obj);

  // 序列化单个对象 - 增量格式（仅Present_Value）
  static std::vector<uint8_t>
  serializeIncremental(const xslot_bacnet_object_t &obj);

  // 批量序列化 - 完整格式
  static std::vector<uint8_t>
  serializeFullBatch(const xslot_bacnet_object_t *objects, uint8_t count);

  // 批量序列化 - 增量格式
  static std::vector<uint8_t>
  serializeIncrementalBatch(const xslot_bacnet_object_t *objects,
                            uint8_t count);

  // -------------------------------------------------------------------------
  // 反序列化（解码）
  // -------------------------------------------------------------------------

  // 反序列化单个对象（自动识别格式）
  // @param is_incremental: 输出参数，true=增量格式, false=完整格式
  static bool deserialize(const uint8_t *data, size_t len,
                          xslot_bacnet_object_t &obj, bool &is_incremental);

  // 反序列化多个对象（自动识别格式）
  // @param objects: 输出数组
  // @param max_count: 最大对象数
  // @return 实际解析的对象数量，<0表示错误
  static int deserializeBatch(const uint8_t *data, size_t len,
                              xslot_bacnet_object_t *objects,
                              uint8_t max_count);

  // -------------------------------------------------------------------------
  // 辅助函数
  // -------------------------------------------------------------------------

  // 判断是否为增量格式
  static bool isIncremental(const uint8_t *data, size_t len);

  // 计算序列化后的大小
  static size_t getSerializedSize(const xslot_bacnet_object_t &obj,
                                  bool incremental);

  // 估算批量序列化大小
  static size_t estimateBatchSize(const xslot_bacnet_object_t *objects,
                                  uint8_t count, bool incremental);

private:
  // 获取值类型提示
  static uint8_t getTypeHint(uint8_t obj_type);

  // 从类型提示推断对象类型
  static uint8_t inferObjectType(uint8_t type_hint);

  // 序列化值（根据类型）
  static std::vector<uint8_t> serializeValue(const xslot_bacnet_object_t &obj,
                                             uint8_t type_hint);

  // 反序列化值（根据类型）
  static bool deserializeValue(const uint8_t *data, size_t len,
                               xslot_bacnet_object_t &obj, uint8_t type_hint);

  // 最小序列化大小
  static constexpr size_t MIN_FULL_SIZE =
      4; // OBJ_ID(2) + OBJ_TYPE(1) + FLAGS(1)
  static constexpr size_t MIN_INCREMENTAL_SIZE = 3; // OBJ_ID(2) + TYPE_HINT(1)
};

} // namespace xslot

#endif // XSLOT_BACNET_INCREMENTAL_H
