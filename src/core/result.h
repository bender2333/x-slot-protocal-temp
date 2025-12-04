/**
 * @file result.h
 * @brief Result<T, E> 类型 - 替代异常的错误处理 (C++20)
 *
 * 嵌入式环境通常禁用异常，Result类型提供类型安全的错误处理。
 * 设计参考 Rust 的 Result 和 C++23 的 std::expected。
 */
#ifndef XSLOT_RESULT_H
#define XSLOT_RESULT_H

#include "error.h"
#include <cassert>
#include <functional>
#include <new>
#include <type_traits>
#include <utility>

namespace xslot {

/**
 * @brief 空类型标记，用于 Result<void, E>
 */
struct Unit {};

/**
 * @brief Result 类型 - 成功值或错误值的和类型
 *
 * @tparam T 成功时的值类型
 * @tparam E 错误类型，默认为 xslot::Error
 */
template <typename T, typename E = Error> class Result {
public:
  using value_type = T;
  using error_type = E;

  /**
   * @brief 创建成功结果
   */
  static Result ok(T value) noexcept(std::is_nothrow_move_constructible_v<T>) {
    return Result(std::move(value), true);
  }

  /**
   * @brief 创建错误结果
   */
  static Result error(E err) noexcept { return Result(err); }

  /**
   * @brief 从C错误码创建
   */
  static Result from_c_error(int err) noexcept {
    if (err == 0) {
      if constexpr (std::is_default_constructible_v<T>) {
        return ok(T{});
      } else {
        // 对于非默认可构造类型，需要显式提供值
        return error(xslot::from_c_error(err));
      }
    }
    return error(xslot::from_c_error(err));
  }

  // 析构函数
  ~Result() { destroy(); }

  // 移动构造
  Result(Result &&other) noexcept(std::is_nothrow_move_constructible_v<T>)
      : has_value_(other.has_value_) {
    if (has_value_) {
      new (&storage_.value) T(std::move(other.storage_.value));
    } else {
      storage_.error = other.storage_.error;
    }
  }

  // 移动赋值
  Result &
  operator=(Result &&other) noexcept(std::is_nothrow_move_constructible_v<T>) {
    if (this != &other) {
      destroy();
      has_value_ = other.has_value_;
      if (has_value_) {
        new (&storage_.value) T(std::move(other.storage_.value));
      } else {
        storage_.error = other.storage_.error;
      }
    }
    return *this;
  }

  // 禁用拷贝（可根据需要启用）
  Result(const Result &) = delete;
  Result &operator=(const Result &) = delete;

  /**
   * @brief 检查是否成功
   */
  [[nodiscard]] bool is_ok() const noexcept { return has_value_; }

  /**
   * @brief 检查是否失败
   */
  [[nodiscard]] bool is_error() const noexcept { return !has_value_; }

  /**
   * @brief 布尔转换 (用于 if 判断)
   */
  explicit operator bool() const noexcept { return has_value_; }

  /**
   * @brief 获取值引用 (调用前需确保 is_ok())
   */
  [[nodiscard]] T &value() & {
    assert(has_value_ && "Result does not contain a value");
    return storage_.value;
  }

  [[nodiscard]] const T &value() const & {
    assert(has_value_ && "Result does not contain a value");
    return storage_.value;
  }

  [[nodiscard]] T &&value() && {
    assert(has_value_ && "Result does not contain a value");
    return std::move(storage_.value);
  }

  /**
   * @brief 获取错误 (调用前需确保 is_error())
   */
  [[nodiscard]] E error() const noexcept {
    assert(!has_value_ && "Result contains a value, not an error");
    return storage_.error;
  }

  /**
   * @brief 获取值或默认值
   */
  [[nodiscard]] T value_or(T default_value) && noexcept(
      std::is_nothrow_move_constructible_v<T>) {
    if (has_value_) {
      return std::move(storage_.value);
    }
    return std::move(default_value);
  }

  /**
   * @brief 转换为C错误码
   */
  [[nodiscard]] int to_c_error() const noexcept {
    if (has_value_) {
      return 0;
    }
    return xslot::to_c_error(storage_.error);
  }

  /**
   * @brief 链式操作 - 如果成功则执行函数
   */
  template <typename F>
  auto and_then(F &&f) && -> std::invoke_result_t<F, T &&> {
    using ResultType = std::invoke_result_t<F, T &&>;
    if (has_value_) {
      return std::forward<F>(f)(std::move(storage_.value));
    }
    return ResultType::error(storage_.error);
  }

  /**
   * @brief 映射操作 - 转换成功值
   */
  template <typename F>
  auto map(F &&f) && -> Result<std::invoke_result_t<F, T &&>, E> {
    using U = std::invoke_result_t<F, T &&>;
    if (has_value_) {
      return Result<U, E>::ok(std::forward<F>(f)(std::move(storage_.value)));
    }
    return Result<U, E>::error(storage_.error);
  }

private:
  // 构造成功结果
  explicit Result(T value, bool /*tag*/) noexcept(
      std::is_nothrow_move_constructible_v<T>)
      : has_value_(true) {
    new (&storage_.value) T(std::move(value));
  }

  // 构造错误结果
  explicit Result(E err) noexcept : has_value_(false) { storage_.error = err; }

  void destroy() noexcept {
    if (has_value_) {
      storage_.value.~T();
    }
  }

  union Storage {
    T value;
    E error;

    Storage() noexcept {}
    ~Storage() {}
  } storage_;

  bool has_value_;
};

/**
 * @brief Result<void, E> 特化 - 仅表示成功或失败
 */
template <typename E> class Result<void, E> {
public:
  using value_type = void;
  using error_type = E;

  /**
   * @brief 创建成功结果
   */
  static Result ok() noexcept { return Result(true); }

  /**
   * @brief 创建错误结果
   */
  static Result error(E err) noexcept { return Result(err); }

  /**
   * @brief 从C错误码创建
   */
  static Result from_c_error(int err) noexcept {
    if (err == 0) {
      return ok();
    }
    return error(xslot::from_c_error(err));
  }

  [[nodiscard]] bool is_ok() const noexcept { return has_value_; }
  [[nodiscard]] bool is_error() const noexcept { return !has_value_; }
  explicit operator bool() const noexcept { return has_value_; }

  [[nodiscard]] E error() const noexcept {
    assert(!has_value_ && "Result is ok, not an error");
    return error_;
  }

  [[nodiscard]] int to_c_error() const noexcept {
    if (has_value_) {
      return 0;
    }
    return xslot::to_c_error(error_);
  }

  /**
   * @brief 链式操作
   */
  template <typename F> auto and_then(F &&f) && -> std::invoke_result_t<F> {
    using ResultType = std::invoke_result_t<F>;
    if (has_value_) {
      return std::forward<F>(f)();
    }
    return ResultType::error(error_);
  }

private:
  explicit Result(bool success) noexcept : has_value_(success), error_(E{}) {}
  explicit Result(E err) noexcept : has_value_(false), error_(err) {}

  bool has_value_;
  E error_;
};

// 便捷类型别名
template <typename T> using XResult = Result<T, Error>;

using VoidResult = Result<void, Error>;

} // namespace xslot

#endif // XSLOT_RESULT_H
