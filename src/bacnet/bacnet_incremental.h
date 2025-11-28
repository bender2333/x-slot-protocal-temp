/**
 * @file bacnet_incremental.h
 * @brief BACnet 对象增量格式序列化 (COV 上报专用)
 *
 * 增量格式仅传输 Present_Value，省略 OBJ_TYPE 和 FLAGS，节省约 25% 带宽。
 * 格式: [OBJ_ID:2B][TYPE_HINT:1B][VALUE:变长]
 *
 * TYPE_HINT 编码:
 *   - bit7 = 1 表示增量格式
 *   - bit3-0 表示值类型: 0=ANALOG, 1=BINARY, 2=OTHER
 */
#ifndef BACNET_INCREMENTAL_H
#define BACNET_INCREMENTAL_H

#include <stdint.h>
#include <xslot/xslot_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TYPE_HINT 标志位 */
#define INCREMENTAL_FLAG 0x80  /**< bit7=1 表示增量格式 */
#define VALUE_TYPE_ANALOG 0x00 /**< 模拟量 (float) */
#define VALUE_TYPE_BINARY 0x01 /**< 二进制量 (uint8) */
#define VALUE_TYPE_OTHER 0x02  /**< 其他 (raw 16B) */

/**
 * @brief 序列化单个对象 (增量格式)
 * @param obj 对象
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 写入的字节数，失败返回负数
 */
int bacnet_incremental_serialize(const xslot_bacnet_object_t *obj,
                                 uint8_t *buffer, uint8_t buffer_size);

/**
 * @brief 序列化多个对象 (增量格式, COV 批量上报)
 * @param objects 对象数组
 * @param count 对象数量
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 写入的字节数，失败返回负数
 *
 * 输出格式: [COUNT:1B][OBJ1][OBJ2]...
 */
int bacnet_incremental_serialize_batch(const xslot_bacnet_object_t *objects,
                                       uint8_t count, uint8_t *buffer,
                                       uint8_t buffer_size);

/**
 * @brief 反序列化单个对象 (增量格式)
 * @param buffer 输入缓冲区
 * @param len 缓冲区长度
 * @param obj 输出对象 (仅填充 object_id, object_type, present_value)
 * @return 消耗的字节数，失败返回负数
 */
int bacnet_incremental_deserialize(const uint8_t *buffer, uint8_t len,
                                   xslot_bacnet_object_t *obj);

/**
 * @brief 反序列化多个对象 (增量格式)
 * @param buffer 输入缓冲区
 * @param len 缓冲区长度
 * @param objects 输出对象数组
 * @param max_count 数组最大容量
 * @return 解析的对象数量，失败返回负数
 */
int bacnet_incremental_deserialize_batch(const uint8_t *buffer, uint8_t len,
                                         xslot_bacnet_object_t *objects,
                                         uint8_t max_count);

/**
 * @brief 判断数据是否为增量格式
 * @param type_hint TYPE_HINT 字节
 * @return true=增量格式, false=完整格式
 */
static inline bool bacnet_is_incremental_format(uint8_t type_hint) {
  return (type_hint & INCREMENTAL_FLAG) != 0;
}

/**
 * @brief 计算单个对象的增量序列化长度
 * @param obj 对象
 * @return 字节数
 */
uint8_t bacnet_incremental_size(const xslot_bacnet_object_t *obj);

#ifdef __cplusplus
}
#endif

#endif /* BACNET_INCREMENTAL_H */
