// =============================================================================
// X-Slot 无线互联协议 - 最小可运行原型
// Platform: Windows (可移植到 GD32/FreeRTOS)
// =============================================================================

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>


// =============================================================================
// 1. 协议定义
// =============================================================================

#define XSLOT_SYNC_BYTE 0xAA
#define XSLOT_MAX_DATA_LEN 128
#define XSLOT_HEADER_SIZE 7
#define XSLOT_CRC_SIZE 2
#define XSLOT_MAX_FRAME_SIZE                                                   \
  (XSLOT_HEADER_SIZE + XSLOT_MAX_DATA_LEN + XSLOT_CRC_SIZE)

// 地址定义
#define ADDR_HUB 0xFFFE      // 汇聚节点（网关）
#define ADDR_HMI 0xFF00      // HMI固定地址
#define ADDR_NODE_MIN 0xFFBE // 边缘节点最小地址
#define ADDR_NODE_MAX 0xFFFD // 边缘节点最大地址

// 消息类型
enum XSlotCommand : uint8_t {
  CMD_PING = 0x01,     // 心跳请求
  CMD_PONG = 0x02,     // 心跳响应
  CMD_REPORT = 0x10,   // 数据上报（边缘→汇聚）
  CMD_QUERY = 0x11,    // 数据查询（HMI→汇聚）
  CMD_RESPONSE = 0x12, // 查询响应（汇聚→HMI）
  CMD_WRITE = 0x20,    // 远程写入
  CMD_WRITE_ACK = 0x21 // 写入确认
};

// 插槽模式
enum SlotMode { SLOT_EMPTY = 0, SLOT_WIRELESS = 1, SLOT_HMI = 2 };

// 消息帧结构
struct XSlotFrame {
  uint8_t sync;  // 0xAA
  uint16_t from; // 源地址
  uint16_t to;   // 目标地址
  uint8_t seq;   // 序列号
  uint8_t cmd;   // 命令类型
  uint8_t len;   // 数据长度
  uint8_t data[XSLOT_MAX_DATA_LEN];
  uint16_t crc; // CRC16校验

  XSlotFrame()
      : sync(XSLOT_SYNC_BYTE), from(0), to(0), seq(0), cmd(0), len(0), crc(0) {
    memset(data, 0, sizeof(data));
  }
};

// 节点信息
struct NodeInfo {
  uint16_t addr;
  uint32_t last_seen; // 最后心跳时间戳(ms)
  uint8_t rssi;
  bool online;
  std::vector<uint8_t> cached_data; // 缓存的对象数据

  NodeInfo() : addr(0), last_seen(0), rssi(0), online(false) {}
};

// =============================================================================
// 2. CRC16 校验（CCITT标准）
// =============================================================================

class CRC16 {
public:
  static uint16_t Calculate(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
      crc ^= (uint16_t)data[i] << 8;
      for (int j = 0; j < 8; j++) {
        if (crc & 0x8000)
          crc = (crc << 1) ^ 0x1021;
        else
          crc <<= 1;
      }
    }
    return crc;
  }
};

// =============================================================================
// 3. 消息编解码器
// =============================================================================

class MessageCodec {
public:
  // 编码：Frame -> 字节流
  static std::vector<uint8_t> Encode(const XSlotFrame &frame) {
    std::vector<uint8_t> buffer;
    buffer.reserve(XSLOT_MAX_FRAME_SIZE);

    // 头部
    buffer.push_back(frame.sync);
    buffer.push_back(frame.from & 0xFF);
    buffer.push_back((frame.from >> 8) & 0xFF);
    buffer.push_back(frame.to & 0xFF);
    buffer.push_back((frame.to >> 8) & 0xFF);
    buffer.push_back(frame.seq);
    buffer.push_back(frame.cmd);
    buffer.push_back(frame.len);

    // 数据
    for (int i = 0; i < frame.len; i++) {
      buffer.push_back(frame.data[i]);
    }

    // CRC（计算除CRC外的所有字节）
    uint16_t crc = CRC16::Calculate(buffer.data(), buffer.size());
    buffer.push_back(crc & 0xFF);
    buffer.push_back((crc >> 8) & 0xFF);

    return buffer;
  }

