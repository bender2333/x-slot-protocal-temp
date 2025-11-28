/**
 * @file demo_hmi.cpp
 * @brief HMI 直连模式示例
 *
 * 演示如何使用 X-Slot SDK 实现 HMI 客户端，
 * 通过串口直连 DDC 查询和控制对象。
 */
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <xslot/xslot.h>

/* 目标 DDC 地址 */
#define TARGET_DDC_ADDR 0xFFFE

/**
 * @brief 原始数据回调 (查询响应)
 */
void on_data_received(uint16_t from, const uint8_t *data, uint8_t len) {
  printf("[HMI] Response from 0x%04X, %d bytes\n", from, len);

  /* 反序列化对象 */
  xslot_bacnet_object_t objects[16];
  int count = xslot_deserialize_objects(data, len, objects, 16);

  if (count > 0) {
    printf("  Objects:\n");
    for (int i = 0; i < count; i++) {
      const xslot_bacnet_object_t *obj = &objects[i];

      const char *type_name = "?";
      switch (obj->object_type) {
      case XSLOT_OBJ_ANALOG_INPUT:
        type_name = "AI";
        break;
      case XSLOT_OBJ_ANALOG_OUTPUT:
        type_name = "AO";
        break;
      case XSLOT_OBJ_ANALOG_VALUE:
        type_name = "AV";
        break;
      case XSLOT_OBJ_BINARY_INPUT:
        type_name = "BI";
        break;
      case XSLOT_OBJ_BINARY_OUTPUT:
        type_name = "BO";
        break;
      case XSLOT_OBJ_BINARY_VALUE:
        type_name = "BV";
        break;
      }

      if (obj->object_type <= XSLOT_OBJ_ANALOG_VALUE) {
        printf("    %s%d = %.2f\n", type_name, obj->object_id,
               obj->present_value.analog);
      } else {
        printf("    %s%d = %d\n", type_name, obj->object_id,
               obj->present_value.binary);
      }
    }
  }
}

int main(int argc, char *argv[]) {
  printf("=== X-Slot HMI Demo ===\n");

  /* 配置 */
  xslot_config_t config = {0};
  config.local_addr = 0xFF00; /* HMI 固定地址 */
  config.uart_baudrate = 115200;

  /* 命令行参数: 串口 */
  if (argc > 1) {
    snprintf(config.uart_port, sizeof(config.uart_port), "%s", argv[1]);
  } else {
#ifdef _WIN32
    snprintf(config.uart_port, sizeof(config.uart_port), "COM3");
#else
    snprintf(config.uart_port, sizeof(config.uart_port), "/dev/ttyUSB0");
#endif
  }

  printf("Using port: %s\n", config.uart_port);

  /* 初始化 */
  xslot_handle_t handle = xslot_init(&config);
  if (!handle) {
    printf("Error: xslot_init failed\n");
    return -1;
  }

  /* 注册回调 */
  xslot_set_data_callback(handle, on_data_received);

  /* 启动 */
  int ret = xslot_start(handle);
  if (ret != XSLOT_OK) {
    printf("Error: xslot_start failed (%d)\n", ret);
    xslot_deinit(handle);
    return -1;
  }

  /* 检查运行模式 */
  xslot_run_mode_t mode = xslot_get_run_mode(handle);
  switch (mode) {
  case XSLOT_MODE_HMI:
    printf("Running in HMI direct mode\n");
    break;
  case XSLOT_MODE_WIRELESS:
    printf("Warning: Running in WIRELESS mode (expected HMI)\n");
    break;
  case XSLOT_MODE_NONE:
    printf("Error: No device detected\n");
    xslot_deinit(handle);
    return -1;
  }

  printf("HMI started, target DDC: 0x%04X\n\n", TARGET_DDC_ADDR);

  /* 主循环: 周期性查询数据 */
  int query_count = 0;
  while (1) {
    query_count++;
    printf("[%d] Querying AI0-3, BI0-3...\n", query_count);

    /* 查询 AI 对象 */
    uint16_t ai_ids[] = {0, 1, 2, 3};
    ret = xslot_query_objects(handle, TARGET_DDC_ADDR, ai_ids, 4);
    if (ret != XSLOT_OK) {
      printf("Query AI failed: %d\n", ret);
    }

    /* 等待响应 */
#ifdef _WIN32
    Sleep(500);
#else
    usleep(500000);
#endif

    /* 查询 BI 对象 */
    uint16_t bi_ids[] = {0, 1, 2, 3};
    ret = xslot_query_objects(handle, TARGET_DDC_ADDR, bi_ids, 4);
    if (ret != XSLOT_OK) {
      printf("Query BI failed: %d\n", ret);
    }

    /* 等待下一个查询周期 (5秒) */
#ifdef _WIN32
    Sleep(4500);
#else
    sleep(4);
#endif
  }

  /* 清理 */
  xslot_stop(handle);
  xslot_deinit(handle);

  return 0;
}
