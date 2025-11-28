/**
 * @file bacnet_object_def.h
 * @brief BACnet 对象定义和转换接口
 *
 * 本文件定义了 X-Slot 与 DDC 实际使用的 BACnet 对象之间的转换接口。
 * 参考: bacnet/AnalogueInput.h, AnalogueOutput.h, DigitalInput.h,
 * DigitalOutput.h
 */
#ifndef BACNET_OBJECT_DEF_H
#define BACNET_OBJECT_DEF_H

#include <stdbool.h>
#include <stdint.h>
#include <xslot/xslot_types.h>

/* 引入 DDC 实际对象定义 */
#include "../../bacnet/AnalogueInput.h"
#include "../../bacnet/AnalogueOutput.h"
#include "../../bacnet/DigitalInput.h"
#include "../../bacnet/DigitalOutput.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * 对象类型判断
 * =============================================================================
 */

/**
 * @brief 判断是否为模拟量类型
 */
static inline bool xslot_is_analog_type(uint8_t obj_type) {
  return obj_type == XSLOT_OBJ_ANALOG_INPUT ||
         obj_type == XSLOT_OBJ_ANALOG_OUTPUT ||
         obj_type == XSLOT_OBJ_ANALOG_VALUE;
}

/**
 * @brief 判断是否为二进制类型
 */
static inline bool xslot_is_binary_type(uint8_t obj_type) {
  return obj_type == XSLOT_OBJ_BINARY_INPUT ||
         obj_type == XSLOT_OBJ_BINARY_OUTPUT ||
         obj_type == XSLOT_OBJ_BINARY_VALUE;
}

/**
 * @brief 获取值的字节长度
 */
static inline uint8_t xslot_get_value_size(uint8_t obj_type) {
  if (xslot_is_analog_type(obj_type)) {
    return sizeof(float); // 4 bytes
  } else if (xslot_is_binary_type(obj_type)) {
    return 1; // 1 byte
  }
  return 16; // raw data
}

/* =============================================================================
 * 从 DDC 实际对象结构转换
 * =============================================================================
 */

/**
 * @brief 从 ANALOGINPUTOBJECT 创建 X-Slot 对象
 * @param ai 指向 DDC AI 对象的指针
 * @param obj 输出的 X-Slot 对象
 */
static inline void xslot_from_ai(const ANALOGINPUTOBJECT *ai,
                                 xslot_bacnet_object_t *obj) {
  obj->object_id = ai->uidata.index;
  obj->object_type = XSLOT_OBJ_ANALOG_INPUT;
  obj->flags = 0;
  if (ai->uidata.outOfService)
    obj->flags |= XSLOT_FLAG_OUT_OF_SERVICE;
  /* 使用 alarm 字段判断是否变化 */
  if (ai->uidata.alarm)
    obj->flags |= XSLOT_FLAG_CHANGED;
  obj->present_value.analog = ai->uidata.value;
}

/**
 * @brief 从 ANALOGOUTPUTOBJECT 创建 X-Slot 对象
 * @param ao 指向 DDC AO 对象的指针
 * @param obj 输出的 X-Slot 对象
 */
static inline void xslot_from_ao(const ANALOGOUTPUTOBJECT *ao,
                                 xslot_bacnet_object_t *obj) {
  obj->object_id = ao->aodata.index;
  obj->object_type = XSLOT_OBJ_ANALOG_OUTPUT;
  obj->flags = 0;
  if (ao->aodata.outOfService)
    obj->flags |= XSLOT_FLAG_OUT_OF_SERVICE;
  obj->present_value.analog = ao->aodata.value;
}

/**
 * @brief 从 DIGITALINPUTOBJECT 创建 X-Slot 对象 (映射为 BI)
 * @param di 指向 DDC DI 对象的指针
 * @param obj 输出的 X-Slot 对象
 */
static inline void xslot_from_di(const DIGITALINPUTOBJECT *di,
                                 xslot_bacnet_object_t *obj) {
  obj->object_id = di->didata.index;
  obj->object_type = XSLOT_OBJ_BINARY_INPUT;
  obj->flags = 0;
  if (di->didata.outOfService)
    obj->flags |= XSLOT_FLAG_OUT_OF_SERVICE;
  if (di->didata.alarm)
    obj->flags |= XSLOT_FLAG_CHANGED;
  obj->present_value.binary = di->didata.state ? 1 : 0;
}

/**
 * @brief 从 DIGITALOUTPUTOBJECT 创建 X-Slot 对象 (映射为 BO)
 * @param dobj 指向 DDC DO 对象的指针
 * @param obj 输出的 X-Slot 对象
 */
