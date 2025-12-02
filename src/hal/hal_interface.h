/**
 * @file hal_interface.h
 * @brief 硬件抽象层接口定义
 *
 * 移植时需要实现这些函数。
 */
#ifndef HAL_INTERFACE_H
#define HAL_INTERFACE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 时间函数
 * ============================================================================
 */

/**
 * @brief 获取系统时间戳 (毫秒)
 */
uint32_t hal_get_timestamp_ms(void);

/**
 * @brief 延时 (毫秒)
 */
void hal_sleep_ms(uint32_t ms);

/* ============================================================================
 * 串口函数
 * ============================================================================
 */

/**
 * @brief 打开串口
 * @param port 串口名 (如 "COM3" 或 "/dev/ttyUSB0")
 * @param baudrate 波特率
 * @return 串口句柄，失败返回 NULL
 */
void *hal_serial_open(const char *port, uint32_t baudrate);

/**
 * @brief 关闭串口
 */
void hal_serial_close(void *handle);

/**
 * @brief 写入串口
 * @return 实际写入的字节数
 */
int hal_serial_write(void *handle, const uint8_t *data, uint16_t len);

/**
 * @brief 读取串口
 * @param handle 串口句柄
 * @param data 缓冲区
 * @param max_len 最大读取长度
 * @param timeout_ms 超时时间
 * @return 实际读取的字节数
 */
int hal_serial_read(void *handle, uint8_t *data, uint16_t max_len,
                    uint32_t timeout_ms);

/**
 * @brief 清空串口缓冲区
 */
void hal_serial_flush(void *handle);

/* ============================================================================
 * 线程/互斥锁 (可选，用于多线程环境)
 * ============================================================================
 */

/**
 * @brief 创建互斥锁
 */
void *hal_mutex_create(void);

/**
 * @brief 销毁互斥锁
 */
void hal_mutex_destroy(void *mutex);

/**
 * @brief 获取互斥锁
 */
void hal_mutex_lock(void *mutex);

/**
 * @brief 释放互斥锁
 */
void hal_mutex_unlock(void *mutex);

/* ============================================================================
 * 线程函数
 * ============================================================================
 */

/**
 * @brief 线程入口函数类型
 */
typedef void (*hal_thread_func_t)(void *arg);

/**
 * @brief 创建线程
 * @param name 线程名称
 * @param func 入口函数
 * @param arg 参数
 * @param stack_size 栈大小 (字节)
 * @param priority 优先级 (0=默认)
 * @return 线程句柄
 */
void *hal_thread_create(const char *name, hal_thread_func_t func, void *arg,
                        uint32_t stack_size, int priority);

/**
 * @brief 销毁线程 (等待线程结束)
 */
void hal_thread_destroy(void *thread);

#ifdef __cplusplus
}
#endif

#endif /* HAL_INTERFACE_H */
