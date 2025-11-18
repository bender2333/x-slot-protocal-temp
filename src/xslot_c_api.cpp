// =============================================================================
// src/xslot_c_api.cpp - C接口实现
// =============================================================================

#include "xslot.h"
#include "core/xslot_manager.cpp"  // 包含完整的Manager实现
#include <cstring>

// 内部句柄结构
struct xslot_handle {
    xslot::XSlotManager* manager;
    xslot_data_received_cb data_callback;
    xslot_node_online_cb node_callback;
    xslot_write_request_cb write_callback;
};

// =============================================================================
// 导出C接口
// =============================================================================

extern "C" {

xslot_handle_t xslot_init(const xslot_config_t* config) {
    if (!config) return nullptr;
    
    try {
        auto handle = new xslot_handle();
        
        // 创建传输层
        auto transport = xslot::TransportFactory::create(
            "COM1",  // TODO: 从配置获取
            config->uart_baudrate
        );
        
        // 创建管理器
        handle->manager = new xslot::XSlotManager(config->local_addr, 
                                                  std::move(transport));
        handle->data_callback = nullptr;
        handle->node_callback = nullptr;
        handle->write_callback = nullptr;
        
        return handle;
    } catch (const std::exception& e) {
        return nullptr;
    }
}

void xslot_deinit(xslot_handle_t handle) {
    if (!handle) return;
    
    delete handle->manager;
    delete handle;
}

int xslot_start(xslot_handle_t handle) {
    if (!handle || !handle->manager) return XSLOT_ERR_PARAM;
    
    handle->manager->start();
    return XSLOT_OK;
}

void xslot_stop(xslot_handle_t handle) {
    if (!handle || !handle->manager) return;
    
    handle->manager->stop();
}

xslot_slot_mode_t xslot_detect_mode(xslot_handle_t handle) {
    // TODO: 实现探测逻辑
    return XSLOT_MODE_WIRELESS;
}

int xslot_send_ping(xslot_handle_t handle, uint16_t target) {
    if (!handle || !handle->manager) return XSLOT_ERR_PARAM;
    
    handle->manager->sendPing(target);
    return XSLOT_OK;
}

int xslot_report_objects(xslot_handle_t handle, 
                         const xslot_bacnet_object_t* objects,
                         uint8_t count) {
    if (!handle || !handle->manager || !objects) return XSLOT_ERR_PARAM;
    
    // 序列化对象
    auto data = xslot::BACnetSerializer::serializeMultiple(objects, count);
    
    // 发送
    handle->manager->reportData(data.data(), data.size());
    return XSLOT_OK;
}

int xslot_write_object(xslot_handle_t handle,
                       uint16_t target,
                       const xslot_bacnet_object_t* obj) {
    if (!handle || !handle->manager || !obj) return XSLOT_ERR_PARAM;
    
    // 序列化对象
    auto data = xslot::BACnetSerializer::serialize(*obj);
    
    // 发送
    handle->manager->remoteWrite(target, data.data(), data.size());
    return XSLOT_OK;
}

int xslot_query_objects(xslot_handle_t handle,
                        uint16_t target,
                        const uint16_t* object_ids,
                        uint8_t count) {
    if (!handle || !handle->manager || !object_ids) return XSLOT_ERR_PARAM;
    
    // TODO: 实现查询逻辑
    return XSLOT_OK;
}

int xslot_get_nodes(xslot_handle_t handle,
                    xslot_node_info_t* nodes,
                    int max_count) {
    if (!handle || !handle->manager || !nodes) return XSLOT_ERR_PARAM;
    
    auto node_list = handle->manager->getNodeList();
    int count = std::min((int)node_list.size(), max_count);
    
    for (int i = 0; i < count; i++) {
        nodes[i].addr = node_list[i].addr;
        nodes[i].last_seen = node_list[i].last_seen;
        nodes[i].rssi = node_list[i].rssi;
        nodes[i].online = node_list[i].online;
        nodes[i].object_count = 0;  // TODO
    }
    
    return count;
}

void xslot_set_data_callback(xslot_handle_t handle, xslot_data_received_cb callback) {
    if (!handle) return;
    handle->data_callback = callback;
}

void xslot_set_node_callback(xslot_handle_t handle, xslot_node_online_cb callback) {
    if (!handle) return;
    handle->node_callback = callback;
}

void xslot_set_write_callback(xslot_handle_t handle, xslot_write_request_cb callback) {
    if (!handle) return;
    handle->write_callback = callback;
}

int xslot_configure_tp1107(xslot_handle_t handle,
                           uint16_t addr,
                           uint8_t cell_id,
                           int8_t power_dbm) {
    if (!handle || !handle->manager) return XSLOT_ERR_PARAM;
    
    // TODO: 调用AT传输层配置接口
    return XSLOT_OK;
}

const char* xslot_get_version(void) {
    static char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d",
             XSLOT_VERSION_MAJOR, XSLOT_VERSION_MINOR, XSLOT_VERSION_PATCH);
    return version;
}

} // extern "C"