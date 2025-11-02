/* @file EZLog.hpp - C++ FreeRTOS logger */

#ifndef _EZLOG_HPP_
#define _EZLOG_HPP_

#include <stdint.h>
#include <stddef.h>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cmath>
#include <utility>
#include <variant>
#include <type_traits>

#if (defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM))
    #include "freertos/FreeRTOS.h"
    #include "freertos/portmacro.h"
    #include "freertos/task.h"
    #include "freertos/queue.h"
    #include "freertos/message_buffer.h"
#else
    #include "FreeRTOS.h"
    #include "portmacro.h"
    #include "task.h"
    #include "queue.h"
    #include "message_buffer.h"
#endif

// ============================================================================
// GLOBAL LOGGER SETTINGS
// Use build flags to edit default settings - do not override here!
// ============================================================================

#ifndef LOG_GLOBAL_LEVEL
    #define LOG_GLOBAL_LEVEL 4 // 0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG
#endif
// Asserted after Level declaration below

#ifndef LOG_ENABLE_CONSOLE
    #define LOG_ENABLE_CONSOLE 1 // 1 = enable console output, 0 = disable
#endif
static_assert(LOG_ENABLE_CONSOLE == 0 || LOG_ENABLE_CONSOLE == 1,
    "LOG_ENABLE_CONSOLE must be 0 or 1");

#ifndef LOG_ENABLE_JSON_MSGBUF
    #define LOG_ENABLE_JSON_MSGBUF 0 // 1 = enable structured logs, 0 = disable
#endif
static_assert(LOG_ENABLE_JSON_MSGBUF == 0 || LOG_ENABLE_JSON_MSGBUF == 1,
    "LOG_ENABLE_JSON_MSGBUF must be 0 or 1");

#ifndef LOG_ENABLE_TEXT_MSGBUF
    #define LOG_ENABLE_TEXT_MSGBUF 0 // 1 = enable text log subscribers, 0 = disable
#endif
static_assert(LOG_ENABLE_TEXT_MSGBUF == 0 || LOG_ENABLE_TEXT_MSGBUF == 1,
    "LOG_ENABLE_TEXT_MSGBUF must be 0 or 1");

#ifndef LOG_MAX_EVT_SIZE
    #define LOG_MAX_EVT_SIZE 128 // Max size of any CustomLog struct
#endif
static_assert(LOG_MAX_EVT_SIZE > 0,
    "LOG_MAX_EVT_SIZE must be greater than 0");

#if ((LOG_ENABLE_CONSOLE || LOG_ENABLE_JSON_MSGBUF || LOG_ENABLE_TEXT_MSGBUF) && LOG_GLOBAL_LEVEL)
    #define LOG_ENABLED_GLOBAL 1 // Any output sink is enabled & LOG_GLOBAL_LEVEL>OFF -> logging enabled
#else
    #define LOG_ENABLED_GLOBAL 0 // No output sink is enabled or LOG_GLOBAL_LEVEL=OFF -> logging disabled
#endif


// ============================================================================
// WORKER TASK SETTINGS
// Use build flags to edit default settings - do not override here!
// ============================================================================

#ifndef LOG_EVT_QUEUE_LENGTH
    #define LOG_EVT_QUEUE_LENGTH 20 // Max number of events in event queue
#endif
static_assert(LOG_EVT_QUEUE_LENGTH > 0,
    "LOG_EVT_QUEUE_LENGTH must be greater than 0");

#ifndef LOG_JSON_STR_CAP
    #define LOG_JSON_STR_CAP 1024 // Max JSON string buffer size
#endif
static_assert(LOG_JSON_STR_CAP > 0,
    "LOG_JSON_STR_CAP must be greater than 0");

#ifndef LOG_MAX_JSON_SUBSCRIBERS
    #define LOG_MAX_JSON_SUBSCRIBERS 4 // Max number of JSON subscribers
#endif
static_assert(LOG_MAX_JSON_SUBSCRIBERS > 0,
    "LOG_MAX_JSON_SUBSCRIBERS must be greater than 0");

#ifndef LOG_MAX_TEXT_SUBSCRIBERS
    #define LOG_MAX_TEXT_SUBSCRIBERS 4 // Max number of text subscribers
#endif
static_assert(LOG_MAX_TEXT_SUBSCRIBERS > 0,
    "LOG_MAX_TEXT_SUBSCRIBERS must be greater than 0");

#ifndef LOG_TASK_PRIORITY
  #define LOG_TASK_PRIORITY tskIDLE_PRIORITY+1 // Low priority
#endif
static_assert(LOG_TASK_PRIORITY > 0,
    "LOG_TASK_PRIORITY must be greater than 0");
static_assert(LOG_TASK_PRIORITY <= 255,
    "LOG_TASK_PRIORITY must be lower or equal to 255");

#ifndef LOG_TASK_STACK_SIZE
    #if (defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM))
        #define LOG_TASK_STACK_SIZE 2048 // Stack size for console task (in bytes on ESP32)
    #else
        #define LOG_TASK_STACK_SIZE 2048 / 4 // Stack size for console task (in words on other platforms)
    #endif
#endif
static_assert(LOG_TASK_STACK_SIZE > 0,
    "LOG_TASK_STACK_SIZE must be greater than 0");

#ifndef LOG_TASK_CPU_CORE
  #define LOG_TASK_CPU_CORE -1 // -1 = no affinity, 0/1 = pin on a specific core (ESP32 only)
#endif
static_assert(LOG_TASK_CPU_CORE >= -1 && LOG_TASK_CPU_CORE <= 1,
    "LOG_TASK_CPU_CORE must be -1 (no affinity), 0 or 1 (pinned to core, ESP-IDF)");


// ============================================================================
// CONSOLE SETTINGS
// Use build flags to edit default settings - do not override here!
// ============================================================================

#ifndef LOG_CONSOLE_MAX_LINE_SIZE
  #define LOG_CONSOLE_MAX_LINE_SIZE 256 // Max size of any line in console buffer
#endif
static_assert(LOG_CONSOLE_MAX_LINE_SIZE > 0,
    "LOG_CONSOLE_MAX_LINE_SIZE must be greater than 0");

#ifndef LOG_CONSOLE_TIMEOUT_MS
  #define LOG_CONSOLE_TIMEOUT_MS 1000 // Anti-deadlock safety
#endif
static_assert(LOG_CONSOLE_TIMEOUT_MS > 0,
    "LOG_CONSOLE_TIMEOUT_MS must be greater than 0");
static_assert(LOG_CONSOLE_TIMEOUT_MS <= UINT32_MAX,
    "LOG_CONSOLE_TIMEOUT_MS must be lower or equal to UINT32_MAX");

#ifndef LOG_CONSOLE_SHOW_TIMESTAMP
  #define LOG_CONSOLE_SHOW_TIMESTAMP 1 // 1 = show timestamp, 0 = hide
#endif
static_assert(LOG_CONSOLE_SHOW_TIMESTAMP == 0 || LOG_CONSOLE_SHOW_TIMESTAMP == 1,
    "LOG_CONSOLE_SHOW_TIMESTAMP must be 0 or 1");

#ifndef LOG_CONSOLE_SOURCE_CTX
    #define LOG_CONSOLE_SOURCE_CTX 0b101  // Bitmask: file(0b100)|func(0b010)|line(0b001)
#endif
static_assert(LOG_CONSOLE_SOURCE_CTX >= 0b000 && LOG_CONSOLE_SOURCE_CTX <= 0b111,
    "LOG_CONSOLE_SOURCE_CTX must be between 0b000 and 0b111");


namespace ezlog {

// ============================================================================
// SOURCE LOCATION (Compiler Builtins)
// ============================================================================

/*
 * @brief Lightweight source location capture using GCC/Clang builtins
 *
 * This struct mimics std::source_location (C++20) API for drop-in compatibility,
 * still with zero runtime overhead (compile-time constants).
 */
struct SrcLoc {
    constexpr SrcLoc(
        const char* file = __builtin_FILE(),
        const char* func = __builtin_FUNCTION(),
        uint32_t line = __builtin_LINE()
    ) noexcept : _file(file), _func(func), _line(line) {}

