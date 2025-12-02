/**
 * @file hal_windows.cpp
 * @brief Windows 平台 HAL 实现
 */
#ifdef _WIN32

#include "hal_interface.h"
#include <process.h>
#include <stdio.h>
#include <windows.h>

/* ============================================================================
 * 时间函数
 * ============================================================================
 */

uint32_t hal_get_timestamp_ms(void) { return GetTickCount(); }

void hal_sleep_ms(uint32_t ms) { Sleep(ms); }

/* ============================================================================
 * 串口函数
 * ============================================================================
 */

void *hal_serial_open(const char *port, uint32_t baudrate) {
  if (!port)
    return nullptr;

  /* 构建完整的串口路径 */
  char full_port[32];
  if (port[0] != '\\') {
    snprintf(full_port, sizeof(full_port), "\\\\.\\%s", port);
  } else {
    strncpy(full_port, port, sizeof(full_port) - 1);
  }

  HANDLE handle =
      CreateFileA(full_port, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (handle == INVALID_HANDLE_VALUE) {
    return nullptr;
  }

  /* 配置串口参数 */
  DCB dcb = {0};
  dcb.DCBlength = sizeof(DCB);

  if (!GetCommState(handle, &dcb)) {
    CloseHandle(handle);
    return nullptr;
  }

  dcb.BaudRate = baudrate;
  dcb.ByteSize = 8;
  dcb.Parity = NOPARITY;
  dcb.StopBits = ONESTOPBIT;
  dcb.fBinary = TRUE;
  dcb.fParity = FALSE;
  dcb.fOutxCtsFlow = FALSE;
  dcb.fOutxDsrFlow = FALSE;
  dcb.fDtrControl = DTR_CONTROL_ENABLE;
  dcb.fRtsControl = RTS_CONTROL_ENABLE;
  dcb.fOutX = FALSE;
  dcb.fInX = FALSE;

  if (!SetCommState(handle, &dcb)) {
    CloseHandle(handle);
    return nullptr;
  }

  /* 配置超时 */
  COMMTIMEOUTS timeouts = {0};
  timeouts.ReadIntervalTimeout = 10;
  timeouts.ReadTotalTimeoutMultiplier = 0;
  timeouts.ReadTotalTimeoutConstant = 0;
  timeouts.WriteTotalTimeoutMultiplier = 0;
  timeouts.WriteTotalTimeoutConstant = 1000;

  SetCommTimeouts(handle, &timeouts);

  /* 清空缓冲区 */
  PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);

  return handle;
}

void hal_serial_close(void *handle) {
  if (handle) {
    CloseHandle((HANDLE)handle);
  }
}

int hal_serial_write(void *handle, const uint8_t *data, uint16_t len) {
  if (!handle || !data || len == 0)
    return -1;

  DWORD written = 0;
  if (!WriteFile((HANDLE)handle, data, len, &written, nullptr)) {
    return -1;
  }

  return (int)written;
}

int hal_serial_read(void *handle, uint8_t *data, uint16_t max_len,
                    uint32_t timeout_ms) {
  if (!handle || !data || max_len == 0)
    return -1;

  /* 设置读取超时 */
  COMMTIMEOUTS timeouts = {0};
  timeouts.ReadIntervalTimeout = 10;
  timeouts.ReadTotalTimeoutMultiplier = 0;
  timeouts.ReadTotalTimeoutConstant = timeout_ms;
  SetCommTimeouts((HANDLE)handle, &timeouts);

  DWORD read = 0;
  if (!ReadFile((HANDLE)handle, data, max_len, &read, nullptr)) {
    return -1;
  }

  return (int)read;
}

void hal_serial_flush(void *handle) {
  if (handle) {
    PurgeComm((HANDLE)handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
  }
}

/* ============================================================================
 * 互斥锁
 * ============================================================================
 */

void *hal_mutex_create(void) {
  CRITICAL_SECTION *cs = new CRITICAL_SECTION;
  InitializeCriticalSection(cs);
  return cs;
}

void hal_mutex_destroy(void *mutex) {
  if (mutex) {
    DeleteCriticalSection((CRITICAL_SECTION *)mutex);
    delete (CRITICAL_SECTION *)mutex;
  }
}

void hal_mutex_lock(void *mutex) {
  if (mutex) {
    EnterCriticalSection((CRITICAL_SECTION *)mutex);
  }
}

void hal_mutex_unlock(void *mutex) {
  if (mutex) {
    LeaveCriticalSection((CRITICAL_SECTION *)mutex);
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

static unsigned __stdcall thread_entry(void *p) {
  ThreadParam *tp = (ThreadParam *)p;
  if (tp && tp->func) {
    tp->func(tp->arg);
  }
  if (tp) {
    delete tp;
  }
  return 0;
}

void *hal_thread_create(const char *name, hal_thread_func_t func, void *arg,
                        uint32_t stack_size, int priority) {
  ThreadParam *tp = new ThreadParam;
  tp->func = func;
  tp->arg = arg;

  uintptr_t handle =
      _beginthreadex(NULL, stack_size, thread_entry, tp, 0, NULL);
  if (handle == 0) {
    delete tp;
    return nullptr;
  }

  /* 设置优先级 (简单映射) */
  if (priority > 0) {
    SetThreadPriority((HANDLE)handle, THREAD_PRIORITY_ABOVE_NORMAL);
  } else if (priority < 0) {
    SetThreadPriority((HANDLE)handle, THREAD_PRIORITY_BELOW_NORMAL);
  }

  return (void *)handle;
}

void hal_thread_destroy(void *thread) {
  if (thread) {
    WaitForSingleObject((HANDLE)thread, INFINITE);
    CloseHandle((HANDLE)thread);
  }
}

#endif /* _WIN32 */
