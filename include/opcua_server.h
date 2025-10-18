#ifndef OPCUA_SERVER_H
#define OPCUA_SERVER_H

#include "config.h"
#include "open62541/plugin/accesscontrol_default.h"
#include "open62541/server.h"
#include "open62541/server_config_default.h"

// Expose the running flag
extern UA_Boolean running;

/**
 * @brief Initializes and configures the OPC UA server, including security.
 *
 * @param config A pointer to the application configuration.
 * @return A pointer to the configured UA_Server object, or NULL on failure.
 */
UA_Server* opcua_server_init(const modbus_opcua_config_t* config);

/**
 * @brief Adds the data nodes to the OPC UA server based on the Modbus mappings.
 *
 * @param server The OPC UA server instance.
 * @param config A pointer to the application configuration.
 */
void add_opcua_nodes(UA_Server* server, const modbus_opcua_config_t* config);

/**
 * @brief Updates a specific node on the OPC UA server with a new value.
 *
 * @param server The OPC UA server instance.
 * @param mapping The mapping corresponding to the node to be updated.
 * @param value The new value to write to the node.
 * @return UA_STATUSCODE_GOOD on success.
 */
UA_StatusCode update_opcua_node_value(UA_Server* server, const modbus_reg_mapping_t* mapping, float value);

/**
 * @brief Checks if a shutdown has been requested for the OPC UA server.
 *
 * @return Non-zero if shutdown is requested, zero otherwise.
 */
int opcua_shutdown_requested(void);

/**
 * @brief Handles shutdown signals for the OPC UA server.
 * 
 * @return The signal number that triggered the shutdown.
 */
int opcua_shutdown_signal(void);

#endif  // OPCUA_SERVER_H