    static constexpr SrcLoc current(
        const char* file = __builtin_FILE(),
        const char* func = __builtin_FUNCTION(),
        uint32_t line = __builtin_LINE()
    ) noexcept {
        return SrcLoc(file, func, line);
    }

    constexpr const char* file_name() const noexcept { return _file; }
    constexpr const char* function_name() const noexcept { return _func; }
    constexpr uint32_t line() const noexcept { return _line; }
    constexpr uint32_t column() const noexcept { return 0; } // Not available with builtins

private:
    const char* _file;
    const char* _func;
    uint32_t _line;
};


// ============================================================================
// LOG LEVELS
// ============================================================================

/*
 * @brief Log levels
 * @note OFF is not a loggable level, don't use it when calling log:: methods
*/
enum class Level : uint8_t { OFF = 0, ERROR = 1, WARN = 2, INFO = 3, DEBUG = 4 };

inline const char* toString(Level l) {
    switch (l) {
        case Level::OFF:   return "OFF";
        case Level::ERROR: return "ERROR";
        case Level::WARN:  return "WARN";
        case Level::INFO:  return "INFO";
        case Level::DEBUG: return "DEBUG";
        default:           return "INVAL";
    }
}

inline const char* toShortString(Level l) {
    switch (l) {
        case Level::OFF:   return "O";
        case Level::ERROR: return "E";
        case Level::WARN:  return "W";
        case Level::INFO:  return "I";
        case Level::DEBUG: return "D";
        default:           return "X";
    }
}

inline constexpr bool isValidLevel(uint8_t l) {
    return (l >= (uint8_t)Level::OFF) && (l <= (uint8_t)Level::DEBUG);
}

inline constexpr Level toLevel(uint8_t l) {
    if (isValidLevel(l)) {
        return static_cast<Level>(l);
    } else {
        return Level::INFO; // Fallback
    }
}

inline constexpr Level OFF   = Level::OFF;
inline constexpr Level ERROR = Level::ERROR;
inline constexpr Level WARN  = Level::WARN;
inline constexpr Level INFO  = Level::INFO;
inline constexpr Level DEBUG = Level::DEBUG;

// Static assert for global level
static_assert(isValidLevel((uint8_t)LOG_GLOBAL_LEVEL), "Invalid LOG_GLOBAL_LEVEL");
inline constexpr Level GLOBAL_LEVEL = toLevel(LOG_GLOBAL_LEVEL);


// ============================================================================
// SOURCE LOCATION AUTOMAGIC CAPTURE
// ============================================================================

/*
 * @brief Level argument with automatic source location capture
 * @note The SrcLoc is captured at construction time (call site)
 */
struct LevelArg {
    Level level;
    SrcLoc loc;

    constexpr LevelArg(Level l, const SrcLoc& s = SrcLoc::current())
        : level(l), loc(s) {}
};


// ============================================================================
// FLOAT FORMATTING HELPERS
// ============================================================================

/*
 * @brief Compile-time power of 10 calculation
 * @param exp Exponent (0-9)
 * @return 10^exp
 * @note Evaluated at compile-time when exp is constant
 */
inline constexpr int pow10(int exp) {
    int result = 1;
    for (int i = 0; i < exp; ++i) result *= 10;
    return result;
}

/*
 * @brief Extract integer part of a float
 * @param val The float value
 * @return Integer part (truncated towards zero)
 * @note Handles negative values correctly
 */
inline int ftoa_int(float val) {
    return (int)val;
}

/*
 * @brief Extract fractional part of a float as integer
 * @param val The float value
 * @param decimals Number of decimal places (e.g., 2 for cents, 3 for milliseconds)
 * @return Fractional part as integer (e.g., 0.14 with decimals=2 → 14)
 * @note Always returns positive value (absolute fractional part)
 * @note Rounds to nearest (0.5 rounds up)
 *
 * Example:
 *   ftoa_frac(3.14159, 2) → 14   (0.14159 * 100 ≈ 14)
 *   ftoa_frac(3.14159, 3) → 142  (0.14159 * 1000 ≈ 142)
 *   ftoa_frac(-2.7, 1) → 7       (0.7 * 10 = 7, always positive)
 */
inline int ftoa_frac(float val, int decimals) {
    // Extract fractional part (always positive)
    float frac = (val >= 0) ? (val - (int)val) : ((int)val - val);

    // Multiply by 10^decimals and round
    return (int)(frac * pow10(decimals) + 0.5f);
}


// ============================================================================
// CHARBUFFER (String builder)
// ============================================================================

/* @brief Lightweight string builder for safe & efficient C-string formatting
 * @note Wraps a raw char buffer with safe append operations and automatic null-termination
 * @note Zero allocations, stack-friendly, perfect for formatting in constrained environments
 */
class CharBuffer {
public:
    CharBuffer() noexcept : _data(nullptr), _len(0), _cap(0) {}

    /* @brief Wrap an existing buffer for writing
     * @param buf The buffer to write to
     * @param capacity The buffer capacity (INCLUDING space for null terminator)
     */
    CharBuffer(char* buf, size_t capacity) noexcept
        : _data(buf), _len(0), _cap(capacity) {
        if (_data && _cap > 0) {
            _data[0] = '\0';
        }
    }

    // READ-ONLY ACCESSORS

    const char* data() const { return _data; }
    const char* c_str() const { return _data ? _data : ""; }
    size_t length() const { return _len; }
    size_t capacity() const { return _cap; }
    size_t remaining() const { return _cap > _len ? _cap - _len - 1 : 0; } // -1 for null terminator
    bool empty() const { return _len == 0; }
    bool full() const { return remaining() == 0; }

    // BASIC OPERATIONS

    void clear() {
        _len = 0;
        if (_data && _cap > 0) {
            _data[0] = '\0';
        }
    }

    /* @brief Append a single character */
    bool push(char c) {
        if (!_data || remaining() == 0) return false;
        _data[_len++] = c;
        _data[_len] = '\0';
        return true;
    }

    /* @brief Append a null-terminated string (truncates if not enough space)
     * @return true if entire string was written, false if truncated
     */
    bool push(const char* str) {
        if (!str || !_data || _cap == 0) return false;
        size_t maxLen = _cap - 1;
        size_t avail = (_len < maxLen) ? (maxLen - _len) : 0;
        size_t strLen = strlen(str);
        size_t n = (strLen < avail) ? strLen : avail;  // min(strLen, avail)

        if (n > 0) {
            memcpy(_data + _len, str, n);
            _len += n;
            _data[_len] = '\0';
        }
        return n == strLen;  // true if entire string written
    }

    /* @brief Append n characters from string (truncates if not enough space)
     * @return true if all n chars were written, false if truncated
     */
    bool push_n(const char* str, size_t n) {
        if (!str || !_data || _cap == 0) return false;
        // Find actual length (stop at null or n, whichever comes first)
        size_t len = 0;
        while (len < n && str[len]) ++len;

        size_t maxLen = _cap - 1;
        size_t avail = (_len < maxLen) ? (maxLen - _len) : 0;
        size_t toCopy = (len < avail) ? len : avail;  // min(len, avail)

        if (toCopy > 0) {
            memcpy(_data + _len, str, toCopy);
            _len += toCopy;
            _data[_len] = '\0';
        }
        return toCopy == len;  // true if all requested chars written
    }

