/* EZLogger Test Suite - Native Build (macOS/Linux) */

#include <cstdio>
#include <thread>
#include <chrono>
#include <vector>
#include <string>

// Include EZLogger with IoT Gateway config
#include "../../examples/advanced_logger/IoTGatewayLogConfig.hpp"

using namespace std::chrono_literals;
using namespace iotgw;

// JSON subscriber for batch printing
#if LOG_ENABLE_JSON_MSGBUF
static MessageBufferHandle_t jsonMsgBuffer = nullptr;
static std::vector<std::string> jsonAccumulator;

void readJsonBuffer() {
    char buf[2048];
    while (true) {
        size_t received = xMessageBufferReceive(jsonMsgBuffer, buf, sizeof(buf) - 1, 0);
        if (received == 0) break;
        buf[received] = '\0';
        jsonAccumulator.push_back(buf);
    }
}

void flushJsons() {
    readJsonBuffer();
    if (jsonAccumulator.empty()) return;

    printf("\n");
    printf("────────────────────────────────────────────────────────────────────────────────\n");
    printf("STRUCTURED LOGS (JSON)\n");
    printf("────────────────────────────────────────────────────────────────────────────────\n");
    for (const auto& json : jsonAccumulator) {
        printf("%s\n", json.c_str());
    }
    printf("\n");
    jsonAccumulator.clear();
}
#endif

// Test utilities
void printSeparator(const char* title) {
    printf("\n");
    printf("================================================================================\n");
    printf("TEST: %s\n", title);
    printf("================================================================================\n");
}

void waitForLogs() {
    std::this_thread::sleep_for(100ms); // Give worker thread time to process
}

// Test functions
void testTextLogs() {
    printSeparator("Text Logs (Simple & Formatted)");

    log::ln(INFO, "Simple info message");
    waitForLogs();

    log::ln(DEBUG, "Debug with value = %d", 42);
    waitForLogs();

    log::ln(WARN, "Warning: temperature = %.2f°C", 25.67f);
    waitForLogs();

    log::ln(ERROR, "Error occurred with code %d and status %s", 500, "FAIL");
    waitForLogs();

    #if LOG_ENABLE_JSON_MSGBUF
        flushJsons();
    #endif
}

void testErrorLogs() {
    printSeparator("Error Events");

    log::evt(ERROR, ErrorLog{"ERR_TIMEOUT", "mqtt"}, "MQTT connection timeout");
    waitForLogs();

    log::evt(ERROR, ErrorLog{"ERR_INVALID", "config"}, "Invalid parameter in config");
    waitForLogs();

    log::evt(WARN, ErrorLog{"ERR_BUSY", "modbus"}, "Resource busy, retry later");
    waitForLogs();

    #if LOG_ENABLE_JSON_MSGBUF
        flushJsons();
    #endif
}

void testModbusLogs() {
    printSeparator("Modbus Operations");

    ModbusLog read_success{
        "read", "CO2_SP", "holding", 142, 800.0f, true
    };
    log::evt(INFO, read_success, "Setpoint read successfully");
    waitForLogs();

    ModbusLog read_fail{
        "read", "TEMP_PV", "input", 100, 0.0f, false
    };
    log::evt(ERROR, read_fail, "Sensor communication failed");
    waitForLogs();

    ModbusLog write_success{
        "write", "HUMIDITY_SP", "holding", 150, 55.0f, true
    };
    log::evt(INFO, write_success);
    waitForLogs();

    #if LOG_ENABLE_JSON_MSGBUF
        flushJsons();
    #endif
}

void testHttpLogs() {
    printSeparator("HTTP Requests");

    log::evt(INFO, HttpLog{"GET", "/api/sensors", 200, 15});
    waitForLogs();

    log::evt(INFO, HttpLog{"POST", "/api/config", 201, 45}, "Config updated");
    waitForLogs();

    log::evt(WARN, HttpLog{"GET", "/api/invalid", 404, 5});
    waitForLogs();

    log::evt(ERROR, HttpLog{"POST", "/api/actuators", 500, 120}, "Internal server error");
    waitForLogs();

    #if LOG_ENABLE_JSON_MSGBUF
        flushJsons();
    #endif
}

void testSensorLogs() {
    printSeparator("Sensor Readings");

    log::evt(INFO, SensorLog{1, 0, 22.5f, true}, "Temperature sensor");
    waitForLogs();

    log::evt(INFO, SensorLog{2, 1, 65.3f, true}, "Humidity sensor");
    waitForLogs();

    log::evt(WARN, SensorLog{3, 2, 0.0f, false}, "Pressure sensor offline");
    waitForLogs();

    #if LOG_ENABLE_JSON_MSGBUF
        flushJsons();
    #endif
}

