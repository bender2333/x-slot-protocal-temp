/ =============================================================================
// src/transport/xslot_transport.h - 传输层抽象
// =============================================================================

#ifndef XSLOT_TRANSPORT_H
#define XSLOT_TRANSPORT_H

#include <cstdint>
#include <memory>

namespace xslot {

// 传输层接口
class ITransport {
public:
    virtual ~ITransport() = default;
    
    // 发送数据
    virtual bool send(uint16_t dest, const uint8_t* data, size_t len) = 0;
    
    // 接收数据（阻塞，带超时）
    virtual int receive(uint8_t* buffer, size_t max_len, uint32_t timeout_ms) = 0;
    
    // 探测插槽模式
    virtual xslot_slot_mode_t detect() = 0;
    
    // 打开/关闭
    virtual bool open() = 0;
    virtual void close() = 0;
};

// 传输工厂
class TransportFactory {
public:
    static std::unique_ptr<ITransport> create(const char* port, uint32_t baudrate);
};

} // namespace xslot

#endif // XSLOT_TRANSPORT_H