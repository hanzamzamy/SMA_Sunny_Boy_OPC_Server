# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.10)

# Define the project name and specify it's a C and C++ project
project(modbus_opcua_gateway C CXX)

# Set the C and C++ standards
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use PkgConfig to find libraries, which is standard on Linux
find_package(PkgConfig REQUIRED)

# Find the required libraries using their .pc files
# These will be available if the libraries are installed from source correctly
pkg_check_modules(LIBMODBUS REQUIRED libmodbus)
pkg_check_modules(OPEN62541 REQUIRED open62541)

# Find yaml-cpp using its config file (modern CMake approach)
find_package(yaml-cpp 0.6 REQUIRED)

# Add the include directories for all dependencies
include_directories(
    ${LIBMODBUS_INCLUDE_DIRS}
    ${OPEN62541_INCLUDE_DIRS}
    ${yaml-cpp_INCLUDE_DIRS}
)

# Add the source files for our executable
add_executable(modbus_opcua_gateway
    main.c
    config_parser.cpp # The new C++ parser
    modbus_client.c
    opcua_server.c
    logger.c
)

# Link the executable against the required libraries
target_link_libraries(modbus_opcua_gateway
    PRIVATE
    ${LIBMODBUS_LIBRARIES}
    ${OPEN62541_LIBRARIES}
    yaml-cpp # Link with yaml-cpp
)

