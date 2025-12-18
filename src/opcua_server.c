#include "opcua_server.h"

#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "logger.h"

static volatile sig_atomic_t shutdown_requested  = 0;
static volatile sig_atomic_t shutdown_signal_num = 0;

static HistoryData *historyNodes = NULL;
static size_t historyNodeCount = 0;
static pthread_mutex_t historyMutex = PTHREAD_MUTEX_INITIALIZER;

static void stop_handler(int sig) {
  shutdown_signal_num = sig;
  shutdown_requested  = 1;
}

int opcua_shutdown_requested(void) {
  return shutdown_requested != 0;
}

int opcua_shutdown_signal(void) {
  return shutdown_signal_num;
}

// Struct to hold user credentials for the callback
static struct {
  UA_String username;
  UA_String password;
} user_credentials;

static UA_StatusCode
usernamePasswordLogin(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext, const UA_String *userName, const UA_String *password) {
  if (UA_String_equal(userName, &user_credentials.username) && UA_String_equal(password, &user_credentials.password)) {
    log_message(LOG_LEVEL_INFO, "Successful login by user: %.*s", (int) userName->length, userName->data);
    return UA_STATUSCODE_GOOD;
  }
  log_message(LOG_LEVEL_WARN, "Failed login attempt by user: %.*s", (int) userName->length, userName->data);
  return UA_STATUSCODE_BADIDENTITYTOKENINVALID;
}

UA_Server *opcua_server_init(const modbus_opcua_config_t *config) {
  signal(SIGINT, stop_handler);
  signal(SIGTERM, stop_handler);

  UA_Server       *server    = UA_Server_new();
  UA_ServerConfig *ua_config = UA_Server_getConfig(server);
  UA_ServerConfig_setMinimal(ua_config, config->opcua_port, NULL);

  // Setup user authentication if username and password are provided
  if (config->opcua_username && config->opcua_username[0] != '\0' && config->opcua_password) {
    log_message(LOG_LEVEL_INFO, "OPC UA security enabled with user: %s", config->opcua_username);

    user_credentials.username = UA_STRING((char *) config->opcua_username);
    user_credentials.password = UA_STRING((char *) config->opcua_password);

    // Define a user/password login provider
    UA_UsernamePasswordLogin logins[1] = {{user_credentials.username, user_credentials.password}};

    // Set the access control plugin to the server config
    ua_config->accessControl.clear(&ua_config->accessControl);
    UA_AccessControl_default(ua_config, false, &ua_config->securityPolicies[ua_config->securityPoliciesSize - 1].policyUri, 1, logins);
  } else {
    log_message(LOG_LEVEL_WARN, "OPC UA security is disabled. No username/password configured.");
  }

  return server;
}

