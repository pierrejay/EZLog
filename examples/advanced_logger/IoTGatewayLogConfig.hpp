/* @file IoTGatewayLogConfig.hpp - IoT Gateway Logger Configuration
 *
 * Advanced example with 7 custom event types + extended logger with shortcuts.
 * For detailed hook contracts and multi-platform examples, see `examples/LogConfig_Template.hpp`.
 */

#pragma once

#include "EZLog.hpp"
#include <esp_timer.h>
#include <variant>

// ============================================================================
// CUSTOM EVENT TYPES
// ============================================================================

struct ErrorLog {
    const char* code = nullptr;
    const char* context = nullptr;

    static constexpr const char* NAME = "Error";

    size_t toConsole(char* out, size_t cap) const {
        if (context) {
            return snprintf(out, cap, "%s Error: %s", context, code ? code : "");
        }
        return snprintf(out, cap, "Error: %s", code ? code : "");
    }

    void toJson(ezlog::JsonBuilder& json) const {
        if (code) json.add("code", code);
        if (context) json.add("context", context);
    }
};

struct ModbusLog {
    const char* operation = nullptr;
    const char* regName = nullptr;
    const char* regType = nullptr;
    uint16_t startAddr = 0;
    float value = 0.0f;
    bool success = false;

    static constexpr const char* NAME = "Modbus";

    size_t toConsole(char* out, size_t cap) const {
        return snprintf(out, cap, "Modbus: %s %s (%s %u) val=%d.%03d %s",
                       operation ? operation : "?",
                       regName ? regName : "?",
                       regType ? regType : "?",
                       startAddr,
                       FTOA(value, 3),
                       success ? "OK" : "FAIL");
    }

    void toJson(ezlog::JsonBuilder& json) const {
        if (operation) json.add("operation", operation);
        if (regName) json.add("reg_name", regName);
        if (regType) json.add("reg_type", regType);
        json.add("start_addr", startAddr);
        json.add("value", value);
        json.add("success", success);
    }
};

struct HttpLog {
    const char* method = nullptr;
    const char* route = nullptr;
    uint16_t statusCode = 0;
    uint32_t durationMs = 0;

    static constexpr const char* NAME = "HTTP";

    size_t toConsole(char* out, size_t cap) const {
        return snprintf(out, cap, "HTTP: %s %s -> %u (%ums)",
                       method ? method : "?",
                       route ? route : "?",
                       statusCode,
                       durationMs);
    }

    void toJson(ezlog::JsonBuilder& json) const {
        if (method) json.add("method", method);
        if (route) json.add("route", route);
        json.add("status", statusCode);
        json.add("duration_ms", durationMs);
    }
};

struct SensorLog {
    uint16_t sensor_id = 0;
    uint8_t sensor_type = 0;  // 0=temp, 1=humidity, 2=pressure
    float value = 0.0f;
    bool valid = false;

    static constexpr const char* NAME = "Sensor";

    size_t toConsole(char* out, size_t cap) const {
        const char* typeStr = (sensor_type == 0) ? "temp" : (sensor_type == 1) ? "humi" : "pres";
        return snprintf(out, cap, "Sensor[%u] %s=%d.%02d %s",
                       sensor_id, typeStr,
                       FTOA(value, 2),
                       valid ? "OK" : "FAIL");
    }

    void toJson(ezlog::JsonBuilder& json) const {
        json.add("sensor_id", sensor_id);
        json.add("sensor_type", sensor_type);
        json.add("value", value);
        json.add("valid", valid);
    }
};

struct DeviceLog {
    uint16_t device_id = 0;
    uint8_t action = 0;   // 0=on, 1=off, 2=toggle
    uint8_t state = 0;    // 0=off, 1=on
    bool success = false;

    static constexpr const char* NAME = "Device";

    size_t toConsole(char* out, size_t cap) const {
        const char* actionStr = (action == 0) ? "ON" : (action == 1) ? "OFF" : "TOGGLE";
        const char* stateStr = (state == 0) ? "off" : "on";
        return snprintf(out, cap, "Device[%u] %s -> %s %s",
                       device_id, actionStr, stateStr,
                       success ? "OK" : "FAIL");
    }

