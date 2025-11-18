// =============================================================================
// demo/demo_gateway.cpp - 网关节点示例
// =============================================================================

#include "xslot.h"
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

void on_data_received(uint16_t from, const uint8_t *data, uint8_t len) {
  printf("[Gateway] Received %d bytes from 0x%04X\n", len, from);

  // 解析BACnet对象
  xslot_bacnet_object_t objects[16];
  // TODO: 反序列化

  printf("  Object ID: %d, Type: %d\n", objects[0].object_id,
         objects[0].object_type);
}

void on_node_status(uint16_t addr, bool online) {
  printf("[Gateway] Node 0x%04X is %s\n", addr, online ? "ONLINE" : "OFFLINE");
}

void on_write_request(uint16_t from, const xslot_bacnet_object_t *obj) {
  printf("[Gateway] Write request from 0x%04X: OBJ=%d\n", from, obj->object_id);
}

int main(int argc, char *argv[]) {
  printf("========================================\n");
  printf("X-Slot Gateway Node Demo\n");
  printf("Version: %s\n", xslot_get_version());
  printf("========================================\n\n");

  signal(SIGINT, signal_handler);

  // 配置
  xslot_config_t config = {0};
  config.local_addr = XSLOT_ADDR_HUB; // 网关地址0xFFFE
  config.cell_id = 1;
  config.power_dbm = 20;
  config.wakeup_period_ms = 0; // 常接收
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
  xslot_set_data_callback(handle, on_data_received);
  xslot_set_node_callback(handle, on_node_status);
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
  printf("Starting gateway...\n");
  if (xslot_start(handle) != XSLOT_OK) {
    fprintf(stderr, "Failed to start\n");
    xslot_deinit(handle);
    return 1;
  }

  printf("Gateway running. Press Ctrl+C to exit.\n\n");

  // 主循环
  int loop_count = 0;
  while (running) {
    SLEEP_MS(1000);

    // 每10秒打印节点列表
    if (++loop_count % 10 == 0) {
      xslot_node_info_t nodes[XSLOT_MAX_NODES];
      int count = xslot_get_nodes(handle, nodes, XSLOT_MAX_NODES);

      printf("\n=== Node Status (%d nodes) ===\n", count);
      for (int i = 0; i < count; i++) {
        printf("  0x%04X: %s, RSSI=%d, LastSeen=%u ms\n", nodes[i].addr,
               nodes[i].online ? "ONLINE " : "OFFLINE", nodes[i].rssi,
               nodes[i].last_seen);
      }
      printf("============================\n\n");
    }
  }

  // 清理
  printf("\nStopping gateway...\n");
  xslot_stop(handle);
  xslot_deinit(handle);

  printf("Gateway stopped.\n");
  return 0;
}