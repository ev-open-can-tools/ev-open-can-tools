#pragma once

#ifdef ESP_PLATFORM

#include <atomic>
#include <esp_attr.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include "drivers/can_driver.h"
#include "platform/espidf_runtime.h"

// Keep these named constants visible: they are safety timing policy, not UI
// preferences. The CAN delay starts when the controller actually initializes.
static const unsigned long DRIVER_WAKE_DELAY_MS = 10000;
static const unsigned long INJECTION_DELAY_MS = 15000;
static const uint32_t CAN_LIVE_FRAME_THRESHOLD = 1000;

namespace RuntimeDiagnostics
{
enum class NvsState : uint8_t
{
    Unknown,
    Ready,
    Recovered,
    Error,
};

struct StaticSystemInfo
{
    esp_chip_info_t chip = {};
    uint32_t flashBytes = 0;
    size_t internalRamBytes = 0;
    size_t psramBytes = 0;
};

inline std::atomic<uint32_t> mainHeartbeat{0};
inline std::atomic<uint32_t> canHeartbeat{0};
inline std::atomic<uint32_t> canRxHeartbeat{0};
inline std::atomic<uint32_t> webHeartbeat{0};
inline std::atomic<uint32_t> canFrames{0};
inline std::atomic<uint32_t> lastCanFrameMs{0};
inline std::atomic<uint32_t> canInitializedMs{0};
inline std::atomic<uint32_t> txOk{0};
inline std::atomic<uint32_t> txFail{0};
inline std::atomic<uint32_t> heartbeatLogs{0};
inline std::atomic<uint32_t> noCanWarnings{0};
inline std::atomic<uint32_t> supportRequests{0};
inline std::atomic<uint32_t> lastSupportBytes{0};
inline std::atomic<uint32_t> lastStatusBytes{0};
inline std::atomic<NvsState> nvsState{NvsState::Unknown};
inline std::atomic<int32_t> nvsInitialError{ESP_OK};
inline std::atomic<int32_t> nvsFinalError{ESP_OK};
inline uint32_t lastHeartbeatLogMs = 0;
inline uint32_t lastNoCanWarningMs = 0;
inline esp_reset_reason_t bootResetReason = ESP_RST_UNKNOWN;
inline StaticSystemInfo systemInfo;
inline TaskHandle_t mainTaskHandle = nullptr;
RTC_DATA_ATTR inline uint32_t rtcBootCount = 0;

inline const char *resetReasonName(esp_reset_reason_t reason)
{
    switch (reason)
    {
    case ESP_RST_POWERON:
        return "power-on";
    case ESP_RST_EXT:
        return "external";
    case ESP_RST_SW:
        return "software";
    case ESP_RST_PANIC:
        return "panic";
    case ESP_RST_INT_WDT:
        return "interrupt-watchdog";
    case ESP_RST_TASK_WDT:
        return "task-watchdog";
    case ESP_RST_WDT:
        return "watchdog";
    case ESP_RST_DEEPSLEEP:
        return "deep-sleep";
    case ESP_RST_BROWNOUT:
        return "brownout";
    case ESP_RST_SDIO:
        return "sdio";
    default:
        return "unknown";
    }
}

inline void begin()
{
    mainTaskHandle = xTaskGetCurrentTaskHandle();
    rtcBootCount++;
    bootResetReason = esp_reset_reason();
    esp_chip_info(&systemInfo.chip);
    esp_flash_get_size(esp_flash_default_chip, &systemInfo.flashBytes);
    systemInfo.internalRamBytes = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    systemInfo.psramBytes = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    Serial.printf("[BOOT] reset=%d (%s) rtcBootCount=%lu\n", static_cast<int>(bootResetReason),
                  resetReasonName(bootResetReason), static_cast<unsigned long>(rtcBootCount));
    if (bootResetReason == ESP_RST_BROWNOUT)
        Serial.println("[WARN] Previous reset was caused by brownout");
    Serial.printf("[BOOT] ESP-IDF %s\n", esp_get_idf_version());
}

inline void noteNvsInitialization(esp_err_t initialError, esp_err_t finalError, bool recovered)
{
    nvsInitialError.store(initialError, std::memory_order_relaxed);
    nvsFinalError.store(finalError, std::memory_order_relaxed);
    nvsState.store(finalError != ESP_OK ? NvsState::Error
                   : recovered          ? NvsState::Recovered
                                        : NvsState::Ready,
                   std::memory_order_relaxed);
}

inline const char *nvsStateName()
{
    switch (nvsState.load(std::memory_order_relaxed))
    {
    case NvsState::Ready:
        return "ready";
    case NvsState::Recovered:
        return "recovered";
    case NvsState::Error:
        return "error";
    default:
        return "unknown";
    }
}

inline void noteMainLoop() { mainHeartbeat.fetch_add(1, std::memory_order_relaxed); }
inline void noteCanLoop() { canHeartbeat.fetch_add(1, std::memory_order_relaxed); }
inline void noteWebLoop() { webHeartbeat.fetch_add(1, std::memory_order_relaxed); }

inline void noteCanFrame()
{
    canFrames.fetch_add(1, std::memory_order_relaxed);
    canRxHeartbeat.fetch_add(1, std::memory_order_relaxed);
    lastCanFrameMs.store(millis(), std::memory_order_relaxed);
}

inline void noteCanInitialized()
{
    uint32_t expected = 0;
    canInitializedMs.compare_exchange_strong(expected, millis(), std::memory_order_relaxed);
}

inline void noteTransmit(bool ok)
{
    (ok ? txOk : txFail).fetch_add(1, std::memory_order_relaxed);
}

inline uint32_t canAgeMs(uint32_t now = millis())
{
    uint32_t seen = lastCanFrameMs.load(std::memory_order_relaxed);
    return seen == 0 ? UINT32_MAX : now - seen;
}

inline bool injectionReady(uint32_t now = millis())
{
    const uint32_t initialized = canInitializedMs.load(std::memory_order_relaxed);
    return initialized != 0 && now - initialized >= INJECTION_DELAY_MS &&
           canFrames.load(std::memory_order_relaxed) > CAN_LIVE_FRAME_THRESHOLD;
}

inline uint32_t injectionDelayRemainingMs(uint32_t now = millis())
{
    const uint32_t initialized = canInitializedMs.load(std::memory_order_relaxed);
    if (initialized == 0)
        return INJECTION_DELAY_MS;
    const uint32_t elapsed = now - initialized;
    return elapsed >= INJECTION_DELAY_MS ? 0 : INJECTION_DELAY_MS - elapsed;
}

inline void logHeartbeat(const CanDriver *driver)
{
    const uint32_t now = millis();
    if (now - lastHeartbeatLogMs < 5000)
        return;
    lastHeartbeatLogMs = now;
    heartbeatLogs.fetch_add(1, std::memory_order_relaxed);

    const uint32_t totalFrames = canFrames.load(std::memory_order_relaxed);
    const uint32_t age = canAgeMs(now);
    Serial.printf(
        "[BEAT] uptime=%lus main=%lu can=%lu canRx=%lu web=%lu frames=%lu ageMs=%lu txOk=%lu txFail=%lu heap=%lu ready=%s\n",
        static_cast<unsigned long>(now / 1000),
        static_cast<unsigned long>(mainHeartbeat.load(std::memory_order_relaxed)),
        static_cast<unsigned long>(canHeartbeat.load(std::memory_order_relaxed)),
        static_cast<unsigned long>(canRxHeartbeat.load(std::memory_order_relaxed)),
        static_cast<unsigned long>(webHeartbeat.load(std::memory_order_relaxed)),
        static_cast<unsigned long>(totalFrames), static_cast<unsigned long>(age),
        static_cast<unsigned long>(txOk.load(std::memory_order_relaxed)),
        static_cast<unsigned long>(txFail.load(std::memory_order_relaxed)),
        static_cast<unsigned long>(esp_get_free_heap_size()), injectionReady(now) ? "yes" : "no");

    if (driver)
    {
        char diagnostics[640] = {};
        driver->diagnosticsSummary(diagnostics, sizeof(diagnostics));
        Serial.print("[CAN] ");
        Serial.println(diagnostics);
    }

    const uint32_t initialized = canInitializedMs.load(std::memory_order_relaxed);
    if (initialized != 0 && totalFrames == 0 && now - initialized >= 20000 &&
        now - lastNoCanWarningMs >= 5000)
    {
        lastNoCanWarningMs = now;
        noCanWarnings.fetch_add(1, std::memory_order_relaxed);
        Serial.println("[WARN] No CAN frames yet, staying alive.");
    }
}
} // namespace RuntimeDiagnostics

#endif