void testDeviceLogs() {
    printSeparator("Device Control");

    log::evt(INFO, DeviceLog{10, 0, 1, true}, "Heater turned on");
    waitForLogs();

    log::evt(INFO, DeviceLog{11, 1, 0, true}, "Fan turned off");
    waitForLogs();

    log::evt(ERROR, DeviceLog{12, 2, 1, false}, "Failed to toggle humidifier");
    waitForLogs();

    #if LOG_ENABLE_JSON_MSGBUF
        flushJsons();
    #endif
}

void testNetworkLogs() {
    printSeparator("Network Operations");

    log::evt(INFO, NetworkLog{0, "mqtt://broker.local", 200, 512, true}, "MQTT message sent");
    waitForLogs();

    log::evt(WARN, NetworkLog{1, "https://api.example.com", 503, 0, false}, "HTTP timeout");
    waitForLogs();

    log::evt(INFO, NetworkLog{2, "coap://sensor.local", 200, 128, true});
    waitForLogs();

    #if LOG_ENABLE_JSON_MSGBUF
        flushJsons();
    #endif
}

void testSystemLogs() {
    printSeparator("System Events");

    log::evt(INFO, SystemLog{"wifi", "init", 0}, "WiFi initializing");
    waitForLogs();

    log::evt(INFO, SystemLog{"wifi", "ready", 1}, "WiFi connected");
    waitForLogs();

    log::evt(ERROR, SystemLog{"storage", "error", -1}, "Flash write failed");
    waitForLogs();

    log::evt(WARN, SystemLog{"memory", "low", 2048}, "Low heap warning");
    waitForLogs();

    #if LOG_ENABLE_JSON_MSGBUF
        flushJsons();
    #endif
}

void testEdgeCases() {
    printSeparator("Edge Cases & Stress Tests");

    // nullptr strings
    log::evt(ERROR, ModbusLog{nullptr, nullptr, nullptr, 0, 0.0f, false}, "All fields null");
    waitForLogs();

    // Very long formatted string
    log::ln(DEBUG, "Long message: %d %d %d %d %d %d %d %d %d %d",
            1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    waitForLogs();

    // Float precision edge cases
    log::ln(INFO, "Pi = %.10f", 3.14159265359);
    waitForLogs();

    // Empty message
    log::ln(WARN, "");
    waitForLogs();

    // Rapid fire (stress test)
    printf("\n[Stress test: 20 rapid logs]\n");
    for (int i = 0; i < 20; ++i) {
        log::ln(DEBUG, "Rapid log #%d", i);
    }
    waitForLogs();

    #if LOG_ENABLE_JSON_MSGBUF
        flushJsons();
    #endif
}

void testFilterLevels() {
    printSeparator("Runtime Filter Levels");

    printf("  Current level: %s\n\n", ezlog::toString(log::getFilterLevel()));

    log::ln(ERROR, "ERROR level message (always visible)");
    log::ln(WARN, "WARN level message");
    log::ln(INFO, "INFO level message");
    log::ln(DEBUG, "DEBUG level message");
    waitForLogs();

    printf("\n  Setting filter to WARN...\n\n");
    log::setFilterLevel(ezlog::WARN);

    log::ln(ERROR, "ERROR (visible)");
    log::ln(WARN, "WARN (visible)");
    log::ln(INFO, "INFO (FILTERED)");
    log::ln(DEBUG, "DEBUG (FILTERED)");
    waitForLogs();

    printf("\n  Restoring filter to DEBUG...\n\n");
    log::setFilterLevel(ezlog::DEBUG);

    #if LOG_ENABLE_JSON_MSGBUF
        flushJsons();
    #endif
}

// Main test runner
int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                         EZLogger Test Suite                                ║\n");
    printf("║                    Native Build (macOS/Linux/Windows)                      ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════════╝\n");

    #if LOG_ENABLE_JSON_MSGBUF
        // Create JSON subscriber MessageBuffer
        static uint8_t jsonBufStorage[8192];
        static StaticMessageBuffer_t jsonBufStruct;
        jsonMsgBuffer = xMessageBufferCreateStatic(sizeof(jsonBufStorage), jsonBufStorage, &jsonBufStruct);
        log::subscribeJson(jsonMsgBuffer);
    #endif

    // Run all tests
    testTextLogs();
    testErrorLogs();
    testModbusLogs();
    testHttpLogs();
    testSensorLogs();
    testDeviceLogs();
    testNetworkLogs();
    testSystemLogs();
    testEdgeCases();
    testFilterLevels();

    // Final separator
    printSeparator("All Tests Complete");

    printf("\n  Waiting for worker thread to finish...\n");
    std::this_thread::sleep_for(500ms);

    printf("\n  ✓ All tests completed successfully!\n\n");

    return 0;
}
