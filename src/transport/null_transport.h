/**
 * @file null_transport.h
 * @brief 空传输层 (无设备时使用)
 */
#ifndef NULL_TRANSPORT_H
#define NULL_TRANSPORT_H

#include "i_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建 Null 传输层
 */
i_transport_t *null_transport_create(void);

#ifdef __cplusplus
}
#endif

#endif /* NULL_TRANSPORT_H */