void add_opcua_nodes(UA_Server *server, const modbus_opcua_config_t *config) {
  for (int i = 0; i < config->num_mappings; i++) {
    modbus_reg_mapping_t* mapping = &config->mappings[i];
    UA_VariableAttributes attr    = UA_VariableAttributes_default;
    attr.displayName              = UA_LOCALIZEDTEXT("en-US", mapping->name);
    attr.accessLevel              = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    UA_NodeId node_id = UA_NODEID_STRING(1, mapping->opcua_node_id);

    // Set the appropriate OPC UA data type based on format
    if (mapping->format) {
      if (strcmp(mapping->format, "ENUM") == 0 && mapping->enum_values && mapping->num_enum_values > 0) {
        // Create a proper OPC UA Enumeration DataType
        char enum_type_name[128];
        snprintf(enum_type_name, sizeof(enum_type_name), "%s_EnumType", mapping->name);
        
        char enum_type_id[256];
        snprintf(enum_type_id, sizeof(enum_type_id), "EnumType.%s", mapping->opcua_node_id);
        UA_NodeId enum_type_node_id = UA_NODEID_STRING(1, enum_type_id);
        
        // Create the DataType attributes
        UA_DataTypeAttributes dt_attr = UA_DataTypeAttributes_default;
        dt_attr.displayName = UA_LOCALIZEDTEXT("en-US", enum_type_name);
        dt_attr.description = UA_LOCALIZEDTEXT("en-US", "Custom enumeration type");
        
        // Add the custom DataType node (inherits from Enumeration)
        UA_StatusCode result = UA_Server_addDataTypeNode(server, 
                                                        enum_type_node_id,
                                                        UA_NODEID_NUMERIC(0, UA_NS0ID_ENUMERATION),
                                                        UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
                                                        UA_QUALIFIEDNAME(1, enum_type_name),
                                                        dt_attr, 
                                                        NULL, 
                                                        NULL);
        
        if (result == UA_STATUSCODE_GOOD) {
          log_message(LOG_LEVEL_INFO, "Created enum DataType for '%s' with %d values", mapping->name, mapping->num_enum_values);
          
          // Create EnumValues property for the DataType
          char enum_values_id[300];
          snprintf(enum_values_id, sizeof(enum_values_id), "EnumValues.%s", mapping->opcua_node_id);
          UA_NodeId enum_values_node_id = UA_NODEID_STRING(1, enum_values_id);
          
          // Create EnumValueType array
          UA_EnumValueType *enum_value_types = (UA_EnumValueType*)UA_Array_new(mapping->num_enum_values, &UA_TYPES[UA_TYPES_ENUMVALUETYPE]);
          for (int j = 0; j < mapping->num_enum_values; j++) {
            enum_value_types[j].value = mapping->enum_values[j].value;
            enum_value_types[j].displayName = UA_LOCALIZEDTEXT_ALLOC("en-US", mapping->enum_values[j].name);
            enum_value_types[j].description = UA_LOCALIZEDTEXT_ALLOC("en-US", mapping->enum_values[j].name);
          }
          
          UA_VariableAttributes enum_values_attr = UA_VariableAttributes_default;
          enum_values_attr.displayName = UA_LOCALIZEDTEXT("en-US", "EnumValues");
          enum_values_attr.description = UA_LOCALIZEDTEXT("en-US", "Enumeration value definitions");
          enum_values_attr.dataType = UA_TYPES[UA_TYPES_ENUMVALUETYPE].typeId;
          enum_values_attr.valueRank = 1; // Array
          enum_values_attr.arrayDimensionsSize = 1;
          UA_UInt32 arrayDim = mapping->num_enum_values;
          enum_values_attr.arrayDimensions = &arrayDim;
          
          UA_Variant enum_values_variant;
          UA_Variant_init(&enum_values_variant);
          UA_Variant_setArray(&enum_values_variant, enum_value_types, mapping->num_enum_values, &UA_TYPES[UA_TYPES_ENUMVALUETYPE]);
          enum_values_attr.value = enum_values_variant;
          
          // Add EnumValues as a property of the DataType
          UA_StatusCode enum_prop_result = UA_Server_addVariableNode(server,
                                   enum_values_node_id,
                                   enum_type_node_id,
                                   UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                                   UA_QUALIFIEDNAME(0, "EnumValues"),
                                   UA_NODEID_NUMERIC(0, UA_NS0ID_PROPERTYTYPE),
                                   enum_values_attr,
                                   NULL, NULL);
          
          if (enum_prop_result == UA_STATUSCODE_GOOD) {
            log_message(LOG_LEVEL_INFO, "Added EnumValues property to DataType '%s'", enum_type_name);
            
            // Create the variable using Int32 type
            // The enum information is stored in the DataType definition
            attr.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
            
            // Set initial value to the first enum value
            UA_Int32 initial_value = mapping->enum_values[0].value;
            UA_Variant_setScalar(&attr.value, &initial_value, &UA_TYPES[UA_TYPES_INT32]);
            
            // Add a custom attribute to reference the enum DataType
            attr.description = UA_LOCALIZEDTEXT_ALLOC("en-US", enum_type_name);
            
            // Create the variable node
            UA_StatusCode var_result = UA_Server_addVariableNode(server, node_id, 
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), 
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                      UA_QUALIFIEDNAME(1, mapping->name), 
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), 
                                      attr, NULL, NULL);
            
            if (var_result == UA_STATUSCODE_GOOD) {
              log_message(LOG_LEVEL_INFO, "Created enum variable '%s' with node ID '%s'", mapping->name, mapping->opcua_node_id);
              
              // Add a reference from the variable to the enum DataType for better SCADA recognition
              char ref_prop_id[300];
              snprintf(ref_prop_id, sizeof(ref_prop_id), "EnumDataType.%s", mapping->opcua_node_id);
              UA_NodeId ref_prop_node_id = UA_NODEID_STRING(1, ref_prop_id);
              
              UA_VariableAttributes ref_attr = UA_VariableAttributes_default;
              ref_attr.displayName = UA_LOCALIZEDTEXT("en-US", "EnumDataType");
              ref_attr.description = UA_LOCALIZEDTEXT("en-US", "Reference to enumeration DataType");
              ref_attr.dataType = UA_TYPES[UA_TYPES_NODEID].typeId;
              
              UA_Variant ref_variant;
              UA_Variant_init(&ref_variant);
              UA_Variant_setScalar(&ref_variant, &enum_type_node_id, &UA_TYPES[UA_TYPES_NODEID]);
              ref_attr.value = ref_variant;
              
              UA_Server_addVariableNode(server,
                                       ref_prop_node_id,
                                       node_id,
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                                       UA_QUALIFIEDNAME(0, "EnumDataType"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_PROPERTYTYPE),
                                       ref_attr,
                                       NULL, NULL);
              
            } else {
              log_message(LOG_LEVEL_ERROR, "Failed to create enum variable '%s': 0x%08x", mapping->name, var_result);
            }
          } else {
            log_message(LOG_LEVEL_ERROR, "Failed to add EnumValues property: 0x%08x", enum_prop_result);
          }
          
        } else {
          log_message(LOG_LEVEL_WARN, "Failed to create enum DataType for '%s', falling back to Int32", mapping->name);
          // Fallback to Int32
          attr.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
          UA_Int32 value = 0;
          UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_INT32]);
          
          UA_Server_addVariableNode(server, node_id, 
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), 
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                  UA_QUALIFIEDNAME(1, mapping->name), 
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), 
                                  attr, NULL, NULL);
        }
        
      } else if (strcmp(mapping->format, "ENUM") == 0) {
        log_message(LOG_LEVEL_WARN, "ENUM format specified for '%s' but no enum_values provided, using Int32", mapping->name);
        // Fallback to Int32 if no enum values
        attr.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
        UA_Int32 value = 0;
        UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_INT32]);
        
        // Create variable node
        UA_Server_addVariableNode(server, node_id, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                  UA_QUALIFIEDNAME(1, mapping->name), UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);
        
      } else if (strcmp(mapping->format, "FW") == 0) {
        // Firmware version as string
        attr.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
        UA_String value = UA_STRING_NULL;
        UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_STRING]);
        
        // Create variable node
        UA_Server_addVariableNode(server, node_id, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                  UA_QUALIFIEDNAME(1, mapping->name), UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);
        
      } else if (strcmp(mapping->format, "DT") == 0 || strcmp(mapping->format, "TM") == 0) {
        // DateTime
        attr.dataType = UA_TYPES[UA_TYPES_DATETIME].typeId;
        UA_DateTime value = 0;
        UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_DATETIME]);
        
        // Create variable node
        UA_Server_addVariableNode(server, node_id, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                  UA_QUALIFIEDNAME(1, mapping->name), UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);
        
      } else if (strcmp(mapping->format, "Duration") == 0) {
        // Duration as Float (milliseconds)
        attr.dataType = UA_TYPES[UA_TYPES_FLOAT].typeId;
        UA_Float value = 0.0f;
        UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_FLOAT]);
        
        // Create variable node
        UA_Server_addVariableNode(server, node_id, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                  UA_QUALIFIEDNAME(1, mapping->name), UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);
        
      } else {
        // FIXn, TEMP, or other numeric formats - use Float
        attr.dataType = UA_TYPES[UA_TYPES_FLOAT].typeId;
        UA_Float value = 0.0f;
        UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_FLOAT]);
        
        // Create variable node
        UA_Server_addVariableNode(server, node_id, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                  UA_QUALIFIEDNAME(1, mapping->name), UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);
      }
    } else {
      // No format specified, default to Float
      attr.dataType = UA_TYPES[UA_TYPES_FLOAT].typeId;
      UA_Float value = 0.0f;
      UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_FLOAT]);
      
      // Create variable node
      UA_Server_addVariableNode(server, node_id, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                UA_QUALIFIEDNAME(1, mapping->name), UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);
    }
  }
}

