# SMA Inverter Modbus TCP to OPC UA Gateway

This gateway is an industrial-grade C/C++ application that bridges the gap between **SMA Modbus TCP** protocol and the **OPC UA (IEC 62541)** standard. It is specifically tuned to handle the nuances of SMA data structures, including Big Endian byte orders and proprietary Not-a-Number (NaN) signaling.

## Core Architecture

### 1. Polling Engine & Priority

The gateway does not poll everything at once. Each mapping in `sma_opcua_config.yaml` has an individual `poll_interval_ms`:

- **Dynamic Data**: Active Power and Voltage are polled every 1s.

- **Static Data**: Serial Numbers and Firmware versions are polled every 300s.
This reduces network congestion and prevents overwhelming the inverter's CPU.

### 2. Config Parser (`config_parser.cpp`)

The `load_config_from_yaml` function parses the `sma_opcua_config.yaml` file to populate a `modbus_opcua_config_t` structure. This includes settings for the Modbus TCP connection, the OPC UA server, security credentials, and granular register mappings. It uses [`yaml-cpp`](https://github.com/jbeder/yaml-cpp) for robust YAML parsing.

### 3. SMA Data Processing (`main.c`)

The gateway includes specialized logic for SMA's data types:

- **Endianness**: SMA sends 32-bit values as two 16-bit registers. The gateway correctly reassembles these into a single integer before scaling.

- **Scaling (`FIXn`)**: If a register is marked `FIX3`, the gateway reads the integer `12345` and converts it to a float `12.345` on the OPC UA side automatically.

- **NaN Handling**: SMA uses specific values (e.g., `0xFFFF`) to indicate a sensor is not available. The gateway detects these and prevents garbage data from reaching your SCADA.

### 4. Logger (`logger.c`)

The gateway features a robust file logger that records events and errors to a configurable log file or `stdout`. It supports different log levels (`ERROR`, `WARN`, `INFO`, `DEBUG`) and includes timestamps for all entries.

### 5. OPC UA Server (`opcua_server.c`)

Built on top of [`open62541`](https://github.com/open62541/open62541), it creates a full address space. It supports:

- **Custom DataTypes**: It creates custom Enumeration types so SCADA clients see `OK` or `Error` instead of raw numbers like `307` or `35`.

- **Authentication**: Integrated `AccessControl` plugin for username/password security.

### 6. Modbus Client (`modbus_client.c`)

The gateway acts as a Modbus TCP client. It uses [`libmodbus`](https://github.com/stephane/libmodbus) to establish connections, manage timeouts, and handle register reading. This library is crucial because it abstracts the complex bit-shifting and error handling required for reliable Modbus communication.

## Prerequisites

- **C/C++ Compiler**: Support for C99 and C++17.

- **CMake**: Version 3.10+.

- **Dependencies**: `libmodbus`, `open62541`, and `yaml-cpp`.

## Building on Linux/macOS

### Option 1: Install Prebuilt Packages (Linux only)

On Ubuntu/Debian:
```bash
sudo apt update
sudo apt install build-essential cmake git autoconf libtool pkg-config libmodbus-dev libopen62541-dev libyaml-cpp-dev
```
Then, jump to step 4.

### Option 2: Build Dependencies from Source

For production environments where you cannot rely on system repositories:

#### 1. Build `open62541`
```bash
git clone https://github.com/open62541/open62541.git
cd open62541 && mkdir build && cd build
cmake -DUA_ENABLE_AMALGAMATION=ON -DBUILD_SHARED_LIBS=ON ..
make -j$(nproc)
sudo make install
```

#### 2. Build yaml-cpp
```bash
git clone https://github.com/jbeder/yaml-cpp.git
cd yaml-cpp && mkdir build && cd build
cmake -DYAML_BUILD_SHARED_LIBS=ON ..
make -j$(nproc)
sudo make install
```

#### 3. Build libmodbus
```bash
git clone https://github.com/stephane/libmodbus.git
cd libmodbus
./autogen.sh
./configure
make && sudo make install
```

#### 4. Build the Gateway
```bash
git clone https://github.com/hanzamzamy/SMA_Sunny_Boy_OPC_Server.git
cd SMA_OPCUA_Gateway
mkdir build && cd build
cmake ..
make
```

## Building on Windows (vcpkg)

### 1. Install vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg
.\vcpkg\bootstrap-vcpkg.bat
```

### 2. Install Dependencies
```powershell
vcpkg install libmodbus:x64-windows yaml-cpp:x64-windows open62541:x64-windows
```

### 3. Build the Gateway
```powershell
cmake -B build -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

## Development & Hacking

### Hacking the Node Space:

You can modify `opcua_server.c` to change how the tree structure looks. Currently, it uses a flat list under `Objects`, but you can modify `add_opcua_nodes` to create nested folders (e.g., `Inverter1/Power/Active`).

### Adding New SMA Formats:

If SMA introduces a new format (e.g., a special 128-bit hash), you can hack the `process_modbus_value_formatted` function in `main.c` to add a new `else if` block for that specific format string.

## Beyond SMA

While optimized for SMA, this gateway can be hacked to work with any Modbus device:

- **Generic Modbus**: Just update the `modbus_address` and `data_type` in the YAML config.

- **Non-SMA NaN**: If your device uses `0x0000` for NaN, simply update the defines in `main.h`.

## Pairing

This Gateway is a **Modbus Client**. It needs a server to talk to.

1. **With Simulator**: You can use this with sister project [SMA Sunny Boy Digital Twin Simulator](https://github.com/hanzamzamy/SMA_Sunny_Boy_Simulator). Start the simulator and this gateway pointing to the simulator's IP.
2. **With Real Hardware**: If you have a physical SMA Inverter, find its IP address in your network. Configure the Gateway to use that IP and the standard Modbus port `502`.
3. **Connect OPC UA Client**: Use OPC UA client (e.g., [UaExpert](https://www.unified-automation.com/downloads/opc-ua-clients/)) to verify your data. Your SCADA software can now connect to the Gateway at `opc.tcp://[IP]:4840` to see all solar data.

## License

MIT License - 2025 Rayhan Zamzamy.
