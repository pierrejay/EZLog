/* @file IoTGateway.hpp - IoT Gateway Functions */

#pragma once

#include "IoTGatewayLogConfig.hpp"

namespace iotgw {

// ============================================================================
// SENSOR SUBSYSTEM
// ============================================================================

static void readSensors() {
    // Temperature sensor
    log::evt(INFO, SensorLog{1, 0, 23.45f, true}, "Temperature reading");

    // Humidity sensor
    log::evt(INFO, SensorLog{2, 1, 65.2f, true}, "Humidity reading");

    // Pressure sensor (failure)
    log::evt(WARN, SensorLog{3, 2, 0.0f, false}, "Pressure sensor failed");
    log::evt(WARN, ErrorLog{"ERR_SENSOR_TIMEOUT", "pressure"});
}

// ============================================================================
// DEVICE CONTROL SUBSYSTEM
// ============================================================================

static void controlDevices() {
    // Turn on relay
    log::evt(INFO, DeviceLog{1, 0, 1, true}, "Relay 1 activated");

    // Turn off LED
    log::evt(INFO, DeviceLog{2, 1, 0, true}, "LED 2 disabled");

    // Control failure
    log::evt(ERROR, DeviceLog{3, 2, 0, false}, "Relay 3 control failed");
    log::err("ERR_DEVICE_TIMEOUT", nullptr, "relay3"); // Uses err() shortcut
}

// ============================================================================
// MODBUS SUBSYSTEM
// ============================================================================

static void modbusOperations() {
    // Read holding register
    log::evt(INFO, ModbusLog{"read", "SETPOINT", "holding", 100, 22.5f, true},
             "Read setpoint register");

    // Write holding register
    log::evt(INFO, ModbusLog{"write", "OUTPUT", "holding", 200, 75.0f, true},
             "Write output value");

    // Read timeout
    log::evt(ERROR, ModbusLog{"read", "SENSOR_INPUT", "input", 300, 0.0f, false},
             "Modbus timeout");
    log::err("ERR_MODBUS_TIMEOUT", "Timeout reading sensor", "slave1");
}

// ============================================================================
// NETWORK SUBSYSTEM
// ============================================================================

static void networkOperations() {
    // HTTP POST
    log::evt(INFO, HttpLog{"POST", "/api/sensors", 201, 45},
             "Sensor data uploaded");

    // HTTP GET
    log::evt(INFO, HttpLog{"GET", "/api/config", 200, 23},
             "Config retrieved");

    // HTTP error
    log::evt(WARN, HttpLog{"PUT", "/api/device/999", 404, 12},
             "Device not found");

    // MQTT publish failure
    log::evt(ERROR, NetworkLog{0, "sensors/temperature", 0, 0, false},
             "MQTT publish failed");
    log::err("ERR_MQTT_PUBLISH", "Failed to publish sensor data", "broker");

    // MQTT connection failure
    log::evt(ERROR, NetworkLog{0, "mqtt://broker.local:1883", 0, 0, false},
             "MQTT connection to broker failed");
    log::err("ERR_MQTT_CONNECT", "Cannot connect to MQTT broker", "broker");
}

// ============================================================================
// SYSTEM SUBSYSTEM
// ============================================================================

static void systemEvents() {
    // WiFi events
    log::evt(INFO, SystemLog{"wifi", "init", 0}, "Initializing WiFi");
    log::evt(INFO, SystemLog{"wifi", "connected", -45}, "WiFi connected (RSSI: -45 dBm)");

    // MQTT events
    log::evt(INFO, SystemLog{"mqtt", "connect", 1883}, "Connecting to MQTT broker");
    log::evt(INFO, SystemLog{"mqtt", "connected", 0}, "MQTT broker connected");

    // System error
    log::evt(ERROR, SystemLog{"storage", "error", -1}, "SD card mount failed");
    log::err("ERR_MOUNT_FAILED", "Cannot mount SD card", "sd_card");
}

// ============================================================================
// ISR HANDLER
// ============================================================================

volatile bool buttonPressed = false;

void IRAM_ATTR buttonISR() {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    buttonPressed = true;

    // ISR-safe logging - multiple calls accumulate wake flag
    log::lnFromISR(INFO, "GPIO interrupt triggered", &xHigherPriorityTaskWoken);
    log::evtFromISR(INFO, SystemLog{"gpio", "button", 1}, nullptr, &xHigherPriorityTaskWoken);

    // Single yield at end (FreeRTOS standard pattern)
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

} // namespace iotgw