UA_StatusCode update_opcua_node_value_typed(UA_Server *server, const modbus_reg_mapping_t *mapping, UA_Variant *value) {
  UA_NodeId node_id = UA_NODEID_STRING(1, (char *) mapping->opcua_node_id);

  // Write and log result
  UA_DataValue dv;
  UA_DataValue_init(&dv);
  dv.hasValue = true;
  dv.value = *value;
  dv.hasSourceTimestamp = true;
  dv.sourceTimestamp = UA_DateTime_now();
  dv.hasServerTimestamp = true;
  dv.serverTimestamp = dv.sourceTimestamp;
  dv.status = UA_STATUSCODE_GOOD;

  // Write with timestamps
  UA_StatusCode rc = UA_Server_writeDataValue(server, node_id, dv);
  if (rc != UA_STATUSCODE_GOOD) {
    log_message(LOG_LEVEL_ERROR, "UA_Server_writeDataValue failed for '%s' (NodeId=%s): 0x%08x", mapping->name, mapping->opcua_node_id, rc);
    return rc;
  }

  // Read back to verify
  UA_Variant read_back;
  UA_Variant_init(&read_back);
  rc = UA_Server_readValue(server, node_id, &read_back);
  if (rc != UA_STATUSCODE_GOOD) {
    log_message(LOG_LEVEL_WARN, "UA_Server_readValue failed for '%s' after write: 0x%08x", mapping->name, rc);
  } else {
    if (read_back.type == &UA_TYPES[UA_TYPES_FLOAT]) {
      float v = *(UA_Float*)read_back.data;
      log_message(LOG_LEVEL_DEBUG, "Wrote/Read back '%s' = %f", mapping->name, v);
    } else if (read_back.type == &UA_TYPES[UA_TYPES_INT32]) {
      int32_t v = *(UA_Int32*)read_back.data;
      log_message(LOG_LEVEL_DEBUG, "Wrote/Read back '%s' = %d", mapping->name, v);
    } else if (read_back.type == &UA_TYPES[UA_TYPES_STRING]) {
      UA_String *s = (UA_String*)read_back.data;
      log_message(LOG_LEVEL_DEBUG, "Wrote/Read back '%s' = %.*s", mapping->name, (int)s->length, s->data);
    } else {
      log_message(LOG_LEVEL_DEBUG, "Wrote/Read back '%s' (type %d)", mapping->name, (int)read_back.type->typeId.identifier.numeric);
    }
    UA_Variant_clear(&read_back);
  }

  return UA_STATUSCODE_GOOD;
}

