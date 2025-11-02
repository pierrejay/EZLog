/*
 * LogConfig Template - Reference implementation for EZLog configuration
 *
 * This file serves as a template for creating your own logger configuration.
 * Copy this file to your project and adapt the platform hooks to your target.
 *
 * See also:
 * - examples/simple_logger/LogConfig.hpp (text-only logging)
 * - examples/advanced_logger/LogConfig.hpp (custom events + extended logger)
 */

#pragma once
#include "EZLog.hpp"

// ============================================================================
// CUSTOM EVENT TYPES (optional)
// ============================================================================

/*
 * Define your custom event types here. Each event type must provide:
 * - static constexpr const char* NAME - Event type identifier for JSON output
 * - size_t toConsole(char* out, size_t cap) const - Format for console output
 * - void toJson(ezlog::JsonBuilder& json) const - Serialize to JSON
 *
 * Guidelines:
 * - Use string literals only (const char*), never std::string or Arduino String
 * - Keep toConsole() output concise (default: max 256 chars TOTAL, including timestamp/context/tag/level)
 * - toJson() should add all relevant fields to the JsonBuilder
 *
 * Example:
 */
struct ErrorLog {
    const char* subsystem;  // Use string literals only: "mqtt", "modbus", etc.
    int32_t errorCode;
    const char* details;

    // Name displayed for this event type in console & JSON output
    static constexpr const char* NAME = "Error";

    // Write directly to the `out` buffer & return the number of 
    // characters written (excluding the null terminator)
    size_t toConsole(char* out, size_t cap) const {
        return snprintf(out, cap, "Error[%s]: code=%d msg=%s",
                       subsystem, errorCode, details);
    }

    // Use EZLog's internal JSON builder to create the output JSON object
    void toJson(ezlog::JsonBuilder& json) const {
        json.add("subsystem", subsystem);
        json.add("error_code", errorCode);
        json.add("details", details);
    }
};


// ============================================================================
// LOGGER CONFIGURATION
// ============================================================================

struct LogConfig {
    // ────────────────────────────────────────────────────────────────────────
    // MODULE IDENTIFICATION
    // ────────────────────────────────────────────────────────────────────────

    /*
     * @brief Module identifier tag
     * @note Appears in console output and JSON "tag" field
     * @note Use short, lowercase identifiers (e.g., "app", "net", "sensor")
     * @note Must be a string literal (not dynamically allocated)
     */
    static constexpr const char* TAG = "app";


    // ────────────────────────────────────────────────────────────────────────
    // COMPILE-TIME LOG LEVEL FILTERING
    // ────────────────────────────────────────────────────────────────────────

    /*
     * @brief Compile-time log level threshold
     * @note Log calls below this level are completely compiled out (zero overhead)
     * @note Available levels: ezlog::ERROR, ezlog::WARN, ezlog::INFO, ezlog::DEBUG
     * @note Can be overridden at runtime with setRuntimeLevel() (if enabled)
     *
     * Example:
     *   MODULE_LEVEL = ERROR  → Only ERROR logs compiled in
     *   MODULE_LEVEL = DEBUG  → All logs compiled in (ERROR, WARN, INFO, DEBUG)
     */
    static constexpr ezlog::Level MODULE_LEVEL = ezlog::DEBUG;


    // ────────────────────────────────────────────────────────────────────────
    // EVENT VARIANT
    // ────────────────────────────────────────────────────────────────────────

    /*
     * @brief Event type variant definition
     * @note Must ALWAYS include ezlog::TextLog as first type
     * @note Add your custom event types after TextLog
     * @note Order matters: earlier types have slightly lower overhead
     *
     * Examples:
     *   Text-only:           std::variant<ezlog::TextLog>
     *   With custom events:  std::variant<ezlog::TextLog, ErrorLog, ModbusLog>
     */
    using EventVariant = std::variant<
        ezlog::TextLog,  // Required - do not remove
        ErrorLog         // Optional - add your custom events here
        // ... add more custom event types as needed
    >;


    // ────────────────────────────────────────────────────────────────────────
    // PLATFORM HOOK: TIMESTAMP
    // ────────────────────────────────────────────────────────────────────────