  // 解码：字节流 -> Frame
  static bool Decode(const uint8_t *buffer, size_t len, XSlotFrame &frame) {
    if (len < XSLOT_HEADER_SIZE + XSLOT_CRC_SIZE) {
      return false;
    }

    // 检查同步字节
    if (buffer[0] != XSLOT_SYNC_BYTE) {
      return false;
    }

    // 解析头部
    frame.sync = buffer[0];
    frame.from = buffer[1] | (buffer[2] << 8);
    frame.to = buffer[3] | (buffer[4] << 8);
    frame.seq = buffer[5];
    frame.cmd = buffer[6];
    frame.len = buffer[7];

    // 验证长度
    size_t expected_len = XSLOT_HEADER_SIZE + frame.len + XSLOT_CRC_SIZE;
    if (len < expected_len) {
      return false;
    }

    // 解析数据
    memcpy(frame.data, buffer + XSLOT_HEADER_SIZE, frame.len);

    // 校验CRC
    size_t crc_pos = XSLOT_HEADER_SIZE + frame.len;
    uint16_t received_crc = buffer[crc_pos] | (buffer[crc_pos + 1] << 8);
    uint16_t calculated_crc = CRC16::Calculate(buffer, crc_pos);

    if (received_crc != calculated_crc) {
      std::cerr << "[CODEC] CRC mismatch! RX=" << std::hex << received_crc
                << " CALC=" << calculated_crc << std::dec << std::endl;
      return false;
    }

    frame.crc = received_crc;
    return true;
  }

  // 调试：打印帧信息
  static void DumpFrame(const XSlotFrame &frame, const char *prefix = "") {
    std::cout << prefix << "[Frame] FROM=0x" << std::hex << frame.from
              << " TO=0x" << frame.to << " SEQ=" << std::dec << (int)frame.seq
              << " CMD=0x" << std::hex << (int)frame.cmd << " LEN=" << std::dec
              << (int)frame.len << " CRC=0x" << std::hex << frame.crc
              << std::dec << std::endl;
  }
};

// =============================================================================
// 4. 传输层抽象接口
// =============================================================================

class ITransport {
public:
  virtual ~ITransport() = default;

  // 发送数据到目标地址
  virtual bool Send(uint16_t dest, const uint8_t *data, size_t len) = 0;

  // 接收数据（阻塞，带超时）
  virtual int Receive(uint8_t *buffer, size_t max_len, uint32_t timeout_ms) = 0;

  // 探测插槽模式
  virtual SlotMode Detect() = 0;
};

// 模拟传输层（用于Windows测试，实际用串口替换）
class SimulatedTransport : public ITransport {
private:
  uint16_t local_addr_;
  std::queue<std::vector<uint8_t>> rx_queue_;
  std::mutex queue_mutex_;

  // 模拟网络（所有节点共享）
  static std::map<uint16_t, SimulatedTransport *> network_;
  static std::mutex network_mutex_;

public:
  SimulatedTransport(uint16_t local_addr) : local_addr_(local_addr) {
    std::lock_guard<std::mutex> lock(network_mutex_);
    network_[local_addr] = this;
  }

  ~SimulatedTransport() {
    std::lock_guard<std::mutex> lock(network_mutex_);
    network_.erase(local_addr_);
  }

  bool Send(uint16_t dest, const uint8_t *data, size_t len) override {
    std::lock_guard<std::mutex> lock(network_mutex_);
    auto it = network_.find(dest);
    if (it != network_.end()) {
      std::vector<uint8_t> packet(data, data + len);
      it->second->EnqueuePacket(packet);
      return true;
    }
    return false;
  }

