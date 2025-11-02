/* @file main_iot_gateway.cpp - Advanced EZLog example for IoT Gateway */

#include <Arduino.h>
#include "IoTGateway.hpp"

// Note: log alias and level constants (ERROR, WARN, INFO, DEBUG) are declared
// in IoTGatewayConfig.hpp and scoped to the iotgw namespace.

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("=========================================================");
    Serial.println("     EZLog Advanced Logger Example - IoT Gateway");
    Serial.println("=========================================================");
    delay(100);

    // Call subsystem functions
    iotgw::systemEvents();
    iotgw::readSensors();
    iotgw::controlDevices();
    iotgw::modbusOperations();
    iotgw::networkOperations();

    delay(100);
    Serial.println("=========================================================");
    Serial.println("                   Example complete");
    Serial.println("=========================================================");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
    // ISR demo
    Serial.println("ISR Logging Demo - Simulating GPIO interrupt...");
    delay(100);

    iotgw::buttonISR();  // Simulate ISR
    delay(100);

    if (iotgw::buttonPressed) {
        Serial.println("Button press detected");
        iotgw::buttonPressed = false;
    }

    delay(1000);
}