    /* @brief Append an integral value as decimal string (supports int8_t → uint64_t)
     * @tparam T Any integral type except bool and char (char has its own overload)
     * @param value The value to append
     * @return true if fully written, false if truncated
     * @note Automatically handles signed/unsigned and 32/64-bit types
     */
    template<typename T>
    typename std::enable_if_t<
        std::is_integral_v<T>
        && !std::is_same_v<std::decay_t<T>, char>
        && !std::is_same_v<std::decay_t<T>, bool>,
        bool
    >
    push(T value) {
        // Handle sign for signed types
        using UnsignedT = std::make_unsigned_t<T>;
        UnsignedT uval;

        if constexpr (std::is_signed_v<T>) {
            if (value < 0) {
                if (!push('-')) return false;
                uval = static_cast<UnsignedT>(-value);
            } else {
                uval = static_cast<UnsignedT>(value);
            }
        } else {
            uval = value;
        }

        // Convert to digits (max 20 for uint64_t, 10 for uint32_t)
        constexpr int maxDigits = sizeof(T) == 8 ? 20 : 10;
        char digits[maxDigits];
        int ndigits = 0;

        do {
            digits[ndigits++] = '0' + (uval % 10);
            uval /= 10;
        } while (uval > 0);

        // Write in correct order
        for (int i = ndigits - 1; i >= 0; --i) {
            if (!push(digits[i])) return false;
        }
        return true;
    }

    /* @brief Append a float as decimal string with fixed precision
     * @param value The float value to append
     * @param decimals Number of decimal places (1-7)
     * @return true if fully written, false if truncated
     * @note Non-finite values (NaN, Inf) are NOT handled (caller must check)
     * @note Use push(float) overload for automatic trailing zero removal
     */
    bool push(float value, int decimals) {
        // Extract integer and fractional parts
        int intPart = ftoa_int(value);
        int fracPart = ftoa_frac(value, decimals);

        // Push integer part
        if (!push(intPart)) return false;
        if (!push('.')) return false;

        // Pad fractional part with leading zeros
        int divisor = pow10(decimals - 1);
        while (divisor > 1 && fracPart < divisor) {
            if (!push('0')) return false;
            divisor /= 10;
        }

        return push(static_cast<uint32_t>(fracPart));
    }

    /* @brief Append a float as decimal string with automatic trailing zero removal
     * @param value The float value to append
     * @return true if fully written, false if truncated
     * @note Non-finite values (NaN, Inf) are NOT handled (caller must check)
     * @note Uses 7 decimal places (float precision), strips trailing zeros, keeps at least .0
     * @note Example: 3.14159 → "3.14159", 2.5 → "2.5", 42.0 → "42.0"
     */
    bool push(float value) {
        int intPart = ftoa_int(value);
        int fracPart = ftoa_frac(value, 7);

        if (!push(intPart)) return false;
        if (!push('.')) return false;

        // Strip trailing zeros NUMERICALLY (not by manipulating buffer)
        int effectiveDecimals = 7;
        while (effectiveDecimals > 1 && (fracPart % 10) == 0) {
            fracPart /= 10;
            effectiveDecimals--;
        }

        // Pad fractional part with leading zeros
        int padDivisor = pow10(effectiveDecimals - 1);
        while (padDivisor > 1 && fracPart < padDivisor) {
            if (!push('0')) return false;
            padDivisor /= 10;
        }

        return push(static_cast<uint32_t>(fracPart));
    }

    /* @brief Append uint with zero padding (e.g., push_uint_pad(5, 3) -> "005") */
    bool push_uint_pad(uint32_t value, int width) {
        char digits[10];
        int ndigits = 0;
        do {
            digits[ndigits++] = '0' + (value % 10);
            value /= 10;
        } while (value > 0);

        // Calculate total chars needed (max of width or ndigits)
        int total = (width > ndigits) ? width : ndigits;
        int zeros = total - ndigits;

        // Check space once
        if (remaining() < (size_t)total) return false;

        // Write leading zeros (single check, no branching per char)
        for (int i = 0; i < zeros; ++i) {
            _data[_len++] = '0';
        }

        // Write digits in correct order
        for (int i = ndigits - 1; i >= 0; --i) {
            _data[_len++] = digits[i];
        }

        _data[_len] = '\0';
        return true;
    }

    /* @brief Manually advance the buffer position (for external writes)
     * @param n Number of characters that were written directly to the buffer
     * @note Use this when external code writes directly to buffer
     */
    void advance(size_t n) {
        if (!_data) return;
        size_t maxLen = (_cap > 0) ? _cap - 1 : 0;
        if (_len > maxLen) _len = maxLen;  // Safety clamp
        if (n > maxLen - _len) n = maxLen - _len;  // Clamp advance
        _len += n;
        if (_cap > 0) _data[_len] = '\0';
    }

private:
    char*  _data;
    size_t _len;
    size_t _cap;
}; // class CharBuffer


// ============================================================================
// JSON BUILDER (lean serialization)
// ============================================================================

/*
 * @brief Lightweight JSON object builder with automatic field separation
 * @note RAII-style: constructor opens '{', destructor closes '}'
 * @note Automatically handles comma separation between fields
 * @note Zero allocations, writes directly to CharBuffer
 *
 * Usage:
 *   char buf[JSON_MAX_SIZE];
 *   CharBuffer cb(buf, sizeof(buf));
 *   {
 *       JsonBuilder jb(cb);
 *       jb.add("name", "John");
 *       jb.add("age", 30);
 *       jb.add("temp", 23.5f);
 *   } // Automatically closes '}'
 */
class JsonBuilder {
public:
    JsonBuilder(CharBuffer& cb) noexcept
        : _cb(cb), _firstField(true) {
        _cb.push('{');
    }

    ~JsonBuilder() {
        _cb.push('}');
    }

    // Delete copy/move to prevent misuse
    JsonBuilder(const JsonBuilder&) = delete;
    JsonBuilder& operator=(const JsonBuilder&) = delete;

    /* @brief Add a string field */
    void add(const char* key, const char* value) {
        handleSeparator();
        pushJsonString(key);
        _cb.push(':');
        pushJsonString(value);
    }

    /* @brief Add a boolean field */
    void add(const char* key, bool value) {
        handleSeparator();
        pushJsonString(key);
        _cb.push(':');
        _cb.push(value ? "true" : "false");
    }

    /* @brief Add an integer field (any integral type: int8_t → uint64_t) */
    template<typename T>
    void add(const char* key, T value) {
        static_assert(std::is_integral_v<T> && !std::is_same_v<T, bool>,
                      "JsonBuilder::add() only accepts integral types, float, bool & const char*");

        handleSeparator();
        pushJsonString(key);
        _cb.push(':');
        _cb.push(value);  // Uses CharBuffer template overload
    }

    /* @brief Add a float field (automatic trailing zero removal) */
    void add(const char* key, float value) {
        handleSeparator();
        pushJsonString(key);
        _cb.push(':');
        pushJsonFloat(value);
    }

    /* @brief Add a nested object via lambda/functor
     * @param key The field name
     * @param func Callable that receives a JsonBuilder& and populates the nested object
     * @note Uses RAII - nested object automatically closes on scope exit
     *
     * Example:
     *   JsonBuilder root(cb);
     *   root.addObject("meta", [&](JsonBuilder& meta) {
     *       meta.add("ts", timestamp);
     *       meta.add("file", filename);
     *   });
     */
    template<typename Func>
    void addObject(const char* key, Func&& func) {
        handleSeparator();
        pushJsonString(key);
        _cb.push(':');
        JsonBuilder nested(_cb);
        func(nested);
        // RAII: nested destructor closes '}' automatically
    }

    /* @brief Get current JSON size in bytes (useful for tuning LOG_JSON_STR_CAP) */
    size_t size() const {
        return _cb.length();
    }

    /* @brief Get underlying CharBuffer (for special cases like size injection) */
    CharBuffer& buffer() {
        return _cb;
    }

private:
    /*
     * @brief Push a JSON-escaped string to internal CharBuffer
     * @param str String to escape and push (null → "null")
     * @note Escapes ", \, \n, \r, \t characters according to JSON spec
     */
    void pushJsonString(const char* str) {
        if (!str) {
            _cb.push("null");
            return;
        }
        _cb.push('"');
        for (const char* p = str; *p; ++p) {
            switch (*p) {
                case '"':  _cb.push("\\\""); break;
                case '\\': _cb.push("\\\\"); break;
                case '\n': _cb.push("\\n");  break;
                case '\r': _cb.push("\\r");  break;
                case '\t': _cb.push("\\t");  break;
                default:   _cb.push(*p);     break;
            }
        }
        _cb.push('"');
    }

