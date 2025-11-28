/**
 * @file direct_transport.h
 * @brief HMI 直连传输层
 */
#ifndef DIRECT_TRANSPORT_H
#define DIRECT_TRANSPORT_H

#include "i_transport.h"
#include <xslot/xslot_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建 Direct 传输层
 */
i_transport_t *direct_transport_create(const xslot_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* DIRECT_TRANSPORT_H */