static inline void xslot_from_do(const DIGITALOUTPUTOBJECT *dobj,
                                 xslot_bacnet_object_t *obj) {
  obj->object_id = dobj->dodata.index;
  obj->object_type = XSLOT_OBJ_BINARY_OUTPUT;
  obj->flags = 0;
  if (dobj->dodata.outOfService)
    obj->flags |= XSLOT_FLAG_OUT_OF_SERVICE;
  obj->present_value.binary = dobj->dodata.out ? 1 : 0;
}

/* =============================================================================
 * 简化创建接口 (直接传值)
 * =============================================================================
 */

/**
 * @brief 创建 AI X-Slot 对象
 */
static inline void xslot_make_ai_object(uint16_t instance, float present_value,
                                        bool changed, bool out_of_service,
                                        xslot_bacnet_object_t *obj) {
  obj->object_id = instance;
  obj->object_type = XSLOT_OBJ_ANALOG_INPUT;
  obj->flags = 0;
  if (changed)
    obj->flags |= XSLOT_FLAG_CHANGED;
  if (out_of_service)
    obj->flags |= XSLOT_FLAG_OUT_OF_SERVICE;
  obj->present_value.analog = present_value;
}

/**
 * @brief 创建 AO X-Slot 对象
 */
static inline void xslot_make_ao_object(uint16_t instance, float present_value,
                                        bool changed, bool out_of_service,
                                        xslot_bacnet_object_t *obj) {
  obj->object_id = instance;
  obj->object_type = XSLOT_OBJ_ANALOG_OUTPUT;
  obj->flags = 0;
  if (changed)
    obj->flags |= XSLOT_FLAG_CHANGED;
  if (out_of_service)
    obj->flags |= XSLOT_FLAG_OUT_OF_SERVICE;
  obj->present_value.analog = present_value;
}

/**
 * @brief 创建 AV X-Slot 对象
 */
static inline void xslot_make_av_object(uint16_t instance, float present_value,
                                        bool changed, bool out_of_service,
                                        xslot_bacnet_object_t *obj) {
  obj->object_id = instance;
  obj->object_type = XSLOT_OBJ_ANALOG_VALUE;
  obj->flags = 0;
  if (changed)
    obj->flags |= XSLOT_FLAG_CHANGED;
  if (out_of_service)
    obj->flags |= XSLOT_FLAG_OUT_OF_SERVICE;
  obj->present_value.analog = present_value;
}

/**
 * @brief 创建 BI X-Slot 对象
 */
static inline void xslot_make_bi_object(uint16_t instance,
                                        uint8_t present_value, bool changed,
                                        bool out_of_service,
                                        xslot_bacnet_object_t *obj) {
  obj->object_id = instance;
  obj->object_type = XSLOT_OBJ_BINARY_INPUT;
  obj->flags = 0;
  if (changed)
    obj->flags |= XSLOT_FLAG_CHANGED;
  if (out_of_service)
    obj->flags |= XSLOT_FLAG_OUT_OF_SERVICE;
  obj->present_value.binary = present_value ? 1 : 0;
}

/**
 * @brief 创建 BO X-Slot 对象
 */
static inline void xslot_make_bo_object(uint16_t instance,
                                        uint8_t present_value, bool changed,
                                        bool out_of_service,
                                        xslot_bacnet_object_t *obj) {
  obj->object_id = instance;
  obj->object_type = XSLOT_OBJ_BINARY_OUTPUT;
  obj->flags = 0;
  if (changed)
    obj->flags |= XSLOT_FLAG_CHANGED;
  if (out_of_service)
    obj->flags |= XSLOT_FLAG_OUT_OF_SERVICE;
  obj->present_value.binary = present_value ? 1 : 0;
}

/**
 * @brief 创建 BV X-Slot 对象
 */
static inline void xslot_make_bv_object(uint16_t instance,
                                        uint8_t present_value, bool changed,
                                        bool out_of_service,
                                        xslot_bacnet_object_t *obj) {
  obj->object_id = instance;
  obj->object_type = XSLOT_OBJ_BINARY_VALUE;
  obj->flags = 0;
  if (changed)
    obj->flags |= XSLOT_FLAG_CHANGED;
  if (out_of_service)
    obj->flags |= XSLOT_FLAG_OUT_OF_SERVICE;
  obj->present_value.binary = present_value ? 1 : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* BACNET_OBJECT_DEF_H */
