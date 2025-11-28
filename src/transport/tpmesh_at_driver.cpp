/**
 * @file tpmesh_at_driver.cpp
 * @brief TPMesh AT 驱动层实现
 */
#include "tpmesh_at_driver.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <xslot/xslot_error.h>

/* HAL 函数声明 */
extern "C" {
void *hal_serial_open(const char *port, uint32_t baudrate);
void hal_serial_close(void *handle);
int hal_serial_write(void *handle, const uint8_t *data, uint16_t len);
int hal_serial_read(void *handle, uint8_t *data, uint16_t max_len,
                    uint32_t timeout_ms);
uint32_t hal_get_timestamp_ms(void);
void hal_sleep_ms(uint32_t ms);
}

#define AT_BUFFER_SIZE 512
#define AT_DEFAULT_TIMEOUT 1000

struct tpmesh_at_driver {
  void *serial;
  char port[64];
  uint32_t baudrate;
  bool running;

  /* URC 回调 */
  tpmesh_urc_cb urc_cb;
  void *urc_ctx;

  /* 接收缓冲区 */
  char rx_buffer[AT_BUFFER_SIZE];
  uint16_t rx_len;
};

/**
 * @brief 解析 URC
 */
static bool parse_urc(const char *line, tpmesh_urc_t *urc) {
  if (std::strncmp(line, "+NNMI:", 6) == 0) {
    urc->type = URC_NNMI;
    /* +NNMI:<SRC>,<DEST>,<RSSI>,<LEN>,<DATA> */
    unsigned int src, dest, len;
    int rssi;
    if (std::sscanf(line + 6, "%X,%X,%d,%u,", &src, &dest, &rssi, &len) == 4) {
      urc->src_addr = src;
      urc->dest_addr = dest;
      urc->rssi = rssi;
      urc->data_len = len;

      /* 找到数据部分 */
      const char *data_start = std::strchr(line + 6, ',');
      for (int i = 0; i < 4 && data_start; i++) {
        data_start = std::strchr(data_start + 1, ',');
      }
      if (data_start) {
        data_start++;
        /* 解析十六进制数据 */
        for (uint16_t i = 0; i < len && data_start[i * 2]; i++) {
          unsigned int byte;
          if (std::sscanf(data_start + i * 2, "%2X", &byte) == 1) {
            urc->data[i] = byte;
          }
        }
      }
      return true;
    }
  } else if (std::strncmp(line, "+SEND:", 6) == 0) {
    urc->type = URC_SEND;
    unsigned int sn;
    char result[32];
    if (std::sscanf(line + 6, "%u,%31s", &sn, result) >= 1) {
      urc->sn = sn;
      std::strncpy(urc->result, result, sizeof(urc->result) - 1);
      return true;
    }
  } else if (std::strncmp(line, "+ROUTE:", 7) == 0) {
    urc->type = URC_ROUTE;
    std::strncpy(urc->result, line + 7, sizeof(urc->result) - 1);
    return true;
  } else if (std::strncmp(line, "+ACK:", 5) == 0) {
    urc->type = URC_ACK;
    unsigned int src, sn;
    int rssi;
    if (std::sscanf(line + 5, "%X,%d,%u", &src, &rssi, &sn) == 3) {
      urc->src_addr = src;
      urc->rssi = rssi;
      urc->sn = sn;
      return true;
    }
  } else if (std::strncmp(line, "+BOOT", 5) == 0) {
    urc->type = URC_BOOT;
    return true;
  } else if (std::strncmp(line, "+READY", 6) == 0) {
    urc->type = URC_READY;
    return true;
  }

  return false;
}

/**
 * @brief 处理接收到的行
 */
static void process_line(tpmesh_at_driver_t drv, const char *line) {
  if (!line || !*line)
    return;

  /* 检查是否为 URC */
  if (line[0] == '+') {
    tpmesh_urc_t urc;
    std::memset(&urc, 0, sizeof(urc));
    if (parse_urc(line, &urc) && drv->urc_cb) {
      drv->urc_cb(drv->urc_ctx, &urc);
    }
  }
}

tpmesh_at_driver_t tpmesh_at_create(const char *port, uint32_t baudrate) {
  tpmesh_at_driver_t drv =
      (tpmesh_at_driver_t)std::calloc(1, sizeof(struct tpmesh_at_driver));
  if (!drv)
    return nullptr;

  std::strncpy(drv->port, port ? port : "", sizeof(drv->port) - 1);
  drv->baudrate = baudrate ? baudrate : 115200;
  drv->running = false;

  return drv;
}

void tpmesh_at_destroy(tpmesh_at_driver_t drv) {
  if (!drv)
    return;
  tpmesh_at_stop(drv);
  std::free(drv);
}

