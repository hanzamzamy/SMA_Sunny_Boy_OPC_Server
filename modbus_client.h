#ifndef MODBUS_CLIENT_H
#define MODBUS_CLIENT_H

#include <modbus/modbus.h>
#include "config.h"

/**
 * @brief Establishes a connection to a Modbus TCP server.
 *
 * @param config A pointer to the application configuration.
 * @return A pointer to a modbus_t context object on success, or NULL on failure.
 */
modbus_t* modbus_tcp_connect(const modbus_opcua_config_t* config);

/**
 * @brief Reads data from Modbus registers based on the provided mappings.
 *
 * @param ctx The Modbus context.
 * @param mapping The register mapping to read.
 * @param dest A buffer to store the read data.
 * @return 0 on success, -1 on failure.
 */
int read_modbus_data(modbus_t* ctx, const modbus_reg_mapping_t* mapping, uint16_t* dest);

#endif // MODBUS_CLIENT_H
