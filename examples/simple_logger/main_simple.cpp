/* @file main_simple.cpp - Simple EZLog example */

#include <Arduino.h>
#include "SimpleLogConfig.hpp"

// Import logger namespace to call log:: directly
using namespace logger;

void setup() {
    delay(2000);

    log::ln(INFO, "=== EZLog Simple Logger Example ===");

    // Different severity levels
    log::ln(ERROR, "System error detected");
    log::ln(WARN, "Low battery warning");
    log::ln(INFO, "Device initialized");
    log::ln(DEBUG, "Debug trace");

    delay(100);

    log::ln(INFO, "--- Formatted logging ---");

    // Formatted text
    int value = 42;
    float temp = 23.5f;

    log::ln(INFO, "Sensor value: %d", value);
    log::ln(INFO, "Temperature: %.2f°C", temp);
    log::ln(DEBUG, "Multiple values: %d, %.1f, %s", value, temp, "OK");
    log::ln(DEBUG, "Multiple values (using FTOA): %d, %d.%02d, %s", value, FTOA(temp, 2), "OK");

    delay(100);

    log::ln(INFO, "--- Runtime filtering ---");

    log::ln(INFO, "Current level: INFO");
    log::setFilterLevel(INFO);

    log::ln(ERROR, "ERROR (visible)");
    log::ln(WARN, "WARN (visible)");
    log::ln(INFO, "INFO (visible)");
    log::ln(DEBUG, "DEBUG (filtered out)");

    delay(100);

    log::ln(INFO, "Restoring level: DEBUG");
    log::setFilterLevel(DEBUG);

    log::ln(INFO, "=== Example complete ===");
}

void loop() {
    log::ln(INFO, "Idle...");
    delay(1000);
}
