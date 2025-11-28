/**
 * @file xslot_error.h
 * @brief X-Slot 错误码定义
 */
#ifndef XSLOT_ERROR_H
#define XSLOT_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 错误码枚举
 */
typedef enum {
  XSLOT_OK = 0,             /**< 成功 */
  XSLOT_ERR_PARAM = -1,     /**< 参数错误 */
  XSLOT_ERR_TIMEOUT = -2,   /**< 超时 */
  XSLOT_ERR_CRC = -3,       /**< CRC 校验失败 */
  XSLOT_ERR_NO_MEM = -4,    /**< 内存不足 */
  XSLOT_ERR_BUSY = -5,      /**< 系统繁忙 */
  XSLOT_ERR_OFFLINE = -6,   /**< 节点离线 */
  XSLOT_ERR_NO_DEVICE = -7, /**< 未检测到设备 */
  XSLOT_ERR_NOT_INIT = -8,  /**< 未初始化 */
  XSLOT_ERR_SEND_FAIL = -9, /**< 发送失败 */
} xslot_error_t;

/**
 * @brief 获取错误码描述字符串
 * @param err 错误码
 * @return 错误描述
 */
const char *xslot_strerror(xslot_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* XSLOT_ERROR_H */
