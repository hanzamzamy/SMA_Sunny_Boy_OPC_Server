#ifndef OPCUA_SERVER_H
#define OPCUA_SERVER_H

#include "config.h"
#include "open62541/plugin/accesscontrol_default.h"
#include "open62541/plugin/log_stdout.h"
#include "open62541/server.h"
#include "open62541/server_config_default.h"

// Expose the running flag
extern UA_Boolean running;

/**
 * @brief Configuration for historical data storage.
 */
typedef struct {
  size_t      max_history_entries;  // Maximum number of historical entries per node
  UA_Duration history_interval;     // History collection interval in milliseconds
} HistoryConfig;


/**
 * @brief Structure to hold historical data for a node.
 */
typedef struct {
    UA_NodeId nodeId;
    UA_DataValue *values;
    size_t maxSize;
    size_t currentSize;
    size_t currentIndex;
    pthread_mutex_t mutex;
} HistoryData;

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
 * @brief Updates a specific node on the OPC UA server with a new typed value.
 *
 * @param server The OPC UA server instance.
 * @param mapping The mapping corresponding to the node to be updated.
 * @param value The new typed value to write to the node.
 * @return UA_STATUSCODE_GOOD on success.
 */
UA_StatusCode update_opcua_node_value_typed(UA_Server* server, const modbus_reg_mapping_t* mapping, UA_Variant* value);

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

/**
 * @brief Adds a history node to the OPC UA server for historical data storage.
 *
 * @param server The OPC UA server instance.
 * @param nodeId The NodeId of the node to enable history for.
 * @param maxHistoryEntries The maximum number of historical entries to store.
 * @return UA_STATUSCODE_GOOD on success.
 */
UA_StatusCode opcua_add_history_node(UA_Server* server, UA_NodeId nodeId, size_t maxHistoryEntries);

/**
 * @brief Updates the historical data for a specific node.
 *
 * @param server The OPC UA server instance.
 * @param nodeId The NodeId of the node to update history for.
 * @param value The new value to add to the history.
 * @return UA_STATUSCODE_GOOD on success.
 */
UA_StatusCode opcua_update_history(UA_Server* server, UA_NodeId nodeId, UA_Variant* value);

/**
 * @brief Cleans up historical data resources on server shutdown.
 */
void opcua_cleanup_history(void);


/**
 * @brief Callback function to read historical data for a node.
 * 
 * @param server The OPC UA server instance.
 * @param sessionId The session ID of the client requesting the data.
 * @param sessionContext The session context.
 * @param nodeId The NodeId of the node to read history from.
 * @param sourceTimeStamp Whether to include source timestamps.
 * @param range The numeric range for the read.
 * @param timestampsToReturn The type of timestamps to return.
 * @param details The read details specifying time range and other parameters.
 * @param result Pointer to store the resulting historical data.
 * @return UA_STATUSCODE_GOOD on success, or an appropriate error code.
 */
UA_StatusCode readHistoryData(UA_Server* server, const UA_NodeId* sessionId, void* sessionContext, const UA_NodeId* nodeId,
                              UA_Boolean sourceTimeStamp, const UA_NumericRange* range, UA_TimestampsToReturn timestampsToReturn,
                              const UA_ReadRawModifiedDetails* details, UA_HistoryData* result);

/**
 * @brief Finds the historical data structure for a given node.
 * 
 * @param nodeId The NodeId of the node to find history data for.
 * @return Pointer to the HistoryData structure, or NULL if not found.
 */
HistoryData* findHistoryData(const UA_NodeId *nodeId);

#endif  // OPCUA_SERVER_H
