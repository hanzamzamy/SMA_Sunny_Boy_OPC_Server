#include <math.h>  // For NAN
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>  // For gettimeofday
#include <unistd.h>
#include <ctype.h>     // For isdigit

#include "config.h"
#include "config_parser.h"  // Use the new parser header
#include "logger.h"
#include "modbus_client.h"
#include "opcua_server.h"

// SMA Modbus profile defines NaN values for different data types.
// See section 3.6 in the SMA Modbus documentation.
#define SMA_NAN_S16 0x8000
#define SMA_NAN_S32 0x80000000
#define SMA_NAN_U16 0xFFFF
#define SMA_NAN_U32 0xFFFFFFFF
#define SMA_NAN_U64 0xFFFFFFFFFFFFFFFF

/**
 * @brief Gets the current time in milliseconds.
 * @return The current time as a 64-bit integer.
 */
static int64_t get_time_ms() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (int64_t) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/**
 * @brief Processes raw Modbus register data according to SMA format specification.
 * @param regs Pointer to the raw register data (uint16_t array).
 * @param mapping The configuration mapping for this data point.
 * @param out_variant Pointer to UA_Variant where the result will be stored.
 * @return true if conversion is successful, false if a NaN value is detected.
 */
bool process_modbus_value_formatted(const uint16_t *regs, const modbus_reg_mapping_t *mapping, UA_Variant *out_variant) {
  UA_Variant_init(out_variant);
  
  // First, combine registers according to data type and check for NaN
  uint64_t raw_value = 0;
  bool is_nan = false;
  
  if (strcmp(mapping->data_type, "U16") == 0) {
    raw_value = regs[0];
    is_nan = (regs[0] == SMA_NAN_U16);
  } else if (strcmp(mapping->data_type, "S16") == 0) {
    raw_value = regs[0];
    is_nan = (regs[0] == SMA_NAN_S16);
  } else if (strcmp(mapping->data_type, "U32") == 0) {
    raw_value = ((uint32_t) regs[0] << 16) | regs[1];
    is_nan = (raw_value == SMA_NAN_U32);
  } else if (strcmp(mapping->data_type, "S32") == 0) {
    raw_value = ((uint32_t) regs[0] << 16) | regs[1];
    is_nan = (raw_value == SMA_NAN_S32);
  } else if (strcmp(mapping->data_type, "U64") == 0) {
    raw_value = ((uint64_t) regs[0] << 48) | ((uint64_t) regs[1] << 32) | ((uint64_t) regs[2] << 16) | regs[3];
    is_nan = (raw_value == SMA_NAN_U64);
  } else {
    log_message(LOG_LEVEL_WARN, "Unsupported data type for '%s': %s", mapping->name, mapping->data_type);
    return false;
  }
  
  if (is_nan) {
    return false;
  }
  
  // Process according to format
  if (!mapping->format) {
    log_message(LOG_LEVEL_WARN, "No format specified for '%s', cannot process value.", mapping->name);
    return false;
  }
  
  // Format-specific processing
  if (strncmp(mapping->format, "FIX", 3) == 0) {
    // FIXn format
    int decimal_places = 0;
    if (strlen(mapping->format) > 3) {
      decimal_places = atoi(mapping->format + 3);
    }
    float scale = 1.0f;
    for (int i = 0; i < decimal_places; i++) {
      scale *= 0.1f;
    }
    
    float *float_val = UA_Float_new();
    if (strcmp(mapping->data_type, "S16") == 0 || strcmp(mapping->data_type, "S32") == 0) {
      *float_val = (float)((int64_t)raw_value) * scale;
    } else {
      *float_val = (float)raw_value * scale;
    }
    UA_Variant_setScalar(out_variant, float_val, &UA_TYPES[UA_TYPES_FLOAT]);
    
  } else if (strcmp(mapping->format, "ENUM") == 0) {
    // ENUM format
    UA_Int32 *int_val = UA_Int32_new();
    *int_val = (UA_Int32)raw_value;
    UA_Variant_setScalar(out_variant, int_val, &UA_TYPES[UA_TYPES_INT32]);
    
  } else if (strcmp(mapping->format, "FW") == 0) {
    // Firmware version format
    uint32_t fw_val = (uint32_t)raw_value;
    uint8_t major = (fw_val >> 24) & 0xFF;
    uint8_t minor = (fw_val >> 16) & 0xFF;
    uint8_t build = (fw_val >> 8) & 0xFF;
    uint8_t release = fw_val & 0xFF;
    
    char release_char = 'R';
    switch (release) {
      case 3: release_char = 'B'; break;
      case 4: release_char = 'R'; break;
      default: release_char = '?'; break;
    }
    
    char fw_string[32];
    snprintf(fw_string, sizeof(fw_string), "%d.%d.%d.%c", major, minor, build, release_char);
    
    UA_String *string_val = UA_String_new();
    *string_val = UA_STRING_ALLOC(fw_string);
    UA_Variant_setScalar(out_variant, string_val, &UA_TYPES[UA_TYPES_STRING]);
    
  } else if (strcmp(mapping->format, "DT") == 0 || strcmp(mapping->format, "TM") == 0) {
    // DateTime format
    uint32_t unix_timestamp = (uint32_t)raw_value;
    UA_DateTime *datetime_val = UA_DateTime_new();
    *datetime_val = (unix_timestamp + 11644473600ULL) * 10000000ULL;
    UA_Variant_setScalar(out_variant, datetime_val, &UA_TYPES[UA_TYPES_DATETIME]);
    
  } else if (strcmp(mapping->format, "Duration") == 0) {
    // Duration format (seconds to milliseconds)
    float *float_val = UA_Float_new();
    *float_val = (float)raw_value * 1000.0f;
    UA_Variant_setScalar(out_variant, float_val, &UA_TYPES[UA_TYPES_FLOAT]);
    
  } else if (strcmp(mapping->format, "TEMP") == 0) {
    // Temperature format (like FIX1)
    float *float_val = UA_Float_new();
    if (strcmp(mapping->data_type, "S32") == 0) {
      *float_val = (float)((int32_t)raw_value) * 0.1f;
    } else {
      *float_val = (float)raw_value * 0.1f;
    }
    UA_Variant_setScalar(out_variant, float_val, &UA_TYPES[UA_TYPES_FLOAT]);
    
  } else {
    log_message(LOG_LEVEL_WARN, "Unknown format '%s' for '%s', using raw value", mapping->format, mapping->name);
    float *float_val = UA_Float_new();
    *float_val = (float)raw_value;
    UA_Variant_setScalar(out_variant, float_val, &UA_TYPES[UA_TYPES_FLOAT]);
  }
  
  return true;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <path_to_config.yaml>\n", argv[0]);
    return EXIT_FAILURE;
  }

  // Load configuration from YAML file
  modbus_opcua_config_t *config = load_config_from_yaml(argv[1]);
  if (!config) {
    // Error is already logged by the parser
    return EXIT_FAILURE;
  }

  if (logger_init(config->log_file, config->log_level) != 0) {
    free_config(config);
    return EXIT_FAILURE;
  }

  log_message(LOG_LEVEL_INFO, "Configuration loaded successfully from %s.", argv[1]);

  modbus_t  *modbus_ctx   = NULL;
  UA_Server *opcua_server = opcua_server_init(config);
  add_opcua_nodes(opcua_server, config);

  UA_StatusCode retval = UA_Server_run_startup(opcua_server);
  if (retval != UA_STATUSCODE_GOOD) {
    log_message(LOG_LEVEL_ERROR, "OPC UA server startup failed with status code %s.", UA_StatusCode_name(retval));
    free_config(config);
    UA_Server_delete(opcua_server);
    logger_close();
    return EXIT_FAILURE;
  }
  log_message(LOG_LEVEL_INFO, "OPC UA Server is running on port %d.", config->opcua_port);

  // Array to track the next poll time for each mapping
  int64_t *next_poll_times = calloc(config->num_mappings, sizeof(int64_t));
  if (!next_poll_times) {
    log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for poll timers.");

    // Perform graceful shutdown/cleanup since we can't continue.
    // At this point modbus_ctx is NULL, but handle defensively.
    if (modbus_ctx) {
      modbus_close(modbus_ctx);
      modbus_free(modbus_ctx);
      modbus_ctx = NULL;
    }

    // Stop and delete OPC UA server
    UA_Server_run_shutdown(opcua_server);
    UA_Server_delete(opcua_server);

    // Free config and close logger
    free_config(config);
    logger_close();

    return EXIT_FAILURE;
  }

  while (!opcua_shutdown_requested()) {
    if (!modbus_ctx) {
      modbus_ctx = modbus_tcp_connect(config);
      if (!modbus_ctx) {
        if (opcua_shutdown_requested())
          break;
        sleep(5);
        continue;
      }
    }

    int64_t current_time_ms = get_time_ms();

    for (int i = 0; i < config->num_mappings; i++) {
      if (opcua_shutdown_requested())
        break;

      if (current_time_ms < next_poll_times[i]) {
        continue;
      }

      next_poll_times[i] = current_time_ms + config->mappings[i].poll_interval_ms;

      uint16_t regs[4];
      int      read_rc = read_modbus_data(modbus_ctx, &config->mappings[i], regs);
      if (read_rc == 0) {
        UA_Variant ua_value;
        if (process_modbus_value_formatted(regs, &config->mappings[i], &ua_value)) {
          // Log the value based on its type
          if (ua_value.type == &UA_TYPES[UA_TYPES_FLOAT]) {
            float val = *(UA_Float*)ua_value.data;
            log_message(LOG_LEVEL_DEBUG, "Read '%s': %f (Poll Rate: %dms)", config->mappings[i].name, val, config->mappings[i].poll_interval_ms);
          } else if (ua_value.type == &UA_TYPES[UA_TYPES_INT32]) {
            int32_t val = *(UA_Int32*)ua_value.data;
            
            // For ENUM format, try to find the corresponding string
            if (config->mappings[i].format && strcmp(config->mappings[i].format, "ENUM") == 0) {
              const char* enum_string = "Unknown";
              for (int j = 0; j < config->mappings[i].num_enum_values; j++) {
                if (config->mappings[i].enum_values[j].value == val) {
                  enum_string = config->mappings[i].enum_values[j].name;
                  break;
                }
              }
              log_message(LOG_LEVEL_DEBUG, "Read '%s': %d (%s) (Poll Rate: %dms)", 
                         config->mappings[i].name, val, enum_string, config->mappings[i].poll_interval_ms);
            } else {
              log_message(LOG_LEVEL_DEBUG, "Read '%s': %d (Poll Rate: %dms)", 
                         config->mappings[i].name, val, config->mappings[i].poll_interval_ms);
            }
          } else if (ua_value.type == &UA_TYPES[UA_TYPES_STRING]) {
            UA_String *str = (UA_String*)ua_value.data;
            log_message(LOG_LEVEL_DEBUG, "Read '%s': %.*s (Poll Rate: %dms)", config->mappings[i].name, (int)str->length, str->data, config->mappings[i].poll_interval_ms);
          } else {
            log_message(LOG_LEVEL_DEBUG, "Read '%s': (complex type) (Poll Rate: %dms)", config->mappings[i].name, config->mappings[i].poll_interval_ms);
          }
          
          update_opcua_node_value_typed(opcua_server, &config->mappings[i], &ua_value);
          UA_Variant_clear(&ua_value);
        } else {
          log_message(LOG_LEVEL_WARN, "Received NaN for '%s' (Modbus Addr: %d). Skipping update.", config->mappings[i].name,
                      config->mappings[i].modbus_address);
        }
      } else if (read_rc == -2) {
        break;
      } else {
        modbus_close(modbus_ctx);
        modbus_free(modbus_ctx);
        modbus_ctx = NULL;
        log_message(LOG_LEVEL_ERROR, "Modbus read failed, will attempt to reconnect.");
        break;
      }
    }

    UA_Server_run_iterate(opcua_server, false);
    usleep(100 * 1000);
  }

  /* Log the fact that shutdown was requested (safe context) */
  int sig = opcua_shutdown_signal();
  if (sig != 0) {
    log_message(LOG_LEVEL_INFO, "Received signal %d, shutting down.", sig);
  } else {
    log_message(LOG_LEVEL_INFO, "Shutdown requested, stopping.");
  }

  free(next_poll_times);

  if (modbus_ctx) {
    modbus_close(modbus_ctx);
    modbus_free(modbus_ctx);
  }

  UA_Server_run_shutdown(opcua_server);
  UA_Server_delete(opcua_server);
  free_config(config);

  log_message(LOG_LEVEL_INFO, "Application terminated cleanly.");
  logger_close();
  
  return EXIT_SUCCESS;
}
