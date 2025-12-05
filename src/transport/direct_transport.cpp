/**
 * @file direct_transport.cpp
 * @brief HMI 直连传输层实现 (Modern C++)
 *
 * 纯串口透传，直接解析 X-Slot 帧。
 */
#include "../core/xslot_protocol.h"
#include "transport.h"
#include <cstring>

/* HAL 函数声明 */
extern "C" {
void *hal_serial_open(const char *port, uint32_t baudrate);
void hal_serial_close(void *handle);
int hal_serial_write(void *handle, const uint8_t *data, uint16_t len);
int hal_serial_read(void *handle, uint8_t *data, uint16_t max_len,
                    uint32_t timeout_ms);
uint32_t hal_get_timestamp_ms(void);
}

namespace xslot {

namespace {
constexpr uint16_t RX_BUFFER_SIZE = 256;
} // namespace

/**
 * @brief HMI 直连传输层实现
 */
class DirectTransport final : public ITransport {
public:
  explicit DirectTransport(const xslot_config_t &config)
      : config_(config), serial_(nullptr), running_(false), rx_len_(0) {}

  ~DirectTransport() override { stop(); }

  VoidResult start() override {
    if (running_) {
      return VoidResult::ok();
    }

    serial_ =
        hal_serial_open(config_.uart_port,
                        config_.uart_baudrate ? config_.uart_baudrate : 115200);
    if (!serial_) {
      return VoidResult::error(Error::NoDevice);
    }

    running_ = true;
    rx_len_ = 0;

    return VoidResult::ok();
  }

  void stop() override {
    if (!running_) {
      return;
    }

    running_ = false;

    if (serial_) {
      hal_serial_close(serial_);
      serial_ = nullptr;
    }
  }

  VoidResult send(std::span<const uint8_t> data) override {
    if (!serial_ || data.empty()) {
      return VoidResult::error(Error::InvalidParam);
    }

    int ret = hal_serial_write(serial_, data.data(),
                               static_cast<uint16_t>(data.size()));
    return (ret == static_cast<int>(data.size()))
               ? VoidResult::ok()
               : VoidResult::error(Error::SendFailed);
  }

  VoidResult probe() override {
    /* 打开串口 */
    void *serial =
        hal_serial_open(config_.uart_port,
                        config_.uart_baudrate ? config_.uart_baudrate : 115200);
    if (!serial) {
      return VoidResult::error(Error::NoDevice);
    }

    /* 等待并尝试接收 X-Slot 帧 */
    uint8_t buffer[32];
    uint32_t start = hal_get_timestamp_ms();
    bool found_sync = false;

    while (hal_get_timestamp_ms() - start < 500) {
      int ret = hal_serial_read(serial, buffer, sizeof(buffer), 50);
      if (ret > 0) {
        for (int i = 0; i < ret; i++) {
          if (buffer[i] == XSLOT_SYNC_BYTE) {
            found_sync = true;
            break;
          }
        }
        if (found_sync)
          break;
      }
    }

    hal_serial_close(serial);

    /* 如果收到同步字节，认为是 HMI 模式 */
    return found_sync ? VoidResult::ok() : VoidResult::error(Error::NoDevice);
  }

  VoidResult configure(uint8_t /*cell_id*/, int8_t /*power_dbm*/) override {
    /* Direct 模式不支持无线配置 */
    return VoidResult::ok();
  }

  void set_receive_callback(TransportReceiveCallback callback) override {
    recv_cb_ = std::move(callback);
  }

  bool is_running() const override { return running_; }

  /**
   * @brief 处理接收数据 (需要外部调用 poll)
   */
  void poll() {
    if (!running_ || !serial_) {
      return;
    }

    /* 读取串口数据 */
    uint16_t space = RX_BUFFER_SIZE - rx_len_;
    if (space > 0) {
      int ret = hal_serial_read(serial_, rx_buffer_ + rx_len_, space, 0);
      if (ret > 0) {
        rx_len_ += static_cast<uint16_t>(ret);
      }
    }

    /* 尝试解析帧 */
    try_parse_frame();
  }

private:
  void try_parse_frame() {
    while (rx_len_ >= XSLOT_FRAME_MIN_SIZE) {
      /* 查找同步字节 */
      uint16_t sync_pos = 0;
      while (sync_pos < rx_len_ && rx_buffer_[sync_pos] != XSLOT_SYNC_BYTE) {
        sync_pos++;
      }

      /* 移除同步字节之前的数据 */
      if (sync_pos > 0) {
        std::memmove(rx_buffer_, rx_buffer_ + sync_pos, rx_len_ - sync_pos);
        rx_len_ -= sync_pos;
      }

      if (rx_len_ < XSLOT_FRAME_MIN_SIZE) {
        break;
      }

      /* 获取数据长度 */
      uint8_t data_len = rx_buffer_[XSLOT_OFFSET_LEN];
      if (data_len > XSLOT_MAX_DATA_LEN) {
        /* 无效长度，跳过同步字节 */
        std::memmove(rx_buffer_, rx_buffer_ + 1, rx_len_ - 1);
        rx_len_--;
        continue;
      }

      uint16_t frame_size = xslot_frame_total_size(data_len);
      if (rx_len_ < frame_size) {
        /* 数据不完整，等待更多数据 */
        break;
      }

      /* 验证 CRC */
      if (xslot_frame_verify_crc(rx_buffer_, frame_size)) {
        /* 帧有效，回调上层 */
        if (recv_cb_) {
          recv_cb_({rx_buffer_, frame_size});
        }

        /* 移除已处理的帧 */
        std::memmove(rx_buffer_, rx_buffer_ + frame_size, rx_len_ - frame_size);
        rx_len_ -= frame_size;
      } else {
        /* CRC 错误，跳过同步字节 */
        std::memmove(rx_buffer_, rx_buffer_ + 1, rx_len_ - 1);
        rx_len_--;
      }
    }
  }

  xslot_config_t config_;
  void *serial_;
  bool running_;

  TransportReceiveCallback recv_cb_;

  uint8_t rx_buffer_[RX_BUFFER_SIZE];
  uint16_t rx_len_;
};

std::unique_ptr<ITransport>
create_direct_transport(const xslot_config_t &config) {
  return std::make_unique<DirectTransport>(config);
}

} // namespace xslot
