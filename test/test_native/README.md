# EZLog Native Test Suite

Native build tests for **macOS/Linux/Windows**. Tests all logger features without Arduino/ESP dependencies.

**Requirements:**
- macOS: Xcode Command Line Tools (clang++)
- Linux: GCC/Clang (g++ or clang++)
- Windows: MinGW/MSYS2 (g++) or WSL

## Build & Run

The Makefile automatically detects your OS (macOS/Linux/Windows) and uses the appropriate compiler and flags.

```bash
cd test/test_native
make          # Build
./ezlog_test  # Run tests (or ezlog_test.exe on Windows)
```

Or build + run in one command:
```bash
make run
```

Clean build artifacts:
```bash
make clean
```

## What it tests

- Text logging (simple & formatted)
- Custom event types (Error, Modbus, HTTP, Sensor, Device, Network, System)
- JSON output serialization
- Runtime log level filtering
- Edge cases (nullptr, long strings, rapid logging)

## Configuration

Uses `IoTGatewayLogConfig.hpp` from examples with:
- Console output enabled
- JSON MessageBuffer enabled
- All 7 custom event types
- DEBUG compile-time level

## Mocks

ESP32 and FreeRTOS primitives are mocked in `mock/` directory:
- `queue.h` - FreeRTOS queues (std::deque + std::thread based)
- `message_buffer.h` - FreeRTOS MessageBuffer (std::deque based)
- `esp_timer.h` - ESP-IDF timer (std::chrono::steady_clock based)

IoTGatewayLogConfig works on native because:
- `esp_timer_get_time()` → mocked with std::chrono
- `fwrite()` → native C stdlib (no mock needed)