  int Receive(uint8_t *buffer, size_t max_len, uint32_t timeout_ms) override {
    auto start = std::chrono::steady_clock::now();
    while (true) {
      {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (!rx_queue_.empty()) {
          auto packet = rx_queue_.front();
          rx_queue_.pop();
          size_t copy_len = std::min(packet.size(), max_len);
          memcpy(buffer, packet.data(), copy_len);
          return (int)copy_len;
        }
      }

      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - start)
                         .count();
      if (elapsed >= timeout_ms) {
        return 0; // 超时
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  SlotMode Detect() override {
    // 模拟探测：假设总是无线模式
    return SLOT_WIRELESS;
  }

private:
  void EnqueuePacket(const std::vector<uint8_t> &packet) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    rx_queue_.push(packet);
  }
};

std::map<uint16_t, SimulatedTransport *> SimulatedTransport::network_;
std::mutex SimulatedTransport::network_mutex_;

// =============================================================================
// 5. X-Slot 管理器（核心逻辑）
// =============================================================================

class XSlotManager {
private:
  uint16_t local_addr_;
  std::unique_ptr<ITransport> transport_;
  std::map<uint16_t, NodeInfo> node_table_;
  uint8_t seq_counter_;
  bool running_;
  std::thread rx_thread_;
  std::thread heartbeat_thread_;
  std::mutex table_mutex_;

  const uint32_t HEARTBEAT_INTERVAL_MS = 5000; // 5秒心跳
  const uint32_t HEARTBEAT_TIMEOUT_MS = 15000; // 15秒超时

public:
  XSlotManager(uint16_t local_addr, std::unique_ptr<ITransport> transport)
      : local_addr_(local_addr), transport_(std::move(transport)),
        seq_counter_(0), running_(false) {}

  ~XSlotManager() { Stop(); }

  // 启动管理器
  void Start() {
    if (running_)
      return;

    running_ = true;
    rx_thread_ = std::thread(&XSlotManager::ReceiveLoop, this);
    heartbeat_thread_ = std::thread(&XSlotManager::HeartbeatLoop, this);

    std::cout << "[Manager] Started on addr=0x" << std::hex << local_addr_
              << std::dec << std::endl;
  }

  // 停止管理器
  void Stop() {
    if (!running_)
      return;

    running_ = false;
    if (rx_thread_.joinable())
      rx_thread_.join();
    if (heartbeat_thread_.joinable())
      heartbeat_thread_.join();

    std::cout << "[Manager] Stopped" << std::endl;
  }

  // 发送心跳
  void SendPing(uint16_t target) {
    XSlotFrame frame;
    frame.from = local_addr_;
    frame.to = target;
    frame.seq = seq_counter_++;
    frame.cmd = CMD_PING;
    frame.len = 0;

    SendFrame(frame);
  }

  // 上报数据（边缘节点 -> 汇聚节点）
  void ReportData(const uint8_t *data, size_t len) {
    if (len > XSLOT_MAX_DATA_LEN) {
      std::cerr << "[Manager] Data too large: " << len << std::endl;
      return;
    }

    XSlotFrame frame;
    frame.from = local_addr_;
    frame.to = ADDR_HUB;
    frame.seq = seq_counter_++;
    frame.cmd = CMD_REPORT;
    frame.len = (uint8_t)len;
    memcpy(frame.data, data, len);

    SendFrame(frame);
  }

  // 远程写入（汇聚节点 -> 边缘节点）
  void RemoteWrite(uint16_t target, const uint8_t *data, size_t len) {
    if (len > XSLOT_MAX_DATA_LEN) {
      std::cerr << "[Manager] Data too large: " << len << std::endl;
      return;
    }

    XSlotFrame frame;
    frame.from = local_addr_;
    frame.to = target;
    frame.seq = seq_counter_++;
    frame.cmd = CMD_WRITE;
    frame.len = (uint8_t)len;
    memcpy(frame.data, data, len);

    SendFrame(frame);
  }

  // 获取节点列表
  std::vector<NodeInfo> GetNodeList() {
    std::lock_guard<std::mutex> lock(table_mutex_);
    std::vector<NodeInfo> nodes;
    for (const auto &pair : node_table_) {
      nodes.push_back(pair.second);
    }
    return nodes;
  }

