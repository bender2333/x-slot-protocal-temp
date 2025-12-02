/**
 * @file demo_hub_node.cpp
 * @brief 汇聚节点示例
 *
 * 演示如何使用 X-Slot SDK 实现中心汇聚节点，
 * 接收边缘节点上报的数据并进行管理。
 */
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <xslot/xslot.h>

/**
 * @brief 数据上报回调
 */
void on_report_received(uint16_t from, const xslot_bacnet_object_t *objects,
                        uint8_t count) {
  printf("[Hub] Received %d objects from node 0x%04X:\n", count, from);

  for (uint8_t i = 0; i < count; i++) {
    const xslot_bacnet_object_t *obj = &objects[i];

    const char *type_name = "Unknown";
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
      printf("  %s%d = %.2f\n", type_name, obj->object_id,
             obj->present_value.analog);
    } else {
      printf("  %s%d = %d\n", type_name, obj->object_id,
             obj->present_value.binary);
    }
  }
}

/**
 * @brief 节点上下线回调
 */
void on_node_status(uint16_t addr, bool online) {
  printf("[Hub] Node 0x%04X %s\n", addr, online ? "ONLINE" : "OFFLINE");
}

int main(int argc, char *argv[]) {
  printf("=== X-Slot Hub Node Demo ===\n");

  /* 配置 */
  xslot_config_t config = {0};
  config.local_addr = XSLOT_ADDR_HUB; /* 汇聚节点固定地址 0xFFFE */
  config.cell_id = 1;
  config.power_dbm = 36;
  config.uart_baudrate = 115200;
  config.power_mode = XSLOT_POWER_MODE_NORMAL; /* Hub 默认也是 Type D */

  /* 命令行参数: 串口 */
  if (argc > 1) {
    snprintf(config.uart_port, sizeof(config.uart_port), "%s", argv[1]);
  } else {
#ifdef _WIN32
    snprintf(config.uart_port, sizeof(config.uart_port), "COM12");
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
  xslot_set_report_callback(handle, on_report_received);
  xslot_set_node_callback(handle, on_node_status);

  /* 启动 */
  int ret = xslot_start(handle);
  if (ret != XSLOT_OK) {
    printf("Error: xslot_start failed (%d)\n", ret);
    xslot_deinit(handle);
    return -1;
  }

  /* 检查运行模式 */
  xslot_run_mode_t mode = xslot_get_run_mode(handle);
  if (mode == XSLOT_MODE_NONE) {
    printf("Error: No device detected\n");
    xslot_deinit(handle);
    return -1;
  } else if (mode == XSLOT_MODE_HMI) {
    printf("Warning: Hub should run in WIRELESS mode, but detected HMI\n");
  } else {
    printf("Hub node started in WIRELESS mode\n");
  }

  printf("Waiting for edge nodes to report...\n\n");

  /* 主循环 */
  int loop_count = 0;
  while (1) {
    loop_count++;

    /* 每 60 秒打印节点状态 */
    if (loop_count % 60 == 0) {
      xslot_node_info_t nodes[16];
      int count = xslot_get_nodes(handle, nodes, 16);

      printf("\n[Hub] Node Status (%d nodes):\n", count);
      for (int i = 0; i < count; i++) {
        printf("  0x%04X: %s, RSSI=%d, last_seen=%u\n", nodes[i].addr,
               nodes[i].online ? "Online" : "Offline", nodes[i].rssi,
               nodes[i].last_seen);
      }
      printf("\n");
    }

#ifdef _WIN32
    Sleep(1000);
#else
    sleep(1);
#endif
  }

  /* 清理 */
  xslot_stop(handle);
  xslot_deinit(handle);

  return 0;
}
