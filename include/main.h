#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <ctype.h>

#include "config.h"
#include "config_parser.h"
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
int64_t get_time_ms();

/**
 * @brief Processes raw Modbus register data according to SMA format specification.
 * @param regs Pointer to the raw register data (uint16_t array).
 * @param mapping The configuration mapping for this data point.
 * @param out_variant Pointer to UA_Variant where the result will be stored.
 * @return true if conversion is successful, false if a NaN value is detected.
 */
bool process_modbus_value_formatted(const uint16_t *regs, const modbus_reg_mapping_t *mapping, UA_Variant *out_variant);