#pragma once

#ifdef ESP_PLATFORM

#include <atomic>
#include <esp_timer.h>
#include <soc/soc_caps.h>

#include "can_frame_types.h"
#include "gvret_protocol.h"
#include "platform/espidf_runtime.h"

namespace GvretSerial
{
using namespace GvretProtocol;
static constexpr uint32_t kMaxRunMs = 10UL * 60UL * 1000UL;
static constexpr uint32_t kClientIdleMs = 15000;

inline std::atomic<bool> enabled{false};
inline std::atomic<bool> clientConnected{false};
inline std::atomic<uint32_t> framesSent{0};
inline std::atomic<uint32_t> framesDropped{0};
inline std::atomic<uint32_t> bytesSent{0};
inline std::atomic<uint32_t> startedMs{0};
inline std::atomic<uint32_t> lastCommandMs{0};
inline TaskHandle_t taskHandle = nullptr;
inline bool waitingForCommand = false;
inline void (*monitorCallback)(bool) = nullptr;

inline void setMonitorCallback(void (*callback)(bool)) { monitorCallback = callback; }

inline void setClientConnected(bool connected)
{
    bool previous = clientConnected.exchange(connected, std::memory_order_relaxed);
    if (previous == connected)
        return;
    Serial.setProtocolMode(connected);
    if (monitorCallback)
        monitorCallback(connected);
    if (!connected)
        waitingForCommand = false;
}

inline size_t sendBytes(const uint8_t *data, size_t len)
{
    if (!clientConnected.load(std::memory_order_relaxed) ||
        Serial.availableForWrite() < static_cast<int>(len))
        return 0;
    size_t written = Serial.write(data, len);
    bytesSent.fetch_add(static_cast<uint32_t>(written), std::memory_order_relaxed);
    return written;
}

inline void sendDeviceInfo()
{
    const uint8_t response[8] = {
        kProtocolStart, kGetDeviceInfo, 0x6A, 0x02, 0x01, 0x20, 0x00, 0x00};
    sendBytes(response, sizeof(response));
}

inline void sendBusCount()
{
    const uint8_t response[3] = {kProtocolStart, kGetBusCount, 1};
    sendBytes(response, sizeof(response));
}

inline void sendCanParams()
{
    // GVRET keeps two five-byte bus slots in this fixed-size reply. Bus 1 is
    // disabled; firmware exposes only bus 0 at 500 kbit/s.
    sendBytes(kCanParamsResponse, sizeof(kCanParamsResponse));
}

inline void sendKeepAlive()
{
    const uint8_t response[4] = {kProtocolStart, kKeepAlive, 0xDE, 0xAD};
    sendBytes(response, sizeof(response));
}

inline bool handleCommand(uint8_t command)
{
    switch (command)
    {
    case kGetDeviceInfo:
        sendDeviceInfo();
        return true;
    case kGetBusCount:
        sendBusCount();
        return true;
    case kGetParams:
        sendCanParams();
        return true;
    case kKeepAlive:
        sendKeepAlive();
        return true;
    default:
        return false;
    }
}

inline void parseInbound(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        const uint8_t byte = data[i];
        if (waitingForCommand)
        {
            waitingForCommand = false;
            if (!enabled.load(std::memory_order_relaxed) || !handleCommand(byte))
                continue;
            lastCommandMs.store(millis(), std::memory_order_relaxed);
            setClientConnected(true);
            // The first response was suppressed until protocol ownership was
            // established. Send it again on the now-exclusive transport.
            handleCommand(byte);
            continue;
        }
        if (byte == kBinaryMode)
            continue;
        if (byte == kProtocolStart)
            waitingForCommand = true;
    }
}

inline void broadcast(const CanFrame &frame)
{
    if (!clientConnected.load(std::memory_order_relaxed))
        return;
    uint8_t buffer[19] = {};
    const uint32_t timestamp = static_cast<uint32_t>(esp_timer_get_time());
    const size_t encoded = encodeCanFrame(frame, timestamp, buffer, sizeof(buffer));
    if (!encoded)
    {
        framesDropped.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    if (sendBytes(buffer, encoded) == encoded)
        framesSent.fetch_add(1, std::memory_order_relaxed);
    else
        framesDropped.fetch_add(1, std::memory_order_relaxed);
}

inline void task(void *)
{
    uint8_t buffer[64];
    for (;;)
    {
        size_t count = 0;
        while (enabled.load(std::memory_order_relaxed) && count < sizeof(buffer) &&
               Serial.available() > 0)
        {
            int byte = Serial.read();
            if (byte < 0)
                break;
            buffer[count++] = static_cast<uint8_t>(byte);
        }
        if (count)
            parseInbound(buffer, count);

        const uint32_t now = millis();
        bool connected = clientConnected.load(std::memory_order_relaxed);
        if (connected && (!Serial.connected() ||
                          now - lastCommandMs.load(std::memory_order_relaxed) >= kClientIdleMs))
            setClientConnected(false);
        if (enabled.load(std::memory_order_relaxed) &&
            now - startedMs.load(std::memory_order_relaxed) >= kMaxRunMs)
        {
            enabled.store(false, std::memory_order_relaxed);
            setClientConnected(false);
            Serial.println("[GVRET] stopped after maximum 10-minute session");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

inline bool begin()
{
    if (taskHandle)
        return true;
#if SOC_CPU_CORES_NUM == 1
    BaseType_t result = xTaskCreate(task, "gvret", 3072, nullptr, 1, &taskHandle);
#else
    BaseType_t result = xTaskCreatePinnedToCore(task, "gvret", 3072, nullptr, 1,
                                                &taskHandle, 0);
#endif
    return result == pdPASS;
}

inline bool start()
{
    if (!begin())
        return false;
    setClientConnected(false);
    enabled.store(true, std::memory_order_relaxed);
    startedMs.store(millis(), std::memory_order_relaxed);
    lastCommandMs.store(millis(), std::memory_order_relaxed);
    framesSent.store(0, std::memory_order_relaxed);
    framesDropped.store(0, std::memory_order_relaxed);
    bytesSent.store(0, std::memory_order_relaxed);
    Serial.println("[GVRET] armed; waiting for SavvyCAN on USB serial");
    return true;
}

inline void stop()
{
    enabled.store(false, std::memory_order_relaxed);
    setClientConnected(false);
    Serial.println("[GVRET] stopped");
}

inline uint32_t runtimeMs()
{
    return enabled.load(std::memory_order_relaxed)
               ? millis() - startedMs.load(std::memory_order_relaxed)
               : 0;
}
} // namespace GvretSerial

#endif
