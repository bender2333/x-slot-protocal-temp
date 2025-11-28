/**
 * @file bacnet_serializer.h
 * @brief BACnet 对象完整格式序列化
 *
 * 完整格式用于首次上报、属性读写，包含完整的对象元数据。
 * 格式: [OBJ_ID:2B][OBJ_TYPE:1B][FLAGS:1B][VALUE:变长]
 */
#ifndef BACNET_SERIALIZER_H
#define BACNET_SERIALIZER_H

#include <stdint.h>
#include <xslot/xslot_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 序列化单个对象 (完整格式)
 * @param obj 对象
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 写入的字节数，失败返回负数
 */
int bacnet_serialize_object(const xslot_bacnet_object_t *obj, uint8_t *buffer,
                            uint8_t buffer_size);

/**
 * @brief 序列化多个对象 (完整格式)
 * @param objects 对象数组
 * @param count 对象数量
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 写入的字节数，失败返回负数
 *
 * 输出格式: [COUNT:1B][OBJ1][OBJ2]...
 */
int bacnet_serialize_objects(const xslot_bacnet_object_t *objects,
                             uint8_t count, uint8_t *buffer,
                             uint8_t buffer_size);

/**
 * @brief 反序列化单个对象 (完整格式)
 * @param buffer 输入缓冲区
 * @param len 缓冲区长度
 * @param obj 输出对象
 * @return 消耗的字节数，失败返回负数
 */
int bacnet_deserialize_object(const uint8_t *buffer, uint8_t len,
                              xslot_bacnet_object_t *obj);

/**
 * @brief 反序列化多个对象 (完整格式)
 * @param buffer 输入缓冲区
 * @param len 缓冲区长度
 * @param objects 输出对象数组
 * @param max_count 数组最大容量
 * @return 解析的对象数量，失败返回负数
 */
int bacnet_deserialize_objects(const uint8_t *buffer, uint8_t len,
                               xslot_bacnet_object_t *objects,
                               uint8_t max_count);

/**
 * @brief 计算单个对象的序列化长度
 * @param obj 对象
 * @return 字节数
 */
uint8_t bacnet_object_serialized_size(const xslot_bacnet_object_t *obj);

#ifdef __cplusplus
}
#endif

#endif /* BACNET_SERIALIZER_H */
