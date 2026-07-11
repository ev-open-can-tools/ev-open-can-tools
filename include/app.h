#pragma once

#include <memory>
#include "can_frame_types.h"
#include "drivers/can_driver.h"
#include "can_helpers.h"
#include "handlers.h"
#ifdef ESP_PLATFORM
#include "gvret_serial.h"
#include "runtime_diagnostics.h"
#endif

#ifndef NATIVE_BUILD
#ifdef ESP_PLATFORM
#include "platform/espidf_runtime.h"
#else
#include <Arduino.h>
#endif
#endif
#if defined(DASH_RGB_STATUS_LED) && !defined(NATIVE_BUILD) && !defined(ESP_PLATFORM)
#include <esp32-hal-rgb-led.h>
#endif
#if defined(DASH_RGB_STATUS_LED) && defined(ESP_PLATFORM)
#include <led_strip.h>
#endif

#ifndef PIN_LED
#define PIN_LED 2
#endif

#if defined(ESP32_DASHBOARD)
#if DASH_DEFAULT_HW == 0
using SelectedHandler = LegacyHandler;
#elif DASH_DEFAULT_HW == 2
using SelectedHandler = HW4Handler;
#else
using SelectedHandler = HW3Handler;
#endif
#elif defined(NAG_KILLER)
using SelectedHandler = NagHandler;
#elif defined(HW4)
using SelectedHandler = HW4Handler;
#elif defined(HW3)
using SelectedHandler = HW3Handler;
#elif defined(LEGACY)
using SelectedHandler = LegacyHandler;
#else
#error "Define HW4, HW3, LEGACY, or NAG_KILLER in build_flags"
#endif

static std::unique_ptr<CanDriver> appDriver;
static std::unique_ptr<CarManagerBase> appHandler;
static Shared<CarManagerBase *> appActiveHandler{nullptr};

class AppHandlerGuard
{
public:
    AppHandlerGuard()
    {
#ifdef ESP_PLATFORM
        if (appHandlerMutex_)
            locked_ = xSemaphoreTakeRecursive(appHandlerMutex_, portMAX_DELAY) == pdTRUE;
#endif
    }

    ~AppHandlerGuard()
    {
#ifdef ESP_PLATFORM
        if (locked_)
            xSemaphoreGiveRecursive(appHandlerMutex_);
#endif
    }

    static bool initialize()
    {
#ifdef ESP_PLATFORM
        if (!appHandlerMutex_)
            appHandlerMutex_ = xSemaphoreCreateRecursiveMutex();
        return appHandlerMutex_ != nullptr;
#else
        return true;
#endif
    }

private:
#ifdef ESP_PLATFORM
    inline static SemaphoreHandle_t appHandlerMutex_ = nullptr;
    bool locked_ = false;
#endif
};

static CarManagerBase *appGetActiveHandler()
{
    return appActiveHandler;
}

// Plugin processing hook — set by dashboard to apply plugin rules after handler
static void (*appPluginProcess)(const CanFrame &, CanDriver &) = nullptr;
static void (*appDashboardTxObserver)(const CanFrame &, bool) = nullptr;

static bool appInjectionReady()
{
#ifdef ESP_PLATFORM
    return RuntimeDiagnostics::injectionReady();
#else
    return true;
#endif
}

static bool appCanTransmitAllowed(const CanFrame &)
{
    return appInjectionReady();
}

static void appOnSendFrame(const CanFrame &frame, bool ok)
{
#ifdef ESP_PLATFORM
    RuntimeDiagnostics::noteTransmit(ok);
#endif
    if (appDashboardTxObserver)
        appDashboardTxObserver(frame, ok);
}

#ifdef ESP_PLATFORM
static void appSetCanMonitorAll(bool enabled)
{
    if (appDriver)
        appDriver->setMonitorAll(enabled);
}
#endif

static volatile bool frameReady = true;
static void canISR() { frameReady = true; }

