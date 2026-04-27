#include <unity.h>
#include "can_frame_types.h"
#include "can_helpers.h"
#include "drivers/mock_driver.h"
#include "handlers.h"

static MockDriver mock;

void setUp()
{
    mock.reset();
    enhancedAutopilotRuntime = true;
}

void tearDown() {}

static CanFrame hw3Mux1Frame()
{
    CanFrame f = {.id = 1021};
    f.data[0] = 0x01;
    setBit(f, 19, true);
    return f;
}

static CanFrame hw4Mux1Frame()
{
    CanFrame f = {.id = 1021};
    f.data[0] = 0x01;
    setBit(f, 19, true);
    return f;
}

static CanFrame gearFrame(uint8_t gear)
{
    CanFrame f = {.id = 390};
    f.dlc = 8;
    f.data[7] = static_cast<uint8_t>(gear << 3);
    return f;
}

static void activateAp(CarManagerBase &handler)
{
    CanFrame f = {.id = 921};
    f.data[0] = 0x03; // ACTIVE_1
    handler.handleMessage(f, mock);
    TEST_ASSERT_TRUE(handler.APActive);
    mock.reset();
}

void test_hw3_enhanced_autopilot_waits_for_ap_before_mux1_injection()
{
    HW3Handler handler;
    handler.enablePrint = false;

    CanFrame beforeAp = hw3Mux1Frame();
    handler.handleMessage(beforeAp, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());

    CanFrame observedUiConfig = {.id = 1021};
    observedUiConfig.data[0] = 0x00;
    observedUiConfig.data[4] = 0x20;
    handler.handleMessage(observedUiConfig, mock);
    TEST_ASSERT_TRUE(handler.ADEnabled);
    TEST_ASSERT_FALSE(handler.APActive);
    mock.reset();

    CanFrame stillBeforeAp = hw3Mux1Frame();
    handler.handleMessage(stillBeforeAp, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());

    activateAp(handler);

    CanFrame afterAp = hw3Mux1Frame();
    handler.handleMessage(afterAp, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_FALSE((mock.sent[0].data[2] >> 3) & 0x01);
    TEST_ASSERT_EQUAL_HEX8(0x40, mock.sent[0].data[5] & 0x40);
}

void test_hw3_enhanced_autopilot_allows_mux1_injection_while_parked()
{
    HW3Handler handler;
    handler.enablePrint = false;

    CanFrame park = gearFrame(1);
    handler.handleMessage(park, mock);
    TEST_ASSERT_TRUE(handler.Parked);
    TEST_ASSERT_FALSE(handler.APActive);

    CanFrame whileParked = hw3Mux1Frame();
    handler.handleMessage(whileParked, mock);

    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_FALSE((mock.sent[0].data[2] >> 3) & 0x01);
    TEST_ASSERT_EQUAL_HEX8(0x40, mock.sent[0].data[5] & 0x40);
}

void test_hw3_enhanced_autopilot_stops_mux1_injection_when_shifted_to_drive()
{
    HW3Handler handler;
    handler.enablePrint = false;

    CanFrame park = gearFrame(1);
    handler.handleMessage(park, mock);
    CanFrame whileParked = hw3Mux1Frame();
    handler.handleMessage(whileParked, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    mock.reset();

    CanFrame drive = gearFrame(4);
    handler.handleMessage(drive, mock);
    TEST_ASSERT_FALSE(handler.Parked);

    CanFrame whileDriving = hw3Mux1Frame();
    handler.handleMessage(whileDriving, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_hw4_enhanced_autopilot_waits_for_ap_before_mux1_injection()
{
    HW4Handler handler;
    handler.enablePrint = false;

    CanFrame beforeAp = hw4Mux1Frame();
    handler.handleMessage(beforeAp, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());

    CanFrame observedUiConfig = {.id = 1021};
    observedUiConfig.data[0] = 0x00;
    observedUiConfig.data[4] = 0x20;
    handler.handleMessage(observedUiConfig, mock);
    TEST_ASSERT_TRUE(handler.ADEnabled);
    TEST_ASSERT_FALSE(handler.APActive);
    mock.reset();

    CanFrame stillBeforeAp = hw4Mux1Frame();
    handler.handleMessage(stillBeforeAp, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());

    activateAp(handler);

    CanFrame afterAp = hw4Mux1Frame();
    handler.handleMessage(afterAp, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_FALSE((mock.sent[0].data[2] >> 3) & 0x01);
    TEST_ASSERT_EQUAL_HEX8(0x80, mock.sent[0].data[5] & 0x80);
}

void test_hw4_enhanced_autopilot_allows_mux1_injection_while_parked()
{
    HW4Handler handler;
    handler.enablePrint = false;

    CanFrame park = gearFrame(1);
    handler.handleMessage(park, mock);
    TEST_ASSERT_TRUE(handler.Parked);
    TEST_ASSERT_FALSE(handler.APActive);

    CanFrame whileParked = hw4Mux1Frame();
    handler.handleMessage(whileParked, mock);

    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_FALSE((mock.sent[0].data[2] >> 3) & 0x01);
    TEST_ASSERT_EQUAL_HEX8(0x80, mock.sent[0].data[5] & 0x80);
}

void test_hw4_enhanced_autopilot_stops_mux1_injection_when_shifted_to_drive()
{
    HW4Handler handler;
    handler.enablePrint = false;

    CanFrame park = gearFrame(1);
    handler.handleMessage(park, mock);
    CanFrame whileParked = hw4Mux1Frame();
    handler.handleMessage(whileParked, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    mock.reset();

    CanFrame drive = gearFrame(4);
    handler.handleMessage(drive, mock);
    TEST_ASSERT_FALSE(handler.Parked);

    CanFrame whileDriving = hw4Mux1Frame();
    handler.handleMessage(whileDriving, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_hw3_enhanced_autopilot_waits_for_ap_before_mux1_injection);
    RUN_TEST(test_hw3_enhanced_autopilot_allows_mux1_injection_while_parked);
    RUN_TEST(test_hw3_enhanced_autopilot_stops_mux1_injection_when_shifted_to_drive);
    RUN_TEST(test_hw4_enhanced_autopilot_waits_for_ap_before_mux1_injection);
    RUN_TEST(test_hw4_enhanced_autopilot_allows_mux1_injection_while_parked);
    RUN_TEST(test_hw4_enhanced_autopilot_stops_mux1_injection_when_shifted_to_drive);

    return UNITY_END();
}
