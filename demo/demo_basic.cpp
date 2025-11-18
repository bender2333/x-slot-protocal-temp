
// =============================================================================
// demo/demo_basic.cpp - 基础通信测试
// =============================================================================

#include "xslot.h"
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

int main() {
  printf("========================================\n");
  printf("X-Slot Basic Communication Test\n");
  printf("Version: %s\n", xslot_get_version());
  printf("========================================\n\n");

  // 测试1: 初始化和配置
  printf("Test 1: Initialization...\n");
  xslot_config_t config = {0};
  config.local_addr = XSLOT_ADDR_HUB;
  config.cell_id = 1;
  config.power_dbm = 20;
  config.uart_baudrate = 115200;
  config.heartbeat_interval_ms = 5000;
  config.heartbeat_timeout_ms = 15000;

  xslot_handle_t handle = xslot_init(&config);
  if (!handle) {
    printf("  FAILED\n");
    return 1;
  }
  printf("  OK\n\n");

  // 测试2: 插槽探测
  printf("Test 2: Slot detection...\n");
  xslot_slot_mode_t mode = xslot_detect_mode(handle);
  printf("  Mode: %s\n", mode == XSLOT_MODE_WIRELESS ? "WIRELESS"
                         : mode == XSLOT_MODE_HMI    ? "HMI"
                                                     : "EMPTY");
  printf("  OK\n\n");

  // 测试3: BACnet序列化
  printf("Test 3: BACnet serialization...\n");
  xslot_bacnet_object_t obj;
  obj.object_id = 1;
  obj.object_type = BACNET_OBJ_ANALOG_INPUT;
  obj.flags = 0;
  obj.value.analog_value = 23.5f;

  printf("  Object: ID=%d, Type=%d, Value=%.2f\n", obj.object_id,
         obj.object_type, obj.value.analog_value);
  printf("  OK\n\n");

  // 测试4: 启动和停止
  printf("Test 4: Start/Stop...\n");
  if (xslot_start(handle) != XSLOT_OK) {
    printf("  Start FAILED\n");
    xslot_deinit(handle);
    return 1;
  }
  printf("  Started\n");

  SLEEP_MS(2000);

  xslot_stop(handle);
  printf("  Stopped\n");
  printf("  OK\n\n");

  // 清理
  xslot_deinit(handle);

  printf("========================================\n");
  printf("All tests passed!\n");
  printf("========================================\n");

  return 0;
}