#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD) && defined(DASH_RGB_STATUS_LED)
static void appRefreshStatusLed(bool force = false);
static void appWriteStatusLed(uint8_t red, uint8_t green, uint8_t blue);
#endif
#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD) && defined(DASH_INJECTION_TOGGLE_PIN)
static void appPollInjectionToggleButton();
#endif

#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD)
#include "web/mcp2515_dashboard.h"
#endif

#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD) && defined(DASH_RGB_STATUS_LED)
static void appWriteStatusLed(uint8_t red, uint8_t green, uint8_t blue)
{
#ifdef ESP_PLATFORM
    static led_strip_handle_t strip = nullptr;
    if (!strip)
    {
        led_strip_config_t scfg = {};
        scfg.strip_gpio_num = PIN_LED;
        scfg.max_leds = 1;
        scfg.led_model = LED_MODEL_WS2812;
        scfg.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
        led_strip_rmt_config_t rcfg = {};
        rcfg.resolution_hz = 10 * 1000 * 1000;
        if (led_strip_new_rmt_device(&scfg, &rcfg, &strip) != ESP_OK)
            return;
    }
    led_strip_set_pixel(strip, 0, red, green, blue);
    led_strip_refresh(strip);
#elif defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    rgbLedWrite(PIN_LED, red, green, blue);
#else
    neopixelWrite(PIN_LED, red, green, blue);
#endif
}

static void appRefreshStatusLed(bool force)
{
#ifdef ESP_PLATFORM
    static SemaphoreHandle_t ledMutex = xSemaphoreCreateMutex();
    bool ledLocked = ledMutex && xSemaphoreTake(ledMutex, portMAX_DELAY) == pdTRUE;
#endif
    static bool known = false;
    static bool lastInjecting = false;
    static bool lastConnected = false;
    static bool lastOta = false;
    static uint8_t lastLevel = 0;
    static bool lastEmittedOn = true;

    bool injecting = canActive;
    bool connected = (WiFi.softAPgetStationNum() > 0) || (WiFi.status() == WL_CONNECTED);
    bool ota = Update.isRunning();
    uint8_t level = dashLedBrightness;

    // Solid when connected (or OTA), 1 Hz blink otherwise.
    bool blinkOn = (millis() % 1000UL) < 500UL;
    bool emittedOn = (connected || ota) ? true : blinkOn;

    if (!force && known && lastInjecting == injecting && lastConnected == connected && lastOta == ota && lastLevel == level && lastEmittedOn == emittedOn)
    {
#ifdef ESP_PLATFORM
        if (ledLocked)
            xSemaphoreGive(ledMutex);
#endif
        return;
    }

    uint8_t r = 0, g = 0, b = 0;
    if (emittedOn)
    {
        if (ota)
            b = level; // OTA: solid blue
        else if (injecting)
            g = level;
        else
            r = level;
    }
    appWriteStatusLed(r, g, b);

    lastInjecting = injecting;
    lastConnected = connected;
    lastOta = ota;
    lastLevel = level;
    lastEmittedOn = emittedOn;
    known = true;
#ifdef ESP_PLATFORM
    if (ledLocked)
        xSemaphoreGive(ledMutex);
#endif
}
#endif

#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD) && defined(DASH_INJECTION_TOGGLE_PIN)
static void appPollInjectionToggleButton()
{
    static bool rawState = HIGH;
    static bool stableState = HIGH;
    static unsigned long lastChangeMs = 0;

    bool sample = digitalRead(DASH_INJECTION_TOGGLE_PIN);
    unsigned long now = millis();

    if (sample != rawState)
    {
        rawState = sample;
        lastChangeMs = now;
    }

    if ((now - lastChangeMs) < 35 || sample == stableState)
        return;

    stableState = sample;
    if (stableState == LOW)
        dashToggleCanActive("GPIO41");
}
#endif

template <typename Driver>
static void appPrepare(std::unique_ptr<Driver> drv)
{
    delay(1500);
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 1000)
    {
    }
    if (!AppHandlerGuard::initialize())
    {
        Serial.println("Handler mutex allocation failed");
        for (;;)
            delay(1000);
    }
    appHandler = std::make_unique<SelectedHandler>();
    appActiveHandler = appHandler.get();

