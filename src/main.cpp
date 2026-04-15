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

#if defined(BENCH_LOOPBACK_SELFTEST) && defined(DRIVER_ESP32_EXT_MCP2515)
static bool benchLoopbackPassed = false;
static bool benchLoopbackFinished = false;

static void runBenchLoopbackSelfTest()
{
    Serial.println();
    Serial.println("=== MCP2515 Bench Loopback Self-Test ===");
    Serial.println("This checks ESP32 <-> MCP2515 SPI wiring only.");
    Serial.println("CAN-H and CAN-L should remain unconnected for this test.");

    // Reuse the same SPI/pin defines as the normal external MCP2515 driver.
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, PIN_CAN_CS);
    SPI.setFrequency(8000000);

    MCP2515 mcp(PIN_CAN_CS);
    mcp.reset();

    MCP2515::ERROR bitrateResult = mcp.setBitrate(CAN_500KBPS, MCP_CRYSTAL_FREQ);
    if (bitrateResult != MCP2515::ERROR_OK)
    {
        Serial.printf("FAIL: setBitrate() returned %d\n", static_cast<int>(bitrateResult));
        Serial.println("Likely causes: wrong SPI wiring, wrong CS pin, wrong crystal frequency.");
        benchLoopbackFinished = true;
        return;
    }

    MCP2515::ERROR modeResult = mcp.setLoopbackMode();
    if (modeResult != MCP2515::ERROR_OK)
    {
        Serial.printf("FAIL: setLoopbackMode() returned %d\n", static_cast<int>(modeResult));
        benchLoopbackFinished = true;
        return;
    }

    // Push one known frame through MCP2515 loopback mode and make sure it
    // comes back unchanged. This is a controller/SPI sanity check only.
    can_frame tx = {};
    tx.can_id = 0x123;
    tx.can_dlc = 8;
    tx.data[0] = 0xDE;
    tx.data[1] = 0xAD;
    tx.data[2] = 0xBE;
    tx.data[3] = 0xEF;
    tx.data[4] = 0x12;
    tx.data[5] = 0x34;
    tx.data[6] = 0x56;
    tx.data[7] = 0x78;

    MCP2515::ERROR sendResult = mcp.sendMessage(&tx);
    if (sendResult != MCP2515::ERROR_OK)
    {
        Serial.printf("FAIL: sendMessage() returned %d\n", static_cast<int>(sendResult));
        benchLoopbackFinished = true;
        return;
    }

    can_frame rx = {};
    bool gotFrame = false;
    unsigned long deadline = millis() + 1000;
    while (millis() < deadline)
    {
        if (mcp.readMessage(&rx) == MCP2515::ERROR_OK)
        {
            gotFrame = true;
            break;
        }
        delay(10);
    }

    if (!gotFrame)
    {
        Serial.println("FAIL: no loopback frame received within 1 second.");
        Serial.println("Likely causes: MCP2515 not responding correctly or wrong crystal setting.");
        benchLoopbackFinished = true;
        return;
    }

    bool frameMatches = rx.can_id == tx.can_id &&
                        rx.can_dlc == tx.can_dlc &&
                        memcmp(rx.data, tx.data, tx.can_dlc) == 0;

    if (!frameMatches)
    {
        Serial.printf("FAIL: loopback frame mismatch. RX id=0x%03lX dlc=%u\n",
                      static_cast<unsigned long>(rx.can_id),
                      static_cast<unsigned int>(rx.can_dlc));
        benchLoopbackFinished = true;
        return;
    }

    Serial.println("PASS: MCP2515 loopback succeeded.");
    Serial.println("SPI wiring and MCP2515 controller config look sane.");
    Serial.println("This does NOT verify CAN-H/CAN-L bus wiring or transceiver behavior.");
    benchLoopbackPassed = true;
    benchLoopbackFinished = true;
}
#endif

void setup()
{
#if defined(BENCH_LOOPBACK_SELFTEST) && defined(DRIVER_ESP32_EXT_MCP2515)
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 1000)
    {
    }
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);
    delay(300);
    runBenchLoopbackSelfTest();
    digitalWrite(PIN_LED, benchLoopbackPassed ? LOW : HIGH);
#else
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
#endif
}

void loop()
{
#if defined(BENCH_LOOPBACK_SELFTEST) && defined(DRIVER_ESP32_EXT_MCP2515)
    if (benchLoopbackFinished)
    {
        // PASS = solid LED, FAIL = blink.
        digitalWrite(PIN_LED, benchLoopbackPassed ? LOW : ((millis() / 250) % 2 ? LOW : HIGH));
        delay(50);
        return;
    }
#endif
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
