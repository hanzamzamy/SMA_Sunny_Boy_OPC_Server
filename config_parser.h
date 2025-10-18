#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include "config.h"  // The C struct definition

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Loads the gateway configuration from a YAML file.
 *
 * This function is implemented in C++ but exposed as a C function.
 * It parses the specified YAML file and populates a modbus_opcua_config_t struct.
 *
 * @param filename The path to the YAML configuration file.
 * @return A pointer to a newly allocated modbus_opcua_config_t struct, or NULL on error.
 */
modbus_opcua_config_t* load_config_from_yaml(const char* filename);

/**
 * @brief Frees all memory associated with a modbus_opcua_config_t struct.
 * @param config The configuration struct to free.
 */
void free_config(modbus_opcua_config_t* config);

#ifdef __cplusplus
}
#endif

#endif  // CONFIG_PARSER_H
