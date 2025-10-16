#ifndef OPCUA_SERVER_H
#define OPCUA_SERVER_H

#include "open62541/server.h"
#include "config.h"

// Expose the running flag
extern UA_Boolean running;

/**
 * @brief Initializes and configures the OPC UA server, including security.
 *
 * @param config A pointer to the application configuration.
 * @return A pointer to the configured UA_Server object, or NULL on failure.
 */
UA_Server* opcua_server_init(const config_t* config);

/**
 * @brief Adds the data nodes to the OPC UA server based on the Modbus mappings.
 *
 * @param server The OPC UA server instance.
 * @param config A pointer to the application configuration.
 */
void add_opcua_nodes(UA_Server* server, const config_t* config);

/**
 * @brief Updates a specific node on the OPC UA server with a new value.
 *
 * @param server The OPC UA server instance.
 * @param mapping The mapping corresponding to the node to be updated.
 * @param value The new value to write to the node.
 * @return UA_STATUSCODE_GOOD on success.
 */
UA_StatusCode update_opcua_node_value(UA_Server* server, const modbus_mapping_t* mapping, float value);

#endif // OPCUA_SERVER_H

