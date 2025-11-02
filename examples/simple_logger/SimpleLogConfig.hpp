/* @file SimpleLogConfig.hpp - Simple Logger Configuration
 *
 * Minimal text-only logger for Arduino.
 * For detailed hook contracts and multi-platform examples, see `examples/LogConfig_Template.hpp`.
 */

#pragma once

#include "EZLog.hpp"
#include <Arduino.h>

// ============================================================================
// SIMPLE LOGGER CONFIGURATION
// ============================================================================

struct SimpleLogConfig {
    // Use "app" as module tag
    static constexpr const char* TAG = "app";

    // No specific module level in this example, just use LOG_GLOBAL_LEVEL
    static constexpr ezlog::Level MODULE_LEVEL = static_cast<ezlog::Level>(LOG_GLOBAL_LEVEL);

    // Only TextLog in this example (no custom events)
    using EventVariant = std::variant<ezlog::TextLog>;

    // Use Arduino millis() for timestamp (will wrap at ~50 days)
    static inline uint64_t getTimeMs() {
        return (uint64_t)millis();
    }

    // Print log to Serial
    static inline int printLog(const char* data, size_t len) {
        if (!Serial) return -1;
        const size_t written = Serial.write(data, len);
        Serial.flush();
        return (int)written;
    }
};

// ============================================================================
// CONVENIENCE EXPORTS
// ============================================================================

// Exported to logger namespace (simple example)
namespace logger {
    using log = ezlog::Logger<SimpleLogConfig>;
    using ezlog::ERROR;
    using ezlog::WARN;
    using ezlog::INFO;
    using ezlog::DEBUG;
    using ezlog::OFF;
}
