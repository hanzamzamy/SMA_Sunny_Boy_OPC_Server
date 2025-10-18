cmake_minimum_required(VERSION 3.10)
project(modbus_opcua_gateway C CXX)

# --- Compiler and Standard ---
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Find Dependencies ---
find_package(yaml-cpp REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBMODBUS REQUIRED libmodbus)
pkg_check_modules(OPEN62541 REQUIRED open62541)

# --- Include Directories ---
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LIBMODBUS_INCLUDE_DIRS}
    ${OPEN62541_INCLUDE_DIRS}
    ${yaml-cpp_INCLUDE_DIRS}
)

# --- Add Executable ---
add_executable(modbus_opcua_gateway
    src/main.c
    src/config_parser.cpp
    src/modbus_client.c
    src/opcua_server.c
    src/logger.c
)

# --- Link Libraries ---
target_link_libraries(modbus_opcua_gateway
    PRIVATE
    ${LIBMODBUS_LIBRARIES}
    ${OPEN62541_LIBRARIES}
    yaml-cpp::yaml-cpp
)

# --- Set RPATH for runtime library search path ---
set_target_properties(modbus_opcua_gateway PROPERTIES
    INSTALL_RPATH "/usr/local/lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# --- Copy YAML Configuration File ---
add_custom_command(
    TARGET modbus_opcua_gateway POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/sma_opcua_config.yaml
            $<TARGET_FILE_DIR:modbus_opcua_gateway>/sma_opcua_config.yaml
    COMMENT "Copying sma_opcua_config.yaml to the output directory"
)

# --- Install Executable and Configuration File ---
install(TARGETS modbus_opcua_gateway RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/sma_opcua_config.yaml DESTINATION etc)