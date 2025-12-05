/**
 * @file demo_edge_node.cpp
 * @brief 边缘节点示例
 *
 * 演示如何使用 X-Slot SDK 实现边缘 DDC 节点，
 * 周期性上报 BACnet 对象数据到汇聚节点。
 */
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "../src/bacnet/bacnet_object_def.h"
#include <xslot/xslot.h>

/* 模拟的 DDC 对象实例 */
static ANALOGINPUTOBJECT ai_objects[4];
static DIGITALINPUTOBJECT di_objects[4];

/**
 * @brief 初始化模拟对象
 */
void init_objects(void) {
  for (int i = 0; i < 4; i++) {
    ai_objects[i].uidata.index = i;
    ai_objects[i].uidata.value = 25.0f + i * 0.5f;
    ai_objects[i].uidata.outOfService = 0;
    ai_objects[i].uidata.alarm = 0;

    di_objects[i].didata.index = i;
    di_objects[i].didata.state = (i % 2);
    di_objects[i].didata.outOfService = 0;
    di_objects[i].didata.alarm = 0;
  }
}

/**
 * @brief 写入请求回调 (汇聚节点远程控制)
 */
void on_write_request(uint16_t from, const xslot_bacnet_object_t *obj) {
  printf("[Edge] Write request from 0x%04X: Type=%d, ID=%d\n", from,
         obj->object_type, obj->object_id);

  if (obj->object_type == XSLOT_OBJ_ANALOG_OUTPUT) {
    printf("  -> Set AO%d = %.2f\n", obj->object_id, obj->present_value.analog);
  } else if (obj->object_type == XSLOT_OBJ_BINARY_OUTPUT) {
    printf("  -> Set BO%d = %d\n", obj->object_id, obj->present_value.binary);
  }
}

/**
 * @brief 更新模拟传感器数据
 */
void update_sensor_data(void) {
  for (int i = 0; i < 4; i++) {
    /* 模拟温度变化 */
    ai_objects[i].uidata.value += ((float)(rand() % 100) - 50) / 100.0f;
    if (ai_objects[i].uidata.value < 20.0f)
      ai_objects[i].uidata.value = 20.0f;
    if (ai_objects[i].uidata.value > 30.0f)
      ai_objects[i].uidata.value = 30.0f;
  }

  /* 模拟开关状态变化 */
  if (rand() % 10 == 0) {
    int idx = rand() % 4;
    di_objects[idx].didata.state ^= 1;
  }
}

int main(int argc, char *argv[]) {
  printf("=== X-Slot Edge Node Demo ===\n");

  /* 初始化模拟对象 */
  init_objects();

  /* 配置 */
  xslot_config_t config = {0};
  config.local_addr = 0xFFFD; /* 边缘节点地址 */
  config.cell_id = 1;
  config.power_dbm = 36;
  config.uart_baudrate = 115200;
  config.power_mode = XSLOT_POWER_MODE_NORMAL; /* 使用非低功耗模式 (Type D) */

  /* 命令行参数: 串口 */
  if (argc > 1) {
    snprintf(config.uart_port, sizeof(config.uart_port), "%s", argv[1]);
    char *ptr;
    config.local_addr = strtoul(argv[2], &ptr, 16);
  } else {
#ifdef _WIN32
    snprintf(config.uart_port, sizeof(config.uart_port), "COM23");
#else
    snprintf(config.uart_port, sizeof(config.uart_port), "/dev/ttyUSB0");
#endif
  }

  printf("Using port: %s\n", config.uart_port);
  printf("Using addr: %X \n", config.local_addr);

  /* 初始化 */
  xslot_handle_t handle = xslot_init(&config);
  if (!handle) {

    printf("Error: xslot_init failed\n");
    return -1;
  }

  /* 注册写入回调 */
  xslot_set_write_callback(handle, on_write_request);

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
  case XSLOT_MODE_WIRELESS:
    printf("Running in WIRELESS mode\n");
    break;
  case XSLOT_MODE_HMI:
    printf("Warning: Running in HMI mode (expected WIRELESS)\n");
    break;
  case XSLOT_MODE_NONE:
    printf("Error: No device detected\n");
    xslot_deinit(handle);
    return -1;
  }

  printf("Edge node started, reporting to hub (0x%04X)\n", XSLOT_ADDR_HUB);

  /* 主循环: 周期性上报数据 */
  int report_count = 0;
  while (1) {
    /* 更新传感器数据 */
    update_sensor_data();

    /* 构建对象数组 - 使用 xslot_from_xx 从实际 DDC 对象转换 */
    xslot_bacnet_object_t objects[8];
    int obj_count = 0;

    /* 从 AI 对象转换 */
    for (int i = 0; i < 4; i++) {
      xslot_from_ai(&ai_objects[i], &objects[obj_count++]);
    }

    /* 从 DI 对象转换 */
    for (int i = 0; i < 4; i++) {
      xslot_from_di(&di_objects[i], &objects[obj_count++]);
    }

    /* 上报 */
    ret = xslot_report_objects(handle, objects, obj_count);
    if (ret == XSLOT_OK) {
      report_count++;
      printf("[%d] Reported %d objects: AI=%.2f,%.2f,%.2f,%.2f "
             "DI=%d,%d,%d,%d\n",
             report_count, obj_count, ai_objects[0].uidata.value,
             ai_objects[1].uidata.value, ai_objects[2].uidata.value,
             ai_objects[3].uidata.value, di_objects[0].didata.state,
             di_objects[1].didata.state, di_objects[2].didata.state,
             di_objects[3].didata.state);
    } else {
      printf("Report failed: %d\n", ret);
    }

    /* 等待下一个上报周期 (30秒) */
#ifdef _WIN32
    Sleep(30000);
#else
    sleep(30);
#endif
  }

  /* 清理 */
  xslot_stop(handle);
  xslot_deinit(handle);

  return 0;
}
