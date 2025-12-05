/**
 * @file manager.cpp
 * @brief X-Slot 协议栈管理器实现
 */
#include "manager.h"
#include <cstring>
#include <xslot/xslot_error.h>

// HAL 函数声明
extern "C" {
uint32_t hal_get_timestamp_ms(void);
void hal_sleep_ms(uint32_t ms);
}

namespace xslot {

// ============================================================================
// 构造与析构
// ============================================================================

Manager::Manager(const xslot_config_t &config)
    : config_(config), mode_(RunMode::None), running_(false), seq_(0),
      transport_(nullptr) {}

Manager::~Manager() { stop(); }

// ============================================================================
// 启动与停止
// ============================================================================

VoidResult Manager::start() {
  if (running_) {
    return VoidResult::ok();
  }

  // 检测并创建传输层
  transport_ = detect_and_create_transport();
  if (!transport_) {
    mode_ = RunMode::None;
    return VoidResult::error(Error::NoDevice);
  }

  // 设置接收回调
  transport_->set_receive_callback(
      [this](std::span<const uint8_t> data) { on_frame_received(data); });

  // 启动传输层
  auto result = transport_->start();
  if (!result) {
    transport_.reset();
    mode_ = RunMode::None;
    return result;
  }

  running_ = true;
  return VoidResult::ok();
}

void Manager::stop() {
  if (!running_) {
    return;
  }

  running_ = false;
  if (transport_) {
    transport_->stop();
    transport_.reset();
  }
}

// ============================================================================
// 业务操作
// ============================================================================

VoidResult Manager::send_frame(const Frame &frame) {
  if (!running_ || !transport_) {
    return VoidResult::error(Error::NotInitialized);
  }

  uint8_t buffer[frame_const::MAX_SIZE];
  auto encode_result = frame.encode({buffer, sizeof(buffer)});
  if (!encode_result) {
    return VoidResult::error(encode_result.error());
  }

  return transport_->send({buffer, encode_result.value()});
}

VoidResult Manager::report(std::span<const xslot_bacnet_object_t> objects) {
  if (objects.empty()) {
    return VoidResult::error(Error::InvalidParam);
  }

  auto frame_result = message::build_report(config_.local_addr, XSLOT_ADDR_HUB,
                                            next_seq(), objects,
                                            true // 使用增量格式
  );

  if (!frame_result) {
    return VoidResult::error(frame_result.error());
  }

  return send_frame(frame_result.value());
}

VoidResult Manager::write(uint16_t target, const xslot_bacnet_object_t &obj) {
  auto frame_result =
      message::build_write(config_.local_addr, target, next_seq(), obj);

  if (!frame_result) {
    return VoidResult::error(frame_result.error());
  }

  return send_frame(frame_result.value());
}

VoidResult Manager::query(uint16_t target,
                          std::span<const uint16_t> object_ids) {
  if (object_ids.empty()) {
    return VoidResult::error(Error::InvalidParam);
  }

  auto frame_result =
      message::build_query(config_.local_addr, target, next_seq(), object_ids);

  if (!frame_result) {
    return VoidResult::error(frame_result.error());
  }

  return send_frame(frame_result.value());
}

VoidResult Manager::ping(uint16_t target) {
  Frame frame = message::build_ping(config_.local_addr, target, next_seq());
  return send_frame(frame);
}

// ============================================================================
// 节点管理
// ============================================================================

void Manager::check_node_timeout(uint32_t timeout_ms) {
  node_table_.check_timeout(timeout_ms, [](uint16_t addr, bool online) {
    // 这里可以调用回调，但由于是静态 lambda，无法访问 this
    // 实际使用时需要通过其他方式传递上下文
    (void)addr;
    (void)online;
  });
}

// ============================================================================
// 运行时配置
// ============================================================================

VoidResult Manager::update_config(uint8_t cell_id, int8_t power_dbm) {
  config_.cell_id = cell_id;
  config_.power_dbm = power_dbm;

  if (transport_ && mode_ == RunMode::Wireless) {
    return transport_->configure(cell_id, power_dbm);
  }

  return VoidResult::ok();
}

// ============================================================================
// 内部方法
// ============================================================================

void Manager::on_frame_received(std::span<const uint8_t> data) {
  auto frame_result = Frame::decode(data);
  if (!frame_result) {
    return;
  }

  Frame &frame = frame_result.value();

  // 检查目标地址
  if (frame.to() != config_.local_addr && frame.to() != XSLOT_ADDR_BROADCAST) {
    return;
  }

  handle_frame(frame);
}

void Manager::handle_frame(const Frame &frame) {
  // 更新节点表
  bool is_new = node_table_.update(frame.from(), 0);
  if (is_new) {
    if (node_cb_) {
      node_cb_(frame.from(), true);
    }
    if (c_node_cb_) {
      c_node_cb_(frame.from(), true);
    }
  }

  // 根据命令类型处理
  switch (frame.command()) {
  case Command::Ping: {
    // 回复 PONG
    Frame pong =
        message::build_pong(config_.local_addr, frame.from(), frame.seq());
    (void)send_frame(pong);
    break;
  }

  case Command::Pong:
    // 心跳响应，节点表已更新
    break;

  case Command::Report: {
    // 数据上报 (汇聚节点接收)
    xslot_bacnet_object_t objects[16];
    auto result = message::parse_report(frame, {objects, 16});
    if (result && result.value() > 0) {
      size_t count = result.value();
      if (report_cb_) {
        report_cb_(frame.from(), {objects, count});
      }
      if (c_report_cb_) {
        c_report_cb_(frame.from(), objects, static_cast<uint8_t>(count));
      }
    }
    break;
  }

  case Command::Write: {
    // 写入请求 (边缘节点接收)
    xslot_bacnet_object_t obj;
    auto result = message::parse_write(frame, obj);
    if (result) {
      if (write_cb_) {
        write_cb_(frame.from(), obj);
      }
      if (c_write_cb_) {
        c_write_cb_(frame.from(), &obj);
      }
    }
    // 回复 ACK
    Frame ack = message::build_write_ack(config_.local_addr, frame.from(),
                                         frame.seq(), XSLOT_OK);
    (void)send_frame(ack);
    break;
  }

  case Command::Response:
  case Command::Query:
    // 原始数据回调
    if (data_cb_) {
      data_cb_(frame.from(), frame.data_span());
    }
    if (c_data_cb_) {
      c_data_cb_(frame.from(), frame.data(), frame.len());
    }
    break;

  default:
    break;
  }
}

std::unique_ptr<ITransport> Manager::detect_and_create_transport() {
  // 尝试 TPMesh 传输层
  auto tpmesh = create_tpmesh_transport(config_);
  if (tpmesh && tpmesh->probe()) {
    mode_ = RunMode::Wireless;
    return tpmesh;
  }

  // 尝试 Direct 传输层
  auto direct = create_direct_transport(config_);
  if (direct && direct->probe()) {
    mode_ = RunMode::Hmi;
    return direct;
  }

  // 创建 Null 传输层
  mode_ = RunMode::None;
  return create_null_transport();
}

} // namespace xslot
