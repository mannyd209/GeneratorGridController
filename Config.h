#ifndef CONFIG_H
#define CONFIG_H

//=============================================================================
// USER CONFIGURATION SECTION
// Edit these settings according to your setup
//=============================================================================

// WiFi Settings (Required)
#define WIFI_SSID "Your WiFi Network Name"        // Your WiFi network name
#define WIFI_PASSWORD "Your WiFi Password"      // Your WiFi password

// HomeKit Settings
#define HOMEKIT_SETUP_PIN "12345678"      // 8-digit code for HomeKit pairing

// Operating Mode
#define DEFAULT_MODE MODE_AUTO            // System starts in this mode:
                                         // MODE_AUTO: Automatic grid monitoring
                                         // MODE_MANUAL: Manual control only
                                         // MODE_OFF: System disabled

// GPIO Pin Configuration
#define GRID_MONITOR_PIN 32               // Pin to monitor grid power status
#define GENERATOR_MONITOR_PIN 33          // Pin to monitor generator running status
#define GENERATOR_POWER_PIN 25            // Pin to control generator power
#define GENERATOR_CHOKE_PIN 26            // Pin to control generator choke
#define GENERATOR_STARTER_PIN 27          // Pin to control generator starter motor
#define TRANSFER_SWITCH_PIN 14            // Pin to control transfer switch

// Maximum start attempts
#define MAX_START_ATTEMPTS 3

//=============================================================================
// TIMING CONFIGURATIONS (all values in milliseconds) 1000ms = 1 second
// Only modify these if you understand the implications
//=============================================================================

// Grid Power Monitoring
#define GRID_OUTAGE_CONFIRM_DELAY    10000   // How long to wait before confirming power outage
#define GRID_RESTORE_CONFIRM_DELAY   10000   // How long to wait before confirming power restoration
#define GRID_MONITOR_DEBOUNCE        10000   // Debounce time for grid status changes

// Generator Control Timings
#define POWER_STABILIZE_DELAY        2000    // Time to wait after turning on generator power
#define CHOKE_ENGAGE_DELAY           3000    // Time to wait after engaging choke
#define STARTER_CRANK_DURATION       3000    // How long to run the starter motor
#define START_MONITOR_DURATION       10000   // How long to wait for generator to start
#define RETRY_ATTEMPT_DELAY          5000    // Time between start attempts

// Engine Management
#define CHOKE_WARMUP_DELAY          3000    // Time before releasing choke
#define POST_CHOKE_WARMUP_DELAY     5000    // Additional warm-up after choke release

// Safety Delays
#define TRANSFER_SWITCH_DELAY       10000    // Time before switching power to generator
#define MODE_CHANGE_CHECK_INTERVAL   50      // How often to check for mode changes
#define MANUAL_START_PREP_DELAY     10000    // Delay before manual start

#endif
