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
#include <vector>

namespace xslot {

/**
 * @brief 缓冲区写入器
 */
class BufferWriter {
public:
  explicit BufferWriter(std::span<uint8_t> buffer)
      : buffer_(buffer), offset_(0) {}

  /**
   * @brief 写入 POD 类型数据
   */
  template <typename T>
    requires std::is_standard_layout_v<T>
  bool write(const T &value) {
    if (offset_ + sizeof(T) > buffer_.size())
      return false;
    std::memcpy(buffer_.data() + offset_, &value, sizeof(T));
    offset_ += sizeof(T);
    return true;
  }

  /**
   * @brief 写入字节数组
   */
  bool write_bytes(std::span<const uint8_t> data) {
    if (offset_ + data.size() > buffer_.size())
      return false;
    std::memcpy(buffer_.data() + offset_, data.data(), data.size());
    offset_ += data.size();
    return true;
  }

  /**
   * @brief 获取当前偏移量 (已写入字节数)
   */
  size_t offset() const { return offset_; }

  /**
   * @brief 获取剩余空间
   */
  size_t remaining_size() const { return buffer_.size() - offset_; }

  /**
   * @brief 获取剩余缓冲区
   */
  std::span<uint8_t> remaining_span() { return buffer_.subspan(offset_); }

private:
  std::span<uint8_t> buffer_;
  size_t offset_;
};

/**
 * @brief 缓冲区读取器
 */
class BufferReader {
public:
  explicit BufferReader(std::span<const uint8_t> buffer)
      : buffer_(buffer), offset_(0) {}

  /**
   * @brief 读取 POD 类型数据
   */
  template <typename T>
    requires std::is_standard_layout_v<T>
  std::optional<T> read() {
    if (offset_ + sizeof(T) > buffer_.size())
      return std::nullopt;
    T value;
    std::memcpy(&value, buffer_.data() + offset_, sizeof(T));
    offset_ += sizeof(T);
    return value;
  }

  /**
   * @brief 读取字节数组
   */
  bool read_bytes(std::span<uint8_t> out_buffer) {
    if (offset_ + out_buffer.size() > buffer_.size())
      return false;
    std::memcpy(out_buffer.data(), buffer_.data() + offset_, out_buffer.size());
    offset_ += out_buffer.size();
    return true;
  }

  /**
   * @brief 跳过指定字节数
   */
  bool skip(size_t count) {
    if (offset_ + count > buffer_.size())
      return false;
    offset_ += count;
    return true;
  }

  /**
   * @brief 查看前方的字节 (不移动偏移)
   */
  std::optional<uint8_t> peek(size_t lookahead = 0) const {
    if (offset_ + lookahead >= buffer_.size())
      return std::nullopt;
    return buffer_[offset_ + lookahead];
  }

  /**
   * @brief 获取当前偏移量
   */
  size_t offset() const { return offset_; }

  /**
   * @brief 获取剩余数据大小
   */
  size_t remaining_size() const { return buffer_.size() - offset_; }

private:
  std::span<const uint8_t> buffer_;
  size_t offset_;
};

} // namespace xslot

#endif // BUFFER_UTILS_H
