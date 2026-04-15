/*
    PlatformIO entry point.
    Shared build settings live in platformio_profile.h.
    Logic is in the shared headers under include/.
*/

#include <Arduino.h>
#include "app.h"

#ifdef DRIVER_MCP2515
#include <SPI.h>
#include "drivers/mcp2515_driver.h"
#elif defined(DRIVER_ESP32_EXT_MCP2515)
#include <SPI.h>
#include "drivers/esp32_mcp2515_driver.h"
#elif defined(DRIVER_SAME51)
#include "drivers/same51_driver.h"
#elif defined(DRIVER_TWAI)
#include "drivers/twai_driver.h"
#include <Preferences.h>
#ifndef TWAI_TX_PIN
#define TWAI_TX_PIN GPIO_NUM_5
#endif
#ifndef TWAI_RX_PIN
#define TWAI_RX_PIN GPIO_NUM_4
#endif
#else
#error "Define DRIVER_MCP2515, DRIVER_ESP32_EXT_MCP2515, DRIVER_SAME51, or DRIVER_TWAI in build_flags"
#endif

void setup()
{
#ifdef DRIVER_MCP2515
    appSetup<MCP2515Driver>(std::make_unique<MCP2515Driver>(PIN_CAN_CS), "MCP25625 ready @ 500k");
#ifdef ESP32_DASHBOARD
    delay(2000);
    mcpDashboardSetup(appHandler.get(), appDriver.get());
    appHandler->onFrame = mcpDashOnFrame;
    appHandler->onSend = mcpDashOnSend;
#endif
#elif defined(DRIVER_ESP32_EXT_MCP2515)
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, PIN_CAN_CS);
    SPI.setFrequency(8000000);
    auto drv = std::make_unique<ESP32_MCP2515Driver>(PIN_CAN_CS);
    MCP2515 *mcpPtr = &drv->mcp();
    appSetup<ESP32_MCP2515Driver>(std::move(drv), "ESP32 + MCP2515 ready @ 500k");
#ifdef ESP32_DASHBOARD
    appHandler->onFrame = mcpDashOnFrame;
    appHandler->onSend = mcpDashOnSend;
    mcpDashboardSetup(appHandler.get(), appDriver.get(), mcpPtr);
#endif
#elif defined(DRIVER_SAME51)
    appSetup<SAME51Driver>(std::make_unique<SAME51Driver>(), "SAME51 CAN ready @ 500k");
#elif defined(DRIVER_TWAI)
    // Load TWAI pins from NVS (survives OTA); fall back to compile-time defaults
    gpio_num_t twaiTx = TWAI_TX_PIN;
    gpio_num_t twaiRx = TWAI_RX_PIN;
    {
        Preferences canPrefs;
        if (canPrefs.begin("can", true)) {
            int8_t tx = canPrefs.getChar("tx", -1);
            int8_t rx = canPrefs.getChar("rx", -1);
            canPrefs.end();
            if (tx >= 0 && tx <= 39) twaiTx = (gpio_num_t)tx;
            if (rx >= 0 && rx <= 39) twaiRx = (gpio_num_t)rx;
        }
    }
    appSetup<TWAIDriver>(std::make_unique<TWAIDriver>(twaiTx, twaiRx), "ESP32 TWAI ready @ 500k");
#ifdef ESP32_DASHBOARD
    delay(2000);
    mcpDashboardSetup(appHandler.get(), appDriver.get());
    appHandler->onFrame = mcpDashOnFrame;
    appHandler->onSend = mcpDashOnSend;
#endif
#endif
}

void loop()
{
#ifdef DRIVER_MCP2515
    appLoop<MCP2515Driver>();
#ifdef ESP32_DASHBOARD
    mcpDashboardLoop();
#endif
#elif defined(DRIVER_ESP32_EXT_MCP2515)
    appLoop<ESP32_MCP2515Driver>();
#ifdef ESP32_DASHBOARD
    mcpDashboardLoop();
#endif
#elif defined(DRIVER_SAME51)
    appLoop<SAME51Driver>();
#elif defined(DRIVER_TWAI)
    appLoop<TWAIDriver>();
#ifdef ESP32_DASHBOARD
    mcpDashboardLoop();
#endif
#endif
}
