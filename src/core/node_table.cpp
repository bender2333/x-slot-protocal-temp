/**
 * @file node_table.cpp
 * @brief 节点表管理实现
 */
#include "node_table.h"
#include <cstdlib>
#include <cstring>

/* 获取当前时间戳 (需要 HAL 层实现) */
extern "C" uint32_t hal_get_timestamp_ms(void);

struct node_entry {
  uint16_t addr;
  uint32_t last_seen;
  int8_t rssi;
  bool online;
  uint8_t object_count;
};

struct node_table {
  node_entry *entries;
  uint8_t max_nodes;
  uint8_t count;
};

node_table_t node_table_create(uint8_t max_nodes) {
  node_table_t table = (node_table_t)std::malloc(sizeof(struct node_table));
  if (!table)
    return nullptr;

  table->entries =
      (node_entry *)std::calloc(max_nodes, sizeof(struct node_entry));
  if (!table->entries) {
    std::free(table);
    return nullptr;
  }

  table->max_nodes = max_nodes;
  table->count = 0;

  return table;
}

void node_table_destroy(node_table_t table) {
  if (table) {
    std::free(table->entries);
    std::free(table);
  }
}

/**
 * @brief 查找节点索引
 * @return 索引，未找到返回 -1
 */
static int find_node_index(node_table_t table, uint16_t addr) {
  for (uint8_t i = 0; i < table->count; i++) {
    if (table->entries[i].addr == addr) {
      return i;
    }
  }
  return -1;
}

bool node_table_update(node_table_t table, uint16_t addr, int8_t rssi) {
  if (!table)
    return false;

  uint32_t now = hal_get_timestamp_ms();
  int idx = find_node_index(table, addr);

  if (idx >= 0) {
    // 已存在，更新
    table->entries[idx].last_seen = now;
    table->entries[idx].rssi = rssi;
    if (!table->entries[idx].online) {
      table->entries[idx].online = true;
      return true; // 重新上线
    }
    return false;
  }

  // 新节点
  if (table->count >= table->max_nodes) {
    // 表满，尝试替换最老的离线节点
    int oldest_offline = -1;
    uint32_t oldest_time = UINT32_MAX;

    for (uint8_t i = 0; i < table->count; i++) {
      if (!table->entries[i].online &&
          table->entries[i].last_seen < oldest_time) {
        oldest_time = table->entries[i].last_seen;
        oldest_offline = i;
      }
    }

    if (oldest_offline >= 0) {
      idx = oldest_offline;
    } else {
      return false; // 表满且无可替换
    }
  } else {
    idx = table->count++;
  }

  table->entries[idx].addr = addr;
  table->entries[idx].last_seen = now;
  table->entries[idx].rssi = rssi;
  table->entries[idx].online = true;
  table->entries[idx].object_count = 0;

  return true; // 新节点上线
}

void node_table_check_timeout(node_table_t table, uint32_t timeout_ms,
                              xslot_node_online_cb offline_cb) {
  if (!table)
    return;

  uint32_t now = hal_get_timestamp_ms();

  for (uint8_t i = 0; i < table->count; i++) {
    if (table->entries[i].online) {
      if (now - table->entries[i].last_seen > timeout_ms) {
        table->entries[i].online = false;
        if (offline_cb) {
          offline_cb(table->entries[i].addr, false);
        }
      }
    }
  }
}

bool node_table_is_online(node_table_t table, uint16_t addr) {
  if (!table)
    return false;

  int idx = find_node_index(table, addr);
  if (idx >= 0) {
    return table->entries[idx].online;
  }
  return false;
}

bool node_table_get_node(node_table_t table, uint16_t addr,
                         xslot_node_info_t *info) {
  if (!table || !info)
    return false;

  int idx = find_node_index(table, addr);
  if (idx >= 0) {
    info->addr = table->entries[idx].addr;
    info->last_seen = table->entries[idx].last_seen;
    info->rssi = table->entries[idx].rssi;
    info->online = table->entries[idx].online;
    info->object_count = table->entries[idx].object_count;
    return true;
  }
  return false;
}

int node_table_get_all(node_table_t table, xslot_node_info_t *nodes,
                       int max_count) {
  if (!table || !nodes)
    return 0;

  int count = 0;
  for (uint8_t i = 0; i < table->count && count < max_count; i++) {
    nodes[count].addr = table->entries[i].addr;
    nodes[count].last_seen = table->entries[i].last_seen;
    nodes[count].rssi = table->entries[i].rssi;
    nodes[count].online = table->entries[i].online;
    nodes[count].object_count = table->entries[i].object_count;
    count++;
  }
  return count;
}

int node_table_online_count(node_table_t table) {
  if (!table)
    return 0;

  int count = 0;
  for (uint8_t i = 0; i < table->count; i++) {
    if (table->entries[i].online) {
      count++;
    }
  }
  return count;
}

void node_table_remove(node_table_t table, uint16_t addr) {
  if (!table)
    return;

  int idx = find_node_index(table, addr);
  if (idx >= 0) {
    // 移动后续元素
    for (uint8_t i = idx; i < table->count - 1; i++) {
      table->entries[i] = table->entries[i + 1];
    }
    table->count--;
  }
}

void node_table_clear(node_table_t table) {
  if (table) {
    table->count = 0;
  }
}
