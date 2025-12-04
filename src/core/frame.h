/**
 * @file frame.h
 * @brief X-Slot 协议帧类 (C++20)
 *
 * 封装协议帧的构建、编码和解码操作。
 */
#ifndef XSLOT_FRAME_H
#define XSLOT_FRAME_H

#include "buffer_utils.h"
#include "result.h"
#include "xslot_protocol.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <xslot/xslot_types.h>

namespace xslot {

/**
 * @brief 命令类型枚举类
 */
enum class Command : uint8_t {
  Ping = XSLOT_CMD_PING,
  Pong = XSLOT_CMD_PONG,
  Report = XSLOT_CMD_REPORT,
  Query = XSLOT_CMD_QUERY,
  Response = XSLOT_CMD_RESPONSE,
  Write = XSLOT_CMD_WRITE,
  WriteAck = XSLOT_CMD_WRITE_ACK,
};

/**
 * @brief 帧格式常量
 */
namespace frame_const {
constexpr uint8_t SYNC_BYTE = XSLOT_SYNC_BYTE;
constexpr uint8_t HEADER_SIZE = 8; // SYNC(1)+FROM(2)+TO(2)+SEQ(1)+CMD(1)+LEN(1)
constexpr uint8_t CRC_SIZE = 2;
constexpr uint8_t MIN_SIZE = HEADER_SIZE + CRC_SIZE;
constexpr uint8_t MAX_DATA_LEN = XSLOT_MAX_DATA_LEN;
constexpr uint16_t MAX_SIZE = HEADER_SIZE + MAX_DATA_LEN + CRC_SIZE;

// 字段偏移
constexpr uint8_t OFFSET_SYNC = 0;
constexpr uint8_t OFFSET_FROM = 1;
constexpr uint8_t OFFSET_TO = 3;
constexpr uint8_t OFFSET_SEQ = 5;
constexpr uint8_t OFFSET_CMD = 6;
constexpr uint8_t OFFSET_LEN = 7;
constexpr uint8_t OFFSET_DATA = 8;
} // namespace frame_const

/**
 * @brief CRC16-CCITT 计算
 */
uint16_t calculate_crc16(std::span<const uint8_t> data) noexcept;

/**
 * @brief 协议帧类
 *
 * RAII 风格的帧封装，提供类型安全的访问和构建方法。
 */
class Frame {
public:
  /**
   * @brief 默认构造 - 创建空帧
   */
  Frame() noexcept { clear(); }

  /**
   * @brief 从参数构造帧
   */
  Frame(uint16_t from, uint16_t to, uint8_t seq, Command cmd) noexcept
      : sync_(frame_const::SYNC_BYTE), from_(from), to_(to), seq_(seq),
        cmd_(static_cast<uint8_t>(cmd)), len_(0), crc_(0) {
    std::memset(data_, 0, sizeof(data_));
  }

  /**
   * @brief 从 C 结构体构造
   */
  explicit Frame(const xslot_frame_t &cframe) noexcept
      : sync_(cframe.sync), from_(cframe.from), to_(cframe.to),
        seq_(cframe.seq), cmd_(cframe.cmd), len_(cframe.len), crc_(cframe.crc) {
    std::memcpy(data_, cframe.data, sizeof(data_));
  }

  // 访问器
  [[nodiscard]] uint8_t sync() const noexcept { return sync_; }
  [[nodiscard]] uint16_t from() const noexcept { return from_; }
  [[nodiscard]] uint16_t to() const noexcept { return to_; }
  [[nodiscard]] uint8_t seq() const noexcept { return seq_; }
  [[nodiscard]] uint8_t cmd() const noexcept { return cmd_; }
  [[nodiscard]] Command command() const noexcept {
    return static_cast<Command>(cmd_);
  }
  [[nodiscard]] uint8_t len() const noexcept { return len_; }
  [[nodiscard]] uint16_t crc() const noexcept { return crc_; }

  // 数据访问
  [[nodiscard]] const uint8_t *data() const noexcept { return data_; }
  [[nodiscard]] uint8_t *data() noexcept { return data_; }
  [[nodiscard]] std::span<const uint8_t> data_span() const noexcept {
    return {data_, len_};
  }
  [[nodiscard]] std::span<uint8_t> data_span() noexcept {
    return {data_, len_};
  }

  // 设置器
  void set_from(uint16_t addr) noexcept { from_ = addr; }
  void set_to(uint16_t addr) noexcept { to_ = addr; }
  void set_seq(uint8_t seq) noexcept { seq_ = seq; }
  void set_cmd(uint8_t cmd) noexcept { cmd_ = cmd; }
  void set_command(Command cmd) noexcept { cmd_ = static_cast<uint8_t>(cmd); }

