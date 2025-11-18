// =============================================================================
// demo/demo_edge_node.cpp - 边缘节点示例
// =============================================================================

#include "xslot.h"
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>


#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

static volatile int running = 1;

void signal_handler(int sig) { running = 0; }

void on_write_request(uint16_t from, const xslot_bacnet_object_t *obj) {
  printf("[Edge] Received write request from 0x%04X\n", from);
  printf("  Object ID: %d, Type: %d\n", obj->object_id, obj->object_type);

  if (obj->object_type == BACNET_OBJ_ANALOG_OUTPUT) {
    printf("  New value: %.2f\n", obj->value.analog_value);
  } else if (obj->object_type == BACNET_OBJ_BINARY_OUTPUT) {
    printf("  New value: %d\n", obj->value.binary_value);
  }
}

int main(int argc, char *argv[]) {
  printf("========================================\n");
  printf("X-Slot Edge Node Demo\n");
  printf("Version: %s\n", xslot_get_version());
  printf("========================================\n\n");

  signal(SIGINT, signal_handler);

  // 解析命令行参数（节点地址）
  uint16_t node_addr = 0xFFBE; // 默认地址
  if (argc > 1) {
    node_addr = (uint16_t)strtol(argv[1], NULL, 16);
  }

  printf("Node address: 0x%04X\n", node_addr);

  // 配置
  xslot_config_t config = {0};
  config.local_addr = node_addr;
  config.cell_id = 1;
  config.power_dbm = 20;
  config.wakeup_period_ms = 1000; // 1秒唤醒周期
  config.uart_baudrate = 115200;
  config.heartbeat_interval_ms = 5000;
  config.heartbeat_timeout_ms = 15000;

  // 初始化
  xslot_handle_t handle = xslot_init(&config);
  if (!handle) {
    fprintf(stderr, "Failed to initialize X-Slot\n");
    return 1;
  }

  // 设置回调
  xslot_set_write_callback(handle, on_write_request);

  // 配置TP1107模块
  printf("Configuring TP1107 module...\n");
  if (xslot_configure_tp1107(handle, config.local_addr, config.cell_id,
                             config.power_dbm) != XSLOT_OK) {
    fprintf(stderr, "Failed to configure TP1107\n");
    xslot_deinit(handle);
    return 1;
  }

  // 启动
  printf("Starting edge node...\n");
  if (xslot_start(handle) != XSLOT_OK) {
    fprintf(stderr, "Failed to start\n");
    xslot_deinit(handle);
    return 1;
  }

  printf("Edge node running. Press Ctrl+C to exit.\n\n");

  // 模拟传感器数据
  float temperature = 25.0f;
  float humidity = 60.0f;
  uint8_t alarm = 0;

  // 主循环
  int loop_count = 0;
  while (running) {
    SLEEP_MS(1000);
    loop_count++;

    // 模拟数据变化
    temperature = 25.0f + 5.0f * sinf(loop_count * 0.1f);
    humidity = 60.0f + 10.0f * cosf(loop_count * 0.15f);

    // 每5秒上报一次数据
    if (loop_count % 5 == 0) {
      printf("\n[Edge] Reporting data...\n");

      // 准备BACnet对象
      xslot_bacnet_object_t objects[3];

      // 温度传感器 (AI0)
      objects[0].object_id = 0;
      objects[0].object_type = BACNET_OBJ_ANALOG_INPUT;
      objects[0].flags = 0;
      objects[0].value.analog_value = temperature;

      // 湿度传感器 (AI1)
      objects[1].object_id = 1;
      objects[1].object_type = BACNET_OBJ_ANALOG_INPUT;
      objects[1].flags = 0;
      objects[1].value.analog_value = humidity;

      // 告警状态 (BI0)
      objects[2].object_id = 0;
      objects[2].object_type = BACNET_OBJ_BINARY_INPUT;
      objects[2].flags = 0;
      objects[2].value.binary_value = (temperature > 28.0f) ? 1 : 0;

      // 上报
      if (xslot_report_objects(handle, objects, 3) == XSLOT_OK) {
        printf("  Temperature: %.2f °C\n", temperature);
        printf("  Humidity: %.2f %%\n", humidity);
        printf("  Alarm: %s\n", objects[2].value.binary_value ? "ON" : "OFF");
      } else {
        fprintf(stderr, "  Failed to report data\n");
      }
    }
  }

  // 清理
  printf("\nStopping edge node...\n");
  xslot_stop(handle);
  xslot_deinit(handle);

  printf("Edge node stopped.\n");
  return 0;
}