    /*
     * @brief Push a float as JSON value to internal CharBuffer
     * @param value Float value to format
     * @note Non-finite values (NaN, Inf) are serialized as JSON null
     * @note Automatically strips trailing zeros, keeps at least .0
     */
    void pushJsonFloat(float value) {
        if (!std::isfinite(value)) {
            _cb.push("null");
            return;
        }
        _cb.push(value);  // Uses intelligent overload with auto trailing zeros removal
    }

    void handleSeparator() {
        if (!_firstField) {
            _cb.push(',');
        }
        _firstField = false;
    }

    CharBuffer& _cb;
    bool _firstField;
};


// ============================================================================
// NATIVE TEXT EVENT TYPE (REQUIRED)
// ============================================================================

/*
 * @brief Text event
 * @note NATIVE EVENT - ALWAYS REQUIRED IN ALL CONFIGURATIONS!
 * @note The `formatted` field is only used for formatted logs that aren't static strings:
 * @note - From producer side, _emitEvent will parse formatted strings into `formatted`
 * @note - From consumer side, the worker will detect `formatted` and use it as the message if not empty
 */
struct TextLog {
    char formatted[LOG_MAX_EVT_SIZE] = {'\0'}; // Only used for formatted logs

    static constexpr const char* NAME = "Text";

    // No-op methods (text is managed in the global "message" field)
    size_t toConsole(char* out, size_t cap) const { return 0; }

    void toJson(JsonBuilder& jb) const {
        // No structured data for TextLog (text is in "message" field)
    }
};


// ============================================================================
// STRUCTURED SUBSCRIBERS
// ============================================================================

/*
 * @brief MessageBuffer subscriber (receives JSON or text via MessageBuffer)
 */
struct MsgBufSubscriber {
    MessageBufferHandle_t msgBuffer;
    QueueHandle_t signalQueue; // Optional signal queue
    volatile uint32_t dropped; // Dropped messages counter for this subscriber
};


// ============================================================================
// GENERIC LOGGER CLASS (TEMPLATE)
// ============================================================================

/*
 * @brief Generic Logger Template Class
 *
 * @tparam T_Config Configuration policy class that must provide:
 *   - static constexpr const char* TAG : Module tag for logging
 *   - static constexpr uint8_t MODULE_LEVEL : Module-specific log level (0-4)
 *   - static constexpr Level MAX_BUILD_LEVEL : Final computed max level
 *   - using EventVariant : std::variant containing TextLog + custom event types
 *   - static uint64_t getTimeMs() : Function to get current time in milliseconds
 *
 * @note This class is 100% generic and should NEVER be modified by users.
 * @note Users configure the logger by defining their own T_Config policy class.
 * @note Multiple Logger instances can coexist (e.g., Logger<HvacConfig>, Logger<IoConfig>)
 */
template<typename T_Config>
class Logger {
private:
    // ========================================================================
    // CONFIGURATION VALIDATION (C++17 compatible)
    // ========================================================================

    // Helper traits to check T_Config members at compile-time (C++17 SFINAE)
    template<typename T, typename = void>
    struct has_TAG : std::false_type {};
    template<typename T>
    struct has_TAG<T, std::void_t<decltype(T::TAG)>> : std::true_type {};

    template<typename T, typename = void>
    struct has_MODULE_LEVEL : std::false_type {};
    template<typename T>
    struct has_MODULE_LEVEL<T, std::void_t<decltype(T::MODULE_LEVEL)>> : std::true_type {};

    template<typename T, typename = void>
    struct has_EventVariant : std::false_type {};
    template<typename T>
    struct has_EventVariant<T, std::void_t<typename T::EventVariant>> : std::true_type {};

    template<typename T, typename = void>
    struct has_getTimeMs : std::false_type {};
    template<typename T>
    struct has_getTimeMs<T, std::void_t<decltype(T::getTimeMs())>> : std::true_type {};

#if (LOG_ENABLE_CONSOLE)
    template<typename T, typename = void>
    struct has_printLog : std::false_type {};
    template<typename T>
    struct has_printLog<T, std::void_t<decltype(T::printLog(std::declval<const char*>(), std::declval<size_t>()))>> : std::true_type {};
#endif

    // Validate that T_Config provides the required members
    static_assert(has_TAG<T_Config>::value,
        "LogConfig must define: static constexpr const char* TAG");
    static_assert(has_MODULE_LEVEL<T_Config>::value,
        "LogConfig must define: static constexpr Level MODULE_LEVEL");
    static_assert(has_EventVariant<T_Config>::value,
        "LogConfig must define: using EventVariant = std::variant<TextLog, ...>");
    static_assert(has_getTimeMs<T_Config>::value,
        "LogConfig must define: static uint64_t getTimeMs()");
#if (LOG_ENABLE_CONSOLE)
    static_assert(has_printLog<T_Config>::value,
        "LogConfig must define: static int printLog(const char* data, size_t len)");
#endif

    // Validate configuration values
    static_assert(static_cast<uint8_t>(T_Config::MODULE_LEVEL) >= 0 &&
                  static_cast<uint8_t>(T_Config::MODULE_LEVEL) <= 4,
        "LogConfig::MODULE_LEVEL must be a valid log level (0-4)");


    // ========================================================================
    // COMPILE-TIME CONFIGURATION (computed by template)
    // ========================================================================

    // Compute final MAX_BUILD_LEVEL for this module
    static constexpr Level MAX_BUILD_LEVEL = LOG_ENABLED_GLOBAL ?
        // If global logging enabled, compute the min of MODULE_LEVEL and GLOBAL_LEVEL
        static_cast<Level>(std::min(static_cast<uint8_t>(T_Config::MODULE_LEVEL),
                                     static_cast<uint8_t>(LOG_GLOBAL_LEVEL)))
        // If global logging disabled, set to OFF
        : OFF;

    static constexpr bool LOG_ENABLED_LOCAL = MAX_BUILD_LEVEL > OFF;


    // ========================================================================
    // INTERNAL TYPES
    // ========================================================================

    // Import EventVariant from configuration
    using EventVariant = typename T_Config::EventVariant;

    // Validate that EventVariant contains TextLog
    static_assert(std::variant_size_v<EventVariant> > 0,
        "EventVariant must contain at least TextLog");

    /*
     * @brief Unified record structure for all event types
     * @note This structure is used for all logging (text + structured events)
     * @note The 'data' variant holds the event payload (TextLog, ErrorLog, ModbusLog, etc.)
     */
    struct Record {
        const char* name;           // Event name (Text, Error, Modbus, etc.)
        const char* tag;            // Module/component tag
        Level       level;          // Log level
        uint64_t    timestampMs;    // Timestamp in milliseconds
        SrcLoc      loc;            // Source location (file, line, func)
        const char* message;        // Event message
        EventVariant data;          // Event payload (variant)
    };


    // ========================================================================
    // STATIC MEMBERS (one set per Logger<T_Config> instantiation)
    // ========================================================================

    // Runtime level
    static inline Level _runtimeLevel = MAX_BUILD_LEVEL;

    // Log queue consumed by worker task
    static inline QueueHandle_t _logEvtQueue = nullptr;
    static inline StaticQueue_t _logEvtQueueStorage;
    static inline uint8_t _logEvtQueueBuffer[LOG_EVT_QUEUE_LENGTH * sizeof(Record)];

    // JSON & text subscribers
    static inline MsgBufSubscriber _jsonSubscribers[LOG_MAX_JSON_SUBSCRIBERS];
    static inline size_t _jsonSubscriberCount = 0;
    static inline MsgBufSubscriber _textSubscribers[LOG_MAX_TEXT_SUBSCRIBERS];
    static inline size_t _textSubscriberCount = 0;

