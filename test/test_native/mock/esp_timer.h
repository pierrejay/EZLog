/* Mock esp_timer.h for native testing */

#ifndef MOCK_ESP_TIMER_H
#define MOCK_ESP_TIMER_H

#include <cstdint>
#include <chrono>

// ESP timer functions
inline int64_t esp_timer_get_time() {
    auto now = std::chrono::steady_clock::now();
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
    return micros.count();
}

#endif // MOCK_ESP_TIMER_H
