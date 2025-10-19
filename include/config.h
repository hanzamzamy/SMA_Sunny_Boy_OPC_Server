#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stdint.h>

/*
 * @brief Represents an enumeration value mapping for ENUM format.
 */
typedef struct {
  int   value;  // The numeric value
  char* name;   // The text label
} enum_value_mapping_t;

/*
 * @brief Defines the configuration for a single Modbus register mapping.
 * This struct maps a Modbus register address to an OPC UA node.
 */
typedef struct {
  char* name;              // A descriptive name for the value being read
  int   modbus_address;    // The Modbus register address to read from
  char* opcua_node_id;     // The identifier for the corresponding OPC UA node
  char* data_type;         // The data type, e.g., "U16", "S32", "FLOAT32"
  char* format;            // Format: "FIXn", "ENUM", "FW", "DT", "TM", "Duration", "TEMP"
  float scale;             // A scaling factor to apply to the raw value (deprecated, use format)
  int   poll_interval_ms;  // Individual polling interval for this mapping
  
  // For ENUM format
  enum_value_mapping_t* enum_values;     // Array of enum mappings
  int                   num_enum_values; // Number of enum mappings
} modbus_reg_mapping_t;

/*
 * @brief Holds the complete configuration for the Modbus to OPC UA gateway.
 * This includes settings for the Modbus TCP connection, the OPC UA server,
 * and the register mappings.
 */
typedef struct {
  // Modbus TCP client configuration
  char* modbus_ip;
  int   modbus_port;
  int   modbus_slave_id;
  int   modbus_timeout_sec;
  int   modbus_poll_interval_ms;

  // OPC UA server configuration
  char*    opcua_server_url;
  uint16_t opcua_port;

  // Security configuration
  char* opcua_username;
  char* opcua_password;

  // Logging configuration
  char* log_file;
  int   log_level;  // 0: ERR, 1: WARN, 2: INFO, 3: DEBUG

  // Watchdog configuration
  int watchdog_sec;

  // Modbus to OPC UA mappings
  modbus_reg_mapping_t* mappings;
  int                   num_mappings;
} modbus_opcua_config_t;

#endif  // CONFIG_H