    // Worker task
    static inline TaskHandle_t _logWorkerHandle = nullptr;
    static inline StaticTask_t _logWorkerTaskBuffer;
    static inline StackType_t _logWorkerStack[LOG_TASK_STACK_SIZE];

    // Dropped messages counters
    static inline volatile uint32_t _droppedConsole{0};
    static inline volatile uint32_t _droppedJsonTotal{0};
    static inline volatile uint32_t _droppedTextTotal{0};


    // ========================================================================
    // PRIVATE METHODS
    // ========================================================================

    /*
     * @brief Runtime filter checker
     * @param lvl The level to check
     * @return True if this level is allowed, false otherwise
     */
    static bool _runtimeAllows(Level lvl) {
        if (lvl == Level::OFF) return false; // OFF is not a loggable level
        if (_runtimeLevel == Level::OFF) return false; // Module disabled
        return static_cast<uint8_t>(lvl) <= static_cast<uint8_t>(_runtimeLevel);
    }

    /*
     * @brief Push JSON to all structured subscribers
     * @param json The JSON string to push
     * @note Thread-safe: uses critical section to allow multiple logger instances to share the same MessageBuffer
     */
    static void _pushJsonToSubscribers(const char* json) {
        if (!json || _jsonSubscriberCount == 0) return;
        const size_t n = strlen(json) + 1;

        static portMUX_TYPE jsonMux = portMUX_INITIALIZER_UNLOCKED;

        for (size_t i = 0; i < _jsonSubscriberCount; ++i) {
            auto& s = _jsonSubscribers[i];

            // Critical section protects xMessageBufferSend for MP-safety,
            // in case the user provides a single MessageBuffer shared between
            // multiple Logger instances
            portENTER_CRITICAL(&jsonMux);
            size_t sent = xMessageBufferSend(s.msgBuffer, json, n, 0);
            portEXIT_CRITICAL(&jsonMux);

            if (sent != n) {
                s.dropped = s.dropped + 1;
                _droppedJsonTotal = _droppedJsonTotal + 1;
            }
            else if (s.signalQueue) {
                uint8_t one = 1;
                xQueueSend(s.signalQueue, &one, 0);  // Queue is already MP-safe
            }
        }
    }

    /*
     * @brief Push formatted text to all text subscribers
     * @param text The formatted console text to push (includes \r\n)
     * @note Thread-safe: uses critical section to allow multiple logger instances to share the same MessageBuffer
     */
    static void _pushTextToSubscribers(const char* text) {
        if (!text || _textSubscriberCount == 0) return;
        const size_t n = strlen(text) + 1;

        static portMUX_TYPE textMux = portMUX_INITIALIZER_UNLOCKED;

        for (size_t i = 0; i < _textSubscriberCount; ++i) {
            auto& s = _textSubscribers[i];

            // Critical section protects xMessageBufferSend for MP-safety,
            // in case the user provides a single MessageBuffer shared between
            // multiple Logger instances
            portENTER_CRITICAL(&textMux);
            size_t sent = xMessageBufferSend(s.msgBuffer, text, n, 0);
            portEXIT_CRITICAL(&textMux);

            if (sent != n) {
                s.dropped = s.dropped + 1;
                _droppedTextTotal = _droppedTextTotal + 1;
            }
            else if (s.signalQueue) {
                uint8_t one = 1;
                xQueueSend(s.signalQueue, &one, 0);  // Queue is already MP-safe
            }
        }
    }

    /*
     * @brief Print a line to console with retry and timeout
     * @param line Line to print (already includes \r\n)
     * @param timeoutMs Timeout in milliseconds
     */
    static void _consolePrintLine(const char* line, uint32_t timeoutMs = LOG_CONSOLE_TIMEOUT_MS) {
        if (!line) return;

        const char* p = line;
        size_t left = strlen(line);
        uint64_t t0 = T_Config::getTimeMs();
        bool fullySent = true;

        while (left) {
            int r = T_Config::printLog(p, left);
            if (r < 0) break; // Drop
            if (r == 0) {
                if (T_Config::getTimeMs() - t0 > timeoutMs) {
                    fullySent = false;
                    break; // Timeout -> drop
                }
                vTaskDelay(1);
                continue;
            }
            p += (size_t)r;
            left -= (size_t)r;
            t0 = T_Config::getTimeMs();
        }

        if (!fullySent) {
            _droppedConsole = _droppedConsole + 1;
        }
    }

    /*
     * @brief LogWorker task - processes ALL events from global queue
     * @param param Unused task parameter
     * @note This task handles console output, text subscribers, and JSON structured subscribers
     * @note Uses duck typing - all events have toConsole() and toJson() methods
     */
    static void _logWorkerTask(void* param) {
        (void)param;

        // Static buffers (single writer, no need for stack allocation)
        static Record rec;
        static char line[LOG_CONSOLE_MAX_LINE_SIZE];
    #if (LOG_CONSOLE_SHOW_TIMESTAMP)
        static char ts[16];
    #endif
    #if (LOG_CONSOLE_SOURCE_CTX)
        static char ctxStr[128];
    #endif
    #if (LOG_ENABLE_JSON_MSGBUF)
        static char jsonBuf[LOG_JSON_STR_CAP];
    #endif

        while (true) {
            // Wait for event from queue (blocking)
            if (xQueueReceive(_logEvtQueue, &rec, portMAX_DELAY) != pdTRUE) {
                continue;
            }

            // Format timestamp and context (common for all events)
        #if (LOG_CONSOLE_SHOW_TIMESTAMP)
            formatTimestampMs(rec.timestampMs, ts, sizeof(ts));
        #endif
        #if (LOG_CONSOLE_SOURCE_CTX)
            formatSrcLoc(ctxStr, sizeof(ctxStr), rec.loc);
        #endif

            // Generic visitor for processing events
            static auto visitor = [&](const auto& eventType) {
                using EventType = std::decay_t<decltype(eventType)>;

        #if (LOG_ENABLE_CONSOLE || LOG_ENABLE_TEXT_MSGBUF)
                // Build complete line with prefix + body + newline
                CharBuffer cb(line, sizeof(line));

                // Add prefix
                cb.push('[');
                cb.push(toShortString(rec.level));
                #if (LOG_CONSOLE_SHOW_TIMESTAMP)
                    cb.push('|');
                    cb.push(ts);
                #endif
                #if (LOG_CONSOLE_SOURCE_CTX)
                    cb.push('|');
                    cb.push(ctxStr);
                #endif
                if (rec.tag && *rec.tag) {
                    cb.push('|');
                    cb.push(rec.tag);
                }
                cb.push("] ");

                // Add body (event content)
                if constexpr (std::is_same_v<EventType, TextLog>) {
                    // TextLog: use rec.message if set, otherwise formatted buffer
                    if (rec.message && *rec.message) {
                        cb.push(rec.message);
                    } else if (eventType.formatted[0]) {
                        cb.push(eventType.formatted);
                    }
                } else {
                    // Structured events: toConsole() formats data
                    size_t pos = cb.length();
                    if (pos < sizeof(line)) {
                        size_t written = eventType.toConsole(line + pos, sizeof(line) - pos);
                        cb.advance(written);  // Sync CharBuffer state after external write
                    }
                    // Add " - message" after event data if set
                    if (rec.message && *rec.message) {
                        cb.push(" - ");
                        cb.push(rec.message);
                    }
                }

            #ifdef LOG_CONSOLE_DEBUG_SIZE
                // Inject size indicator showing line size BEFORE this debug suffix
                // (actual line size, not including <size> itself)
                size_t lineSizeBeforeDebug = cb.length() + 2 + 1;  // +\r +\n +\0
                cb.push(" <");
                cb.push((uint32_t)lineSizeBeforeDebug);
                cb.push('>');
            #endif

                // Add newline at the end
                cb.push('\r');
                cb.push('\n');

                // Console direct print via hook
            #if (LOG_ENABLE_CONSOLE)
                _consolePrintLine(line);
            #endif

                // Push to text subscribers (if any)
            #if (LOG_ENABLE_TEXT_MSGBUF)
                if (_textSubscriberCount > 0) {
                    _pushTextToSubscribers(line);
                }
            #endif

        #endif // (LOG_ENABLE_CONSOLE || LOG_ENABLE_TEXT_MSGBUF)

            #if (LOG_ENABLE_JSON_MSGBUF)
                // Structured: serialize to JSON using JsonBuilder
                if (_jsonSubscriberCount > 0) {
                    CharBuffer jsonCb(jsonBuf, sizeof(jsonBuf));

                    // Unified JSON construction via JsonBuilder
                    {
                        JsonBuilder root(jsonCb);

                        // Top-level fields
                        root.add("tag", rec.tag ? rec.tag : "");
                        root.add("event", rec.name ? rec.name : "");
                        root.add("level", toString(rec.level));

                        // Message field (TextLog uses formatted buffer if message is null)
                        if constexpr (std::is_same_v<EventType, TextLog>) {
                            if (rec.message && *rec.message) {
                                root.add("message", rec.message);
                            } else if (eventType.formatted[0]) {
                                root.add("message", eventType.formatted);
                            } else {
                                root.add("message", "");
                            }
                        } else {
                            root.add("message", rec.message ? rec.message : "");
                        }

                        // Structured data
                        root.addObject("data", [&](JsonBuilder& data) {
                            eventType.toJson(data);
                        });

                        // Metadata
                        root.addObject("meta", [&](JsonBuilder& meta) {
                            meta.add("ts_ms", rec.timestampMs);  // Full 64-bit timestamp
                            meta.add("file", basename(rec.loc.file_name()));
                            meta.add("func", rec.loc.function_name());
                            meta.add("line", rec.loc.line());
                        });

                    #ifdef LOG_JSON_DEBUG_SIZE
                        // Inject _size field showing size BEFORE this debug field
                        // (actual event data size, not including _size itself)
                        size_t sizeBeforeDebug = root.buffer().length() + 1 + 1;  // +} +\0
                        root.add("_size", static_cast<uint32_t>(sizeBeforeDebug));
                    #endif
                    } // RAII: root destructor closes '}'

                    // Push to subscribers
                    if (jsonCb.length() > 0) {
                    #ifdef LOG_JSON_DEBUG_PRINT
                        // Debug mode: print JSON directly to console
                        Serial.print("  JSON: ");
                        Serial.println(jsonBuf);
                    #endif
                        if (_jsonSubscriberCount > 0) {
                            _pushJsonToSubscribers(jsonBuf);
                        }
                    } 
                } // if (_jsonSubscriberCount > 0)
            #endif // LOG_ENABLE_JSON_MSGBUF
            }; // visitor

            // Single dispatch point - replaces all switch statements
            std::visit(visitor, rec.data);

        } // while (true)
    } // _logWorkerTask()