  // 打印节点表
  void DumpNodeTable() {
    std::lock_guard<std::mutex> lock(table_mutex_);
    std::cout << "\n=== Node Table (Local=0x" << std::hex << local_addr_
              << std::dec << ") ===" << std::endl;
    for (const auto &pair : node_table_) {
      const auto &node = pair.second;
      std::cout << "  Node 0x" << std::hex << node.addr << std::dec
                << " | Online=" << (node.online ? "YES" : "NO")
                << " | LastSeen=" << node.last_seen << "ms"
                << " | DataLen=" << node.cached_data.size() << std::endl;
    }
    std::cout << "==========================\n" << std::endl;
  }

private:
  // 发送帧
  void SendFrame(const XSlotFrame &frame) {
    auto buffer = MessageCodec::Encode(frame);
    bool ok = transport_->Send(frame.to, buffer.data(), buffer.size());

    if (ok) {
      MessageCodec::DumpFrame(frame, "[TX] ");
    } else {
      std::cerr << "[TX] Failed to send to 0x" << std::hex << frame.to
                << std::dec << std::endl;
    }
  }

  // 接收循环
  void ReceiveLoop() {
    uint8_t buffer[XSLOT_MAX_FRAME_SIZE];

    while (running_) {
      int len = transport_->Receive(buffer, sizeof(buffer), 100);
      if (len > 0) {
        XSlotFrame frame;
        if (MessageCodec::Decode(buffer, len, frame)) {
          MessageCodec::DumpFrame(frame, "[RX] ");
          HandleFrame(frame);
        }
      }
    }
  }

  // 心跳循环
  void HeartbeatLoop() {
    while (running_) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(HEARTBEAT_INTERVAL_MS));

      // 边缘节点：向汇聚节点发送心跳
      if (local_addr_ >= ADDR_NODE_MIN && local_addr_ <= ADDR_NODE_MAX) {
        SendPing(ADDR_HUB);
      }

      // 检查超时节点
      CheckTimeouts();
    }
  }

  // 处理收到的帧
  void HandleFrame(const XSlotFrame &frame) {
    uint32_t now = GetTimestamp();

    // 更新节点表
    UpdateNodeInfo(frame.from, now);

    switch (frame.cmd) {
    case CMD_PING:
      HandlePing(frame);
      break;
    case CMD_PONG:
      HandlePong(frame);
      break;
    case CMD_REPORT:
      HandleReport(frame);
      break;
    case CMD_WRITE:
      HandleWrite(frame);
      break;
    case CMD_WRITE_ACK:
      HandleWriteAck(frame);
      break;
    default:
      std::cerr << "[Handler] Unknown command: 0x" << std::hex << (int)frame.cmd
                << std::dec << std::endl;
    }
  }

  void HandlePing(const XSlotFrame &frame) {
    // 回应PONG
    XSlotFrame pong;
    pong.from = local_addr_;
    pong.to = frame.from;
    pong.seq = frame.seq;
    pong.cmd = CMD_PONG;
    pong.len = 0;
    SendFrame(pong);
  }

  void HandlePong(const XSlotFrame &frame) {
    std::cout << "[Handler] Received PONG from 0x" << std::hex << frame.from
              << std::dec << std::endl;
  }

  void HandleReport(const XSlotFrame &frame) {
    std::lock_guard<std::mutex> lock(table_mutex_);
    auto it = node_table_.find(frame.from);
    if (it != node_table_.end()) {
      // 缓存数据
      it->second.cached_data.assign(frame.data, frame.data + frame.len);
      std::cout << "[Handler] Cached " << (int)frame.len
                << " bytes from node 0x" << std::hex << frame.from << std::dec
                << std::endl;
    }
  }

  void HandleWrite(const XSlotFrame &frame) {
    std::cout << "[Handler] Remote write " << (int)frame.len << " bytes from 0x"
              << std::hex << frame.from << std::dec << std::endl;

    // 模拟处理写入
    // TODO: 实际应用中这里会更新BACnet对象

    // 发送确认
    XSlotFrame ack;
    ack.from = local_addr_;
    ack.to = frame.from;
    ack.seq = frame.seq;
    ack.cmd = CMD_WRITE_ACK;
    ack.len = 1;
    ack.data[0] = 0x00; // 成功
    SendFrame(ack);
  }

  void HandleWriteAck(const XSlotFrame &frame) {
    std::cout << "[Handler] Write ACK from 0x" << std::hex << frame.from
              << std::dec << " result=" << (int)frame.data[0] << std::endl;
  }

  // 更新节点信息
  void UpdateNodeInfo(uint16_t addr, uint32_t timestamp) {
    std::lock_guard<std::mutex> lock(table_mutex_);
    auto &node = node_table_[addr];
    node.addr = addr;
    node.last_seen = timestamp;
    node.online = true;
  }

  // 检查超时节点
  void CheckTimeouts() {
    uint32_t now = GetTimestamp();
    std::lock_guard<std::mutex> lock(table_mutex_);

    for (auto &pair : node_table_) {
      auto &node = pair.second;
      if (now - node.last_seen > HEARTBEAT_TIMEOUT_MS) {
        if (node.online) {
          std::cout << "[Timeout] Node 0x" << std::hex << node.addr << std::dec
                    << " offline" << std::endl;
          node.online = false;
        }
      }
    }
  }

  // 获取时间戳（毫秒）
  uint32_t GetTimestamp() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               now - start)
        .count();
  }
};