  /**
   * @brief 设置载荷数据
   * @return true 成功, false 数据过长
   */
  bool set_data(std::span<const uint8_t> payload) noexcept {
    if (payload.size() > frame_const::MAX_DATA_LEN) {
      return false;
    }
    std::memcpy(data_, payload.data(), payload.size());
    len_ = static_cast<uint8_t>(payload.size());
    return true;
  }

  /**
   * @brief 追加数据到载荷
   */
  bool append_data(std::span<const uint8_t> payload) noexcept {
    if (len_ + payload.size() > frame_const::MAX_DATA_LEN) {
      return false;
    }
    std::memcpy(data_ + len_, payload.data(), payload.size());
    len_ += static_cast<uint8_t>(payload.size());
    return true;
  }

  /**
   * @brief 设置载荷长度 (用于直接写入 data_ 后)
   */
  void set_len(uint8_t len) noexcept {
    len_ = (len <= frame_const::MAX_DATA_LEN) ? len : frame_const::MAX_DATA_LEN;
  }

  /**
   * @brief 获取帧总长度
   */
  [[nodiscard]] uint16_t total_size() const noexcept {
    return frame_const::HEADER_SIZE + len_ + frame_const::CRC_SIZE;
  }

  /**
   * @brief 清空帧
   */
  void clear() noexcept {
    sync_ = frame_const::SYNC_BYTE;
    from_ = 0;
    to_ = 0;
    seq_ = 0;
    cmd_ = 0;
    len_ = 0;
    crc_ = 0;
    std::memset(data_, 0, sizeof(data_));
  }

  /**
   * @brief 编码帧到缓冲区
   * @param buffer 输出缓冲区
   * @return 编码后的字节数，失败返回错误
   */
  [[nodiscard]] Result<size_t>
  encode(std::span<uint8_t> buffer) const noexcept {
    const uint16_t required_size = total_size();
    if (buffer.size() < required_size) {
      return Result<size_t>::error(Error::NoMemory);
    }

    BufferWriter writer(buffer);

    writer.write(sync_);
    writer.write(from_);
    writer.write(to_);
    writer.write(seq_);
    writer.write(cmd_);
    writer.write(len_);

    if (len_ > 0) {
      writer.write_bytes({data_, len_});
    }

    // 计算并写入 CRC
    const uint16_t crc =
        calculate_crc16(buffer.first(frame_const::HEADER_SIZE + len_));
    writer.write(crc);

    return Result<size_t>::ok(writer.offset());
  }

  /**
   * @brief 从缓冲区解码帧
   * @param buffer 输入缓冲区
   * @return 解码后的帧
   */
  [[nodiscard]] static Result<Frame>
  decode(std::span<const uint8_t> buffer) noexcept {
    if (buffer.size() < frame_const::MIN_SIZE) {
      return Result<Frame>::error(Error::InvalidParam);
    }

    BufferReader reader(buffer);

    auto sync = reader.read<uint8_t>();
    if (!sync || *sync != frame_const::SYNC_BYTE) {
      return Result<Frame>::error(Error::InvalidParam);
    }

    auto from = reader.read<uint16_t>();
    auto to = reader.read<uint16_t>();
    auto seq = reader.read<uint8_t>();
    auto cmd = reader.read<uint8_t>();
    auto len = reader.read<uint8_t>();

    if (!from || !to || !seq || !cmd || !len) {
      return Result<Frame>::error(Error::InvalidParam);
    }

    if (*len > frame_const::MAX_DATA_LEN) {
      return Result<Frame>::error(Error::InvalidParam);
    }

    // 验证缓冲区长度
    const uint16_t expected_size =
        frame_const::HEADER_SIZE + *len + frame_const::CRC_SIZE;
    if (buffer.size() < expected_size) {
      return Result<Frame>::error(Error::InvalidParam);
    }

    Frame frame;
    frame.sync_ = *sync;
    frame.from_ = *from;
    frame.to_ = *to;
    frame.seq_ = *seq;
    frame.cmd_ = *cmd;
    frame.len_ = *len;

    if (*len > 0) {
      reader.read_bytes({frame.data_, *len});
    }

    auto crc = reader.read<uint16_t>();
    if (!crc) {
      return Result<Frame>::error(Error::InvalidParam);
    }
    frame.crc_ = *crc;

    // 验证 CRC
    const uint16_t calc_crc =
        calculate_crc16(buffer.first(frame_const::HEADER_SIZE + *len));
    if (calc_crc != frame.crc_) {
      return Result<Frame>::error(Error::CrcError);
    }

    return Result<Frame>::ok(std::move(frame));
  }