    /*
     * @brief Start the LogWorker task
     * @return True if started successfully, false otherwise
     */
    static bool _startLogWorker() {
        if (_logWorkerHandle) return true; // Already started

    #if (LOG_TASK_CPU_CORE >= 0)
        _logWorkerHandle = xTaskCreateStaticPinnedToCore(
            _logWorkerTask,
            "LogWorker",
            (uint32_t)LOG_TASK_STACK_SIZE,
            nullptr,
            (UBaseType_t)LOG_TASK_PRIORITY,
            _logWorkerStack,
            &_logWorkerTaskBuffer,
            LOG_TASK_CPU_CORE
        );
    #else
        _logWorkerHandle = xTaskCreateStatic(
            _logWorkerTask,
            "LogWorker",
            (uint32_t)LOG_TASK_STACK_SIZE,
            nullptr,
            (UBaseType_t)LOG_TASK_PRIORITY,
            _logWorkerStack,
            &_logWorkerTaskBuffer
        );
    #endif

        return _logWorkerHandle != nullptr;
    }

    /*
     * @brief Initialize the global event queue (lazy init)
     * @return True if initialized successfully, false otherwise
     * @note Automatically starts the LogWorker task on first initialization
     */
    static bool _initEventQueue() {
        if (_logEvtQueue) return true; // Already initialized
        static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
        portENTER_CRITICAL(&mux);
        if (!_logEvtQueue) {
            _logEvtQueue = xQueueCreateStatic(
                LOG_EVT_QUEUE_LENGTH,
                sizeof(Record),
                _logEvtQueueBuffer,
                &_logEvtQueueStorage
            );
            if (_logEvtQueue) _startLogWorker();
        }
        portEXIT_CRITICAL(&mux);
        return _logEvtQueue != nullptr;
    }

    /*
     * @brief Generic event emitter - SINGLE ENTRY POINT for ALL logging
     * @tparam T Event type (TextLog, ErrorLog, ModbusLog, etc.)
     * @param evt The event to emit
     * @param tag The module/component tag
     * @param level The log level
     * @param loc The source location
     * @note This is the ONLY place where events enter the logging system
     */
    template<typename T>
    __attribute__((noinline))
    static void _emitEvent(const T& evt, const char* tag, Level level, const SrcLoc& loc, const char* msg) {
        // Safety check: ensure event type is trivially copyable for FreeRTOS queue
        static_assert(std::is_trivially_copyable_v<T>,
                      "Event types must be trivially copyable for safe FreeRTOS queue operations. "
                      "Avoid std::string, std::vector, or virtual functions. Use POD types and const char*.");

        // Lazy init event queue on first use
        if (!_initEventQueue()) return;

        // Create Record with event payload
        Record rec{};
        rec.name        = T::NAME;
        rec.tag         = tag;
        rec.level       = level;
        rec.timestampMs = T_Config::getTimeMs();
        rec.loc         = loc;
        rec.message     = msg;
        rec.data        = evt;  // Direct variant assignment

        // Queue for worker (non-blocking)
        xQueueSend(_logEvtQueue, &rec, 0);
    }

    /*
     * @brief Generic event emitter for ISR context
     * @tparam T Event type (TextLog, ErrorLog, ModbusLog, etc.)
     * @param hpw Optional FreeRTOS "higher priority task woken" flag for context switching
     * @note ISR-safe: uses xQueueSendFromISR
     * @note User must call portYIELD_FROM_ISR() after all ISR logging calls
     * @note NOT marked noinline to allow compiler optimization
     */
    template<typename T>
    static void _emitEventFromISR(const T& evt, const char* tag, Level level, const SrcLoc& loc, const char* msg,
                                   BaseType_t* hpw) {
        if (!_logEvtQueue) return;

        Record rec{};
        rec.name        = T::NAME;
        rec.tag         = tag;
        rec.level       = level;
        rec.timestampMs = T_Config::getTimeMs();
        rec.loc         = loc;
        rec.message     = msg;
        rec.data        = evt;

        xQueueSendFromISR(_logEvtQueue, &rec, hpw);
    }

    // ============================================================================
    // UTILITY FUNCTIONS
    // ============================================================================
    
    /*
     * @brief Get the basename of a path
     * @param path The path to get the basename of
     * @return The basename of the path (stripped of the directory path)
     * @note Implements Windows (backslash) & MacOS/Linux (slash) path separators
     */
    static inline const char* basename(const char* path) {
        if (!path) return "";
        const char* base = path;
        if (const char* s = strrchr(path, '/'))  base = s + 1;
        if (const char* s = strrchr(path, '\\'); s && s > base) base = s + 1;
        return base;
    }
    
