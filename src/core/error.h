/**
 * @file error.h
 * @brief X-Slot 错误类型定义 (C++20)
 */
#ifndef XSLOT_ERROR_H_INTERNAL
#define XSLOT_ERROR_H_INTERNAL

#include <cstdint>
#include <xslot/xslot_error.h>

namespace xslot {

/**
 * @brief 错误码枚举类 (类型安全)
 */
enum class Error : int {
  Ok = XSLOT_OK,
  InvalidParam = XSLOT_ERR_PARAM,
  Timeout = XSLOT_ERR_TIMEOUT,
  CrcError = XSLOT_ERR_CRC,
  NoMemory = XSLOT_ERR_NO_MEM,
  Busy = XSLOT_ERR_BUSY,
  Offline = XSLOT_ERR_OFFLINE,
  NoDevice = XSLOT_ERR_NO_DEVICE,
  NotInitialized = XSLOT_ERR_NOT_INIT,
  SendFailed = XSLOT_ERR_SEND_FAIL,
};

/**
 * @brief 转换为C错误码
 */
constexpr int to_c_error(Error e) noexcept { return static_cast<int>(e); }

/**
 * @brief 从C错误码转换
 */
constexpr Error from_c_error(int e) noexcept { return static_cast<Error>(e); }

/**
 * @brief 检查是否成功
 */
constexpr bool is_ok(Error e) noexcept { return e == Error::Ok; }

/**
 * @brief 获取错误描述
 */
inline const char *error_message(Error e) noexcept {
  return xslot_strerror(static_cast<xslot_error_t>(to_c_error(e)));
}

} // namespace xslot

#endif // XSLOT_ERROR_H_INTERNAL