  /**
   * @brief 验证缓冲区中帧的 CRC
   */
  [[nodiscard]] static bool
  verify_crc(std::span<const uint8_t> buffer) noexcept {
    if (buffer.size() < frame_const::MIN_SIZE) {
      return false;
    }

    const uint8_t data_len = buffer[frame_const::OFFSET_LEN];
    if (data_len > frame_const::MAX_DATA_LEN) {
      return false;
    }

    const uint16_t expected_size =
        frame_const::HEADER_SIZE + data_len + frame_const::CRC_SIZE;
    if (buffer.size() < expected_size) {
      return false;
    }

    const uint16_t calc_crc =
        calculate_crc16(buffer.first(frame_const::HEADER_SIZE + data_len));
    const uint16_t frame_crc =
        buffer[frame_const::HEADER_SIZE + data_len] |
        (buffer[frame_const::HEADER_SIZE + data_len + 1] << 8);

    return calc_crc == frame_crc;
  }

  /**
   * @brief 转换为 C 结构体
   */
  [[nodiscard]] xslot_frame_t to_c_frame() const noexcept {
    xslot_frame_t cframe{};
    cframe.sync = sync_;
    cframe.from = from_;
    cframe.to = to_;
    cframe.seq = seq_;
    cframe.cmd = cmd_;
    cframe.len = len_;
    cframe.crc = crc_;
    std::memcpy(cframe.data, data_, sizeof(cframe.data));
    return cframe;
  }

  /**
   * @brief 获取载荷的 BufferWriter
   */
  [[nodiscard]] BufferWriter payload_writer() noexcept {
    return BufferWriter({data_, frame_const::MAX_DATA_LEN});
  }

  /**
   * @brief 获取载荷的 BufferReader
   */
  [[nodiscard]] BufferReader payload_reader() const noexcept {
    return BufferReader({data_, len_});
  }

private:
  uint8_t sync_;
  uint16_t from_;
  uint16_t to_;
  uint8_t seq_;
  uint8_t cmd_;
  uint8_t len_;
  uint8_t data_[frame_const::MAX_DATA_LEN];
  uint16_t crc_;
};

/**
 * @brief 帧构建器 - 流式构建帧
 */
class FrameBuilder {
public:
  explicit FrameBuilder(uint16_t from = 0) noexcept
      : frame_(from, 0, 0, Command::Ping), payload_offset_(0) {}

  FrameBuilder &from(uint16_t addr) noexcept {
    frame_.set_from(addr);
    return *this;
  }

  FrameBuilder &to(uint16_t addr) noexcept {
    frame_.set_to(addr);
    return *this;
  }

  FrameBuilder &seq(uint8_t s) noexcept {
    frame_.set_seq(s);
    return *this;
  }

  FrameBuilder &cmd(Command c) noexcept {
    frame_.set_command(c);
    return *this;
  }

  FrameBuilder &cmd(uint8_t c) noexcept {
    frame_.set_cmd(c);
    return *this;
  }

  /**
   * @brief 写入载荷数据
   */
  template <typename T>
    requires std::is_standard_layout_v<T>
  FrameBuilder &write(const T &value) noexcept {
    if (payload_offset_ + sizeof(T) <= frame_const::MAX_DATA_LEN) {
      std::memcpy(frame_.data() + payload_offset_, &value, sizeof(T));
      payload_offset_ += sizeof(T);
      frame_.set_len(static_cast<uint8_t>(payload_offset_));
    }
    return *this;
  }

  /**
   * @brief 写入字节数组
   */
  FrameBuilder &write_bytes(std::span<const uint8_t> data) noexcept {
    if (payload_offset_ + data.size() <= frame_const::MAX_DATA_LEN) {
      std::memcpy(frame_.data() + payload_offset_, data.data(), data.size());
      payload_offset_ += data.size();
      frame_.set_len(static_cast<uint8_t>(payload_offset_));
    }
    return *this;
  }

  /**
   * @brief 构建帧
   */
  [[nodiscard]] Frame build() noexcept { return std::move(frame_); }

  /**
   * @brief 重置构建器
   */
  void reset() noexcept {
    frame_.clear();
    payload_offset_ = 0;
  }

private:
  Frame frame_;
  size_t payload_offset_;
};

} // namespace xslot

#endif // XSLOT_FRAME_H
