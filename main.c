#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <math.h> // For NAN
#include <sys/time.h> // For gettimeofday

#include "config.h"
#include "config_parser.h" // Use the new parser header
#include "modbus_client.h"
#include "opcua_server.h"
#include "logger.h"

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
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}


/**
 * @brief Processes raw Modbus register data, checking for NaN, handling
 * endianness (Modbus is Big Endian), and applying scaling.
 *
 * @param regs Pointer to the raw register data (uint16_t array).
 * @param mapping The configuration mapping for this data point.
 * @param out_value Pointer to a float where the result will be stored.
 * @return true if conversion is successful, false if a NaN value is detected.
 */
bool process_modbus_value(const uint16_t* regs, const modbus_reg_mapping_t* mapping, float* out_value) {
    if (strcmp(mapping->data_type, "U16") == 0) {
        if (regs[0] == SMA_NAN_U16) return false;
        *out_value = (float)regs[0] * mapping->scale;
    } else if (strcmp(mapping->data_type, "S16") == 0) {
        if (regs[0] == SMA_NAN_S16) return false;
        *out_value = (float)((int16_t)regs[0]) * mapping->scale;
    } else if (strcmp(mapping->data_type, "U32") == 0) {
        // Modbus (Big Endian): regs[0] is high word, regs[1] is low word.
        uint32_t val = ((uint32_t)regs[0] << 16) | regs[1];
        if (val == SMA_NAN_U32) return false;
        *out_value = (float)val * mapping->scale;
    } else if (strcmp(mapping->data_type, "S32") == 0) {
        uint32_t raw_val = ((uint32_t)regs[0] << 16) | regs[1];
        if (raw_val == SMA_NAN_S32) return false;
        *out_value = (float)((int32_t)raw_val) * mapping->scale;
    } else if (strcmp(mapping->data_type, "U64") == 0) {
        uint64_t val = ((uint64_t)regs[0] << 48) | ((uint64_t)regs[1] << 32) |
        ((uint64_t)regs[2] << 16) | regs[3];
        if (val == SMA_NAN_U64) return false;
        *out_value = (float)val * mapping->scale;
    } else {
        log_message(LOG_LEVEL_WARN, "Unsupported data type for '%s': %s", mapping->name, mapping->data_type);
        return false;
    }
    return true;
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path_to_config.yaml>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Load configuration from YAML file
    modbus_opcua_config_t* config = load_config_from_yaml(argv[1]);
    if (!config) {
        // Error is already logged by the parser
        return EXIT_FAILURE;
    }

    if (logger_init(config->log_file, config->log_level) != 0) {
        free_config(config);
        return EXIT_FAILURE;
    }

    log_message(LOG_LEVEL_INFO, "Configuration loaded successfully from %s.", argv[1]);

    modbus_t* modbus_ctx = NULL;
    UA_Server* opcua_server = opcua_server_init(config);
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
    int64_t* next_poll_times = calloc(config->num_mappings, sizeof(int64_t));
    if(!next_poll_times) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for poll timers.");
        running = false; // Trigger shutdown
    }

    while (running) {
        if (!modbus_ctx) {
            modbus_ctx = modbus_tcp_connect(config);
            if (!modbus_ctx) {
                // If connection fails, wait before retrying
                sleep(5);
                continue;
            }
        }

        int64_t current_time_ms = get_time_ms();

        for (int i = 0; i < config->num_mappings; i++) {
            // Check if it's time to poll this specific mapping
            if (current_time_ms < next_poll_times[i]) {
                continue; // Not yet time
            }

            // It's time to poll, set the next poll time
            next_poll_times[i] = current_time_ms + config->mappings[i].poll_interval_ms;

            uint16_t regs[4]; // Buffer for up to 4 registers (64-bit)
            if (read_modbus_data(modbus_ctx, &config->mappings[i], regs) == 0) {
                float value = NAN;
                if (process_modbus_value(regs, &config->mappings[i], &value)) {
                    log_message(LOG_LEVEL_DEBUG, "Read '%s': %f (Poll Rate: %dms)",
                                config->mappings[i].name, value, config->mappings[i].poll_interval_ms);
                    update_opcua_node_value(opcua_server, &config->mappings[i], value);
                } else {
                    log_message(LOG_LEVEL_WARN, "Received NaN for '%s' (Modbus Addr: %d). Skipping update.",
                                config->mappings[i].name, config->mappings[i].modbus_address);
                }
            } else {
                // Handle read error - try to reconnect
                modbus_close(modbus_ctx);
                modbus_free(modbus_ctx);
                modbus_ctx = NULL;
                log_message(LOG_LEVEL_ERROR, "Modbus read failed, will attempt to reconnect.");
                break; // Exit the for loop to attempt reconnection
            }
        }

        // We iterate the server more frequently to keep it responsive,
        // even if no Modbus polls are happening.
        UA_Server_run_iterate(opcua_server, false);
        usleep(100 * 1000); // Sleep for 100ms
    }

    free(next_poll_times);

    if (modbus_ctx) {
        modbus_close(modbus_ctx);
        modbus_free(modbus_ctx);
    }

    UA_Server_run_shutdown(opcua_server);
    UA_Server_delete(opcua_server);
    free_config(config);
    logger_close();

    log_message(LOG_LEVEL_INFO, "Application terminated cleanly.");
    return EXIT_SUCCESS;
}