    /*
     * @brief Format a timestamp in milliseconds
     * @param ms The timestamp in milliseconds
     * @param buf The buffer to format the timestamp into
     * @param size The size of the buffer
     * @return The number of characters written to the buffer
     * @note Uses CharBuffer for safe string building without snprintf's huge stack usage
     */
    static inline size_t formatTimestampMs(uint64_t ms, char* buf, size_t size) {
        CharBuffer cb(buf, size);

        uint32_t sec   = static_cast<uint32_t>(ms / 1000ULL);
        uint32_t milli = static_cast<uint32_t>(ms % 1000ULL);

        cb.push(sec);
        cb.push('.');
        cb.push_uint_pad(milli, 3);  // Always 3 digits with leading zeros

        return cb.length();
    }
    
    /*
     * @brief Format the source context based on LOG_CONSOLE_SOURCE_CTX flags
     * @param buf The buffer to format the source context into
     * @param size The size of the buffer
     * @param loc The source location
     * @note Uses CharBuffer for clean, safe string building
     */
    static inline void formatSrcLoc(char* buf, size_t size, const SrcLoc& loc) {
        CharBuffer cb(buf, size);
    
    #if (LOG_CONSOLE_SOURCE_CTX > 0)
    
        #if (LOG_CONSOLE_SOURCE_CTX & 0b100)  // File
            cb.push(basename(loc.file_name()));
        #endif

        #if ((LOG_CONSOLE_SOURCE_CTX & 0b100) && (LOG_CONSOLE_SOURCE_CTX & 0b010))  // File && Function
            cb.push("::");
        #endif

        #if (LOG_CONSOLE_SOURCE_CTX & 0b010)  // Function
            cb.push(loc.function_name());
        #endif

        #if (LOG_CONSOLE_SOURCE_CTX & 0b001)  // Line
            cb.push(':');
            cb.push(loc.line());
        #endif

    #endif
    }

public:
    // ========================================================================
    // PUBLIC API - RUNTIME FILTER CONTROL
    // ========================================================================

    /*
     * @brief Set the runtime filter level for this module
     * @param lvl The level to set
     * @note The filter is capped to MAX_BUILD_LEVEL (compile-time ceiling)
     */
    static void setFilterLevel(Level lvl) {
        if (static_cast<uint8_t>(lvl) > (uint8_t)MAX_BUILD_LEVEL) {
            _runtimeLevel = MAX_BUILD_LEVEL;
        } else {
            _runtimeLevel = lvl;
        }
    }

    /*
     * @brief Get the current runtime filter level for this module
     * @return The current runtime level
     */
    static Level getFilterLevel() {
        return _runtimeLevel;
    }


    // ========================================================================
    // PUBLIC API - STRUCTURED LOGGING SUBSCRIPTION
    // ========================================================================

    /*
     * @brief Subscribe to structured logs (JSON)
     * @param buf The message buffer to send JSON to
     * @param sigQ Optional signal queue to notify subscriber
     * @return True if the subscription is successful, false otherwise
     * @note Maximum of LOG_MAX_JSON_SUBSCRIBERS subscribers allowed
     */
    static bool subscribeJson(MessageBufferHandle_t buf, QueueHandle_t sigQ = nullptr) {
        if (!buf) return false;

        // Check if already subscribed
        for (size_t i = 0; i < _jsonSubscriberCount; ++i) {
            if (_jsonSubscribers[i].msgBuffer == buf) return true;
        }

        // Check capacity
        if (_jsonSubscriberCount >= LOG_MAX_JSON_SUBSCRIBERS) return false;

        // Add new subscriber
        portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
        portENTER_CRITICAL(&mux);
        _jsonSubscribers[_jsonSubscriberCount++] = {buf, sigQ, 0};
        portEXIT_CRITICAL(&mux);
        return true;
    }

    /*
     * @brief Subscribe to text logs (formatted console output)
     * @param buf The message buffer to send text to
     * @param sigQ Optional signal queue to notify subscriber
     * @return True if the subscription is successful, false otherwise
     * @note Maximum of LOG_MAX_TEXT_SUBSCRIBERS subscribers allowed
     */
    static bool subscribeText(MessageBufferHandle_t buf, QueueHandle_t sigQ = nullptr) {
        if (!buf) return false;

        // Check if already subscribed
        for (size_t i = 0; i < _textSubscriberCount; ++i) {
            if (_textSubscribers[i].msgBuffer == buf) return true;
        }

        // Check capacity
        if (_textSubscriberCount >= LOG_MAX_TEXT_SUBSCRIBERS) return false;

        // Add new subscriber
        portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
        portENTER_CRITICAL(&mux);
        _textSubscribers[_textSubscriberCount++] = {buf, sigQ, 0};
        portEXIT_CRITICAL(&mux);
        return true;
    }


    // ========================================================================
    // PUBLIC API - LOGGING METHODS
    // ========================================================================

#if (LOG_ENABLED_GLOBAL)

    /*
    * @brief Log a text line (unified API for simple and formatted logs)
    * @param arg The log level and source location
    * @param msg The message to log
    * @note Automatically detects if formatting is needed based on arguments
    *
    * Examples:
    *   log::ln(INFO, "Simple message");           // Optimized path (no formatting)
    *   log::ln(DEBUG, "Value = %d", 42);          // Formatted path
    *   log::ln(ERROR, "x=%d y=%f", 10, 3.14f);    // Multiple args
    */
    static void ln(const LevelArg& arg, const char* msg) {
        if constexpr (LOG_ENABLED_LOCAL) {
            if (_runtimeAllows(arg.level) && msg) {
                TextLog evt{};
                _emitEvent(evt, T_Config::TAG, arg.level, arg.loc, msg);
            }
        }
    }

    /*
    * @brief Log a text line (unified API for simple and formatted logs)
    * @param arg The log level and source location
    * @param msg The message to log
    * @note Automatically detects if formatting is needed based on arguments
    *
    * Examples:
    *   log::ln(INFO, "Simple message");           // Optimized path (no formatting)
    *   log::ln(DEBUG, "Value = %d", 42);          // Formatted path
    *   log::ln(ERROR, "x=%d y=%f", 10, 3.14f);    // Multiple args
    */
    template<typename... Args>
    static void ln(const LevelArg& arg, const char* fmt, Args&&... args) {
        if constexpr (LOG_ENABLED_LOCAL && sizeof...(Args) > 0) {
            if (_runtimeAllows(arg.level) && fmt) {
                TextLog evt{};
                snprintf(evt.formatted, sizeof(evt.formatted), fmt, std::forward<Args>(args)...);
                _emitEvent(evt, T_Config::TAG, arg.level, arg.loc, nullptr);
            }
        }
    }

    /*
    * @brief Log a structured event
    * Example: log::evt(ERROR, ModbusLog{"read", "CO2_SP", "holding register", 142, 450.0f, false}, "Sensor inop");
    */
    template<typename T>
    static void evt(const LevelArg& arg, const T& evt, const char* msg = nullptr) {
        if constexpr (LOG_ENABLED_LOCAL) {
            if (_runtimeAllows(arg.level)) {
                _emitEvent(evt, T_Config::TAG, arg.level, arg.loc, msg);
            }
        }
    }

    /*
    * @brief Log a text line from ISR context (simple string only)
    * @param arg The log level and source location
    * @param msg The message to log (must be a string literal or static string)
    * @param hpw Optional pointer to FreeRTOS "higher priority task woken" flag
    * @note ISR-safe: uses xQueueSendFromISR
    * @note Formatted strings are NOT supported in ISR context (see below)
    * @note Caller must call portYIELD_FROM_ISR() after all ISR logging calls
    *
    * Example:
    *   void IRAM_ATTR myISR() {
    *       BaseType_t hpw = pdFALSE;
    *       log::lnFromISR(ERROR, "ISR triggered", &hpw);
    *       portYIELD_FROM_ISR(hpw);
    *   }
    */
    static void lnFromISR(const LevelArg& arg, const char* msg, BaseType_t* hpw = nullptr) {
        if constexpr (LOG_ENABLED_LOCAL) {
            if (_runtimeAllows(arg.level) && msg) {
                TextLog evt{};
                _emitEventFromISR(evt, T_Config::TAG, arg.level, arg.loc, msg, hpw);
            }
        }
    }

