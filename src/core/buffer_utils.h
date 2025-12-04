/**
 * @file buffer_utils.h
 * @brief 缓冲区操作工具类 (C++20)
 */
#ifndef BUFFER_UTILS_H
#define BUFFER_UTILS_H

#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <type_traits>

namespace xslot {

/**
 * @brief 字节序转换工具
 */
namespace endian {

/**
 * @brief 将主机字节序转换为小端
 */
template <typename T>
  requires std::is_integral_v<T>
constexpr T to_little_endian(T value) noexcept {
  // 假设主机是小端序（大多数嵌入式平台）
  // 如果需要支持大端，在这里添加检测和转换
  return value;
}

/**
 * @brief 将小端字节序转换为主机字节序
 */
template <typename T>
  requires std::is_integral_v<T>
constexpr T from_little_endian(T value) noexcept {
  return value;
}

} // namespace endian

/**
 * @brief 缓冲区写入器
 *
 * 提供安全的、类型感知的缓冲区写入操作。
 * 所有多字节值按小端序写入。
 */
class BufferWriter {
public:
  /**
   * @brief 构造写入器
   * @param buffer 目标缓冲区
   */
  explicit BufferWriter(std::span<uint8_t> buffer) noexcept
      : buffer_(buffer), offset_(0) {}

  /**
   * @brief 写入 POD 类型数据 (小端序)
   * @return true 成功, false 空间不足
   */
  template <typename T>
    requires std::is_standard_layout_v<T>
  bool write(const T &value) noexcept {
    if (offset_ + sizeof(T) > buffer_.size()) {
      return false;
    }
    std::memcpy(buffer_.data() + offset_, &value, sizeof(T));
    offset_ += sizeof(T);
    return true;
  }

  /**
   * @brief 写入单个字节
   */
  bool write_byte(uint8_t byte) noexcept {
    if (offset_ >= buffer_.size()) {
      return false;
    }
    buffer_[offset_++] = byte;
    return true;
  }

  /**
   * @brief 写入字节数组
   */
  bool write_bytes(std::span<const uint8_t> data) noexcept {
    if (offset_ + data.size() > buffer_.size()) {
      return false;
    }
    std::memcpy(buffer_.data() + offset_, data.data(), data.size());
    offset_ += data.size();
    return true;
  }

  /**
   * @brief 填充指定数量的字节
   */
  bool fill(uint8_t value, size_t count) noexcept {
    if (offset_ + count > buffer_.size()) {
      return false;
    }
    std::memset(buffer_.data() + offset_, value, count);
    offset_ += count;
    return true;
  }

  /**
   * @brief 获取当前偏移量 (已写入字节数)
   */
  [[nodiscard]] size_t offset() const noexcept { return offset_; }

  /**
   * @brief 获取剩余空间
   */
  [[nodiscard]] size_t remaining_size() const noexcept {
    return buffer_.size() - offset_;
  }

  /**
   * @brief 获取已写入的数据
   */
  [[nodiscard]] std::span<const uint8_t> written() const noexcept {
    return buffer_.first(offset_);
  }

  /**
   * @brief 获取剩余缓冲区
   */
  [[nodiscard]] std::span<uint8_t> remaining_span() noexcept {
    return buffer_.subspan(offset_);
  }

  /**
   * @brief 回退指定字节数
   */
  bool rewind(size_t count) noexcept {
    if (count > offset_) {
      return false;
    }
    offset_ -= count;
    return true;
  }

  /**
   * @brief 重置到起始位置
   */
  void reset() noexcept { offset_ = 0; }

private:
  std::span<uint8_t> buffer_;
  size_t offset_;
};

/**
 * @brief 缓冲区读取器
 *
 * 提供安全的、类型感知的缓冲区读取操作。
 * 所有多字节值按小端序读取。
 */
class BufferReader {
public:
  /**
   * @brief 构造读取器
   * @param buffer 源缓冲区
   */
  explicit BufferReader(std::span<const uint8_t> buffer) noexcept
      : buffer_(buffer), offset_(0) {}

  /**
   * @brief 读取 POD 类型数据 (小端序)
   * @return 读取的值，失败返回 nullopt
   */
  template <typename T>
    requires std::is_standard_layout_v<T>
  [[nodiscard]] std::optional<T> read() noexcept {
    if (offset_ + sizeof(T) > buffer_.size()) {
      return std::nullopt;
    }
    T value;
    std::memcpy(&value, buffer_.data() + offset_, sizeof(T));
    offset_ += sizeof(T);
    return value;
  }

