/**
 * @file unique_handle.h
 * @brief 轻量级 RAII 句柄包装器 (C++20)
 *
 * 类似 std::unique_ptr 但支持自定义删除器和无效值。
 * 用于包装 C 风格的句柄（如串口、互斥锁等）。
 */
#ifndef XSLOT_UNIQUE_HANDLE_H
#define XSLOT_UNIQUE_HANDLE_H

#include <type_traits>
#include <utility>

namespace xslot {

/**
 * @brief 句柄特性模板 - 定义句柄的无效值和删除方法
 *
 * 使用时需要特化此模板，例如：
 * @code
 * template <>
 * struct HandleTraits<void*> {
 *     static void* invalid() noexcept { return nullptr; }
 *     static void close(void* h) noexcept { hal_serial_close(h); }
 * };
 * @endcode
 */
template <typename Handle> struct HandleTraits {
  // 子类需要实现:
  // static Handle invalid() noexcept;
  // static void close(Handle h) noexcept;
};

/**
 * @brief 指针句柄的默认特性
 */
template <typename T> struct PointerHandleTraits {
  static T *invalid() noexcept { return nullptr; }
  // close 方法需要由用户提供
};

/**
 * @brief RAII 句柄包装器
 *
 * @tparam Handle 句柄类型
 * @tparam Traits 句柄特性类，需提供 invalid() 和 close() 静态方法
 */
template <typename Handle, typename Traits = HandleTraits<Handle>>
class UniqueHandle {
public:
  using handle_type = Handle;
  using traits_type = Traits;

  /**
   * @brief 默认构造（无效句柄）
   */
  constexpr UniqueHandle() noexcept : handle_(Traits::invalid()) {}

  /**
   * @brief 从原始句柄构造
   */
  explicit constexpr UniqueHandle(Handle h) noexcept : handle_(h) {}

  /**
   * @brief 析构 - 自动关闭句柄
   */
  ~UniqueHandle() { close(); }

  // 禁用拷贝
  UniqueHandle(const UniqueHandle &) = delete;
  UniqueHandle &operator=(const UniqueHandle &) = delete;

  /**
   * @brief 移动构造
   */
  UniqueHandle(UniqueHandle &&other) noexcept : handle_(other.release()) {}

  /**
   * @brief 移动赋值
   */
  UniqueHandle &operator=(UniqueHandle &&other) noexcept {
    if (this != &other) {
      reset(other.release());
    }
    return *this;
  }

  /**
   * @brief 检查句柄是否有效
   */
  [[nodiscard]] constexpr bool valid() const noexcept {
    return handle_ != Traits::invalid();
  }

  /**
   * @brief 布尔转换
   */
  [[nodiscard]] constexpr explicit operator bool() const noexcept {
    return valid();
  }

  /**
   * @brief 获取原始句柄
   */
  [[nodiscard]] constexpr Handle get() const noexcept { return handle_; }

  /**
   * @brief 解引用（适用于指针句柄）
   */
  template <typename H = Handle>
    requires std::is_pointer_v<H>
  [[nodiscard]] constexpr auto operator*() const noexcept
      -> std::add_lvalue_reference_t<std::remove_pointer_t<H>> {
    return *handle_;
  }

  /**
   * @brief 箭头操作符（适用于指针句柄）
   */
  template <typename H = Handle>
    requires std::is_pointer_v<H>
  [[nodiscard]] constexpr H operator->() const noexcept {
    return handle_;
  }

  /**
   * @brief 释放所有权，返回原始句柄
   */
  [[nodiscard]] Handle release() noexcept {
    Handle h = handle_;
    handle_ = Traits::invalid();
    return h;
  }

  /**
   * @brief 重置句柄
   */
  void reset(Handle h = Traits::invalid()) noexcept {
    if (handle_ != Traits::invalid()) {
      Traits::close(handle_);
    }
    handle_ = h;
  }

  /**
   * @brief 交换
   */
  void swap(UniqueHandle &other) noexcept { std::swap(handle_, other.handle_); }

private:
  void close() noexcept {
    if (handle_ != Traits::invalid()) {
      Traits::close(handle_);
      handle_ = Traits::invalid();
    }
  }

  Handle handle_;
};

/**
 * @brief 交换两个 UniqueHandle
 */
template <typename Handle, typename Traits>
void swap(UniqueHandle<Handle, Traits> &lhs,
          UniqueHandle<Handle, Traits> &rhs) noexcept {
  lhs.swap(rhs);
}

/**
 * @brief 带自定义删除器的通用唯一指针
 *
 * 比 std::unique_ptr 更轻量，删除器通过模板参数传入。
 */
template <typename T, auto Deleter> class UniquePtrWith {
public:
  constexpr UniquePtrWith() noexcept : ptr_(nullptr) {}
  explicit constexpr UniquePtrWith(T *p) noexcept : ptr_(p) {}

  ~UniquePtrWith() {
    if (ptr_) {
      Deleter(ptr_);
    }
  }

  UniquePtrWith(const UniquePtrWith &) = delete;
  UniquePtrWith &operator=(const UniquePtrWith &) = delete;

  UniquePtrWith(UniquePtrWith &&other) noexcept : ptr_(other.release()) {}
  UniquePtrWith &operator=(UniquePtrWith &&other) noexcept {
    if (this != &other) {
      reset(other.release());
    }
    return *this;
  }

  [[nodiscard]] T *get() const noexcept { return ptr_; }
  [[nodiscard]] T *operator->() const noexcept { return ptr_; }
  [[nodiscard]] T &operator*() const noexcept { return *ptr_; }
  [[nodiscard]] explicit operator bool() const noexcept {
    return ptr_ != nullptr;
  }

  T *release() noexcept {
    T *p = ptr_;
    ptr_ = nullptr;
    return p;
  }

  void reset(T *p = nullptr) noexcept {
    if (ptr_) {
      Deleter(ptr_);
    }
    ptr_ = p;
  }

private:
  T *ptr_;
};

/**
 * @brief Scope Guard - 作用域结束时执行清理函数
 */
template <typename F> class ScopeGuard {
public:
  explicit ScopeGuard(F &&f) noexcept
      : func_(std::forward<F>(f)), active_(true) {}

  ~ScopeGuard() {
    if (active_) {
      func_();
    }
  }

  ScopeGuard(const ScopeGuard &) = delete;
  ScopeGuard &operator=(const ScopeGuard &) = delete;

  ScopeGuard(ScopeGuard &&other) noexcept
      : func_(std::move(other.func_)), active_(other.active_) {
    other.dismiss();
  }

  /**
   * @brief 取消执行清理函数
   */
  void dismiss() noexcept { active_ = false; }

private:
  F func_;
  bool active_;
};

/**
 * @brief 创建 ScopeGuard
 */
template <typename F>
[[nodiscard]] ScopeGuard<std::decay_t<F>> make_scope_guard(F &&f) {
  return ScopeGuard<std::decay_t<F>>(std::forward<F>(f));
}

} // namespace xslot

// 便捷宏：作用域结束时执行代码
#define XSLOT_SCOPE_EXIT                                                       \
  auto XSLOT_CONCAT_(scope_guard_, __LINE__) = ::xslot::make_scope_guard
#define XSLOT_CONCAT_(a, b) XSLOT_CONCAT_IMPL_(a, b)
#define XSLOT_CONCAT_IMPL_(a, b) a##b

#endif // XSLOT_UNIQUE_HANDLE_H