    /*
        * @brief Deleted overload to prevent formatted logging from ISR
        * @note Formatted strings use vsnprintf() which is NOT ISR-safe
        */
    template<typename... Args>
    static void lnFromISR(const LevelArg& arg, const char* fmt, Args&&... args) {
        static_assert(sizeof...(Args) == -1,
            "Formatted strings are not supported in ISR context. Use lnFromISR() with string literals only.");
    }

    /*
        * @brief Log a structured event from ISR context
        * @param arg The log level and source location
        * @param evt The event struct to log
        * @param msg Optional message (must be a string literal or static string)
        * @param hpw Optional pointer to FreeRTOS "higher priority task woken" flag
        * @note ISR-safe: uses xQueueSendFromISR
        * @note Caller must call portYIELD_FROM_ISR() after all ISR logging calls
        *
        * Example:
        *   void IRAM_ATTR myISR() {
        *       BaseType_t hpw = pdFALSE;
        *       log::evtFromISR(ERROR, ErrorLog{0x42, "ISR error"}, "Fault", &hpw);
        *       portYIELD_FROM_ISR(hpw);
        *   }
        */
    template<typename T>
    static void evtFromISR(const LevelArg& arg, const T& evt, const char* msg = nullptr,
                            BaseType_t* hpw = nullptr) {
        if constexpr (LOG_ENABLED_LOCAL) {
            if (_runtimeAllows(arg.level)) {
                _emitEventFromISR(evt, T_Config::TAG, arg.level, arg.loc, msg, hpw);
            }
        }
    }

#else // LOG_ENABLED_GLOBAL

    // No-op implementations when global logging is disabled
    // Totally removes all logging code from the build with -O2 or -Os
    // (will not even evaluate args)

    template<typename... Args>
    static constexpr void ln(Args&&...) {}

    template<typename... Args>
    static constexpr void evt(Args&&...) {}

    template<typename... Args>
    static constexpr void lnFromISR(Args&&...) {}

    template<typename... Args>
    static constexpr void evtFromISR(Args&&...) {}

#endif // LOG_ENABLED_GLOBAL

    
    // ========================================================================
    // PUBLIC API - LOGGING METRICS
    // ========================================================================

    /*
     * @brief Get the number of dropped console messages
     * @return The number of dropped console messages
     */
    static uint32_t getDroppedConsole() {
        return _droppedConsole;
    }

    /*
     * @brief Get the number of dropped JSON messages
     * @return The number of dropped JSON messages
     */
    static uint32_t getDroppedJsonTotal() {
        return _droppedJsonTotal;
    }

    /*
     * @brief Get the number of dropped JSON messages for a specific subscriber
     * @param handle The message buffer handle of the subscriber
     * @return The number of dropped JSON messages, or -1 if the subscriber is not found
     */
    static int32_t getDroppedJson(MessageBufferHandle_t handle) {
        for (size_t i = 0; i < _jsonSubscriberCount; ++i) {
            if (_jsonSubscribers[i].msgBuffer == handle) {
                return (int32_t)_jsonSubscribers[i].dropped;
            }
        }
        return -1;
    }

    /*
     * @brief Get the total number of dropped text messages across all text subscribers
     * @return The total number of dropped text messages
     */
    static uint32_t getDroppedTextTotal() {
        return _droppedTextTotal;
    }

    /*
     * @brief Get the number of dropped text messages for a specific subscriber
     * @param handle The message buffer handle of the subscriber
     * @return The number of dropped text messages, or -1 if the subscriber is not found
     */
    static int32_t getDroppedText(MessageBufferHandle_t handle) {
        for (size_t i = 0; i < _textSubscriberCount; ++i) {
            if (_textSubscribers[i].msgBuffer == handle) {
                return (int32_t)_textSubscribers[i].dropped;
            }
        }
        return -1;
    }


#ifdef ARDUINO
// ========================================================================
// PUBLIC API - ARDUINO STREAM ADAPTER
// ========================================================================

private:
    /*
     * @brief Arduino Stream adapter for logging
     * @note Buffers characters and logs on newline, buffer full, or flush()
     * @note Inherits from Arduino Stream class for compatibility with libraries
     * @note Read operations are not supported (write-only stream)
     * @note Handles \r\n pairs correctly (single flush, not double)
     * @note Auto-flushes when buffer is full (255 chars), then continues buffering
     * @note Source location is not captured since we have to match the exact Arduino Stream interface
     */
    class StreamAdapter : public Stream {
        char _buffer[LOG_CONSOLE_MAX_LINE_SIZE];
        size_t _bufPos;
        Level _level;
        char _lastChar;  // Track last character to handle \r\n pairs

    public:
        explicit StreamAdapter(Level lvl = INFO) : _bufPos(0), _level(lvl), _lastChar(0) {}

        void setLevel(Level lvl) { _level = lvl; }

        size_t write(uint8_t c) override {
            // Handle newline characters (\r or \n)
            if (c == '\n' || c == '\r') {
                // Skip second char of \r\n or \n\r pair to avoid double flush
                if ((_lastChar == '\r' && c == '\n') || (_lastChar == '\n' && c == '\r')) {
                    _lastChar = c;
                    return 1;
                }

                // Flush current buffer (even if empty → blank line)
                _buffer[_bufPos] = '\0';
                Logger::ln(_level, "%s", _buffer);
                _bufPos = 0;
                _lastChar = c;
                return 1;
            }

            // Handle normal characters
            // Auto-flush if buffer is full, then add character
            if (_bufPos >= sizeof(_buffer) - 1) {
                _buffer[_bufPos] = '\0';
                Logger::ln(_level, "%s", _buffer);
                _bufPos = 0;
            }

            // Add character to buffer
            _buffer[_bufPos] = c;
            _bufPos++;
            _lastChar = c;
            return 1;
        }

        int available() override { return 0; }
        int read() override { return -1; }
        int peek() override { return -1; }

        void flush() override {
            if (_bufPos > 0) {
                _buffer[_bufPos] = '\0';
                Logger::ln(_level, "%s", _buffer);
                _bufPos = 0;
            }
            _lastChar = 0;  // Reset tracker
        }
    };

public:
    /*
     * @brief Get a Stream-compatible interface for Arduino libraries
     * @param level The log level to use for messages (default: INFO)
     * @return Reference to a static Stream adapter
     * @note Thread-safe for single level, not safe if multiple threads use different levels
     * @note Useful for libraries that require a Stream* (e.g., WiFiManager, ArduinoOTA)
     *
     * Example:
     *   WiFiManager wifiManager;
     *   wifiManager.setDebugOutput(true);
     *   wifiManager.debugStream(&log::stream(DEBUG));
     *
     *   ArduinoOTA.setDebugStream(&log::stream(INFO));
     */
    static Stream& stream(Level level = INFO) {
        static StreamAdapter adapter(level);
        adapter.setLevel(level);  // Update level in case it changed
        return adapter;
    }

#endif // ARDUINO

}; // class Logger


// ============================================================================
// FLOAT FORMATTING MACRO (uses ftoa_ helpers defined above)
// ============================================================================

/*
 * @brief Convenience macro for lean printf-style float formatting
 * @param val Float value to format
 * @param dec Number of decimal places
 * @return Two comma-separated arguments (int, int) for use in printf
 *
 * Usage:
 *   snprintf(out, cap, "temp=%d.%02d°C", FTOA(temperature, 2));
 *   snprintf(out, cap, "value=%d.%03d", FTOA(value, 3));
 *
 * Notes:
 *   - No stack overhead (pure math, no buffers)
 *   - No limit on number of FTOA() calls in one printf
 *   - User must match decimal padding in format string (%02d for 2 decimals, %03d for 3, etc.)
 *   - Works with any number of decimals (limited by float precision ~7 digits)
 */
#define FTOA(val, dec) ezlog::ftoa_int(val), ezlog::ftoa_frac(val, dec)

} // namespace ezlog

#endif // _EZLOG_HPP_
