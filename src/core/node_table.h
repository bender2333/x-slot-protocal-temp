/**
 * @file node_table.h
 * @brief X-Slot 节点表类 (C++20)
 *
 * RAII 风格的节点表管理，支持范围遍历。
 */
#ifndef XSLOT_NODE_TABLE_H
#define XSLOT_NODE_TABLE_H

#include "result.h"
#include <cstdint>
#include <optional>
#include <span>
#include <xslot/xslot_types.h>

// HAL 函数声明
extern "C" uint32_t hal_get_timestamp_ms(void);

namespace xslot {

/**
 * @brief 节点信息结构体
 */
struct NodeInfo {
  uint16_t addr = 0;
  uint32_t last_seen = 0;
  int8_t rssi = 0;
  bool online = false;
  uint8_t object_count = 0;

  /**
   * @brief 从 C 结构体转换
   */
  static NodeInfo from_c(const xslot_node_info_t &c) noexcept {
    NodeInfo info;
    info.addr = c.addr;
    info.last_seen = c.last_seen;
    info.rssi = c.rssi;
    info.online = c.online;
    info.object_count = c.object_count;
    return info;
  }

  /**
   * @brief 转换为 C 结构体
   */
  [[nodiscard]] xslot_node_info_t to_c() const noexcept {
    xslot_node_info_t c{};
    c.addr = addr;
    c.last_seen = last_seen;
    c.rssi = rssi;
    c.online = online;
    c.object_count = object_count;
    return c;
  }
};

/**
 * @brief 节点表类
 *
 * 管理网络中的节点信息，支持自动超时检测。
 * 使用固定大小的内部数组，不进行动态内存分配。
 *
 * @tparam MaxNodes 最大节点数量
 */
template <uint8_t MaxNodes = XSLOT_MAX_NODES> class NodeTable {
public:
  using offline_callback = void (*)(uint16_t addr, bool online);

  /**
   * @brief 默认构造
   */
  NodeTable() noexcept : count_(0) {}

  // 禁用拷贝
  NodeTable(const NodeTable &) = delete;
  NodeTable &operator=(const NodeTable &) = delete;

  // 允许移动
  NodeTable(NodeTable &&other) noexcept = default;
  NodeTable &operator=(NodeTable &&other) noexcept = default;

  /**
   * @brief 更新节点 (收到心跳或数据时调用)
   * @param addr 节点地址
   * @param rssi 信号强度
   * @return true=新节点上线或重新上线, false=已存在节点
   */
  bool update(uint16_t addr, int8_t rssi = 0) noexcept {
    uint32_t now = hal_get_timestamp_ms();

    // 查找现有节点
    for (uint8_t i = 0; i < count_; i++) {
      if (entries_[i].addr == addr) {
        entries_[i].last_seen = now;
        entries_[i].rssi = rssi;
        if (!entries_[i].online) {
          entries_[i].online = true;
          return true; // 重新上线
        }
        return false;
      }
    }

    // 新节点
    if (count_ >= MaxNodes) {
      // 表满，尝试替换最老的离线节点
      int oldest_offline = -1;
      uint32_t oldest_time = UINT32_MAX;

      for (uint8_t i = 0; i < count_; i++) {
        if (!entries_[i].online && entries_[i].last_seen < oldest_time) {
          oldest_time = entries_[i].last_seen;
          oldest_offline = i;
        }
      }

      if (oldest_offline >= 0) {
        entries_[oldest_offline] = {addr, now, rssi, true, 0};
        return true;
      }
      return false; // 表满且无可替换
    }

    entries_[count_++] = {addr, now, rssi, true, 0};
    return true;
  }

  /**
   * @brief 检查并标记超时节点
   * @param timeout_ms 超时时间
   * @param callback 离线回调 (可为 nullptr)
   */
  void check_timeout(uint32_t timeout_ms,
                     offline_callback callback = nullptr) noexcept {
    uint32_t now = hal_get_timestamp_ms();

    for (uint8_t i = 0; i < count_; i++) {
      if (entries_[i].online) {
        if (now - entries_[i].last_seen > timeout_ms) {
          entries_[i].online = false;
          if (callback) {
            callback(entries_[i].addr, false);
          }
        }
      }
    }
  }

  /**
   * @brief 检查节点是否在线
   */
  [[nodiscard]] bool is_online(uint16_t addr) const noexcept {
    for (uint8_t i = 0; i < count_; i++) {
      if (entries_[i].addr == addr) {
        return entries_[i].online;
      }
    }
    return false;
  }

  /**
   * @brief 获取节点信息
   */
  [[nodiscard]] std::optional<NodeInfo> get(uint16_t addr) const noexcept {
    for (uint8_t i = 0; i < count_; i++) {
      if (entries_[i].addr == addr) {
        return entries_[i];
      }
    }
    return std::nullopt;
  }

  /**
   * @brief 获取所有节点
   */
  [[nodiscard]] size_t get_all(std::span<NodeInfo> out) const noexcept {
    size_t count = std::min(static_cast<size_t>(count_), out.size());
    for (size_t i = 0; i < count; i++) {
      out[i] = entries_[i];
    }
    return count;
  }

  /**
   * @brief 获取所有节点 (C 结构体)
   */
  [[nodiscard]] size_t
  get_all_c(std::span<xslot_node_info_t> out) const noexcept {
    size_t count = std::min(static_cast<size_t>(count_), out.size());
    for (size_t i = 0; i < count; i++) {
      out[i] = entries_[i].to_c();
    }
    return count;
  }

  /**
   * @brief 获取在线节点数量
   */
  [[nodiscard]] size_t online_count() const noexcept {
    size_t count = 0;
    for (uint8_t i = 0; i < count_; i++) {
      if (entries_[i].online) {
        count++;
      }
    }
    return count;
  }

  /**
   * @brief 获取总节点数量
   */
  [[nodiscard]] size_t size() const noexcept { return count_; }

  /**
   * @brief 检查是否为空
   */
  [[nodiscard]] bool empty() const noexcept { return count_ == 0; }

  /**
   * @brief 获取容量
   */
  [[nodiscard]] static constexpr size_t capacity() noexcept { return MaxNodes; }

  /**
   * @brief 移除节点
   */
  void remove(uint16_t addr) noexcept {
    for (uint8_t i = 0; i < count_; i++) {
      if (entries_[i].addr == addr) {
        // 移动后续元素
        for (uint8_t j = i; j < count_ - 1; j++) {
          entries_[j] = entries_[j + 1];
        }
        count_--;
        return;
      }
    }
  }

  /**
   * @brief 清空节点表
   */
  void clear() noexcept { count_ = 0; }

  // 迭代器支持
  [[nodiscard]] NodeInfo *begin() noexcept { return entries_; }
  [[nodiscard]] NodeInfo *end() noexcept { return entries_ + count_; }
  [[nodiscard]] const NodeInfo *begin() const noexcept { return entries_; }
  [[nodiscard]] const NodeInfo *end() const noexcept {
    return entries_ + count_;
  }

private:
  NodeInfo entries_[MaxNodes]{};
  uint8_t count_;
};

// 默认节点表类型
using DefaultNodeTable = NodeTable<XSLOT_MAX_NODES>;

} // namespace xslot

#endif // XSLOT_NODE_TABLE_H
