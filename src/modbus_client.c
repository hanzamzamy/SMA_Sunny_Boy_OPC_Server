#include "modbus_client.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "logger.h"
#include "opcua_server.h"

modbus_t* modbus_tcp_connect(const modbus_opcua_config_t* config) {
  modbus_t* ctx = modbus_new_tcp(config->modbus_ip, config->modbus_port);
  if (ctx == NULL) {
    log_message(LOG_LEVEL_ERROR, "Failed to create modbus context: %s", modbus_strerror(errno));
    return NULL;
  }

  modbus_set_slave(ctx, config->modbus_slave_id);

  struct timeval timeout;
  timeout.tv_sec  = config->modbus_timeout_sec;
  timeout.tv_usec = 0;
  modbus_set_response_timeout(ctx, timeout.tv_sec, timeout.tv_usec);

  if (modbus_connect(ctx) == -1) {
    if (errno == EINTR && opcua_shutdown_requested()) {
      modbus_free(ctx);
      return NULL;
    }

    log_message(LOG_LEVEL_ERROR, "Modbus connection failed to %s:%d : %s", config->modbus_ip, config->modbus_port, modbus_strerror(errno));
    modbus_free(ctx);
    return NULL;
  }

  log_message(LOG_LEVEL_INFO, "Successfully connected to Modbus server at %s:%d", config->modbus_ip, config->modbus_port);
  return ctx;
}

int read_modbus_data(modbus_t* ctx, const modbus_reg_mapping_t* mapping, uint16_t* dest) {
  int num_regs = 1;  // Default to reading one register
  if (strcmp(mapping->data_type, "S32") == 0 || strcmp(mapping->data_type, "U32") == 0 || strcmp(mapping->data_type, "FLOAT32") == 0) {
    num_regs = 2;
  } else if (strcmp(mapping->data_type, "U64") == 0) {
    num_regs = 4;
  }

  // Using read_input_registers (function code 0x04) as per SMA doc for measurements
  int rc = modbus_read_input_registers(ctx, mapping->modbus_address, num_regs, dest);
  if (rc == -1) {
    if (errno == EINTR && opcua_shutdown_requested()) {
      return -2;
    }

    log_message(LOG_LEVEL_ERROR, "Failed to read Modbus register %d: %s", mapping->modbus_address, modbus_strerror(errno));
    return -1;
  }
  return 0;
}
