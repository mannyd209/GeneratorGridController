#include <EEPROM.h>
#include "config.h"
#include "GeneratorControl.h"
#include "HomeSpanIntegration.h"
#include "modes.h"

// Declare currentMode as an external variable
SystemMode currentMode = DEFAULT_MODE;  // Default to configured mode

GeneratorControl genControl;
unsigned long lastGridCheck = 0;
bool gridStatus = true;

void printStatus() {
    Serial.println("\n=== Generator Control Status ===");
    Serial.print("Current Mode: ");
    switch(currentMode) {
        case MODE_AUTO: Serial.println("AUTO"); break;
        case MODE_MANUAL: Serial.println("MANUAL"); break;
        case MODE_OFF: Serial.println("OFF"); break;
    }
    Serial.print("Grid Status: ");
    Serial.println(gridStatus ? "ON" : "OFF");
    Serial.print("Generator State: ");
    Serial.println(genControl.getState() == GEN_RUNNING ? "RUNNING" : "OFF");
    Serial.println("Commands: 's'=Status");
    Serial.println("==============================\n");
}

// Add helper function for mode changes
void changeMode(SystemMode newMode) {
    if(currentMode != newMode) {
        // Update mode first
        currentMode = newMode;
        genControl.setModePtr(&currentMode);
        
        // Force immediate HomeSpan updates
        syncHomeSpan();
        homeSpan.poll();
        
        // Log mode change
        Serial.printf("MODE CHANGE: Switching to %s mode\n", 
            newMode == MODE_AUTO ? "AUTO" :
            newMode == MODE_MANUAL ? "MANUAL" : "OFF");
            
        // Additional HomeSpan polls for reliability
        homeSpan.poll();
    }
}

void processSerialCommand() {
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 's' || cmd == 'S') {
            printStatus();
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\nGenerator Control System Starting...");
    
    // Configure pins first
    pinMode(GRID_MONITOR_PIN, INPUT_PULLUP);
    pinMode(GENERATOR_MONITOR_PIN, INPUT_PULLUP);
    pinMode(GENERATOR_POWER_PIN, OUTPUT);
    pinMode(GENERATOR_CHOKE_PIN, OUTPUT);
    pinMode(GENERATOR_STARTER_PIN, OUTPUT);
    pinMode(TRANSFER_SWITCH_PIN, OUTPUT);
    
    // Initialize all relays to inactive state (HIGH)
    digitalWrite(GENERATOR_POWER_PIN, HIGH);
    digitalWrite(GENERATOR_CHOKE_PIN, HIGH);
    digitalWrite(GENERATOR_STARTER_PIN, HIGH);
    digitalWrite(TRANSFER_SWITCH_PIN, HIGH);
    
    // Initialize grid status as ON
    gridStatus = true;
    lastGridCheck = millis();
    
    // Initialize mode pointer
    genControl.setModePtr(&currentMode);
    
    // Setup WiFi first
    homeSpan.setWifiCredentials(WIFI_SSID, WIFI_PASSWORD);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.println("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected!");
    
    // Initialize HomeSpan first (before any mode changes)
    setupHomeSpan();
    
    // Force multiple polls during setup
    for(int i = 0; i < 3; i++) {
        homeSpan.poll();
    }
    
    // Set initial mode after HomeSpan is ready
    changeMode(DEFAULT_MODE);
    syncHomeSpan();  // Ensure initial state is correct
    
    printStatus();
}

// Add this helper function for interruptible delays
bool delayWithModeCheck(unsigned long delayTime, SystemMode expectedMode) {
    unsigned long startTime = millis();
    SystemMode startMode = currentMode;
    
    while (millis() - startTime < delayTime) {
        processSerialCommand();
        homeSpan.poll();  // Poll HomeSpan to handle HomeKit events
        
        // Poll more frequently during long delays
        static unsigned long lastPoll = 0;
        if (millis() - lastPoll >= 50) {
            lastPoll = millis();
            homeSpan.poll();
            syncHomeSpan();  // Keep UI in sync
        }
        
        if (currentMode != startMode) {
            Serial.printf("Mode changed from %d to %d - interrupting\n", startMode, currentMode);
            return false;
        }
        delay(50);
    }
    return true;
}

