
// =============================================================================
// src/hal/xslot_hal.h - 硬件抽象层
// =============================================================================

#ifndef XSLOT_HAL_H
#define XSLOT_HAL_H

#include <cstdint>
#include <functional>

namespace xslot {
namespace hal {

// 线程抽象
class Thread {
public:
  using ThreadFunc = std::function<void()>;

  Thread(ThreadFunc func);
  ~Thread();

  void start();
  void join();
  bool isRunning() const;

private:
  class Impl;
  Impl *impl_;
};

// 互斥锁抽象
class Mutex {
public:
  Mutex();
  ~Mutex();

  void lock();
  void unlock();
  bool tryLock();

private:
  class Impl;
  Impl *impl_;
};

// RAII锁
class LockGuard {
public:
  explicit LockGuard(Mutex &mutex) : mutex_(mutex) { mutex_.lock(); }
  ~LockGuard() { mutex_.unlock(); }

private:
  Mutex &mutex_;
};

// 时间函数
uint32_t getTimestamp(); // 获取时间戳(ms)
void sleep(uint32_t ms); // 延时

// 串口抽象
class SerialPort {
public:
  SerialPort(const char *port, uint32_t baudrate);
  ~SerialPort();

  bool open();
  void close();
  bool isOpen() const;

  int write(const uint8_t *data, size_t len);
  int read(uint8_t *data, size_t len, uint32_t timeout_ms);

  void flush();

private:
  class Impl;
  Impl *impl_;
};

} // namespace hal
} // namespace xslot

#endif // XSLOT_HAL_H