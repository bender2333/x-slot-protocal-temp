/**
 * @file function_ref.h
 * @brief 轻量级函数引用 - 不拥有所有权的函数包装器 (C++20)
 *
 * 类似 std::function_ref (C++26) 或 llvm::function_ref。
 * 不进行堆分配，适合嵌入式环境。
 *
 * 注意：被引用的可调用对象必须在 function_ref 使用期间保持有效。
 */
#ifndef XSLOT_FUNCTION_REF_H
#define XSLOT_FUNCTION_REF_H

#include <cstddef>
#include <type_traits>
#include <utility>

namespace xslot {

/**
 * @brief 轻量级函数引用
 *
 * @tparam Signature 函数签名，如 void(int, float)
 */
template <typename Signature> class function_ref;

template <typename R, typename... Args> class function_ref<R(Args...)> {
public:
  /**
   * @brief 默认构造（空引用）
   */
  constexpr function_ref() noexcept : callable_(nullptr), invoker_(nullptr) {}

  /**
   * @brief 从可调用对象构造
   */
  template <typename F>
    requires(!std::is_same_v<std::decay_t<F>, function_ref>) &&
                std::is_invocable_r_v<R, F &, Args...>
  constexpr function_ref(F &&f) noexcept
      : callable_(
            const_cast<void *>(static_cast<const void *>(std::addressof(f)))),
        invoker_(&invoke_impl<std::remove_reference_t<F>>) {}

  /**
   * @brief 从C风格函数指针构造
   */
  constexpr function_ref(R (*func)(Args...)) noexcept
      : callable_(reinterpret_cast<void *>(func)),
        invoker_(func ? &invoke_func_ptr : nullptr) {}

  /**
   * @brief 拷贝构造
   */
  constexpr function_ref(const function_ref &) noexcept = default;

  /**
   * @brief 拷贝赋值
   */
  constexpr function_ref &operator=(const function_ref &) noexcept = default;

  /**
   * @brief 检查是否有效
   */
  [[nodiscard]] constexpr explicit operator bool() const noexcept {
    return invoker_ != nullptr;
  }

  /**
   * @brief 调用
   */
  R operator()(Args... args) const {
    return invoker_(callable_, std::forward<Args>(args)...);
  }

  /**
   * @brief 交换
   */
  constexpr void swap(function_ref &other) noexcept {
    std::swap(callable_, other.callable_);
    std::swap(invoker_, other.invoker_);
  }

private:
  using invoker_t = R (*)(void *, Args...);

  template <typename F> static R invoke_impl(void *callable, Args... args) {
    return (*static_cast<F *>(callable))(std::forward<Args>(args)...);
  }

  static R invoke_func_ptr(void *callable, Args... args) {
    return reinterpret_cast<R (*)(Args...)>(callable)(
        std::forward<Args>(args)...);
  }

  void *callable_;
  invoker_t invoker_;
};

/**
 * @brief 交换两个 function_ref
 */
template <typename Signature>
constexpr void swap(function_ref<Signature> &lhs,
                    function_ref<Signature> &rhs) noexcept {
  lhs.swap(rhs);
}

// 推导指南
template <typename R, typename... Args>
function_ref(R (*)(Args...)) -> function_ref<R(Args...)>;

} // namespace xslot

#endif // XSLOT_FUNCTION_REF_H