#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD) && defined(DASH_INJECTION_TOGGLE_PIN)
    pinMode(DASH_INJECTION_TOGGLE_PIN, INPUT_PULLUP);
#endif

#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD) && defined(DASH_RGB_STATUS_LED)
    appRefreshStatusLed(true);
#else
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);
#endif

    appDriver = std::move(drv);
    appDriver->allowSendFrame = appCanTransmitAllowed;
    appDriver->onSendFrame = appOnSendFrame;
#ifdef ESP_PLATFORM
    GvretSerial::setMonitorCallback(appSetCanMonitorAll);
    appSetCanMonitorAll(GvretSerial::clientConnected.load(std::memory_order_relaxed));
#endif
}

template <typename Driver>
static bool appStartDriver(const char *readyMsg)
{
    if (!appDriver)
        return false;
    bool canInitOk = appDriver->init();
    if (!canInitOk)
    {
        Serial.println("[WARN] CAN init failed; dashboard remains available and recovery will retry");
    }
    char canDiag[640];
    appDriver->diagnosticsSummary(canDiag, sizeof(canDiag));
    Serial.print("[CAN] ");
    Serial.println(canDiag);

    CarManagerBase *active = appGetActiveHandler();
    if (!active)
        active = appHandler.get();
    if (active)
        appDriver->setFilters(active->filterIds(), active->filterIdCount());
#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD)
    dashReapplyFiltersWithPlugins();
#endif
#ifdef ESP_PLATFORM
    if (appDriver->ready())
        RuntimeDiagnostics::noteCanInitialized();
#endif
    appDriver->diagnosticsSummary(canDiag, sizeof(canDiag));
    Serial.print("[CAN] after filters: ");
    Serial.println(canDiag);

    if constexpr (Driver::kSupportsISR)
    {
        appDriver->enableInterrupt(canISR);
    }

    Serial.println(readyMsg);

#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD)
    delay(2000);
#endif
    return canInitOk;
}

template <typename Driver>
static void appLoop()
{
#ifdef ESP_PLATFORM
    RuntimeDiagnostics::noteCanLoop();
#endif
    if (!appDriver)
    {
        delay(100);
        return;
    }
#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD) && defined(DASH_RGB_STATUS_LED)
    appRefreshStatusLed(false);
#endif
#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD)
    if (Update.isRunning())
    {
        delay(1);
        return;
    }

#if defined(DASH_INJECTION_TOGGLE_PIN)
    appPollInjectionToggleButton();
#endif
#endif

    if constexpr (Driver::kSupportsISR)
    {
        if (!frameReady)
            return;
        frameReady = false;
    }

    CanFrame frame;
    uint8_t framesThisLoop = 0;
    while (appDriver->read(frame))
    {
#ifdef ESP_PLATFORM
        RuntimeDiagnostics::noteCanInitialized();
        RuntimeDiagnostics::noteCanFrame();
#endif
        if (frame.bus == CAN_BUS_ANY)
            frame.bus = CAN_BUS_DEFAULT;
#if !(defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD) && defined(DASH_RGB_STATUS_LED))
        digitalWrite(PIN_LED, LOW);
#endif
        CanFrame original = frame;
#ifdef ESP_PLATFORM
        GvretSerial::broadcast(original);
#endif
        {
            AppHandlerGuard guard;
            CarManagerBase *h = appGetActiveHandler();
            if (!h)
                h = appHandler.get();
            if (h)
            {
                h->frameCount++;
                h->handleMessage(frame, *appDriver);
                if (appPluginProcess)
                    appPluginProcess(original, *appDriver);
            }
        }
#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD)
        if (++framesThisLoop >= 32)
        {
            yield();
            break;
        }
#endif
    }
#if !(defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD) && defined(DASH_RGB_STATUS_LED))
    digitalWrite(PIN_LED, HIGH);
#endif
}
