#include "opcua_server.h"

#include <signal.h>
#include <string.h>

#include "logger.h"

static volatile sig_atomic_t shutdown_requested  = 0;
static volatile sig_atomic_t shutdown_signal_num = 0;

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
    attr.accessLevel              = UA_ACCESSLEVELMASK_READ;

    UA_NodeId node_id = UA_NODEID_STRING(1, mapping->opcua_node_id);

    // Set the appropriate OPC UA data type based on format
    if (mapping->format) {
      if (strcmp(mapping->format, "ENUM") == 0) {
        // For ENUM format, use Int32
        attr.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
        UA_Int32 value = 0;
        UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_INT32]);
        
      } else if (strcmp(mapping->format, "FW") == 0) {
        // Firmware version as string
        attr.dataType = UA_TYPES[UA_TYPES_STRING].typeId;
        UA_String value = UA_STRING_NULL;
        UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_STRING]);
        
      } else if (strcmp(mapping->format, "DT") == 0 || strcmp(mapping->format, "TM") == 0) {
        // DateTime
        attr.dataType = UA_TYPES[UA_TYPES_DATETIME].typeId;
        UA_DateTime value = 0;
        UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_DATETIME]);
        
      } else if (strcmp(mapping->format, "Duration") == 0) {
        // Duration as Float (milliseconds)
        attr.dataType = UA_TYPES[UA_TYPES_FLOAT].typeId;
        UA_Float value = 0.0f;
        UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_FLOAT]);
        
      } else {
        // FIXn, TEMP, or other numeric formats - use Float
        attr.dataType = UA_TYPES[UA_TYPES_FLOAT].typeId;
        UA_Float value = 0.0f;
        UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_FLOAT]);
      }
    } else {
      // No format specified, default to Float
      attr.dataType = UA_TYPES[UA_TYPES_FLOAT].typeId;
      UA_Float value = 0.0f;
      UA_Variant_setScalar(&attr.value, &value, &UA_TYPES[UA_TYPES_FLOAT]);
    }

    UA_Server_addVariableNode(server, node_id, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                              UA_QUALIFIEDNAME(1, mapping->name), UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);
  }
}

// New function to update different data types
UA_StatusCode update_opcua_node_value_typed(UA_Server *server, const modbus_reg_mapping_t *mapping, UA_Variant *value) {
  UA_NodeId node_id = UA_NODEID_STRING(1, (char *) mapping->opcua_node_id);
  return UA_Server_writeValue(server, node_id, *value);
}

UA_StatusCode update_opcua_node_value(UA_Server *server, const modbus_reg_mapping_t *mapping, float value) {
  UA_NodeId  node_id = UA_NODEID_STRING(1, (char *) mapping->opcua_node_id);
  UA_Variant ua_value;
  UA_Variant_init(&ua_value);
  UA_Variant_setScalar(&ua_value, &value, &UA_TYPES[UA_TYPES_FLOAT]);
  return UA_Server_writeValue(server, node_id, ua_value);
}