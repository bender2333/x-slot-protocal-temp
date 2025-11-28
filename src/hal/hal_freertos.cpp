/**
 * @file hal_freertos.cpp
 * @brief FreeRTOS 平台 HAL 实现模板
 *
 * 移植到嵌入式平台时，需要根据具体硬件实现串口操作。
 */
#if defined(XSLOT_PLATFORM_FREERTOS)

#include "hal_interface.h"

/* FreeRTOS 头文件 */
// #include "FreeRTOS.h"
// #include "semphr.h"
// #include "task.h"

/* 硬件驱动头文件 */
// #include "usart.h"  /* 根据具体 MCU 修改 */

/* ============================================================================
 * 时间函数
 * ============================================================================
 */

uint32_t hal_get_timestamp_ms(void) {
  /* TODO: 实现 */
  // return xTaskGetTickCount() * portTICK_PERIOD_MS;
  return 0;
}

void hal_sleep_ms(uint32_t ms) {
  /* TODO: 实现 */
  // vTaskDelay(pdMS_TO_TICKS(ms));
  (void)ms;
}

/* ============================================================================
 * 串口函数
 * ============================================================================
 */

void *hal_serial_open(const char *port, uint32_t baudrate) {
  /* TODO: 根据具体硬件实现 */
  /* 示例：
   * if (strcmp(port, "USART1") == 0) {
   *     MX_USART1_Init(baudrate);
   *     return (void*)&huart1;
   * }
   */
  (void)port;
  (void)baudrate;
  return nullptr;
}

void hal_serial_close(void *handle) {
  /* TODO: 根据具体硬件实现 */
  (void)handle;
}

int hal_serial_write(void *handle, const uint8_t *data, uint16_t len) {
  /* TODO: 根据具体硬件实现 */
  /* 示例：
   * UART_HandleTypeDef* huart = (UART_HandleTypeDef*)handle;
   * if (HAL_UART_Transmit(huart, (uint8_t*)data, len, 1000) == HAL_OK) {
   *     return len;
   * }
   */
  (void)handle;
  (void)data;
  (void)len;
  return -1;
}

int hal_serial_read(void *handle, uint8_t *data, uint16_t max_len,
                    uint32_t timeout_ms) {
  /* TODO: 根据具体硬件实现 */
  /* 示例：
   * UART_HandleTypeDef* huart = (UART_HandleTypeDef*)handle;
   * if (HAL_UART_Receive(huart, data, max_len, timeout_ms) == HAL_OK) {
   *     return max_len;
   * }
   */
  (void)handle;
  (void)data;
  (void)max_len;
  (void)timeout_ms;
  return 0;
}

void hal_serial_flush(void *handle) {
  /* TODO: 根据具体硬件实现 */
  (void)handle;
}

/* ============================================================================
 * 互斥锁
 * ============================================================================
 */

void *hal_mutex_create(void) {
  /* TODO: 实现 */
  // return xSemaphoreCreateMutex();
  return nullptr;
}

void hal_mutex_destroy(void *mutex) {
  /* TODO: 实现 */
  // if (mutex) vSemaphoreDelete((SemaphoreHandle_t)mutex);
  (void)mutex;
}

void hal_mutex_lock(void *mutex) {
  /* TODO: 实现 */
  // if (mutex) xSemaphoreTake((SemaphoreHandle_t)mutex, portMAX_DELAY);
  (void)mutex;
}

void hal_mutex_unlock(void *mutex) {
  /* TODO: 实现 */
  // if (mutex) xSemaphoreGive((SemaphoreHandle_t)mutex);
  (void)mutex;
}

#endif /* XSLOT_PLATFORM_FREERTOS */
