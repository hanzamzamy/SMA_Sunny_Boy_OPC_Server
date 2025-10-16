# Modbus TCP to OPC UA Gateway for SMA Inverters

This project provides a robust, industrial-grade gateway to read data from SMA SUNNY BOY PV inverters via Modbus TCP and expose it on an OPC UA server.

## Features

- **Portable**: C code supports compilation on different platforms.
- **Standard Build System**: Uses `CMake` for cross-platform compatibility.
- **Modbus TCP Communication**: Uses `libmodbus` for reliable communication with the inverter.
- **OPC UA Server**: Implements an OPC UA server with `open62541` to provide data to SCADA systems or other clients.
- **YAML Configuration**: A human-readable `config.yaml` file allows for easy setup of IP addresses, ports, security, logging, and granular Modbus polling rates.
- **Fault-Tolerant**: Includes reconnection logic for the Modbus connection.
- **Industrial Grade**:
	- **File Logging**: Logs events and errors to a configurable file.
	- **Data Validation**: Checks for and handles SMA-specific `NaN` (Not-a-Number) values.
	- **Endianness Correction**: Correctly interprets multi-register (Big Endian) values from the inverter.
	- **OPC UA Security**: Supports basic username/password authentication (can be disabled).

## Building from Source (Dependencies and Gateway)

This guide assumes you are on a Linux-based system and will build everything from source.

### 1. Install Build Tools

First, install the essential tools required for compilation. On a Debian-based system (like Ubuntu):
```sh
sudo apt-get update
sudo apt-get install build-essential cmake git autoconf libtool pkg-config
```

### 2. Build Dependencies from Source

We will clone and install `libmodbus`, `open62541`, and `yaml-cpp`.

#### a. yaml-cpp (for YAML parsing)
```sh
git clone https://github.com/jbeder/yaml-cpp.git
cd yaml-cpp
git checkout yaml-cpp-0.8.0 # Checkout a known stable release
mkdir build
cd build
cmake ..
make
sudo make install
cd ../..
```

#### b. libmodbus (for Modbus communication)
```sh
git clone https://github.com/stephane/libmodbus.git
cd libmodbus
./autogen.sh
./configure
make
sudo make install
cd ..
```

#### c. open62541 (for OPC UA server)
```sh
git clone [https://github.com/open62541/open62541.git](https://github.com/open62541/open62541.git)
cd open62541
git checkout v1.3.9 # Checkout a known stable release
mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=ON -DUA_ENABLE_ENCRYPTION=OFF ..
make
sudo make install
cd ../..
```

#### d. Update Library Cache

After installing new shared libraries, you must update the system's cache.
```sh
sudo ldconfig
```

### 3. Build the Gateway

Now that the dependencies are installed, you can build the gateway application using CMake.
```sh
# Create a build directory
mkdir build
cd build

# Run CMake to prepare the build files
cmake ..

# Compile the project
make
```

The executable `modbus_opcua_gateway` will be created inside the build directory.

### 4. Running the Gateway

You will need to create a `config.yaml` file. You can use the one provided in the project as a template.

#### a. Manual Execution (for testing or non-systemd systems)
```sh
./build/modbus_opcua_gateway /path/to/your/config.yaml
```

To run it in the background, you can use nohup:
```
nohup ./build/modbus_opcua_gateway /path/to/your/config.yaml &
```