    /*
     * @brief Provide monotonic timestamp in milliseconds
     * @return Timestamp in milliseconds since boot (or epoch)
     *
     * Contract:
     * - MUST be monotonic (never decrease)
     * - MUST NOT wrap before ~49 days (uint64_t recommended)
     * - Called from worker task context (NOT ISR)
     * - Thread-safety: concurrent calls possible if using multiple logger instances
     * - Performance: called once per log event, should be fast (<1μs)
     *
     * Platform examples:
     *   Arduino:     return (uint64_t)millis();
     *   ESP-IDF:     return (uint64_t)(esp_timer_get_time() / 1000ULL);
     *   STM32/HAL:   return (uint64_t)HAL_GetTick();
     *   FreeRTOS:    return (uint64_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
     *   Linux/POSIX: struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
     *                return (uint64_t)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
     */
    static inline uint64_t getTimeMs() {
        // TODO: Implement for your platform
        return 0;  // Replace with actual implementation
    }


    // ────────────────────────────────────────────────────────────────────────
    // PLATFORM HOOK: OUTPUT SINK
    // ────────────────────────────────────────────────────────────────────────

    /*
     * @brief Output formatted log to platform-specific sink
     * @param data Null-terminated C string containing formatted log line
     * @param len Length of data in bytes (excluding null terminator)
     * @return Number of bytes successfully written, or -1 on error
     * @note data already includes the newline characters ('\r\n') at the end
     *
     * Contract:
     * - Return the number of characters written (`0` for timeout/would block)
     * - Return `-1` if an error occurred (sink unavailable, critical TX error...)
     * 
     * Additional info:
     * - Called from worker task context (NOT ISR) - safe to block briefly
     * - MUST be thread-safe if multiple logger instances write to same sink
     * - SHOULD flush output to ensure visibility (e.g., Serial.flush())
     * - EZLog will retry the write operation until all characters are written or timeout occurs
     * - If a write fails, the log event is dropped and the next one is processed
     *
     * Performance:
     * - Called once per log event from worker task
     * - Blocking here delays worker task (affects queue processing, not log ingestion from code)
     * - Consider buffering or DMA for high-throughput scenarios
     *
     * Thread-safety:
     * - If using multiple logger instances with same sink (e.g., shared UART):
     *   Protect your sink with a mutex/critical section to prevent interleaved output
     *
     * Platform examples:
     *
     *   Arduino (UART):
     *     if (!Serial) return -1;
     *     const size_t written = Serial.write(data, len);
     *     Serial.flush();
     *     return (int)written;
     *
     *   ESP-IDF (console):
     *     return (int)fwrite(data, 1, len, stdout);
     *
     *   STM32/HAL (UART with timeout):
     *     extern UART_HandleTypeDef huart2;
     *     HAL_StatusTypeDef status = HAL_UART_Transmit(&huart2, (uint8_t*)data, len, 100);
     *     return (status == HAL_OK) ? (int)len : -1;
     *
     *   Linux/POSIX (stderr):
     *     ssize_t written = write(STDERR_FILENO, data, len);
     *     return (int)written;
     *
     *   Thread-safe multi-logger (shared UART):
     *     static SemaphoreHandle_t uartMutex = xSemaphoreCreateMutex();
     *     xSemaphoreTake(uartMutex, portMAX_DELAY);
     *     const size_t written = Serial.write(data, len);
     *     Serial.flush();
     *     xSemaphoreGive(uartMutex);
     *     return (int)written;
     */
    static inline int printLog(const char* data, size_t len) {
        // TODO: Implement for your platform
        (void)data;  // Suppress unused parameter warning
        (void)len;
        return -1;  // Replace with actual implementation
    }
};


// ────────────────────────────────────────────────────────────────────────
// EXPORT LOGGER INSTANCE & LEVELS
// ────────────────────────────────────────────────────────────────────────

// Default namespace : `logger` - You can use an existing namespace or create a new one
namespace logger {
    /*
    * @brief Export logger instance as type alias
    * @note For multi-logger setups, use descriptive names (e.g., net::log, sensor::log)
    *
    * Usage:
    *   using namespace logger;
    *   log::ln(INFO, "Message");
    *   log::evt(ERROR, ErrorLog{"mqtt", -1, "Connection failed"});
    */
    using log = ezlog::Logger<LogConfig>;

    /*
    * @brief Optional: Import log level constants into global namespace
    * @note Allows using ERROR, WARN, INFO, DEBUG without ezlog:: prefix
    * @note Comment out if you prefer explicit ezlog::ERROR, etc.
    */
    using ezlog::ERROR;
    using ezlog::WARN;
    using ezlog::INFO;
    using ezlog::DEBUG;
}
