#pragma once

#include <memory>
#include "can_frame_types.h"
#include "drivers/can_driver.h"
#include "can_helpers.h"
#include "handlers.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
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

static volatile bool frameReady = true;
static void canISR() { frameReady = true; }

#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD)
#include "web/mcp2515_dashboard.h"
#endif

template <typename Driver>
static void appSetup(std::unique_ptr<Driver> drv, const char *readyMsg)
{
    appHandler = std::make_unique<SelectedHandler>();
    delay(1500);
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 1000)
    {
    }

    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);

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
#endif

    if constexpr (Driver::kSupportsISR)
    {
        if (!frameReady)
            return;
        frameReady = false;
    }

    CanFrame frame;
    CarManagerBase *h = appActiveHandler ? appActiveHandler : appHandler.get();
    while (appDriver->read(frame))
    {
        digitalWrite(PIN_LED, LOW);
        h->frameCount++;
        h->handleMessage(frame, *appDriver);
    }
    digitalWrite(PIN_LED, HIGH);
}
