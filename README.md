# HomeKit Generator Grid Controller

A HomeKit-enabled automatic generator controller for ESP32 that monitors grid power and manages generator operations. Built with HomeSpan.

## Features
- HomeKit integration for remote monitoring and control
- Three operating modes:
  - Auto Mode: Automatically starts generator on grid failure
  - Manual Mode: Manual generator control
  - Off Mode: Completely disables generator
- Real-time status updates in Apple Home app
- Grid power monitoring with configurable delays
- Intelligent generator start sequence with choke control
- Transfer switch management
- Multiple start attempts with configurable retry logic
- Safe mode switching with proper shutdown sequences
- Serial monitor status reporting

## Requirements
- ESP32 development board
- HomeSpan library
- Arduino IDE
- Apple HomeKit compatible device
- Generator with:
  - Electric start
  - Choke control
  - Transfer switch
  - Running status sensor

## Hardware Setup
Required connections:
- Grid Monitor Pin (32): Connect to grid power sensor (LOW = power present)
- Generator Monitor Pin (33): Connect to generator running sensor (HIGH = running)
- Generator Power Pin (25): Controls main generator power (LOW = ON)
- Generator Choke Pin (26): Controls generator choke (LOW = ON)
- Generator Starter Pin (27): Controls starter motor (LOW = ON)
- Transfer Switch Pin (14): Controls transfer switch (LOW = ON)

## Installation
1. Install HomeSpan library in Arduino IDE
2. Clone or download this repository
3. Edit config.h with your settings:
   - WiFi credentials
   - HomeKit pairing code
   - Timing adjustments if needed
   - GPIO pin configuration
4. Upload to your ESP32

## Configuration (config.h)
### Required Settings



## Operation
### Auto Mode
1. Monitors grid power status
2. On grid failure:
   - Confirms outage (10s delay)
   - Starts generator
   - Manages choke
   - Engages transfer switch
3. On grid restoration:
   - Confirms stable power (10s delay)
   - Disables transfer switch
   - Stops generator

### Manual Mode
1. Starts generator on demand
2. Manages choke sequence
3. Controls transfer switch
4. Provides safe shutdown

### Off Mode
1. Safely shuts down generator
2. Disables transfer switch
3. Prevents automatic operation

## HomeKit Integration
- Appears as three switches in Apple Home:
  - Auto Mode Switch
  - Manual Mode Switch
  - Off Mode Switch
- Only one mode active at a time
- Real-time status updates
- Remote control capability

## Safety Features
- Grid power confirmation delays
- Multiple start attempt protection (3 attempts max)
- Safe mode switching
- Proper shutdown sequences
- Transfer switch safety delays
- Choke control management
- Immediate stop on mode change

## Serial Monitor
Commands:
- 's': Display current status
Output includes:
- Mode changes
- Grid status
- Generator state
- Start/stop sequences
- Error conditions

## Troubleshooting
1. **Generator won't start**
   - Check pin connections
   - Verify power connections
   - Check serial output for errors
   - Ensure proper timing settings

2. **HomeKit not connecting**
   - Verify WiFi credentials
   - Check HomeKit code
   - Reset ESP32
   - Check network connectivity

3. **Mode changes not updating**
   - Check WiFi connection
   - Monitor serial output
   - Verify HomeKit connectivity

## License
[MIT License](LICENSE)

## Contributing
1. Fork the repository
2. Create feature branch
3. Submit pull request

## Credits
- HomeSpan library: [HomeSpan on GitHub](https://github.com/HomeSpan/HomeSpan)
- Developed by: Your Name
- Version: 1.0.0