    void toJson(ezlog::JsonBuilder& json) const {
        json.add("device_id", device_id);
        json.add("action", action);
        json.add("state", state);
        json.add("success", success);
    }
};

struct NetworkLog {
    uint8_t protocol = 0;  // 0=mqtt, 1=http, 2=coap
    const char* endpoint = nullptr;
    uint16_t status_code = 0;
    uint32_t bytes = 0;
    bool success = false;

    static constexpr const char* NAME = "Network";

    size_t toConsole(char* out, size_t cap) const {
        const char* protoStr = (protocol == 0) ? "MQTT" : (protocol == 1) ? "HTTP" : "CoAP";
        return snprintf(out, cap, "%s %s -> %u (%uB) %s",
                       protoStr, endpoint, status_code, bytes,
                       success ? "OK" : "FAIL");
    }

    void toJson(ezlog::JsonBuilder& json) const {
        json.add("protocol", protocol);
        json.add("endpoint", endpoint);
        json.add("status_code", status_code);
        json.add("bytes", bytes);
        json.add("success", success);
    }
};

struct SystemLog {
    const char* component = nullptr;  // "wifi", "storage", "memory"
    const char* event = nullptr;      // "init", "ready", "error", "restart"
    int32_t value = 0;

    static constexpr const char* NAME = "System";

    size_t toConsole(char* out, size_t cap) const {
        return snprintf(out, cap, "System: %s.%s val=%d",
                       component, event, value);
    }

    void toJson(ezlog::JsonBuilder& json) const {
        json.add("component", component);
        json.add("event", event);
        json.add("value", value);
    }
};

// ============================================================================
// IOT GATEWAY LOGGER CONFIGURATION
// ============================================================================

struct IoTGatewayLogConfig {
    static constexpr const char* TAG = "iot-gw";

    static constexpr ezlog::Level MODULE_LEVEL = static_cast<ezlog::Level>(LOG_GLOBAL_LEVEL);

    using EventVariant = std::variant<
        ezlog::TextLog,
        ErrorLog,
        ModbusLog,
        HttpLog,
        SensorLog,
        DeviceLog,
        NetworkLog,
        SystemLog
    >;

    static inline uint64_t getTimeMs() {
        return (uint64_t)(esp_timer_get_time() / 1000ULL);
    }

    static inline int printLog(const char* data, size_t len) {
        size_t written = fwrite(data, 1, len, stdout);
        fflush(stdout);
        return (int)written;
    }
};

// ============================================================================
// EXTENDED LOGGER WITH CONVENIENCE HELPERS
// ============================================================================

/*
 * @brief Extended logger with convenience helpers for common event types
 * @note Inherits all ezlog::Logger methods and adds module-specific shortcuts
 * @note Example: log::err("timeout", "MQTT connection lost", "mqtt");
 */
class IoTGatewayLogger : public ezlog::Logger<IoTGatewayLogConfig> {
public:
    // Convenience helper for ErrorLog (always ERROR level, auto-captures source location)
    static inline void err(const char* code, 
                           const char* msg = nullptr, 
                           const char* ctx = nullptr,
                           const ezlog::SrcLoc& loc = ezlog::SrcLoc::current()) 
    {
        evt({ezlog::ERROR, loc}, ErrorLog{code, ctx}, msg);
    }
};

// ============================================================================
// CONVENIENCE EXPORTS
// ============================================================================

// In this example, we export the logger instance to the iotgw namespace. Other
// modules of the application can use their own logger instance with the same
// `log` symbol in their own isolated scope without conflict.
namespace iotgw {
    using log = IoTGatewayLogger;  // Use extended logger with shortcuts
    using ezlog::ERROR;
    using ezlog::WARN;
    using ezlog::INFO;
    using ezlog::DEBUG;
    using ezlog::OFF;
}