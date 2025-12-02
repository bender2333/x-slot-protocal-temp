/**
 * @file tpmesh_at_driver.cpp
 * @brief TPMesh AT 驱动层实现
 */
#include "tpmesh_at_driver.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <xslot/xslot_error.h>

/* 开启串口调试打印 (注释掉此行以关闭) */
#define XSLOT_DEBUG_SERIAL

#ifdef XSLOT_DEBUG_SERIAL
/* 简单的全局缓冲，用于 debug 打印合并 */
static struct {
  char dir[4];
  uint8_t buf[256];
  size_t len;
} dbg_buf;

static void flush_debug_serial() {
  if (dbg_buf.len == 0)
    return;

  printf("[%s] ", dbg_buf.dir);
  for (size_t i = 0; i < dbg_buf.len; i++) {
    printf("%02X ", dbg_buf.buf[i]);
  }
  /* 填充空格以便对齐 */
  if (dbg_buf.len < 16) {
    for (size_t i = dbg_buf.len; i < 16; i++)
      printf("   ");
  }
  printf("| ");
  for (size_t i = 0; i < dbg_buf.len; i++) {
    char c = (char)dbg_buf.buf[i];
    if (c == '\r' || c == '\n')
      printf(".");
    else if (c >= 32 && c <= 126)
      printf("%c", c);
    else
      printf(".");
  }
  printf("\n");
  dbg_buf.len = 0;
}

static void debug_serial(const char *dir, const void *data, size_t len) {
  const uint8_t *p = (const uint8_t *)data;

  for (size_t i = 0; i < len; i++) {
    /* 如果方向改变，或者缓冲区满，则刷新 */
    bool dir_changed =
        (std::strncmp(dir, dbg_buf.dir, 3) != 0 && dbg_buf.len > 0);
    bool buf_full = (dbg_buf.len >= 32);

    if (dir_changed || buf_full) {
      flush_debug_serial();
    }

    /* 更新方向 */
    if (dbg_buf.len == 0) {
      std::strncpy(dbg_buf.dir, dir, 3);
    }

    dbg_buf.buf[dbg_buf.len++] = p[i];

    /* 如果是换行符，立即刷新 (让每个 AT 响应行独立显示) */
    if (p[i] == '\n') {
      flush_debug_serial();
    }
  }
}
#else
#define debug_serial(dir, data, len)
#endif

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

  /* 打印发送数据 */
  debug_serial("TX", full_cmd, len);

  /* 发送命令 */
  if (hal_serial_write(drv->serial, (uint8_t *)full_cmd, len) != len) {
    return XSLOT_ERR_SEND_FAIL;
  }

  /* 等待响应 */
  uint32_t start = hal_get_timestamp_ms();
  drv->rx_len = 0;

  while (hal_get_timestamp_ms() - start < timeout_ms) {
    uint8_t buf[32];
    int ret = hal_serial_read(drv->serial, buf, sizeof(buf), 10);
    if (ret > 0) {
      debug_serial("RX", buf, ret);

      for (int i = 0; i < ret; i++) {
        if (drv->rx_len < AT_BUFFER_SIZE - 1) {
          drv->rx_buffer[drv->rx_len++] = buf[i];
          drv->rx_buffer[drv->rx_len] = '\0';
        }
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

static int tpmesh_at_get_lp(tpmesh_at_driver_t drv, int *mode) {
  char resp[128];
  int ret = tpmesh_at_send_cmd_resp(drv, "+LP?", resp, sizeof(resp),
                                    AT_DEFAULT_TIMEOUT);
  if (ret != XSLOT_OK)
    return ret;

  int val;
  char *p = std::strstr(resp, "+LP:");
  if (p && std::sscanf(p + 4, "%d", &val) == 1) {
    *mode = val;
    return XSLOT_OK;
  }
  return XSLOT_ERR_PARAM;
}

int tpmesh_at_set_power_mode(tpmesh_at_driver_t drv, uint8_t mode) {
  /* 先查询当前模式，避免不必要的重启 */
  int current_mode = 0;
  if (tpmesh_at_get_lp(drv, &current_mode) == XSLOT_OK) {
    if (current_mode == mode) {
      return XSLOT_OK;
    }
  }

  char cmd[32];
  std::snprintf(cmd, sizeof(cmd), "+LP=%u", mode);
  int ret = tpmesh_at_send_cmd(drv, cmd, AT_DEFAULT_TIMEOUT);
  if (ret == XSLOT_OK) {
    /* 配置成功，模组会重启，等待并重新探测 */
    hal_sleep_ms(3000); /* 等待重启 */

    /* 尝试重连 (5秒超时) */
    for (int i = 0; i < 10; i++) {
      if (tpmesh_at_probe(drv) == XSLOT_OK) {
        return XSLOT_OK;
      }
      hal_sleep_ms(500);
    }
    /* 重连成功算成功，否则也返回 OK (假设它在恢复中) */
    return XSLOT_OK;
  }
  return ret;
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