void handleAutoMode() {
    SystemMode expectedMode = MODE_AUTO;
    if (currentMode != expectedMode) return;
    
    bool currentGridStatus = digitalRead(GRID_MONITOR_PIN) == LOW;
    
    // Keep HomeKit responsive during grid monitoring
    homeSpan.poll();
    
    if (currentGridStatus != gridStatus && (millis() - lastGridCheck > GRID_MONITOR_DEBOUNCE)) {
        if (!currentGridStatus) {
            Serial.println("Grid outage detected - waiting 10s to confirm...");
            if (!delayWithModeCheck(GRID_OUTAGE_CONFIRM_DELAY, expectedMode)) return;
            
            currentGridStatus = digitalRead(GRID_MONITOR_PIN) == LOW;
            if (!currentGridStatus) {
                gridStatus = false;
                Serial.println("Grid outage confirmed - initiating generator start sequence");
                
                // Keep HomeKit responsive during generator start
                homeSpan.poll();
                
                if (genControl.startGenerator()) {
                    if (!delayWithModeCheck(TRANSFER_SWITCH_DELAY, expectedMode)) return;
                    genControl.setTransferSwitch(true);
                    syncHomeSpan();  // Update UI after successful operation
                } else {
                    Serial.println("Generator start failed in Auto mode - switching to Off mode");
                    changeMode(MODE_OFF);
                    syncHomeSpan();
                }
            }
        }
        lastGridCheck = millis();
    }
}

void handleManualMode() {
    SystemMode expectedMode = MODE_MANUAL;
    if (currentMode != expectedMode) return;
    
    if (genControl.getState() == GEN_OFF) {
        Serial.println("Manual mode - waiting 10s before starting generator...");
        
        // Keep HomeKit responsive during startup sequence
        if (!delayWithModeCheck(MANUAL_START_PREP_DELAY, expectedMode)) return;
        
        homeSpan.poll();  // Additional poll before critical operation
        
        if (!genControl.startGenerator()) {
            Serial.println("Generator start failed in Manual mode - switching to Off mode");
            changeMode(MODE_OFF);
            syncHomeSpan();
            homeSpan.poll();
        } else {
            if (!delayWithModeCheck(TRANSFER_SWITCH_DELAY, expectedMode)) return;
            genControl.setTransferSwitch(true);
            syncHomeSpan();  // Update UI after successful operation
        }
    }
}

void handleOffMode() {
    if (genControl.getState() != GEN_OFF) {
        // Keep HomeKit responsive during shutdown
        homeSpan.poll();
        delay(GRID_MONITOR_DEBOUNCE);
        
        genControl.setTransferSwitch(false);
        homeSpan.poll();  // Poll between operations
        
        genControl.stopGenerator();
        syncHomeSpan();  // Update UI after shutdown complete
    }
}

void syncHomeSpan() {
    // Update all switches immediately
    for(int i = 0; i < HomeSpanSwitch::switchCount; i++) {
        HomeSpanSwitch* sw = HomeSpanSwitch::switches[i];
        if(sw) {
            bool shouldBeOn = (sw->mode == currentMode);
            if(sw->power->getVal() != shouldBeOn) {
                sw->power->setVal(shouldBeOn, true);  // Force immediate update
                homeSpan.poll();  // Process each change immediately
            }
        }
    }
    homeSpan.poll();  // Final update
}

void loop() {
    static unsigned long lastPoll = 0;
    static unsigned long lastSync = 0;
    
    // Regular HomeSpan polling
    if (millis() - lastPoll >= 50) {
        lastPoll = millis();
        homeSpan.poll();
    }
    
    // Periodic state sync
    if (millis() - lastSync >= 1000) {  // Sync every second
        lastSync = millis();
        syncHomeSpan();
    }
    
    processSerialCommand();
    
    // Handle mode-specific operations
    if (currentMode == MODE_OFF && genControl.getState() != GEN_OFF) {
        genControl.setTransferSwitch(false);
        homeSpan.poll();  // Poll between operations
        genControl.stopGenerator();
        syncHomeSpan();
    } else {
        switch (currentMode) {
            case MODE_AUTO:
                handleAutoMode();
                break;
            case MODE_MANUAL:
                handleManualMode();
                break;
            case MODE_OFF:
                handleOffMode();
                break;
        }
    }
    
    delay(50);
}
