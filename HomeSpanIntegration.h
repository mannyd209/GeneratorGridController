#ifndef HOMESPAN_INTEGRATION_H
#define HOMESPAN_DEFINITION_H

#include "config.h"
#include "HomeSpan.h"
#include <WiFi.h>

// Declare changeMode and currentMode as external
extern void changeMode(SystemMode newMode);
extern SystemMode currentMode;

class HomeSpanSwitch : public Service::Switch {
public:
    SpanCharacteristic *power;
    SystemMode mode;
    static HomeSpanSwitch* switches[3];  // Array to hold our three switches
    static int switchCount;

    HomeSpanSwitch(const char *name, SystemMode mode) : Service::Switch() {
        power = new Characteristic::On(mode == DEFAULT_MODE);
        new Characteristic::Name(name);
        this->mode = mode;
        if(switchCount < 3) {
            switches[switchCount++] = this;
        }
    }

    boolean update() override {
        if(power->updated()) {  // Only process if value actually changed
            boolean newState = power->getNewVal();
            
            if(newState) {
                // Switch being turned on
                changeMode(mode);
                
                // Turn off other switches immediately
                for(int i = 0; i < switchCount; i++) {
                    if(switches[i] != this) {
                        switches[i]->power->setVal(false);
                    }
                }
            } else {
                // Prevent switch from being turned off directly
                // Instead activate OFF mode switch
                if(mode != MODE_OFF) {
                    power->setVal(true);  // Keep this switch on
                    for(int i = 0; i < switchCount; i++) {
                        if(switches[i]->mode == MODE_OFF) {
                            switches[i]->power->setVal(true);  // This will trigger OFF mode
                            break;
                        }
                    }
                }
            }
        }
        return(true);
    }
};

// Initialize static members
HomeSpanSwitch* HomeSpanSwitch::switches[3] = {nullptr, nullptr, nullptr};
int HomeSpanSwitch::switchCount = 0;

void setupHomeSpan() {
    Serial.println("Initializing HomeSpan...");
    
    // Initialize HomeSpan
    homeSpan.setStatusPin(0);
    homeSpan.setControlPin(0);
    homeSpan.setPairingCode(HOMEKIT_SETUP_PIN);
    homeSpan.begin(Category::Bridges, "Generator Controller");
    
    Serial.println("Creating bridge accessory...");
    // Create bridge accessory
    new SpanAccessory();
    new Service::AccessoryInformation();
        new Characteristic::Name("Generator Controller Bridge");
        new Characteristic::Manufacturer("MannyDev");
        new Characteristic::SerialNumber("14941");
        new Characteristic::Model("Generator Controller Bridge");
        new Characteristic::FirmwareRevision("1.0");
        new Characteristic::Identify();

    // Create Auto Mode switch accessory
    new SpanAccessory();
    new Service::AccessoryInformation();
        new Characteristic::Name("Auto");
        new Characteristic::Manufacturer("MannyDev");
        new Characteristic::SerialNumber("14941-1");
        new Characteristic::Model("Generator Auto Switch");
        new Characteristic::FirmwareRevision("1.0");
        new Characteristic::Identify();
    new HomeSpanSwitch("Auto Mode", MODE_AUTO);

    // Create Manual Mode switch accessory
    new SpanAccessory();
    new Service::AccessoryInformation();
        new Characteristic::Name("Manual");
        new Characteristic::Manufacturer("MannyDev");
        new Characteristic::SerialNumber("14941-2");
        new Characteristic::Model("Generator Manual Switch");
        new Characteristic::FirmwareRevision("1.0");
        new Characteristic::Identify();
    new HomeSpanSwitch("Manual Mode", MODE_MANUAL);

    // Create Off Mode switch accessory
    new SpanAccessory();
    new Service::AccessoryInformation();
        new Characteristic::Name("Off");
        new Characteristic::Manufacturer("MannyDev");
        new Characteristic::SerialNumber("14941-3");
        new Characteristic::Model("Generator Off Switch");
        new Characteristic::FirmwareRevision("1.0");
        new Characteristic::Identify();
    new HomeSpanSwitch("Off Mode", MODE_OFF);

    // Ensure accessories are properly initialized
    for(int i = 0; i < HomeSpanSwitch::switchCount; i++) {
        HomeSpanSwitch* sw = HomeSpanSwitch::switches[i];
        if(sw) {
            sw->power->setVal(sw->mode == DEFAULT_MODE);
        }
    }
    
    // Force an initial state sync
    homeSpan.poll();

    Serial.println("HomeSpan initialization complete!");
    homeSpan.poll();  // Initial poll to establish connection
}

void syncHomeSpan() {
    // Ensure all switches are in correct state
    for(int i = 0; i < HomeSpanSwitch::switchCount; i++) {
        HomeSpanSwitch* sw = HomeSpanSwitch::switches[i];
        if(sw) {
            bool shouldBeOn = (sw->mode == currentMode);
            if(sw->power->getVal() != shouldBeOn) {
                sw->power->setVal(shouldBeOn, true);  // Force immediate update
                homeSpan.poll();  // Process the change immediately
            }
        }
    }
    
    // Additional polls to ensure state propagation
    for(int i = 0; i < 3; i++) {
        homeSpan.poll();
        delay(50);
    }
}

#endif
