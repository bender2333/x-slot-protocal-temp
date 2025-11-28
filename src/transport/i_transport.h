/**
 * @file i_transport.h
 * @brief 传输层接口定义
 */
#ifndef I_TRANSPORT_H
#define I_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>
#include <xslot/xslot_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 接收回调函数类型
 */
typedef void (*transport_receive_cb)(void *ctx, const uint8_t *data,
                                     uint16_t len);

/**
 * @brief 传输层接口 (虚表)
 */
typedef struct i_transport_vtable {
  int (*start)(void *impl);
  void (*stop)(void *impl);
  int (*send)(void *impl, const uint8_t *data, uint16_t len);
  int (*probe)(void *impl);
  int (*configure)(void *impl, uint8_t cell_id, int8_t power_dbm);
  void (*set_receive_cb)(void *impl, transport_receive_cb cb, void *ctx);
  void (*destroy)(void *impl);
} i_transport_vtable_t;

/**
 * @brief 传输层基类
 */
typedef struct i_transport {
  const i_transport_vtable_t *vtable;
  void *impl;
} i_transport_t;

/* ============================================================================
 * 传输层接口函数
 * ============================================================================
 */

static inline int transport_start(i_transport_t *t) {
  return t && t->vtable->start ? t->vtable->start(t->impl) : -1;
}

static inline void transport_stop(i_transport_t *t) {
  if (t && t->vtable->stop)
    t->vtable->stop(t->impl);
}

static inline int transport_send(i_transport_t *t, const uint8_t *data,
                                 uint16_t len) {
  return t && t->vtable->send ? t->vtable->send(t->impl, data, len) : -1;
}

static inline int transport_probe(i_transport_t *t) {
  return t && t->vtable->probe ? t->vtable->probe(t->impl) : -1;
}

static inline int transport_configure(i_transport_t *t, uint8_t cell_id,
                                      int8_t power_dbm) {
  return t && t->vtable->configure
             ? t->vtable->configure(t->impl, cell_id, power_dbm)
             : -1;
}

static inline void transport_set_receive_callback(i_transport_t *t,
                                                  transport_receive_cb cb,
                                                  void *ctx) {
  if (t && t->vtable->set_receive_cb)
    t->vtable->set_receive_cb(t->impl, cb, ctx);
}

static inline void transport_destroy(i_transport_t *t) {
  if (t && t->vtable->destroy)
    t->vtable->destroy(t->impl);
}

/* ============================================================================
 * 传输层工厂函数
 * ============================================================================
 */

/**
 * @brief 创建 TPMesh 传输层
 */
i_transport_t *tpmesh_transport_create(const xslot_config_t *config);

/**
 * @brief 创建 Direct 传输层 (HMI)
 */
i_transport_t *direct_transport_create(const xslot_config_t *config);

/**
 * @brief 创建 Null 传输层 (空实现)
 */
i_transport_t *null_transport_create(void);

#ifdef __cplusplus
}
#endif

#endif /* I_TRANSPORT_H */
