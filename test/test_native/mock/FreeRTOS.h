/* Mock FreeRTOS for native testing */

#ifndef MOCK_FREERTOS_H
#define MOCK_FREERTOS_H

#include <cstdint>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <atomic>

// FreeRTOS types
using TickType_t = uint32_t;
using BaseType_t = int32_t;
using UBaseType_t = uint32_t;
using TaskHandle_t = void*;
using QueueHandle_t = void*;
using MessageBufferHandle_t = void*;
using StackType_t = uint8_t;

// FreeRTOS constants
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS pdTRUE
#define pdFAIL pdFALSE
#define portMAX_DELAY UINT32_MAX
#define tskNO_AFFINITY -1

// Task priority
#define tskIDLE_PRIORITY 0

// Mutex implementation
struct portMUX_TYPE {
    std::mutex mtx;
};

#define portMUX_INITIALIZER_UNLOCKED portMUX_TYPE{}

inline void portENTER_CRITICAL(portMUX_TYPE* mux) {
    mux->mtx.lock();
}

inline void portEXIT_CRITICAL(portMUX_TYPE* mux) {
    mux->mtx.unlock();
}

// Time conversion
inline constexpr TickType_t pdMS_TO_TICKS(uint32_t ms) {
    return ms; // 1:1 mapping for simplicity
}

// Task delay
inline void vTaskDelay(TickType_t ticks) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ticks));
}

// Queue structures
struct StaticQueue_t {
    std::queue<uint8_t> data;
    std::mutex mtx;
    std::condition_variable cv;
    size_t itemSize;
    size_t maxItems;
};

// Queue operations
inline QueueHandle_t xQueueCreateStatic(UBaseType_t queueLength, UBaseType_t itemSize,
                                         uint8_t* storage, StaticQueue_t* queueBuffer) {
    queueBuffer->itemSize = itemSize;
    queueBuffer->maxItems = queueLength;
    return (QueueHandle_t)queueBuffer;
}

inline BaseType_t xQueueSend(QueueHandle_t queue, const void* item, TickType_t timeout) {
    auto* q = (StaticQueue_t*)queue;
    std::unique_lock<std::mutex> lock(q->mtx);

    if (q->data.size() >= q->maxItems * q->itemSize) {
        if (timeout == 0) return pdFAIL;
        q->cv.wait_for(lock, std::chrono::milliseconds(timeout), [q] {
            return q->data.size() < q->maxItems * q->itemSize;
        });
        if (q->data.size() >= q->maxItems * q->itemSize) return pdFAIL;
    }

    const uint8_t* bytes = (const uint8_t*)item;
    for (size_t i = 0; i < q->itemSize; ++i) {
        q->data.push(bytes[i]);
    }
    q->cv.notify_one();
    return pdPASS;
}

inline BaseType_t xQueueReceive(QueueHandle_t queue, void* buffer, TickType_t timeout) {
    auto* q = (StaticQueue_t*)queue;
    std::unique_lock<std::mutex> lock(q->mtx);

    if (q->data.empty()) {
        if (timeout == 0) return pdFAIL;
        if (timeout == portMAX_DELAY) {
            q->cv.wait(lock, [q] { return !q->data.empty(); });
        } else {
            if (!q->cv.wait_for(lock, std::chrono::milliseconds(timeout), [q] {
                return !q->data.empty();
            })) {
                return pdFAIL;
            }
        }
    }

    uint8_t* bytes = (uint8_t*)buffer;
    for (size_t i = 0; i < q->itemSize; ++i) {
        if (q->data.empty()) return pdFAIL;
        bytes[i] = q->data.front();
        q->data.pop();
    }
    q->cv.notify_one();
    return pdPASS;
}

// Message buffer mock
struct StaticMessageBuffer_t {
    std::queue<std::string> messages;
    std::mutex mtx;
};

inline MessageBufferHandle_t xMessageBufferCreateStatic(size_t bufferSize, uint8_t* storage, void* structBuffer) {
    (void)bufferSize; (void)storage;
    return (MessageBufferHandle_t)structBuffer;
}

inline size_t xMessageBufferSend(MessageBufferHandle_t msgBuffer, const void* data, size_t len, TickType_t timeout) {
    (void)timeout;
    if (!msgBuffer || !data || len == 0) return 0;

    StaticMessageBuffer_t* buf = (StaticMessageBuffer_t*)msgBuffer;
    std::lock_guard<std::mutex> lock(buf->mtx);
    buf->messages.push(std::string((const char*)data, len));
    return len;
}

inline size_t xMessageBufferReceive(MessageBufferHandle_t msgBuffer, void* data, size_t maxLen, TickType_t timeout) {
    (void)timeout;
    if (!msgBuffer || !data) return 0;

    StaticMessageBuffer_t* buf = (StaticMessageBuffer_t*)msgBuffer;
    std::lock_guard<std::mutex> lock(buf->mtx);

    if (buf->messages.empty()) return 0;

    const std::string& msg = buf->messages.front();
    size_t copyLen = std::min(msg.size(), maxLen);
    memcpy(data, msg.data(), copyLen);
    buf->messages.pop();
    return copyLen;
}

// Task structures
struct StaticTask_t {
    std::thread thread;
};

// Task creation
using TaskFunction_t = void (*)(void*);

inline TaskHandle_t xTaskCreateStatic(TaskFunction_t taskFunc, const char* name,
                                       uint32_t stackSize, void* params,
                                       UBaseType_t priority, StackType_t* stack,
                                       StaticTask_t* taskBuffer) {
    taskBuffer->thread = std::thread(taskFunc, params);
    taskBuffer->thread.detach();
    return (TaskHandle_t)taskBuffer;
}

inline TaskHandle_t xTaskCreateStaticPinnedToCore(TaskFunction_t taskFunc, const char* name,
                                                    uint32_t stackSize, void* params,
                                                    UBaseType_t priority, StackType_t* stack,
                                                    StaticTask_t* taskBuffer, BaseType_t core) {
    return xTaskCreateStatic(taskFunc, name, stackSize, params, priority, stack, taskBuffer);
}

#endif // MOCK_FREERTOS_H
