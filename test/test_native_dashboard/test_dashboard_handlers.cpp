#include <unity.h>
#include "can_frame_types.h"
#include "can_helpers.h"
#include "drivers/mock_driver.h"
#include "handlers.h"

static MockDriver mock;
static uint8_t onSendCount = 0;

static void countOnSend(uint8_t /*mux*/, bool /*ok*/)
{
    onSendCount++;
}

template <typename Handler>
static void prepareDashboardHandler(Handler &handler)
{
    handler.enablePrint = false;
    handler.onSend = countOnSend;
}

void setUp()
{
    mock.reset();
    onSendCount = 0;
    bypassTlsscRequirementRuntime = true;
    isaSpeedChimeSuppressRuntime = true;
    emergencyVehicleDetectionRuntime = true;
    enhancedAutopilotRuntime = true;
    nagKillerRuntime = true;
}

void tearDown() {}

void test_dashboard_legacy_mux0_observes_ad_without_injecting()
{
    LegacyHandler handler;
    prepareDashboardHandler(handler);

    CanFrame f = {.id = 1006};
    f.data[0] = 0x00;
    f.data[4] = 0x40;

    handler.handleMessage(f, mock);

    TEST_ASSERT_TRUE(handler.ADEnabled);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(0, handler.framesSent);
    TEST_ASSERT_EQUAL_UINT8(0, onSendCount);
    TEST_ASSERT_EQUAL_HEX8(0x00, f.data[5] & 0x40);
}

void test_dashboard_legacy_mux1_does_not_inject_nag_suppression()
{
    LegacyHandler handler;
    prepareDashboardHandler(handler);

    CanFrame f = {.id = 1006};
    f.data[0] = 0x01;
    setBit(f, 19, true);

    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(0, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(0, handler.framesSent);
    TEST_ASSERT_EQUAL_UINT8(0, onSendCount);
    TEST_ASSERT_TRUE((f.data[2] >> 3) & 0x01);
}

void test_dashboard_hw3_mux0_observes_state_without_injecting()
{
    HW3Handler handler;
    prepareDashboardHandler(handler);

    CanFrame f = {.id = 1021};
    f.data[0] = 0x00;
    f.data[3] = 60;
    f.data[4] = 0x40;

    handler.handleMessage(f, mock);

    TEST_ASSERT_TRUE(handler.ADEnabled);
    TEST_ASSERT_EQUAL_INT(0, handler.speedOffset);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(0, handler.framesSent);
    TEST_ASSERT_EQUAL_UINT8(0, onSendCount);
    TEST_ASSERT_EQUAL_HEX8(0x00, f.data[5] & 0x40);
}

void test_dashboard_hw3_mux1_does_not_inject_nag_suppression()
{
    HW3Handler handler;
    prepareDashboardHandler(handler);

    CanFrame f = {.id = 1021};
    f.data[0] = 0x01;
    setBit(f, 19, true);

    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(0, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(0, handler.framesSent);
    TEST_ASSERT_EQUAL_UINT8(0, onSendCount);
    TEST_ASSERT_TRUE((f.data[2] >> 3) & 0x01);
}

void test_dashboard_hw4_mux0_observes_ad_without_injecting()
{
    HW4Handler handler;
    prepareDashboardHandler(handler);

    CanFrame f = {.id = 1021};
    f.data[0] = 0x00;
    f.data[4] = 0x40;

    handler.handleMessage(f, mock);

    TEST_ASSERT_TRUE(handler.ADEnabled);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(0, handler.framesSent);
    TEST_ASSERT_EQUAL_UINT8(0, onSendCount);
    TEST_ASSERT_EQUAL_HEX8(0x00, f.data[5] & 0x40);
    TEST_ASSERT_EQUAL_HEX8(0x00, f.data[7] & 0x18);
}

void test_dashboard_hw4_mux1_does_not_inject_nag_suppression()
{
    HW4Handler handler;
    prepareDashboardHandler(handler);

    CanFrame f = {.id = 1021};
    f.data[0] = 0x01;
    setBit(f, 19, true);

    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(0, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(0, handler.framesSent);
    TEST_ASSERT_EQUAL_UINT8(0, onSendCount);
    TEST_ASSERT_TRUE((f.data[2] >> 3) & 0x01);
    TEST_ASSERT_EQUAL_HEX8(0x00, f.data[5] & 0x80);
}

void test_dashboard_hw4_isa_suppression_does_not_inject()
{
    HW4Handler handler;
    prepareDashboardHandler(handler);

    CanFrame f = {.id = 921};
    f.data[1] = 0x00;

    handler.handleMessage(f, mock);

    TEST_ASSERT_EQUAL(0, mock.sent.size());
    TEST_ASSERT_EQUAL_UINT32(0, handler.framesSent);
    TEST_ASSERT_EQUAL_UINT8(0, onSendCount);
    TEST_ASSERT_EQUAL_HEX8(0x00, f.data[1] & 0x20);
}

int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_dashboard_legacy_mux0_observes_ad_without_injecting);
    RUN_TEST(test_dashboard_legacy_mux1_does_not_inject_nag_suppression);
    RUN_TEST(test_dashboard_hw3_mux0_observes_state_without_injecting);
    RUN_TEST(test_dashboard_hw3_mux1_does_not_inject_nag_suppression);
    RUN_TEST(test_dashboard_hw4_mux0_observes_ad_without_injecting);
    RUN_TEST(test_dashboard_hw4_mux1_does_not_inject_nag_suppression);
    RUN_TEST(test_dashboard_hw4_isa_suppression_does_not_inject);

    return UNITY_END();
}
