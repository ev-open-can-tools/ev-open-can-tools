#pragma once

#include <memory>
#include "can_frame_types.h"
#include "drivers/can_driver.h"
#include "can_helpers.h"
#include "handlers.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif
#if defined(DASH_RGB_STATUS_LED) && !defined(NATIVE_BUILD)
#include <esp32-hal-rgb-led.h>
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
static CarManagerBase *appActiveHandler = nullptr;

// Plugin processing hook — set by dashboard to apply plugin rules after handler
static void (*appPluginProcess)(const CanFrame &, CanDriver &) = nullptr;

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
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    rgbLedWrite(PIN_LED, red, green, blue);
#else
    neopixelWrite(PIN_LED, red, green, blue);
#endif
}

static void appRefreshStatusLed(bool force)
{
    static bool known = false;
    static bool lastInjecting = false;
#ifdef RGB_BRIGHTNESS
    constexpr uint8_t kStatusLedLevel = RGB_BRIGHTNESS;
#else
    constexpr uint8_t kStatusLedLevel = 32;
#endif

    bool injecting = canActive;
    if (!force && known && lastInjecting == injecting)
        return;

    appWriteStatusLed(injecting ? 0 : kStatusLedLevel, injecting ? kStatusLedLevel : 0, 0);
    lastInjecting = injecting;
    known = true;
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
static void appSetup(std::unique_ptr<Driver> drv, const char *readyMsg)
{
    appHandler = std::make_unique<SelectedHandler>();
    appActiveHandler = appHandler.get();
    delay(1500);
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 1000)
    {
    }

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
    if (!appDriver->init())
    {
        Serial.println("CAN init failed");
    }

    appDriver->setFilters(appHandler->filterIds(), appHandler->filterIdCount());
    if constexpr (Driver::kSupportsISR)
    {
        appDriver->enableInterrupt(canISR);
    }

    Serial.println(readyMsg);

#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD)
    delay(2000);
#endif
}

template <typename Driver>
static void appLoop()
{
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
    CarManagerBase *h = appActiveHandler ? appActiveHandler : appHandler.get();
    uint8_t framesThisLoop = 0;
    while (appDriver->read(frame))
    {
#if !(defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD) && defined(DASH_RGB_STATUS_LED))
        digitalWrite(PIN_LED, LOW);
#endif
        h->frameCount++;
        CanFrame original = frame;
        h->handleMessage(frame, *appDriver);
        if (appPluginProcess)
            appPluginProcess(original, *appDriver);
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