  /**
   * @brief 读取单个字节
   */
  [[nodiscard]] std::optional<uint8_t> read_byte() noexcept {
    if (offset_ >= buffer_.size()) {
      return std::nullopt;
    }
    return buffer_[offset_++];
  }

  /**
   * @brief 读取字节数组到输出缓冲区
   */
  bool read_bytes(std::span<uint8_t> out_buffer) noexcept {
    if (offset_ + out_buffer.size() > buffer_.size()) {
      return false;
    }
    std::memcpy(out_buffer.data(), buffer_.data() + offset_, out_buffer.size());
    offset_ += out_buffer.size();
    return true;
  }

  /**
   * @brief 获取剩余数据的视图 (不复制)
   */
  [[nodiscard]] std::span<const uint8_t> read_span(size_t count) noexcept {
    if (offset_ + count > buffer_.size()) {
      return {};
    }
    auto result = buffer_.subspan(offset_, count);
    offset_ += count;
    return result;
  }

  /**
   * @brief 跳过指定字节数
   */
  bool skip(size_t count) noexcept {
    if (offset_ + count > buffer_.size()) {
      return false;
    }
    offset_ += count;
    return true;
  }

  /**
   * @brief 查看前方的字节 (不移动偏移)
   */
  [[nodiscard]] std::optional<uint8_t>
  peek(size_t lookahead = 0) const noexcept {
    if (offset_ + lookahead >= buffer_.size()) {
      return std::nullopt;
    }
    return buffer_[offset_ + lookahead];
  }

  /**
   * @brief 查看前方的多个字节 (不移动偏移)
   */
  [[nodiscard]] std::span<const uint8_t>
  peek_span(size_t count) const noexcept {
    if (offset_ + count > buffer_.size()) {
      return {};
    }
    return buffer_.subspan(offset_, count);
  }

  /**
   * @brief 获取当前偏移量
   */
  [[nodiscard]] size_t offset() const noexcept { return offset_; }

  /**
   * @brief 获取剩余数据大小
   */
  [[nodiscard]] size_t remaining_size() const noexcept {
    return buffer_.size() - offset_;
  }

  /**
   * @brief 检查是否还有更多数据
   */
  [[nodiscard]] bool has_more() const noexcept {
    return offset_ < buffer_.size();
  }

  /**
   * @brief 获取剩余数据视图
   */
  [[nodiscard]] std::span<const uint8_t> remaining() const noexcept {
    return buffer_.subspan(offset_);
  }

  /**
   * @brief 回退指定字节数
   */
  bool rewind(size_t count) noexcept {
    if (count > offset_) {
      return false;
    }
    offset_ -= count;
    return true;
  }

  /**
   * @brief 重置到起始位置
   */
  void reset() noexcept { offset_ = 0; }

  /**
   * @brief 设置读取位置
   */
  bool seek(size_t pos) noexcept {
    if (pos > buffer_.size()) {
      return false;
    }
    offset_ = pos;
    return true;
  }

private:
  std::span<const uint8_t> buffer_;
  size_t offset_;
};

/**
 * @brief 静态缓冲区 - 固定大小的栈上缓冲区
 *
 * @tparam N 缓冲区大小
 */
template <size_t N> class StaticBuffer {
public:
  constexpr StaticBuffer() noexcept : size_(0) {}

  /**
   * @brief 获取写入器
   */
  [[nodiscard]] BufferWriter writer() noexcept {
    return BufferWriter({data_, N});
  }

  /**
   * @brief 获取读取器
   */
  [[nodiscard]] BufferReader reader() const noexcept {
    return BufferReader({data_, size_});
  }

  /**
   * @brief 获取数据指针
   */
  [[nodiscard]] uint8_t *data() noexcept { return data_; }
  [[nodiscard]] const uint8_t *data() const noexcept { return data_; }

  /**
   * @brief 获取当前数据大小
   */
  [[nodiscard]] size_t size() const noexcept { return size_; }

  /**
   * @brief 设置当前数据大小
   */
  void set_size(size_t s) noexcept { size_ = (s <= N) ? s : N; }

  /**
   * @brief 获取容量
   */
  [[nodiscard]] static constexpr size_t capacity() noexcept { return N; }

  /**
   * @brief 获取 span 视图
   */
  [[nodiscard]] std::span<uint8_t> span() noexcept { return {data_, N}; }
  [[nodiscard]] std::span<const uint8_t> span() const noexcept {
    return {data_, size_};
  }

  /**
   * @brief 清空缓冲区
   */
  void clear() noexcept { size_ = 0; }

private:
  uint8_t data_[N]{};
  size_t size_;
};

} // namespace xslot

#endif // BUFFER_UTILS_H