// =============================================================================
// 6. 测试 Demo
// =============================================================================

void TestBasicCommunication() {
  std::cout << "\n========================================" << std::endl;
  std::cout << "X-Slot Protocol Demo - Basic Communication" << std::endl;
  std::cout << "========================================\n" << std::endl;

  // 创建汇聚节点（网关）
  auto hub_transport = std::make_unique<SimulatedTransport>(ADDR_HUB);
  XSlotManager hub(ADDR_HUB, std::move(hub_transport));
  hub.Start();

  // 创建边缘节点1
  auto node1_transport = std::make_unique<SimulatedTransport>(0xFFBE);
  XSlotManager node1(0xFFBE, std::move(node1_transport));
  node1.Start();

  // 创建边缘节点2
  auto node2_transport = std::make_unique<SimulatedTransport>(0xFFBF);
  XSlotManager node2(0xFFBF, std::move(node2_transport));
  node2.Start();

  std::cout << "\n[Test] Nodes started, waiting for heartbeats...\n"
            << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(2));

  // 边缘节点上报数据
  std::cout << "\n[Test] Node1 reporting data...\n" << std::endl;
  uint8_t data1[] = {0x01, 0x02, 0x03, 0x04, 0x05};
  node1.ReportData(data1, sizeof(data1));

  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::cout << "\n[Test] Node2 reporting data...\n" << std::endl;
  uint8_t data2[] = {0x0A, 0x0B, 0x0C};
  node2.ReportData(data2, sizeof(data2));

  std::this_thread::sleep_for(std::chrono::seconds(1));

  // 汇聚节点远程写入
  std::cout << "\n[Test] Hub writing to Node1...\n" << std::endl;
  uint8_t write_data[] = {0xFF, 0xEE, 0xDD};
  hub.RemoteWrite(0xFFBE, write_data, sizeof(write_data));

  std::this_thread::sleep_for(std::chrono::seconds(2));

  // 打印节点表
  hub.DumpNodeTable();

  // 等待观察心跳
  std::cout << "\n[Test] Running for 10 seconds to observe heartbeats...\n"
            << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(10));

  hub.DumpNodeTable();

  // 停止所有节点
  std::cout << "\n[Test] Stopping nodes...\n" << std::endl;
  hub.Stop();
  node1.Stop();
  node2.Stop();

  std::cout << "\n[Test] Demo completed!\n" << std::endl;
}

// =============================================================================
// 主函数
// =============================================================================

int main() {
  try {
    TestBasicCommunication();
  } catch (const std::exception &e) {
    std::cerr << "Exception: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}