#include "config_parser.h"

#include <yaml-cpp/yaml.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "logger.h"

// Helper to safely get a string value from a YAML node
static char* get_string(const YAML::Node& node) {
  if (!node || !node.IsScalar()) {
    return nullptr;
  }
  return strdup(node.as<std::string>().c_str());
}

extern "C" modbus_opcua_config_t* load_config_from_yaml(const char* filename) {
  try {
    YAML::Node yaml_config = YAML::LoadFile(filename);

    modbus_opcua_config_t* config = (modbus_opcua_config_t*) calloc(1, sizeof(modbus_opcua_config_t));
    if (!config) {
      log_message(LOG_LEVEL_ERROR, "Failed to allocate memory for config struct.");
      return NULL;
    }

    // Parse Modbus settings
    const auto& modbus_node    = yaml_config["modbus"];
    config->modbus_ip          = get_string(modbus_node["ip"]);
    config->modbus_port        = modbus_node["port"].as<int>();
    config->modbus_slave_id    = modbus_node["slave_id"].as<int>();
    config->modbus_timeout_sec = modbus_node["timeout_sec"].as<int>();

    // Parse OPC UA settings
    config->opcua_port = yaml_config["opcua"]["port"].as<int>();

    // Parse Security settings
    const auto& security_node = yaml_config["security"];
    config->opcua_username    = get_string(security_node["username"]);
    config->opcua_password    = get_string(security_node["password"]);

    // Parse Logging settings
    const auto& logging_node = yaml_config["logging"];
    config->log_file         = get_string(logging_node["file"]);
    config->log_level        = logging_node["level"].as<int>();

    // Parse Mappings
    const auto& mappings_node = yaml_config["mappings"];
    if (mappings_node && mappings_node.IsSequence()) {
      config->num_mappings = mappings_node.size();
      config->mappings     = (modbus_reg_mapping_t*) calloc(config->num_mappings, sizeof(modbus_reg_mapping_t));

      for (size_t i = 0; i < config->num_mappings; ++i) {
        const auto& mapping_node             = mappings_node[i];
        config->mappings[i].name             = get_string(mapping_node["name"]);
        config->mappings[i].modbus_address   = mapping_node["modbus_address"].as<int>();
        config->mappings[i].opcua_node_id    = get_string(mapping_node["opcua_node_id"]);
        config->mappings[i].data_type        = get_string(mapping_node["data_type"]);
        config->mappings[i].format           = get_string(mapping_node["format"]);
        config->mappings[i].scale            = mapping_node["scale"] ? mapping_node["scale"].as<float>() : 1.0f;
        config->mappings[i].poll_interval_ms = mapping_node["poll_interval_ms"].as<int>();

        // Parse enum_values if present
        if (mapping_node["enum_values"]) {
          const auto& enum_node               = mapping_node["enum_values"];
          config->mappings[i].num_enum_values = enum_node.size();
          config->mappings[i].enum_values     = (enum_value_mapping_t*) calloc(config->mappings[i].num_enum_values, sizeof(enum_value_mapping_t));

          int j = 0;
          for (auto it = enum_node.begin(); it != enum_node.end(); ++it, ++j) {
            config->mappings[i].enum_values[j].value = it->first.as<int>();
            config->mappings[i].enum_values[j].name  = strdup(it->second.as<std::string>().c_str());
          }
        }
      }
    }

    return config;

  } catch (const YAML::Exception& e) {
    log_message(LOG_LEVEL_ERROR, "Failed to parse YAML file '%s': %s", filename, e.what());
    return NULL;
  }
}

extern "C" void free_config(modbus_opcua_config_t* config) {
  if (!config) {
    return;
  }

  free(config->modbus_ip);
  free(config->opcua_username);
  free(config->opcua_password);
  free(config->log_file);

  if (config->mappings) {
    for (int i = 0; i < config->num_mappings; i++) {
      free(config->mappings[i].name);
      free(config->mappings[i].opcua_node_id);
      free(config->mappings[i].data_type);
      free(config->mappings[i].format);

      if (config->mappings[i].enum_values) {
        for (int j = 0; j < config->mappings[i].num_enum_values; j++) {
          free(config->mappings[i].enum_values[j].name);
        }
        free(config->mappings[i].enum_values);
      }
    }
    free(config->mappings);
  }
  free(config);
}