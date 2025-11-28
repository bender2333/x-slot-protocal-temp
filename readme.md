# X-Slot Protocol SDK

X-Slot 是专为 DDC 楼宇控制器设计的无线互联协议 SDK，支持 TP1107 Mesh 无线模式和 HMI 串口直连模式。

## 特性

- **多模式支持**: 自动检测 TP1107 Mesh 无线模式、HMI 直连模式
- **BACnet 对象传输**: 支持 AI/AO/AV/BI/BO/BV 对象的完整格式和增量格式序列化
- **节点管理**: 心跳监测、节点表维护、上下线回调
- **跨平台**: 支持 Windows、Linux、FreeRTOS (需移植 HAL 层)
- **C API**: 简洁的 C 语言接口，易于集成

## 目录结构

```
x-slot-protocol/
├── include/xslot/          # 公共头文件
│   ├── xslot.h             # 主 API 入口
│   ├── xslot_types.h       # 类型定义
│   └── xslot_error.h       # 错误码
├── src/
│   ├── core/               # 协议核心
│   │   ├── xslot_protocol.*   # 帧格式和 CRC
│   │   ├── message_codec.*    # 消息编解码
│   │   ├── node_table.*       # 节点表管理
│   │   └── xslot_manager.*    # 协议栈管理器
│   ├── transport/          # 传输层
│   │   ├── i_transport.h      # 传输层接口
│   │   ├── tpmesh_*           # TPMesh 无线传输
│   │   ├── direct_*           # HMI 直连传输
│   │   └── null_*             # 空传输 (无设备)
│   ├── bacnet/             # BACnet 序列化
│   │   ├── bacnet_object_def.h   # 对象定义
│   │   ├── bacnet_serializer.*   # 完整格式
│   │   └── bacnet_incremental.*  # 增量格式
│   ├── hal/                # 硬件抽象层
│   │   ├── hal_interface.h    # HAL 接口
│   │   ├── hal_windows.cpp    # Windows 实现
│   │   ├── hal_linux.cpp      # Linux 实现
│   │   └── hal_freertos.cpp   # FreeRTOS 模板
│   └── xslot_c_api.cpp     # C API 实现
├── demo/                   # 示例程序
│   ├── demo_edge_node.cpp  # 边缘节点示例
│   ├── demo_hub_node.cpp   # 汇聚节点示例
│   └── demo_hmi.cpp        # HMI 直连示例
├── bacnet/                 # BACnet 对象定义参考
└── doc/                    # 文档
```

## 快速开始

### 编译

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### 边缘节点示例

```c
#include <xslot/xslot.h>

int main() {
    xslot_config_t config = {0};
    config.local_addr = 0xFFFD;
    config.cell_id = 1;
    snprintf(config.uart_port, sizeof(config.uart_port), "COM3");

    xslot_handle_t handle = xslot_init(&config);
    xslot_start(handle);

    // 上报数据
    xslot_bacnet_object_t obj = {
        .object_id = 0,
        .object_type = XSLOT_OBJ_ANALOG_INPUT,
        .present_value.analog = 25.5f
    };
    xslot_report_objects(handle, &obj, 1);

    xslot_deinit(handle);
    return 0;
}
```

### 汇聚节点示例

```c
#include <xslot/xslot.h>

void on_report(uint16_t from, const xslot_bacnet_object_t* objs, uint8_t count) {
    printf("Received %d objects from 0x%04X\n", count, from);
}

int main() {
    xslot_config_t config = {0};
    config.local_addr = XSLOT_ADDR_HUB;  // 0xFFFE
    config.cell_id = 1;

    xslot_handle_t handle = xslot_init(&config);
    xslot_set_report_callback(handle, on_report);
    xslot_start(handle);

    while (1) { sleep(1); }
    return 0;
}
```

## API 参考

### 初始化与控制

| 函数 | 说明 |
|------|------|
| `xslot_init()` | 初始化协议栈 |
| `xslot_deinit()` | 释放资源 |
| `xslot_start()` | 启动协议栈 (自动检测模式) |
| `xslot_stop()` | 停止协议栈 |
| `xslot_get_run_mode()` | 获取运行模式 |

### 业务数据操作

| 函数 | 说明 |
|------|------|
| `xslot_report_objects()` | 上报 BACnet 对象 (边缘节点) |
| `xslot_write_object()` | 远程写入对象 (汇聚节点) |
| `xslot_query_objects()` | 查询对象 (HMI) |

### 节点管理

| 函数 | 说明 |
|------|------|
| `xslot_send_ping()` | 发送心跳 |
| `xslot_get_nodes()` | 获取节点列表 |
| `xslot_is_node_online()` | 检查节点在线状态 |

### 回调注册

| 函数 | 说明 |
|------|------|
| `xslot_set_data_callback()` | 原始数据回调 |
| `xslot_set_node_callback()` | 节点上下线回调 |
| `xslot_set_write_callback()` | 写入请求回调 (边缘节点) |
| `xslot_set_report_callback()` | 数据上报回调 (汇聚节点) |

## 移植指南

移植到新平台时，需要实现 `src/hal/hal_interface.h` 中定义的函数：

- `hal_get_timestamp_ms()` - 获取系统时间戳
- `hal_sleep_ms()` - 延时
- `hal_serial_open/close/read/write()` - 串口操作
- `hal_mutex_*()` - 互斥锁 (可选)

参考 `hal_freertos.cpp` 模板。

## 许可证

MIT License
