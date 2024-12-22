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
    if(currentMode != newMode) {  // Only process actual changes
        currentMode = newMode;
        genControl.setModePtr(&currentMode);
        
        // Update HomeSpan switches
        syncHomeSpan();
        
        // Force immediate update
        homeSpan.poll();
        
        Serial.printf("Mode changed to: %s\n", 
            newMode == MODE_AUTO ? "AUTO" : 
            newMode == MODE_MANUAL ? "MANUAL" : "OFF");
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
    
    // Set initial mode after HomeSpan is ready
    delay(1000);
    changeMode(DEFAULT_MODE);
    
    printStatus();
}

// Add this helper function for interruptible delays
bool delayWithModeCheck(unsigned long delayTime, SystemMode expectedMode) {
    unsigned long startTime = millis();
    SystemMode startMode = currentMode;
    
    while (millis() - startTime < delayTime) {
        processSerialCommand();
        homeSpan.poll();  // Poll HomeSpan to handle HomeKit events
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
    if (currentMode != expectedMode) return;  // Early exit if mode changed
    
    bool currentGridStatus = digitalRead(GRID_MONITOR_PIN) == LOW;
    
    if (currentGridStatus != gridStatus && (millis() - lastGridCheck > GRID_MONITOR_DEBOUNCE)) {
        if (!currentGridStatus) {  // Grid potentially down
            Serial.println("Grid outage detected - waiting 10s to confirm...");
            if (!delayWithModeCheck(GRID_OUTAGE_CONFIRM_DELAY, expectedMode)) return;  // Interruptible delay
            
            currentGridStatus = digitalRead(GRID_MONITOR_PIN) == LOW;
            if (!currentGridStatus) {
                gridStatus = false;
                Serial.println("Grid outage confirmed - initiating generator start sequence");
                if (genControl.startGenerator()) {
                    if (!delayWithModeCheck(TRANSFER_SWITCH_DELAY, expectedMode)) return;  // Interruptible delay
                    genControl.setTransferSwitch(true);
                } else {
                    Serial.println("Generator start failed in Auto mode - switching to Off mode");
                    
                    // Handle mode change the same way as manual mode
                    for(int i = 0; i < 3; i++) {  // Try up to 3 times to ensure update
                        changeMode(MODE_OFF);
                        syncHomeSpan();
                        homeSpan.poll();
                        delay(100);
                    }
                }
            }
        } else {  // Grid potentially restored
            Serial.println("Grid power detected - waiting 10s to confirm...");
            if (!delayWithModeCheck(GRID_RESTORE_CONFIRM_DELAY, expectedMode)) return;  // Interruptible delay
            
            if (digitalRead(GRID_MONITOR_PIN) == LOW) {
                gridStatus = true;
                Serial.println("Grid restoration confirmed - pausing for 10s");
                if (!delayWithModeCheck(GRID_RESTORE_CONFIRM_DELAY, expectedMode)) return;  // Interruptible delay
                
                if (digitalRead(GRID_MONITOR_PIN) == LOW) {
                    Serial.println("Grid stable - shutting down generator");
                    genControl.setTransferSwitch(false);
                    if (!delayWithModeCheck(GRID_RESTORE_CONFIRM_DELAY, expectedMode)) return;  // Interruptible delay
                    genControl.stopGenerator();
                } else {
                    Serial.println("Grid unstable - maintaining generator operation");
                    gridStatus = false;
                }
            }
        }
        lastGridCheck = millis();
    }
}

void handleManualMode() {
    SystemMode expectedMode = MODE_MANUAL;
    if (currentMode != expectedMode) return;  // Early exit if mode changed
    
    if (genControl.getState() == GEN_OFF) {
        Serial.println("Manual mode - waiting 10s before starting generator...");
        if (!delayWithModeCheck(MANUAL_START_PREP_DELAY, expectedMode)) return;  // Interruptible delay
        
        if (!genControl.startGenerator()) {
            Serial.println("Generator start failed in Manual mode - switching to Off mode");
            changeMode(MODE_OFF);  // This will handle HomeKit sync
            syncHomeSpan();  // Force another sync
            homeSpan.poll(); // Force another HomeKit update
            delay(100);      // Give HomeKit a moment to process
        } else {
            if (!delayWithModeCheck(TRANSFER_SWITCH_DELAY, expectedMode)) return;  // Interruptible delay
            genControl.setTransferSwitch(true);
        }
    }
}

void handleOffMode() {
    if (genControl.getState() != GEN_OFF) {
        delay(GRID_MONITOR_DEBOUNCE);
        genControl.setTransferSwitch(false);
        genControl.stopGenerator();
    }
}

void loop() {
    homeSpan.poll();  // Poll HomeSpan first
    processSerialCommand();
    
    static unsigned long lastPoll = 0;
    if (millis() - lastPoll >= 50) {  // Poll every 50ms
        lastPoll = millis();
        homeSpan.poll();  // Additional polling for state changes
    }
    
    if (currentMode == MODE_OFF && genControl.getState() != GEN_OFF) {
        genControl.setTransferSwitch(false);
        genControl.stopGenerator();
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
    
    delay(50);  // Small delay to prevent tight loops
}
