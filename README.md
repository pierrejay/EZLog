# EZLog documentation

A flexible, real-time & user-friendly logger based on C++17 & FreeRTOS that provides both human-readable console output and (optional) machine-parsable structured JSON logs, with minimal impact on performance-critical paths.

## Table of Contents

- [Introduction](#introduction)
- [Usage at a glance](#usage-at-a-glance)
- [Quick Start Guide](#quick-start-guide)
- [Consuming logs](#consuming-logs)
- [Architecture](#architecture)
- [Global configuration (build flags)](#global-configuration-build-flags)
- [Important considerations](#important-considerations)
- [Resource usage & performance](#resource-usage--performance)
- [Examples & Tests](#examples--tests)
- [Advanced usage](#advanced-usage)
    - [Multi-module deployment strategies](#advanced-usage-multi-module-deployment-strategies)
    - [Controlling source location](#advanced-usage-controlling-source-location)
    - [Float formatting helpers](#advanced-usage-float-formatting-helpers)
    - [Runtime log level filtering](#advanced-usage-runtime-log-level-filtering)
    - [Logging from ISR context](#advanced-usage-logging-from-isr-context)
    - [Shortcuts](#advanced-usage-shortcuts-eg-logerr)
    - [Symbol name conflicts](#advanced-usage-symbol-name-conflicts)
    - [Tuning message size & buffers](#advanced-usage-tuning-log_json_str_cap)
    - [Arduino Stream adapter](#advanced-usage-arduino-stream-adapter)

## Introduction

### Philosophy

- **Asynchronous by design**: Minimal overhead & latency. Heavy lifting (formatting, serialization, I/O) is offloaded to a background FreeRTOS worker task.
- **Native C++**: Type-safe, expressive, macro-free logging experience. Does not pollute your global namespace: you control the logger scope.
- **Zero-cost when disabled**: Logging calls are completely compiled out when disabled/filtered at either the global or local level (with `-O2` or `-Os`), resulting in zero runtime overhead.
- **Versatile**: Generates both human-friendly console output for immediate debugging and optional structured JSON logs.
- **Extensible**: A policy-based template design allows for easy compile-time configuration and extension with custom, user-defined log event types (POD structs).
- **No heap usage**: Uses 100% static memory objects, including the internal FreeRTOS task & queue.

### Key Features

- **Dual output**:
  - **Text**: Formatted text output for live debugging, printed to console or sent to a MessageBuffer.
  - **Structured JSON**: Rich, machine-readable output for telemetry or post-processing.
- **Flexible log levels**: Offers standard `ERROR`, `WARN`, `INFO`, `DEBUG` levels with both compile-time and runtime filtering.
- **Text & structured events**: Supports raw & `printf`-style text logging as well as custom, strongly-typed structured events. Configurable console output format.
- **Automatic context**: Automatically captures call site location (`file`, `function`, `line`) with zero runtime cost. Allows forwarding context from a different call site.
- **Thread-safe & no-lock**: Log methods can be called concurrently from different contexts without blocking.
- **Minimal dependencies**: Requires only C++17 and FreeRTOS. Includes a built-in lightweight JSON builder for structured logs.
- **Hardware-agnostic**: The policy-based design allows to implement a custom time provider & console sink to cater for different MCU platforms.
- **Fast**: Hot-path friendly with only ~5µs latency per producer-side log call, orders of magnitude faster than e.g. `ESP_LOG`.

## Usage at a glance

Logging is designed to be clean and intuitive at the call site. Here's what it looks like in practice.

### 1. Basic text logging

The simplest way to log a static string.

#### Code
```cpp
log::ln(INFO, "Simple info message");
```

#### Console Output
```
I 2420936.938 my-app Simple info message [test_main.cpp:61]
```

#### JSON Output
```json
{
  "tag": "my-app",
  "event": "Text",
  "level": "INFO",
  "message": "Simple info message",
  "data": {},
  "meta": {
    "ts_ms": 2420936938,
    "file": "test_main.cpp",
    "func": "testTextLogs",
    "line": 61
  }
}
```

### 2. Formatted text logging

Use a familiar `printf`-style format for dynamic messages.

#### Code
```cpp
log::ln(WARN, "Warning: temperature = %.2f°C", 65.67f);
```

#### Console Output
```
W 2420937.145 my-app Warning: temperature = 65.67°C [test_main.cpp:67]
```

#### JSON Output
```json
{
  "tag": "my-app",
  "event": "Text",
  "level": "WARN",
  "message": "Warning: temperature = 65.67°C",
  "data": {},
  "meta": {
    "ts_ms": 2420937145,
    "file": "test_main.cpp",
    "func": "testTextLogs",
    "line": 67
  }
}
```

### 3. Structured event logging

Log rich, structured data using custom event types for powerful, machine-readable telemetry.

#### Event schema
```cpp
// To be defined in your config header (see configuration section)
struct ModbusLog {
    const char* operation;
    const char* regName;
    const char* regType;
    uint16_t startAddr;
    float value;
    bool success;
};
```

#### Code

Inline style
```cpp
log::evt(ERROR, ModbusLog{"read", "TEMP_PV", "input", 100, 0.0f, false}, "Sensor communication failed");
````

Verbose style
```cpp
log::evt(ERROR, ModbusLog{
    .operation  = "read",
    .regName    = "TEMP_PV",
    .regType    = "input",
    .startAddr  = 100,
    .value      = 0.0f,
    .success    = false
}, "Sensor communication failed");
```

#### Console Output
```
E 2420937.772 my-app Modbus: read TEMP_PV (input 100) val=0.000 FAIL - Sensor communication failed [test_main.cpp:120]
```

#### JSON Output
```json
{
  "tag": "my-app",
  "event": "Modbus",
  "level": "ERROR",
  "message": "Sensor communication failed",
  "data": {
    "operation": "read",
    "reg_name": "TEMP_PV",
    "reg_type": "input",
    "start_addr": 100,
    "value": 0,
    "success": false
  },
  "meta": {
    "ts_ms": 2420937772,
    "file": "test_main.cpp",
    "func": "testModbusLogs",
    "line": 120
  }
}
```

## Quick Start Guide

EZLog is header-only with zero dependencies beyond C++17 and FreeRTOS. Create a configuration file (e.g., `LogConfig.hpp`) that defines your logger policy and exports the logger instance.

**Reference template**: `examples/LogConfig_Template.hpp` - detailed hook contracts + multi-platform examples.

### Minimal configuration (text-only)

Start simple with just text logging. To keep this example simple, the `printLog` hook is implemented for the Arduino platform but you will find detailed guidelines for other platforms in the reference template.

```cpp
/* @file LogConfig.hpp - Minimal configuration (text-only) */

#pragma once
#include "EZLog.hpp"
#include <Arduino.h>

struct LogConfig {
    static constexpr const char* TAG = "app";
    static constexpr ezlog::Level MODULE_LEVEL = ezlog::DEBUG;

    // Text-only: no custom events needed
    using EventVariant = std::variant<ezlog::TextLog>;

    // Platform hooks (adapt to your MCU)
    static inline uint64_t getTimeMs() { return (uint64_t)millis(); }
    static inline int printLog(const char* data, size_t len) {
        if (!Serial) return -1;
        const size_t written = Serial.write(data, len);
        Serial.flush();
        return (int)written;
    }
};

// Export logger instance to generic "logger" namespace
namespace logger {
    using log = ezlog::Logger<LogConfig>;
    using ezlog::ERROR; using ezlog::WARN; using ezlog::INFO; using ezlog::DEBUG;
}
```

**Usage:** (in your `main.cpp` or other files)
```cpp
#include "LogConfig.hpp"
using namespace logger; // Import "log::" and level symbols in your code

void setup() {
    log::ln(INFO, "Application started");
    log::ln(WARN, "Temperature: %.2f°C", 25.67f);
}
```

Done. That's all you need for basic text logging.

**Bonus:** If you enable JSON output (see [Consuming logs - Structured JSON output](#structured-json-output)), your text logs will be automatically serialized to structured JSON with zero code changes. No custom events are needed to get machine-readable telemetry.

### Adding custom events (optional)

To add structured events, define POD structs with `NAME`, `toConsole()`, and `toJson()` (if using JSON output), then add them to the `EventVariant`:

```cpp
// Define custom event struct
struct ModbusLog {
    const char* operation;  // Use string literals only
    const char* regName;
    uint16_t startAddr;
    float value;
    bool success;

    // Name displayed for this event type in console & JSON output
    static constexpr const char* NAME = "Modbus";

    // Write directly to the `out` buffer & return the number of 
    // characters written (excluding the null terminator)
    size_t toConsole(char* out, size_t cap) const {
        return snprintf(out, cap, "Modbus: %s %s addr=%u val=%.3f %s",
                       operation, regName, startAddr, value, success ? "OK" : "FAIL");
    }

    // Uses EZLog's internal JSON builder
    void toJson(ezlog::JsonBuilder& json) const {
        json.add("operation", operation);
        json.add("reg_name", regName);
        json.add("start_addr", startAddr);
        json.add("value", value);
        json.add("success", success);
    }
};
```

**Note:** `JsonBuilder` is a lightweight custom JSON builder with zero dependencies. Just call `add(key, value)` with plain types (const char*, int/uint, float, bool). No external JSON library required.

```cpp
struct LogConfig {
    // ... (same as before)

    // Add your custom events after TextLog
    using EventVariant = std::variant<
        ezlog::TextLog, // Always required
        ModbusLog
        // ...add more
    >;
};
```

**Usage:**
```cpp
log::evt(ERROR, ModbusLog{"read", "TEMP_PV", 100, 0.0f, false}, "Sensor failed");
```

That's it. Start minimal, extend as needed.

## Consuming logs

The logger worker task handles all consumption in the background to ensure minimal impact on performance-critical paths.

### Console output

The console output is handled automatically by the `printLog` hook you provide in the configuration policy. No additional setup is required. The `LOG_ENABLE_CONSOLE` build flag (default: `1`) controls whether the console output is enabled at compile time.

The contract of this function is simple:
- Return the number of characters written (`0` for timeout/would block) or `-1` if an error occurred.
- EZLog will retry the write operation until all characters are written or timeout occurs.

### Structured JSON output

To consume structured logs, a consumer task subscribes to the logger instance.

1. Enable the structured JSON output at compile time (`LOG_ENABLE_JSON_MSGBUF` build flag set to `1`).
2. Create a FreeRTOS `MessageBuffer` to receive the JSON strings.
3. Subscribe to the logger instance by calling `log::subscribeJson()` with the buffer handle.
4. Implement a consumer task that waits for data on the message buffer.

By default, a JSON message is capped to 1024 bytes (`LOG_JSON_STR_CAP` build flag). In practice, it is recommended to use a 2-4KB buffer, enough to store 10-20 logs at a time.

**Example consumer task**
```cpp
#include "LogConfig.hpp"
#include <freertos/message_buffer.h>

static constexpr size_t JSON_MSGBUF_SIZE = 2048; // 2x JSON_STR_CAP

// Consumer task example
// (Make sure the logger instance is active before this task is spinned up)
void jsonConsumerTask(void* params) {
    char jsonStrBuf[ezlog::LOG_JSON_STR_CAP]; // Buffer to hold one log entry (default: 1KB)

    // 1. Create the message buffer
    auto jsonMsgBufHandle = xMessageBufferCreate(JSON_MSGBUF_SIZE);

    // 2. Subscribe to the logger instance with the MessageBuffer handle
    log::subscribeJson(jsonMsgBufHandle);

    // 3. Consume JSON logs in a loop
    while (true) {
        // Wait indefinitely for the next JSON log entry
        size_t read = xMessageBufferReceive(
            jsonMsgBufHandle,
            (void*)jsonStrBuf,
            sizeof(jsonStrBuf),
            portMAX_DELAY
        );

        if (read > 0) {
            // Process the received JSON string
            // For example, write it to an SD card, send over MQTT, etc.
            printf("JSON Message Received: %s\n", jsonStrBuf);
        }
    }
}
```

### Text output to MessageBuffer

If you want to consume console-formatted text messages from a MessageBuffer, you can follow the exact same steps as for the structured JSON output, with a different build flag & subscriber method: 

1. Enable the text output at compile time (`LOG_ENABLE_TEXT_MSGBUF` build flag set to `1`).
2. Create a FreeRTOS `MessageBuffer` to receive the text strings.
3. Subscribe to the logger instance by calling `log::subscribeText()` with the buffer handle.

This is useful for specific use cases where you want to route console logs to another system, for example a Web UI terminal receiving a copy of console logs through WebSocket.

## Architecture

```
User Code
    │
    ├─> log::ln()
    ├─> log::evt()
    ├─> log::lnFromISR()
    ├─> log::evtFromISR()
    │
    ▼
[FreeRTOS Queue] ←─── Lock-free, non-blocking
    │
    ▼
[Worker Task] ←────── Single writer, no locking
    │
    ├──> Console (text)
    │       └─> printLog() hook
    │
    ├──> JSON Subscribers (MessageBuffers)
    │       ├─> SD card task
    │       ├─> MQTT task
    │       └─> ...
    │
    └──> Text Subscribers (MessageBuffers)
            ├─> WebSocket task
            └─> ...
```

## Global configuration (build flags)

Global settings control the entire logging framework and should be set via build flags (e.g., in `platformio.ini`'s `build_flags` section or your project's `CMakeLists.txt`).

Normally, you don't need to tweak these settings as the default values are usually fine. Just set the `LOG_ENABLE_x` & `LOG_GLOBAL_LEVEL` flags depending on your needs and you should be good.

### Feature Toggles

Enable or disable entire output sinks at compile time. Disabled features are fully compiled out.

| Flag                     | Default | Description                                                               |
| ------------------------ | ------- | ------------------------------------------------------------------------- |
| `LOG_ENABLE_CONSOLE`     | 1       | Console text output via `printLog()` hook. Set to `0` to disable.        |
| `LOG_ENABLE_JSON_MSGBUF` | 0       | Structured JSON output via MessageBuffer subscribers. Set to `1` to enable. |
| `LOG_ENABLE_TEXT_MSGBUF` | 0       | Text output via MessageBuffer subscribers. Set to `1` to enable.          |

### Filtering

Control which log levels are compiled in and processed at runtime.

| Flag                  | Default   | Description                                                               |
| --------------------- | --------- | ------------------------------------------------------------------------- |
| `LOG_GLOBAL_LEVEL`    | 4=DEBUG | Master log level ceiling. `0`=OFF, `1`=ERROR, ..., `4`=DEBUG. Acts as an upper bound for all module-level settings. |

### Resource Tuning

Configure memory usage and task characteristics.

| Flag                      | Default | Description                                                               |
| ------------------------- | ------- | ------------------------------------------------------------------------- |
| `LOG_MAX_EVT_SIZE`        | 128     | Maximum size (in bytes) of any custom log event struct.                  |
| `LOG_EVT_QUEUE_LENGTH`    | 20      | Number of log records the internal FreeRTOS queue can hold.              |
| `LOG_MAX_JSON_SUBSCRIBERS`| 4       | Maximum number of concurrent JSON MessageBuffer subscribers.             |
| `LOG_MAX_TEXT_SUBSCRIBERS`| 4       | Maximum number of concurrent text MessageBuffer subscribers.             |
| `LOG_JSON_STR_CAP`        | 1024    | Maximum length of a single JSON log entry (in bytes).                    |
| `LOG_TASK_STACK_SIZE`     | 2048 / 512    | Stack size (in bytes or words)* for the background logger worker task.             |
| `LOG_TASK_PRIORITY`       | 1       | FreeRTOS priority of the logger worker task.                             |
| `LOG_TASK_CPU_CORE`       | -1      | CPU core to pin the worker task to (ESP32 only). `-1` = no affinity.       |

**ESP-IDF uses bytes, other platforms use words. Defaults to 2KB on any platform (detects ESP32 compile flag).*

### Formatting & Output

Customize the appearance and behavior of console output.

| Flag                       | Default | Description                                                               |
| -------------------------- | ------- | ------------------------------------------------------------------------- |
| `LOG_CONSOLE_MAX_LINE_SIZE`| 256     | Maximum size of a single console line (in characters).                        |
| `LOG_CONSOLE_TIMEOUT_MS`   | 1000     | Timeout (in ms) for the `printLog()` hook when console is busy.          |
| `LOG_CONSOLE_SHOW_TIMESTAMP`| 1      | Show timestamp in console output. Set to `0` to hide.                    |
| `LOG_CONSOLE_SOURCE_CTX`   | `0b101` | Bitmask to show source location: `file` (4), `function` (2), `line` (1). 0 = hidden. |

Note: logs routed to MessageBuffer text subscribers follow the same rules as console output.

## Important considerations

- **Event structs must be trivially copyable**: Custom event structs are copied directly into the FreeRTOS queue. They **must** be POD types.
  - **DO**: Use primitive types (`int`, `float`, `bool`), fixed-size arrays (`char[32]`), and pointers to *static* data (`const char*`).
  - **DO NOT**: Use types with dynamic memory (`std::string`, `std::vector`), virtual functions, or complex constructors/destructors.

- **Pointer lifetime and string safety**: This is **critical** to understand. When logging structured events with `log::evt()`, if your event struct contains `const char*` pointers, **these pointers must remain valid until the worker task processes the event**. The struct itself is copied into the queue, but pointers inside it are not dereferenced until later.

- **Resource usage**: Each logger instantiation (e.g., `ezlog::Logger<LogConfig>`) creates its own static worker task, FreeRTOS queue, and associated buffers. This provides excellent isolation but incurs a fixed ROM and RAM cost for each module that uses its own logger.

### Safe practices for strings:

**✅ String literals in line & events** (always safe, have static lifetime):
```cpp
log::ln(INFO, "Literal");  // String literal → zero-copy (pointer only)
log::evt(INFO, ModbusLog{"read", "TEMP_PV", 100, 25.0f, true}, "Static message"); // Same
```

**✅ Static or global data in events** (safe, has static lifetime):
```cpp
static const char* deviceName = "ESP32-NODE";
log::evt(INFO, SystemLog{deviceName, version}, "Started");
```

**⚠️ Pointer lifetime:** `char*` and `const char*` are **zero-copy** (stores pointer only). Must point to static/global data or string literals. Use formatting (`%s`) to force copy.

**✅ Text logs with format - automatic copy to internal buffer** (`snprintf()` inside):
```cpp
const char* devName = "IOTGW";
int devNo = 42;
log::ln(INFO, "%s-%d", devName, devNo);  // Formatted text → copied in internal buffer (128B by default)
````

### Dangerous practices (AVOID):

**❌ Temporary `std::string`/`String` in events** (DANGER - destroyed before worker reads it):
```cpp
// ❌ WRONG - the std::string is destroyed, pointer becomes invalid!
void readSensor(int id) {
    std::string name = "SENSOR_" + std::to_string(id);
    log::evt(INFO, SensorLog{name.c_str(), value}, "Read");
    // BUG: name is destroyed here, but pointer is in the queue!
    // Use POD fields and/or string literals
}
```

**❌ Local buffer pointers in events** (DANGER - buffer goes out of scope):
```cpp
// ❌ WRONG - local buffer pointer in event struct!
void logData() {
    char operation[32];
    snprintf(operation, 32, "read_%d", opId);
    log::evt(INFO, ModbusLog{operation, "REG", 0, 0.0f, true}, "Op");
    // BUG: operation[] is destroyed, pointer in queue is invalid!
}

// ✅ CORRECT - use string literals or static data:
void logData() {
    log::evt(INFO, ModbusLog{"read", "REG", 0, 0.0f, true}, "Op");
}
```

**Rule of thumb**: Only use string literals or static/global C-strings in event structs. For dynamic data, use primitive types (int, float, bool) instead of char* pointers.

## Resource usage & performance

### Memory Footprint

#### RAM Usage (per logger instance, with default settings)

| Component | Size | Notes |
|-----------|------|-------|
| Worker task stack | 2 KB | `LOG_TASK_STACK_SIZE` |
| Event queue buffer | 3.5 KB | 20 records × (128B event + overhead) |
| Static buffers | 1 KB | Formatting buffers, subscriber arrays |
| **Total RAM** | **6.5 KB** | **Static allocation in .bss** |

The main RAM overhead comes from the event queue buffer. It can be further optimized:
- Reduce `LOG_EVT_QUEUE_LENGTH` if log isn't frequent and/or output sink is fast
- Reduce `LOG_MAX_EVT_SIZE` if you don't need the extra space needed for formatted strings (e.g. `log::ln("fmt %d", val)`). Usually, a custom log event `struct` is 12-32B for a few fields, using `const char*` & string literals when needed.

`LOG_MAX_EVT_SIZE=32` × `LOG_EVT_QUEUE_LENGTH=10` = **1KB RAM saved**

#### ROM Usage

- **Core library**: ~5 KB (excludes string literals and event implementations)
- Fully compiled out when logging is disabled (`LOG_GLOBAL_LEVEL=0` or `MODULE_LEVEL=OFF`)

### Performance benchmarks

Performance measured on **ESP32-S3 (Xtensa CPU @ 240 MHz)** with default settings.

| Operation | Average | Notes |
|-----------|---------|-------|
| `log::ln("text")` | ~4 µs | String literal, no formatting |
| `log::evt(...)` | ~5 µs | Structured event (POD struct) |
| `log::ln("fmt %d", val)` | ~15 µs | Printf-style, integers only |
| `log::ln("fmt %.2f", val)` | ~30 µs | Printf-style with float |

**Notes:**
- Times include **only the log call overhead** (queue send). Formatting and output happen asynchronously in the worker task.
- For maximum performance in hot paths where you still need enriched data, use structured events (`log::evt()`) instead of formatted text.
- **Call from ISR**: use dedicated `fromISR` API methods if logging from ISR context. Formatted text isn't supported (see [Logging from ISR context](#advanced-usage-logging-from-isr-context)).
- For efficient float formatting, use the provided `FTOA()` macro to avoid `snprintf()` overhead (see [Float formatting helpers](#advanced-usage-float-formatting-helpers)).

## Examples & Tests

### Examples

Two basic examples are provided in `examples/`:

**`simple_logger/`** - Text logging only (levels, formatting, runtime filtering, `FTOA()`) with minimal config.

**`advanced_logger/`** - IoT gateway with various custom event types (Error + comm + devices), structured JSON, extended logger with `log::err()` shortcut, ISR logging.

### Native Test Suite

**`test/test_native/`** - Native build tests for desktop environments. Tests all logger features without embedded dependencies (uses mocked FreeRTOS/ESP-IDF). See [test README](test/test_native/README.md) for build instructions.

## Advanced usage

### [Advanced usage] Multi-module deployment strategies

When building applications with multiple modules (e.g., `network`, `sensors`, `control`, etc.), you need to decide how to organize your logger instances. There are two main strategies, each with trade-offs.

#### Strategy 1: Single shared logger instance (Recommended)

Create one logger configuration and share it across all modules by declaring `using namespace logger` in all namespaces that will use it. Alternatively, you can directly export the logger instance & log levels to all your module namespaces in your configuration file.

**Advantages**:
- Minimal resource usage (only 1 worker task, 1 queue)
- Simple setup - no coordination needed
- Naturally safe (calls to `log::ln()` and `log::evt()` are thread-safe)
- All logs go to the same console output and JSON subscribers

#### Strategy 2: Multiple logger instances

Create separate logger configurations for different modules, each with its own worker task and queue. Typical usage: isolate library logging from application logging.

**Advantages**:
- Module isolation (each module can have different log levels, tags & event types, and operate in its own namespace)
- Independent rate limiting (one module's log spam doesn't block others)
- Can route different modules to different sinks

**Caveats**:
- Higher resource usage: Each logger instance creates a dedicated worker task & log queue (default: 2KB task stack + ~3.5KB queue with default settings, total ~6.5KB RAM per instance)
- Console output concurrency: If multiple logger instances write to the same Console output sink, you **must** add synchronization in your `printLog()` function (e.g. mutex).

**How to implement**:
1. Create a separate logger configuration file & struct for each module with their own custom event types
2. In each configuration file, export the logger instance & log levels in their respective namespace
3. Call `log::ln()` and `log::evt()` inside each module: the alias routes those calls to the correct logger instance
4. In your consumer task, subscribe to both logger instances. Two approaches are recommended:
    - Shared MessageBuffer: all loggers write to the same MessageBuffer
    - Signal Queue: each logger has its own MessageBuffer. You provide a FreeRTOS queue handle to `subscribeJson()` (`uint8_t` element size) which you will use to wait for data from any of the logger instances.

```cpp
// ──────────────────────────────────────────────────────────────
// OPTION A: Shared MessageBuffer
// ──────────────────────────────────────────────────────────────

// Both loggers write to the SAME MessageBuffer (thread-safe despite
// SPSC because internally, EZLog uses a critical section)
void jsonConsumerTask_SharedBuffer() {
    MessageBufferHandle_t sharedBuffer = xMessageBufferCreate(4096);
    char jsonData[1024];

    // Subscribe BOTH loggers to the SAME buffer (thread-safe!)
    net::log::subscribeJson(sharedBuffer);
    sensor::log::subscribeJson(sharedBuffer);

    // Simple blocking wait on a single buffer
    while (true) {
        size_t n = xMessageBufferReceive(sharedBuffer, jsonData, sizeof(jsonData), portMAX_DELAY);
        if (n > 0) {
            // Process JSON (could be from net OR sensor logger)
            processJson(jsonData);
        }
    }
}

// ──────────────────────────────────────────────────────────────
// OPTION B: Separate buffers + Shared signalQueue
// ──────────────────────────────────────────────────────────────

// Each logger has its own MessageBuffer, but they share a single signalQueue
// for efficient blocking wait (no polling needed!)
void jsonConsumerTask_SignalQueue() {
    // Create separate MessageBuffers for each logger
    auto netBuffer = xMessageBufferCreate(4096);
    auto sensorBuffer = xMessageBufferCreate(4096);

    // Create ONE shared signalQueue
    auto sharedSignal = xQueueCreate(10, 1);

    // Subscribe both loggers with the same signalQueue
    net::log::subscribeJson(netBuffer, sharedSignal);
    sensor::log::subscribeJson(sensorBuffer, sharedSignal);

    char jsonData[1024];

    while (true) {
        // Block until ANY logger sends a signal
        uint8_t dummy;
        xQueueReceive(sharedSignal, &dummy, portMAX_DELAY);

        // Non-blocking poll of both buffers to see which one has data
        size_t n = xMessageBufferReceive(netBuffer, jsonData, sizeof(jsonData), 0);
        if (n > 0) {
            processJson(jsonData);  // Process net log
        }

        n = xMessageBufferReceive(sensorBuffer, jsonData, sizeof(jsonData), 0);
        if (n > 0) {
            processJson(jsonData);  // Process sensor log
        }
    }
}
```

#### Key takeaways

1. **The logger is thread-safe**: You can call `log::ln()` and `log::evt()` from any context without additional locking.

2. **Multiple instances = Multiple worker tasks**: Each `ezlog::Logger<Config>` instantiation creates its own worker task and queue. Consider the RAM impact.

3. **Shared console sink requires protection**: If using multiple logger instances that write to the same output (e.g., single UART port), protect the `printLog()` function with a mutex.

4. **For most applications, a single shared logger is the best choice**: It's simpler, more efficient, and easier to maintain.

---

### [Advanced usage] Controlling source location
 
#### Automatic capture (default behavior)
 
By default, the logger automagically captures the source location of the call site. This is possible because it implicitly converts the log level you provide (e.g., `INFO`) into a `ezlog::LevelArg` object which combines :
- The log level itself
- An `ezlog::SrcLoc` struct which is the poor man's version of an `std::source_location` (only available in C++20) using compiler built-ins (`__builtin_FILE()`, etc.) that work on pretty much all embedded platforms (ESP32, STM32, RPi MCUs, etc.).
 
```cpp
// When you write this:
log::ln(INFO, "My message");
 
// The compiler effectively does this behind the scenes:
// SrcLoc::current() captures file, func, line
ezlog::LevelArg arg = {INFO, ezlog::SrcLoc::current()};
log::ln(arg, "My message");
```

Everything happens at compile time, there is zero runtime cost in populating the `ezlog::LevelArg` object with source location.
 
#### Forwarding a source location
 
In some cases, you might want to log an event from a helper function but have the log entry point to the original caller. This is called "forwarding" the source location.
 
To do this, your helper function can accept a `SrcLoc` argument and pass it explicitly to the logger.
 
**Example:**
 
```cpp
// A helper utility that logs errors
// Uses a default parameter as last argument to capture the source location of the call site
void errorCheck(int status, const char* context, const ezlog::SrcLoc& loc = ezlog::SrcLoc::current()) {
    if (status != 0) {
        // Manually create the LevelArg to forward the original location
        log::ln({ERROR, loc}, "Error: %s failed", context);
    }
}
 
// The main application logic
void processData() {
    int status = performOperation();
 
    // The location of THIS call is captured and passed to the helper.
    // The log will show 'processData' at line XX, not the location inside 'errorCheck'.
    // No need to provide the third argument!
    errorCheck(status, "my_operation");
}
```

---

### [Advanced usage] Float formatting helpers

EZLog provides dedicated helpers for efficient float formatting that avoid hidden dynamic allocations commonly used by standard library functions like `snprintf()` with float format specifiers on ESP32.

On ESP32 (and some other embedded platforms), formatting floats with `snprintf()` triggers hidden dynamic memory allocations through `newlib`'s `_svfprintf_r()` implementation. This use case can:
- Increase the required stack size for the logger worker task
- Introduce non-deterministic memory allocation overhead
- Risk heap fragmentation in long-running systems

By using the built-in float formatting helpers, EZLog ensures:
- Zero dynamic allocations during float formatting
- Predictable stack usage: the logger worker task stack can remain at a lean **2048 bytes** in typical scenarios (128B max event size, ~10 event types in variant)
- Consistent performance regardless of whether JSON logging is enabled or not

#### Available helpers

```cpp
namespace ezlog {
    // Extract integer part of a float
    int ftoa_int(float val);

    // Extract fractional part of a float with specified decimals
    // Returns the fractional part as an integer (e.g., 0.123 with 3 decimals returns 123)
    int ftoa_frac(float val, int decimals);
}
```

#### The `FTOA()` macro

For convenience, EZLog provides a macro that expands a float into its integer and fractional components:

```cpp
#define FTOA(val, dec) ezlog::ftoa_int(val), ezlog::ftoa_frac(val, dec)
```

**Example usage in console logging:**

```cpp
float temperature = 23.456f;

// Instead of:
log::ln(INFO, "Temperature: %.2f°C", temperature);  // Uses snprintf internally, causes allocations

// Use:
log::ln(INFO, "Temperature: %d.%02d°C", FTOA(temperature, 2));  // Zero allocations
```

**Output:**
```
Temperature: 23.45°C
```

**Note:** When using structured logging (with `toJson()`), the `JsonBuilder::add()` method automatically handles float formatting efficiently. These helpers are primarily useful for custom `toConsole()` implementations or when you need direct control over float formatting.

---

### [Advanced usage] Runtime log level filtering

Use `setFilterLevel()` to dynamically adjust log verbosity at runtime. Useful for remote debugging via API, shell commands, or configuration updates.

```cpp
// Get current filter level
ezlog::Level currentLevel = log::getFilterLevel();

// Change filter level at runtime
log::setFilterLevel(WARN);  // Only ERROR and WARN will be logged

// Restore verbose logging
log::setFilterLevel(DEBUG);
```

**Note:** Runtime filter is capped by the compile-time level (`LOG_GLOBAL_LEVEL` and `MODULE_LEVEL`). For example, if compiled with `INFO` & you call `log::setFilterLevel(DEBUG)`, you won't get any `DEBUG` logs at runtime.

---

### [Advanced usage] Logging from ISR context

Use `lnFromISR()` and `evtFromISR()` to log from interrupt service routines. 

**Pattern follows FreeRTOS ISR conventions**: Caller must provide `BaseType_t* pxHigherPriorityTaskWoken` and call `portYIELD_FROM_ISR()` once at the end.

```cpp
void IRAM_ATTR gpioISR() {
    BaseType_t hpw = pdFALSE;

    // Multiple log calls accumulate wake-up flag
    log::lnFromISR(ERROR, "GPIO triggered", &hpw);
    log::evtFromISR(WARN, ErrorLog{"ERR_GPIO", "sensor"}, nullptr, &hpw);

    // Single yield at the end (standard FreeRTOS pattern)
    portYIELD_FROM_ISR(hpw);
}
```

**Technical notes:**
- `hpw` is optional (defaults to `nullptr` if omitted)
- If omitted, no context switch will occur (acceptable for non-critical logs)
- Formatted strings **forbidden in ISR context**: `log::lnFromISR(ERROR, "Val: %d", val)` triggers compile error

---

### [Advanced usage] Shortcuts (e.g. `log::err()`)

For frequently used event types, extend `Logger<LogConfig>` with convenience methods to reduce boilerplate.

#### Implementation

```cpp
// In your LogConfig.hpp, after defining your config struct
class ExtLogger : public ezlog::Logger<LogConfig> {
public:
    // Convenience helper for ErrorLog
    // Automatically uses ERROR level and captures source location
    static inline void err(const char* code,
                           const char* msg = nullptr,
                           const char* ctx = nullptr,
                           const ezlog::SrcLoc& loc = ezlog::SrcLoc::current())
    {
        evt({ezlog::ERROR, loc}, ErrorLog{code, ctx}, msg);
    }

    // Add other shortcuts here...
};

// Export the extended logger instead of base logger
using log = ExtLogger;
```

#### Usage

```cpp
// Standard API (always available via inheritance)
log::ln(INFO, "Test");
log::evt(ERROR, ErrorLog{"Timeout", "MQTT"}, "Connection failed");

// Convenience shortcuts
log::err("Timeout");                              // Code only
log::err("Timeout", "Connection failed");         // Code + message
log::err("Timeout", "Connection failed", "MQTT"); // Code + msg + context
```

#### Technical notes

- Extended class inherits all `Logger<LogConfig>` methods via public inheritance
- Source location capture uses default parameter `SrcLoc::current()` for automatic call-site tracking
- Zero overhead compared to direct `evt()` calls (static inline, compiler optimization)
- Pattern works for any event type, not limited to errors

---

### [Advanced usage] Symbol name conflicts

If you use `<cmath>` or other libraries that define a global `log()` function, you may encounter symbol conflicts when importing the logger namespace.

#### Problem

```cpp
#include <cmath>      // Defines ::log(x) (natural logarithm)
#include "LogConfig.hpp"

using namespace logger;

void myFunction() {
    log::ln(INFO, "Test");   // ✅ OK - qualified call
    double x = log(2.0);     // ❌ Compile error - ambiguous symbol 'log'
}
```

#### Solution: Use global call syntax

Call the global `log()` function with the global qualifier `::` to avoid ambiguity.

```cpp
void myFunction() {
    log::ln(INFO, "Test");   // ✅ OK - uses logger::log
    double x = ::log(2.0);   // ✅ OK - uses global ::log() from <cmath>
}
```

---

### [Advanced usage] Tuning message size & buffers

EZLog allows runtime message size debug of both JSON and console outputs, activable via build flags. This helps you tune build flag settings & buffer sizes **without modifying any code**.

#### Automatic size injection

Enable size measurement in your build configuration (e.g., `platformio.ini`):

```ini
build_flags =
    -DLOG_JSON_DEBUG_SIZE      # Adds "_size" field to all JSON outputs
    -DLOG_CONSOLE_DEBUG_SIZE   # Adds <size> suffix to all console lines
```

**No code changes needed!** EZLog automatically injects size information into every log output.

**JSON output example:**
```json
{
  "tag": "my-app",
  "event": "Modbus",
  "level": "INFO",
  "message": "Sensor read",
  "data": { "operation": "read", "reg_name": "TEMP_PV", ... },
  "meta": { "ts_ms": 2420937772, "file": "main.cpp", ... },
  "_size": 289
}
```

**Console output example:**
```
I 2420936.938 my-app Modbus: read TEMP_PV (holding 100) val=25.500 OK [main.cpp:61] <84>
```

The `_size` field and `<size>` suffix show the **actual formatted size** including all formatting, metadata, and null terminator (and obviously excluding themselves). This is the real size that counts against your buffer limits.

---

### [Advanced usage] Arduino Stream adapter

Redirect Arduino library debug output (WiFiManager, ArduinoOTA, HTTPClient, etc.) through EZLog using a `Stream`-compatible adapter. Arduino only (`#ifdef ARDUINO`).

```cpp
#include "LogConfig.hpp"
using namespace logger;

void setup() {
    // Redirect WiFiManager debug output to logger
    WiFiManager wifiManager;
    wifiManager.debugStream(&log::stream(DEBUG));

    // Redirect ArduinoOTA status to logger
    ArduinoOTA.setDebugStream(&log::stream(INFO));
}
```

**Notes:**
- Line-buffered: flushes on newline (`\n`, `\r`) or when buffer is full (`LOG_CONSOLE_MAX_LINE_SIZE - 1` chars)
- Auto-splits long messages (>255 chars) into multiple log lines
- Source location shows adapter code, not original library call site