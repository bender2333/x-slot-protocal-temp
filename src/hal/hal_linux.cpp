/**
 * @file hal_linux.cpp
 * @brief Linux 平台 HAL 实现
 */
#if defined(__linux__) || defined(__APPLE__)

#include "hal_interface.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* ============================================================================
 * 时间函数
 * ============================================================================
 */

uint32_t hal_get_timestamp_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void hal_sleep_ms(uint32_t ms) { usleep(ms * 1000); }

/* ============================================================================
 * 串口函数
 * ============================================================================
 */

/**
 * @brief 波特率转换
 */
static speed_t baudrate_to_speed(uint32_t baudrate) {
  switch (baudrate) {
  case 9600:
    return B9600;
  case 19200:
    return B19200;
  case 38400:
    return B38400;
  case 57600:
    return B57600;
  case 115200:
    return B115200;
  case 230400:
    return B230400;
#ifdef B460800
  case 460800:
    return B460800;
#endif
#ifdef B921600
  case 921600:
    return B921600;
#endif
  default:
    return B115200;
  }
}

void *hal_serial_open(const char *port, uint32_t baudrate) {
  if (!port)
    return nullptr;

  int fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd < 0) {
    return nullptr;
  }

  /* 配置串口 */
  struct termios tty;
  memset(&tty, 0, sizeof(tty));

  if (tcgetattr(fd, &tty) != 0) {
    close(fd);
    return nullptr;
  }

  speed_t speed = baudrate_to_speed(baudrate);
  cfsetospeed(&tty, speed);
  cfsetispeed(&tty, speed);

  /* 8N1 */
  tty.c_cflag &= ~PARENB;
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;

  /* 禁用流控 */
  tty.c_cflag &= ~CRTSCTS;
  tty.c_cflag |= CREAD | CLOCAL;

  /* 原始模式 */
  tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);
  tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
  tty.c_oflag &= ~OPOST;

  /* 读取设置 */
  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 1; /* 100ms 超时 */

  if (tcsetattr(fd, TCSANOW, &tty) != 0) {
    close(fd);
    return nullptr;
  }

  /* 清空缓冲区 */
  tcflush(fd, TCIOFLUSH);

  /* 返回文件描述符 (转为指针) */
  return (void *)(intptr_t)(fd + 1); /* +1 避免 fd=0 时返回 NULL */
}

void hal_serial_close(void *handle) {
  if (handle) {
    int fd = (int)(intptr_t)handle - 1;
    close(fd);
  }
}

int hal_serial_write(void *handle, const uint8_t *data, uint16_t len) {
  if (!handle || !data || len == 0)
    return -1;

  int fd = (int)(intptr_t)handle - 1;
  return write(fd, data, len);
}

int hal_serial_read(void *handle, uint8_t *data, uint16_t max_len,
                    uint32_t timeout_ms) {
  if (!handle || !data || max_len == 0)
    return -1;

  int fd = (int)(intptr_t)handle - 1;

  /* 使用 select 实现超时 */
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(fd, &readfds);

  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;

  int ret = select(fd + 1, &readfds, nullptr, nullptr, &tv);
  if (ret <= 0) {
    return 0; /* 超时或错误 */
  }

  return read(fd, data, max_len);
}

void hal_serial_flush(void *handle) {
  if (handle) {
    int fd = (int)(intptr_t)handle - 1;
    tcflush(fd, TCIOFLUSH);
  }
}

/* ============================================================================
 * 互斥锁
 * ============================================================================
 */

void *hal_mutex_create(void) {
  pthread_mutex_t *mutex = new pthread_mutex_t;
  pthread_mutex_init(mutex, nullptr);
  return mutex;
}

void hal_mutex_destroy(void *mutex) {
  if (mutex) {
    pthread_mutex_destroy((pthread_mutex_t *)mutex);
    delete (pthread_mutex_t *)mutex;
  }
}

void hal_mutex_lock(void *mutex) {
  if (mutex) {
    pthread_mutex_lock((pthread_mutex_t *)mutex);
  }
}

void hal_mutex_unlock(void *mutex) {
  if (mutex) {
    pthread_mutex_unlock((pthread_mutex_t *)mutex);
  }
}

/* ============================================================================
 * 线程函数
 * ============================================================================
 */

struct ThreadParam {
  hal_thread_func_t func;
  void *arg;
};

static void *thread_entry(void *p) {
  ThreadParam *tp = (ThreadParam *)p;
  if (tp && tp->func) {
    tp->func(tp->arg);
  }
  if (tp)
    delete tp;
  return nullptr;
}

void *hal_thread_create(const char *name, hal_thread_func_t func, void *arg,
                        uint32_t stack_size, int priority) {
  ThreadParam *tp = new ThreadParam;
  tp->func = func;
  tp->arg = arg;

  pthread_t *thread = new pthread_t;
  pthread_attr_t attr;
  pthread_attr_init(&attr);

  if (stack_size > 0) {
    pthread_attr_setstacksize(&attr, stack_size);
  }

  if (pthread_create(thread, &attr, thread_entry, tp) != 0) {
    pthread_attr_destroy(&attr);
    delete thread;
    delete tp;
    return nullptr;
  }

  pthread_attr_destroy(&attr);

  /* 设置线程名 */
  if (name) {
#if defined(__linux__) && !defined(__ANDROID__)
    pthread_setname_np(*thread, name);
#endif
  }

  return thread;
}

void hal_thread_destroy(void *thread) {
  if (thread) {
    pthread_join(*(pthread_t *)thread, nullptr);
    delete (pthread_t *)thread;
  }
}

#endif /* __linux__ || __APPLE__ */
