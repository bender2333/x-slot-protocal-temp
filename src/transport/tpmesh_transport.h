/**
 * @file tpmesh_transport.h
 * @brief TPMesh 传输层
 */
#ifndef TPMESH_TRANSPORT_H
#define TPMESH_TRANSPORT_H

#include "i_transport.h"
#include <xslot/xslot_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建 TPMesh 传输层
 */
i_transport_t *tpmesh_transport_create(const xslot_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* TPMESH_TRANSPORT_H */
