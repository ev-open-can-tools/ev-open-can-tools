#include <unity.h>
#include "can_frame_types.h"
#include "drivers/can_driver.h"
#include "can_helpers.h"
#include "handlers.h"
#include "drivers/mock_driver.h"

static MockDriver mock;

void setUp()
{
    mock.reset();
    bypassTlsscRequirementRuntime = kBypassTlsscRequirementDefaultEnabled;
}
void tearDown() {}

// --- isADSelectedInUI always returns true under BYPASS_TLSSC_REQUIREMENT ---

void test_bypass_tlssc_helper_returns_true_when_bit_clear()
{
    CanFrame f = {};
    f.data[4] = 0x00; // AD bit NOT set
    TEST_ASSERT_TRUE(isADSelectedInUI(f));
}

void test_bypass_tlssc_helper_returns_true_when_bit_set()
{
    CanFrame f = {};
    f.data[4] = 0x40; // AD bit set
    TEST_ASSERT_TRUE(isADSelectedInUI(f));
}

// --- Legacy: sends on mux 0 even without AD toggle ---

void test_bypass_tlssc_legacy_sends_without_ui_toggle()
{
    LegacyHandler handler;
    handler.enablePrint = false;
    CanFrame f = {.id = 1006};
    f.data[0] = 0x00; // mux 0
    f.data[4] = 0x00; // AD bit NOT set in UI
    handler.handleMessage(f, mock);
    TEST_ASSERT_TRUE(handler.ADEnabled);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x40, mock.sent[0].data[5] & 0x40); // bit 46
}

// --- HW3: sends on mux 0 even without AD toggle ---

void test_bypass_tlssc_hw3_sends_without_ui_toggle()
{
    HW3Handler handler;
    handler.enablePrint = false;
    CanFrame f = {.id = 1021};
    f.data[0] = 0x00; // mux 0
    f.data[4] = 0x00; // AD bit NOT set in UI
    handler.handleMessage(f, mock);
    TEST_ASSERT_TRUE(handler.ADEnabled);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x40, mock.sent[0].data[5] & 0x40); // bit 46
}

void test_bypass_tlssc_hw3_mux2_stays_silent_without_ui_toggle()
{
    HW3Handler handler;
    handler.enablePrint = false;

    // First set ADEnabled via mux 0
    CanFrame f0 = {.id = 1021};
    f0.data[0] = 0x00;
    f0.data[4] = 0x00; // no UI toggle
    handler.handleMessage(f0, mock);

    mock.reset();
    CanFrame f2 = {.id = 1021};
    f2.data[0] = 0x02; // mux 2
    handler.handleMessage(f2, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// --- HW4: sends on mux 0 even without AD toggle ---

void test_bypass_tlssc_hw4_sends_without_ui_toggle()
{
    HW4Handler handler;
    handler.enablePrint = false;
    CanFrame f = {.id = 1021};
    f.data[0] = 0x00; // mux 0
    f.data[4] = 0x00; // AD bit NOT set in UI
    handler.handleMessage(f, mock);
    TEST_ASSERT_TRUE(handler.ADEnabled);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x40, mock.sent[0].data[5] & 0x40); // bit 46
    TEST_ASSERT_EQUAL_HEX8(0x10, mock.sent[0].data[7] & 0x10); // bit 60
}

int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_bypass_tlssc_helper_returns_true_when_bit_clear);
    RUN_TEST(test_bypass_tlssc_helper_returns_true_when_bit_set);

    RUN_TEST(test_bypass_tlssc_legacy_sends_without_ui_toggle);
    RUN_TEST(test_bypass_tlssc_hw3_sends_without_ui_toggle);
    RUN_TEST(test_bypass_tlssc_hw3_mux2_stays_silent_without_ui_toggle);
    RUN_TEST(test_bypass_tlssc_hw4_sends_without_ui_toggle);

    return UNITY_END();
}
