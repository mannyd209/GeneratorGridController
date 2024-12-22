#ifndef GENERATOR_CONTROL_H
#define GENERATOR_CONTROL_H

#include "config.h"
#include "modes.h"
#include <Arduino.h>

class GeneratorControl {
private:
    GeneratorState state;
    uint8_t startAttempts;
    unsigned long startTime;
    bool transferSwitchState;
    bool shouldAbort;  // Add abort flag
    SystemMode *currentModePtr;  // Add pointer to current mode
    SystemMode startingMode;     // Add mode tracking
    SystemMode currentOpMode;  // Current operation mode
    
    void resetStartAttempts() { startAttempts = 0; }
    bool isGeneratorRunning() { 
        return digitalRead(GENERATOR_MONITOR_PIN) == HIGH; // Changed to HIGH for pullup
    }

    // Add interruptible delay method
    bool delayWithModeCheck(unsigned long ms) {
        unsigned long start = millis();
        while(millis() - start < ms) {
            if (shouldAbort || *currentModePtr != startingMode) {
                Serial.println("Mode changed during delay - aborting");
                return false;
            }
            delay(50);
        }
        return true;
    }

    bool isModeChanged() {
        return currentOpMode != *currentModePtr;
    }
    
    void updateOpMode() {
        currentOpMode = *currentModePtr;
    }

    bool executeStartSequence() {
        shouldAbort = false;
        updateOpMode();  // Set initial operation mode
        
        // Initial power and choke sequence
        Serial.println("Initializing power and choke");
        digitalWrite(GENERATOR_POWER_PIN, LOW);
        if (isModeChanged()) {
            Serial.println("Mode changed during power-up - stopping");
            stopGenerator();
            return false;
        }
        delay(POWER_STABILIZE_DELAY);
        
        digitalWrite(GENERATOR_CHOKE_PIN, LOW);
        if (isModeChanged()) {
            Serial.println("Mode changed during choke - stopping");
            stopGenerator();
            return false;
        }
        delay(CHOKE_ENGAGE_DELAY);
        
        // Check if generator is already running
        if (isGeneratorRunning()) {
            if (isModeChanged()) {
                stopGenerator();
                return false;
            }
            Serial.println("Generator already running - releasing choke");
            delay(CHOKE_WARMUP_DELAY);
            digitalWrite(GENERATOR_CHOKE_PIN, HIGH);
            delay(POST_CHOKE_WARMUP_DELAY);
            if (isModeChanged()) {
                stopGenerator();
                return false;
            }
            return true;
        }
        
        // Try up to 3 times
        for (int attempt = 1; attempt <= 3; attempt++) {
            updateOpMode();  // Update mode flag at start of each attempt
            
            if (isModeChanged()) {
                stopGenerator();
                return false;
            }
            
            // Start sequence
            Serial.printf("Start attempt #%d\n", attempt);
            digitalWrite(GENERATOR_STARTER_PIN, LOW);
            delay(STARTER_CRANK_DURATION);
            digitalWrite(GENERATOR_STARTER_PIN, HIGH);
            
            // Monitor for start
            unsigned long startTime = millis();
            while (millis() - startTime < START_MONITOR_DURATION) {
                if (isModeChanged()) {
                    stopGenerator();
                    return false;
                }
                
                if (isGeneratorRunning()) {
                    updateOpMode();
                    Serial.println("Generator running - waiting before choke release");
                    delay(CHOKE_WARMUP_DELAY);
                    
                    if (isModeChanged()) {
                        stopGenerator();
                        return false;
                    }
                    
                    digitalWrite(GENERATOR_CHOKE_PIN, HIGH);
                    delay(POST_CHOKE_WARMUP_DELAY);
                    
                    if (isModeChanged()) {
                        stopGenerator();
                        return false;
                    }
                    return true;
                }
                delay(MODE_CHANGE_CHECK_INTERVAL);
            }
            
            if (attempt < 3 && !isModeChanged()) {
                delay(RETRY_ATTEMPT_DELAY);
            }
        }
        
        // If we get here, all attempts failed
        Serial.println("All start attempts failed - resetting system");
        stopGenerator();  // This will reset everything to OFF state
        return false;
    }

public:
    GeneratorControl() : state(GEN_OFF), startAttempts(0), transferSwitchState(false),
                        currentModePtr(nullptr), currentOpMode(MODE_OFF) {
        Serial.println("Generator Control System Initialized");
    }
    
    bool startGenerator() {
        updateOpMode();  // Update operation mode at start
        if (startAttempts >= MAX_START_ATTEMPTS) {
            Serial.println("ERROR: Maximum start attempts reached");
            stopGenerator();  // Ensure everything is reset
            return false;
        }
        
        state = GEN_STARTING;
        startAttempts++;
        Serial.print("Starting generator - Attempt #");
        Serial.println(startAttempts);
        
        // Check if already running
        if (isGeneratorRunning()) {
            Serial.println("Generator already running - skipping start sequence");
            state = GEN_RUNNING;
            return true;
        }
        
        // Execute start sequence
        if(executeStartSequence()) {
            Serial.println("Generator started successfully");
            state = GEN_RUNNING;
            return true;
        }
        
        Serial.println("Generator failed to start");
        if (startAttempts >= MAX_START_ATTEMPTS) {
            Serial.println("No more attempts remaining - shutting down");
            stopGenerator();
        } else {
            Serial.println("Preparing for next attempt");
        }
        return false;
    }
    
    void stopGenerator() {
        Serial.println("Stopping generator");
        digitalWrite(GENERATOR_POWER_PIN, HIGH);   // Deactivate
        digitalWrite(GENERATOR_CHOKE_PIN, HIGH);   // Deactivate
        digitalWrite(GENERATOR_STARTER_PIN, HIGH); // Deactivate
        digitalWrite(TRANSFER_SWITCH_PIN, HIGH);   // Deactivate
        state = GEN_OFF;
        startAttempts = 0;
        Serial.println("Generator stopped - all systems reset");
        updateOpMode();  // Update operation mode after stop
    }
    
    void setTransferSwitch(bool enable) {
        digitalWrite(TRANSFER_SWITCH_PIN, enable ? LOW : HIGH); // Inverted logic
        transferSwitchState = enable;
        Serial.print("Transfer switch ");
        Serial.println(enable ? "ENABLED" : "DISABLED");
    }
    
    void abortStartup() {
        shouldAbort = true;
        Serial.println("Aborting generator startup sequence");
    }
    
    void setModePtr(SystemMode *modePtr) {
        currentModePtr = modePtr;
        updateOpMode();  // Update operation mode when mode pointer changes
    }
    
    GeneratorState getState() { return state; }
    bool isTransferSwitchEnabled() { return transferSwitchState; }
};

#endif
