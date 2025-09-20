# Real-Time IoT Simulator

An IoT gateway simulator built with FreeRTOS and mbedTLS that demonstrates secure sensor data collection, processing, and transmission to cloud services via MQTT over TLS.

## Features

- **Multi-threaded Architecture**: Uses FreeRTOS tasks for concurrent operation of sensors, network, security, and monitoring
- **Sensor Simulation**: 
  - Temperature sensors with realistic variations
  - Humidity sensors with environmental patterns
  - Motion sensors with event detection
- **Secure Communication**: 
  - TLS 1.2 encryption using mbedTLS
  - Certificate validation for broker authentication
  - Encrypted MQTT communication to `test.mosquitto.org:8883`
- **Data Processing**:
  - Real-time analytics and anomaly detection
  - Statistical analysis (min/max/average)
  - Priority-based message queuing
- **Security Framework**:
  - Data encryption for high-priority messages
  - Message signing for integrity verification
  - Automatic key rotation
- **System Monitoring**:
  - Real-time console dashboard
  - Resource usage tracking
  - Performance metrics visualization

## Architecture

```
Sensor Tasks → Sensor Queue → Data Processor → Network Queue → Network Task → MQTT Broker
                                ↑
                            Security Task (encrypts high-priority messages)
```

### Core Components

1. **Sensor Simulation** (`src/tasks/sensors.c`):
   - Simulates multiple sensor types with realistic data patterns
   - Configurable sensor counts and update intervals

2. **Data Processing** (`src/tasks/data_process.c`):
   - Statistical analysis and anomaly detection
   - Priority-based message handling
   - Batch processing for efficiency

3. **Network Communication** (`src/tasks/network.c`):
   - TLS-secured MQTT client using mbedTLS
   - Connection management and error handling
   - Message publishing to cloud broker

4. **Security** (`src/tasks/security.c`):
   - Encryption for sensitive data
   - Message authentication
   - Key management and rotation

5. **System Monitoring** (`src/tasks/monitor.c`):
   - Real-time dashboard with metrics
   - Resource utilization tracking

## Prerequisites

- CMake 3.10 or higher
- GCC compiler
- POSIX threads (pthread) library
- OpenSSL development libraries (for certificate handling)
- FreeRTOS and mbedTLS libraries (included as submodules)

## Building

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build the project
make

# Run the simulator
./iot_gateway_sim
```

## Configuration

Key parameters can be adjusted in `include/config.h`:

- `NUM_TEMP_SENSORS`: Number of temperature sensors to simulate
- `NUM_HUMIDITY_SENSORS`: Number of humidity sensors to simulate
- `MQTT_BROKER_ADDRESS`: MQTT broker address
- `MQTT_BROKER_PORT`: MQTT broker port (8883 for TLS)
- `TLS_VERIFY_REQUIRED`: Enable/disable strict certificate verification

## Security Testing

The project includes comprehensive MITM attack testing tools in the `tests/mitm/` directory:

```bash
# Run automated security tests
cd tests/mitm
./run_tests.sh
```

Tests include:
- Certificate validation verification
- Encrypted communication confirmation
- MITM attack resistance
- Connection integrity validation

## Project Structure

```
├── build/                 # Build output directory
├── include/               # Header files
├── lib/                   # External libraries (FreeRTOS, mbedTLS)
├── src/                   # Source code
│   ├── tasks/            # Individual task implementations
│   ├── sensors/          # Sensor simulation logic
│   └── main.c            # Main entry point
├── tests/                # Testing framework
│   └── mitm/             # MITM attack testing tools
├── CMakeLists.txt         # Build configuration
└── README.md             # This file
```

## Libraries Used

- **FreeRTOS**: Real-time operating system kernel
- **mbedTLS**: Cryptographic and SSL/TLS library
- **POSIX pthreads**: Threading support for Linux target

## MQTT Topics

The simulator publishes to the following topics:
- `iot/gateway/temperature/sensor_X`: Temperature readings
- `iot/gateway/humidity/sensor_X`: Humidity readings
- `iot/gateway/motion/sensor_X`: Motion detection events


## References

https://github.com/Mbed-TLS/mbedtls

https://www.freertos.org/

https://mqtt.org/

## License

This project is licensed under the MIT License.
