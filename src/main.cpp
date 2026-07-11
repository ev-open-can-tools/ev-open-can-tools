/*
    PlatformIO entry point.
    Shared build settings live in platformio_profile.h.
    Logic is in the shared headers under include/.
*/

#ifdef ESP_PLATFORM
#include "platform/espidf_runtime.h"
#include <exception>
#else
#include <Arduino.h>
#endif
#include "app.h"

#ifdef DRIVER_MCP2515
#include <SPI.h>
#include "drivers/mcp2515_driver.h"
#elif defined(DRIVER_ESP32_EXT_MCP2515)
#ifndef ESP_PLATFORM
#include <SPI.h>
#endif
#include "drivers/esp32_mcp2515_driver.h"
#elif defined(DRIVER_SAME51)
#include "drivers/same51_driver.h"
#elif defined(DRIVER_TWAI)
#include "drivers/twai_driver.h"
#ifndef ESP_PLATFORM
#include <Preferences.h>
#endif
#ifndef TWAI_TX_PIN
#define TWAI_TX_PIN GPIO_NUM_5
#endif
#ifndef TWAI_RX_PIN
#define TWAI_RX_PIN GPIO_NUM_4
#endif
#else
#error "Define DRIVER_MCP2515, DRIVER_ESP32_EXT_MCP2515, DRIVER_SAME51, or DRIVER_TWAI in build_flags"
#endif

#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD)
static constexpr unsigned long kFsdActivationSettleMs = 2000;
static unsigned long fsdActivationApStartedMs = 0;
static bool fsdActivationLastAp = false;

static void appGatedDashboardPluginProcess(const CanFrame &frame, CanDriver &driver)
{
    static uint8_t previousHardwareMode = 0xFF;
    CarManagerBase *handler = appGetActiveHandler();
    if (!handler)
        handler = appHandler.get();
    bool apActive = handler && (bool)handler->APActive;
    unsigned long now = millis();

    // Detect live mode changes in the injection decision path. Configuration
    // persistence must not consume this transition before the CAN path sees
    // it. Reset cached periodic frames/counters and AP settle sequencing.
    if (previousHardwareMode != static_cast<uint8_t>(hwMode))
    {
        previousHardwareMode = static_cast<uint8_t>(hwMode);
        pluginResetPeriodicEmit();
        fsdActivationApStartedMs = 0;
        fsdActivationLastAp = false;
    }

    if (apActive != fsdActivationLastAp)
    {
        fsdActivationApStartedMs = apActive ? now : 0;
        fsdActivationLastAp = apActive;
    }

    bool legacyFsdActivation = frame.id == 0x3EE && frame.dlc > 0 && readMuxID(frame) == 0;
    if (apInjectionGate && legacyFsdActivation)
    {
        bool apStable = apActive &&
                        now - fsdActivationApStartedMs >= kFsdActivationSettleMs;
        if (!apStable)
            return;
    }

    dashPluginProcess(frame, driver);
}
#endif

static void app_main_setup()
{
#ifdef DRIVER_MCP2515
    appPrepare<MCP2515Driver>(std::make_unique<MCP2515Driver>(PIN_CAN_CS));
#ifdef ESP32_DASHBOARD
    mcpDashboardSetup(appHandler.get(), appDriver.get());
#endif
    appStartDriver<MCP2515Driver>("MCP25625 ready @ 500k");
#elif defined(DRIVER_ESP32_EXT_MCP2515)
#ifndef ESP_PLATFORM
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, PIN_CAN_CS);
    SPI.setFrequency(8000000);
#endif
    auto drv = std::make_unique<ESP32_MCP2515Driver>(PIN_CAN_CS);
    ESP32_MCP2515Driver *mcpDriver = drv.get();
    appPrepare<ESP32_MCP2515Driver>(std::move(drv));
#ifdef ESP32_DASHBOARD
    mcpDashboardSetup(appHandler.get(), appDriver.get(), mcpDriver);
#endif
    appStartDriver<ESP32_MCP2515Driver>("ESP32 + MCP2515 ready @ 500k");
#elif defined(DRIVER_SAME51)
    appPrepare<SAME51Driver>(std::make_unique<SAME51Driver>());
    appStartDriver<SAME51Driver>("SAME51 CAN ready @ 500k");
#elif defined(DRIVER_TWAI)
    // Load TWAI pins from NVS (survives OTA); fall back to compile-time defaults
    gpio_num_t twaiTx = TWAI_TX_PIN;
    gpio_num_t twaiRx = TWAI_RX_PIN;
    {
        Preferences canPrefs;
        if (canPrefs.begin("can", false))
        {
            int8_t tx = canPrefs.getChar("tx", -1);
            int8_t rx = canPrefs.getChar("rx", -1);
            canPrefs.end();
            if (tx >= 0 && GPIO_IS_VALID_OUTPUT_GPIO(tx))
                twaiTx = (gpio_num_t)tx;
            if (rx >= 0 && GPIO_IS_VALID_GPIO(rx))
                twaiRx = (gpio_num_t)rx;
        }
    }
    appPrepare<TWAIDriver>(std::make_unique<TWAIDriver>(twaiTx, twaiRx));
#ifdef ESP32_DASHBOARD
    mcpDashboardSetup(appHandler.get(), appDriver.get());
#endif
#ifdef ESP_PLATFORM
    Serial.printf("[BOOT] Driver-wake delay: %lu ms before TWAI initialization\n",
                  DRIVER_WAKE_DELAY_MS);
    delay(DRIVER_WAKE_DELAY_MS);
#endif
    appStartDriver<TWAIDriver>("ESP32 TWAI ready @ 500k");
#endif

#if defined(ESP32_DASHBOARD) && !defined(NATIVE_BUILD)
    appPluginProcess = appGatedDashboardPluginProcess;
#endif
}

static void app_main_loop()
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

#ifdef ESP_PLATFORM
extern "C" void app_main(void)
{
    Serial.begin(115200);
    delay(50);
    RuntimeDiagnostics::begin();
    if (!GvretSerial::begin())
        Serial.println("[WARN] GVRET serial task failed to start");

    esp_err_t nvsErr = nvs_flash_init();
    if (nvsErr == ESP_ERR_NVS_NO_FREE_PAGES || nvsErr == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvsErr = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvsErr);

    app_main_setup();
    while (true)
    {
        RuntimeDiagnostics::noteMainLoop();
        try
        {
            app_main_loop();
        }
        catch (const std::bad_alloc &)
        {
            Serial.println("[ERR] Main loop out of memory");
            delay(100);
        }
        catch (const std::exception &)
        {
            Serial.println("[ERR] Main loop failure");
            delay(100);
        }
        catch (...)
        {
            Serial.println("[ERR] Main loop failure");
            delay(100);
        }
        RuntimeDiagnostics::logHeartbeat(appDriver.get());
        yield();
    }
}
#else
void setup()
{
    app_main_setup();
}

void loop()
{
    app_main_loop();
}
#endif