UA_StatusCode update_opcua_node_value(UA_Server *server, const modbus_reg_mapping_t *mapping, float value) {
  UA_NodeId  node_id = UA_NODEID_STRING(1, (char *) mapping->opcua_node_id);
  UA_Variant ua_value;
  UA_Variant_init(&ua_value);
  UA_Variant_setScalar(&ua_value, &value, &UA_TYPES[UA_TYPES_FLOAT]);
  return UA_Server_writeValue(server, node_id, ua_value);
}

HistoryData* findHistoryData(const UA_NodeId *nodeId) {
    pthread_mutex_lock(&historyMutex);
    for(size_t i = 0; i < historyNodeCount; i++) {
        if(UA_NodeId_equal(&historyNodes[i].nodeId, nodeId)) {
            pthread_mutex_unlock(&historyMutex);
            return &historyNodes[i];
        }
    }
    pthread_mutex_unlock(&historyMutex);
    return NULL;
}

UA_StatusCode
readHistoryData(UA_Server *server, const UA_NodeId *sessionId,
                void *sessionContext, const UA_NodeId *nodeId,
                UA_Boolean sourceTimeStamp,
                const UA_NumericRange *range,
                UA_TimestampsToReturn timestampsToReturn,
                const UA_ReadRawModifiedDetails *details,
                UA_HistoryData *result) {
    
    HistoryData *hd = findHistoryData(nodeId);
    if(!hd)
        return UA_STATUSCODE_BADNODEIDUNKNOWN;

    pthread_mutex_lock(&hd->mutex);
    
    // Filter data based on time range
    size_t count = 0;
    for(size_t i = 0; i < hd->currentSize; i++) {
        UA_DateTime ts = hd->values[i].sourceTimestamp;
        if(ts >= details->startTime && ts <= details->endTime) {
            count++;
        }
    }

    if(count == 0) {
        pthread_mutex_unlock(&hd->mutex);
        return UA_STATUSCODE_GOOD;
    }

    result->dataValuesSize = count;
    result->dataValues = (UA_DataValue*)UA_Array_new(count, &UA_TYPES[UA_TYPES_DATAVALUE]);
    
    size_t idx = 0;
    for(size_t i = 0; i < hd->currentSize && idx < count; i++) {
        UA_DateTime ts = hd->values[i].sourceTimestamp;
        if(ts >= details->startTime && ts <= details->endTime) {
            UA_DataValue_copy(&hd->values[i], &result->dataValues[idx]);
            idx++;
        }
    }

    pthread_mutex_unlock(&hd->mutex);
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode opcua_add_history_node(UA_Server *server, UA_NodeId nodeId, 
                                      size_t maxHistoryEntries) {
    pthread_mutex_lock(&historyMutex);
    
    /* Allocate or expand history nodes array */
    HistoryData *temp = realloc(historyNodes, (historyNodeCount + 1) * sizeof(HistoryData));
    if(!temp) {
        pthread_mutex_unlock(&historyMutex);
        return UA_STATUSCODE_BADOUTOFMEMORY;
    }
    historyNodes = temp;
    
    HistoryData *hd = &historyNodes[historyNodeCount];
    UA_NodeId_copy(&nodeId, &hd->nodeId);
    hd->values = (UA_DataValue*)UA_Array_new(maxHistoryEntries, &UA_TYPES[UA_TYPES_DATAVALUE]);
    hd->maxSize = maxHistoryEntries;
    hd->currentSize = 0;
    hd->currentIndex = 0;
    pthread_mutex_init(&hd->mutex, NULL);
    
    historyNodeCount++;
    pthread_mutex_unlock(&historyMutex);
    
    /* Enable historizing on the node */
    UA_HistorizingNodeIdSettings setting;
    setting.historizingBackend = UA_HistoryDataBackend_Memory(maxHistoryEntries, 100);
    setting.maxHistoryDataResponseSize = 100;
    setting.historizingUpdateStrategy = UA_HISTORIZINGUPDATESTRATEGY_VALUESET;
    
    UA_Server_setHistorizingSetting(server, nodeId, setting);
    
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode opcua_update_history(UA_Server *server, UA_NodeId nodeId, 
                                    UA_Variant *value) {
    HistoryData *hd = findHistoryData(&nodeId);
    if(!hd)
        return UA_STATUSCODE_BADNODEIDUNKNOWN;
    
    pthread_mutex_lock(&hd->mutex);
    
    // Create new data value with timestamp
    UA_DataValue dv;
    UA_DataValue_init(&dv);
    UA_Variant_copy(value, &dv.value);
    dv.hasValue = true;
    dv.sourceTimestamp = UA_DateTime_now();
    dv.hasSourceTimestamp = true;
    dv.hasServerTimestamp = true;
    dv.serverTimestamp = UA_DateTime_now();
    
    // Store in circular buffer
    size_t idx = hd->currentIndex;
    if(hd->currentSize < hd->maxSize) {
        hd->currentSize++;
    } else {
        UA_DataValue_clear(&hd->values[idx]);
    }
    
    hd->values[idx] = dv;
    hd->currentIndex = (hd->currentIndex + 1) % hd->maxSize;
    
    pthread_mutex_unlock(&hd->mutex);
    
    return UA_STATUSCODE_GOOD;
}

void opcua_cleanup_history(void) {
    pthread_mutex_lock(&historyMutex);
    for(size_t i = 0; i < historyNodeCount; i++) {
        pthread_mutex_lock(&historyNodes[i].mutex);
        UA_NodeId_clear(&historyNodes[i].nodeId);
        UA_Array_delete(historyNodes[i].values, historyNodes[i].currentSize, 
                       &UA_TYPES[UA_TYPES_DATAVALUE]);
        pthread_mutex_unlock(&historyNodes[i].mutex);
        pthread_mutex_destroy(&historyNodes[i].mutex);
    }
    free(historyNodes);
    historyNodes = NULL;
    historyNodeCount = 0;
    pthread_mutex_unlock(&historyMutex);
}