int tpmesh_at_start(tpmesh_at_driver_t drv) {
  if (!drv)
    return XSLOT_ERR_PARAM;
  if (drv->running)
    return XSLOT_OK;

  drv->serial = hal_serial_open(drv->port, drv->baudrate);
  if (!drv->serial) {
    return XSLOT_ERR_NO_DEVICE;
  }

  drv->running = true;
  drv->rx_len = 0;

  return XSLOT_OK;
}

void tpmesh_at_stop(tpmesh_at_driver_t drv) {
  if (!drv || !drv->running)
    return;

  drv->running = false;

  if (drv->serial) {
    hal_serial_close(drv->serial);
    drv->serial = nullptr;
  }
}

void tpmesh_at_set_urc_callback(tpmesh_at_driver_t drv, tpmesh_urc_cb cb,
                                void *ctx) {
  if (drv) {
    drv->urc_cb = cb;
    drv->urc_ctx = ctx;
  }
}

int tpmesh_at_send_cmd(tpmesh_at_driver_t drv, const char *cmd,
                       uint32_t timeout_ms) {
  char response[128];
  return tpmesh_at_send_cmd_resp(drv, cmd, response, sizeof(response),
                                 timeout_ms);
}

int tpmesh_at_send_cmd_resp(tpmesh_at_driver_t drv, const char *cmd,
                            char *response, uint16_t resp_size,
                            uint32_t timeout_ms) {
  if (!drv || !drv->serial || !cmd)
    return XSLOT_ERR_PARAM;

  /* 构建完整命令 */
  char full_cmd[256];
  int len = std::snprintf(full_cmd, sizeof(full_cmd), "AT%s\r\n", cmd);
  if (len < 0 || len >= (int)sizeof(full_cmd))
    return XSLOT_ERR_PARAM;

  /* 发送命令 */
  if (hal_serial_write(drv->serial, (uint8_t *)full_cmd, len) != len) {
    return XSLOT_ERR_SEND_FAIL;
  }

  /* 等待响应 */
  uint32_t start = hal_get_timestamp_ms();
  drv->rx_len = 0;

  while (hal_get_timestamp_ms() - start < timeout_ms) {
    uint8_t byte;
    int ret = hal_serial_read(drv->serial, &byte, 1, 10);
    if (ret > 0) {
      if (drv->rx_len < AT_BUFFER_SIZE - 1) {
        drv->rx_buffer[drv->rx_len++] = byte;
        drv->rx_buffer[drv->rx_len] = '\0';
      }

      /* 检查是否收到完整响应 */
      if (std::strstr(drv->rx_buffer, "OK\r\n")) {
        if (response && resp_size > 0) {
          std::strncpy(response, drv->rx_buffer, resp_size - 1);
          response[resp_size - 1] = '\0';
        }
        return XSLOT_OK;
      }
      if (std::strstr(drv->rx_buffer, "ERROR")) {
        return XSLOT_ERR_PARAM;
      }
    }
  }

  return XSLOT_ERR_TIMEOUT;
}

int tpmesh_at_probe(tpmesh_at_driver_t drv) {
  /* 发送简单 AT 命令测试 */
  return tpmesh_at_send_cmd(drv, "", AT_DEFAULT_TIMEOUT);
}

int tpmesh_at_set_addr(tpmesh_at_driver_t drv, uint16_t addr) {
  char cmd[32];
  std::snprintf(cmd, sizeof(cmd), "+ADDR=%04X", addr);
  return tpmesh_at_send_cmd(drv, cmd, AT_DEFAULT_TIMEOUT);
}

int tpmesh_at_set_cell(tpmesh_at_driver_t drv, uint8_t cell_id) {
  char cmd[32];
  std::snprintf(cmd, sizeof(cmd), "+CELL=%u", cell_id);
  return tpmesh_at_send_cmd(drv, cmd, AT_DEFAULT_TIMEOUT);
}

int tpmesh_at_set_power(tpmesh_at_driver_t drv, int8_t power_dbm) {
  char cmd[32];
  std::snprintf(cmd, sizeof(cmd), "+PWR=%d", power_dbm);
  return tpmesh_at_send_cmd(drv, cmd, AT_DEFAULT_TIMEOUT);
}

int tpmesh_at_send_data(tpmesh_at_driver_t drv, uint16_t addr,
                        const uint8_t *data, uint16_t len, uint8_t type) {
  if (!drv || !data || len == 0 || len > 400)
    return XSLOT_ERR_PARAM;

  /* 构建命令: AT+SEND=<ADDR>,<LEN>,<DATA>,<TYPE> */
  char cmd[1024];
  int pos = std::snprintf(cmd, sizeof(cmd), "+SEND=%04X,%u,", addr, len);

  /* 转换数据为十六进制 */
  for (uint16_t i = 0; i < len && pos < (int)sizeof(cmd) - 4; i++) {
    pos += std::snprintf(cmd + pos, sizeof(cmd) - pos, "%02X", data[i]);
  }

  std::snprintf(cmd + pos, sizeof(cmd) - pos, ",%u", type);

  return tpmesh_at_send_cmd(drv, cmd, 3000);
}
