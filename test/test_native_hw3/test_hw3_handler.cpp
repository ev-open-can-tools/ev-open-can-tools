#include <unity.h>
#include "can_frame_types.h"
#include "drivers/can_driver.h"
#include "can_helpers.h"
#include "handlers.h"
#include "drivers/mock_driver.h"

static MockDriver mock;
static HW3Handler handler;

void setUp()
{
    mock.reset();
    handler = HW3Handler();
    handler.enablePrint = false;
    enhancedAutopilotRuntime = true;
}

void tearDown() {}

// --- Speed profile from follow distance (CAN ID 1016) ---

void test_hw3_follow_distance_1_sets_profile_2()
{
    CanFrame f = {.id = 1016};
    f.data[5] = 0b00100000; // followDistance = 1
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(2, handler.speedProfile);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_hw3_follow_distance_2_sets_profile_1()
{
    CanFrame f = {.id = 1016};
    f.data[5] = 0b01000000; // followDistance = 2
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(1, handler.speedProfile);
}

void test_hw3_follow_distance_3_sets_profile_0()
{
    CanFrame f = {.id = 1016};
    f.data[5] = 0b01100000; // followDistance = 3
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(0, handler.speedProfile);
}

void test_hw3_follow_distance_0_keeps_default()
{
    CanFrame f = {.id = 1016};
    f.data[5] = 0x00; // followDistance = 0
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(1, handler.speedProfile); // default
}

void test_hw3_follow_distance_profile_survives_mux0_without_injection()
{
    CanFrame followDistanceFrame = {.id = 1016};
    followDistanceFrame.data[5] = 0b00100000; // followDistance = 1 => profile 2
    handler.handleMessage(followDistanceFrame, mock);
    TEST_ASSERT_EQUAL_INT(2, handler.speedProfile);

    mock.reset();
    CanFrame autopilotFrame = {.id = 1021};
    autopilotFrame.data[0] = 0x00; // mux 0
    autopilotFrame.data[4] = 0x40; // AD selected
    autopilotFrame.data[3] = 60;   // speed offset = 0
    autopilotFrame.data[6] = 0x02;
    handler.handleMessage(autopilotFrame, mock);

    TEST_ASSERT_EQUAL_INT(0, handler.speedOffset);
    TEST_ASSERT_EQUAL_INT(2, handler.speedProfile);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x02, mock.sent[0].data[6] & 0x06);
}

// --- AD shadowing fix regression test ---

void test_hw3_AD_enabled_only_set_on_mux0()
{
    CanFrame f0 = {.id = 1021};
    f0.data[0] = 0x00; // mux 0
    f0.data[4] = 0x40; // AD selected
    handler.handleMessage(f0, mock);
    TEST_ASSERT_TRUE(handler.ADEnabled);

    mock.reset();
    CanFrame f2 = {.id = 1021};
    f2.data[0] = 0x02; // mux 2
    f2.data[4] = 0x00; // AD bit not set in this frame
    handler.handleMessage(f2, mock);
    TEST_ASSERT_TRUE(handler.ADEnabled);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_hw3_AD_disabled_on_mux0_prevents_mux2_send()
{
    // mux 0 with AD disabled
    CanFrame f0 = {.id = 1021};
    f0.data[0] = 0x00;
    f0.data[4] = 0x00; // AD NOT selected
    handler.handleMessage(f0, mock);
    TEST_ASSERT_FALSE(handler.ADEnabled);

    mock.reset();
    CanFrame f2 = {.id = 1021};
    f2.data[0] = 0x02;
    handler.handleMessage(f2, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// --- AD activation (mux 0) ---

void test_hw3_AD_mux0_sends_with_bit46()
{
    CanFrame f = {.id = 1021};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x40, mock.sent[0].data[5] & 0x40);
}

// --- Nag suppression (mux 1) ---

void test_hw3_nag_suppression_clears_bit19_on_mux1()
{
    CanFrame f = {.id = 1021};
    f.data[0] = 0x01;
    setBit(f, 19, true);
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_FALSE((mock.sent[0].data[2] >> 3) & 0x01);
}

void test_hw3_nag_suppression_skips_mux1_changes_when_eap_runtime_disabled()
{
    enhancedAutopilotRuntime = false;
    CanFrame f = {.id = 1021};
    f.data[0] = 0x01;
    setBit(f, 19, true);
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size()); // frame are not sent when runtime disabled
}
void test_hw3_mux1_sets_track_labels_bit46()
{
    CanFrame f = {.id = 1021};
    f.data[0] = 0x01;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x40, mock.sent[0].data[5] & 0x40); // bit 46
}

// --- Track mode request (CAN ID 787) ---

void test_hw3_track_mode_request_forces_on_and_updates_checksum()
{
    CanFrame f = {.id = 787, .dlc = 8};
    f.data[0] = 0xFE;
    f.data[1] = 0x10;
    f.data[2] = 0x20;
    f.data[3] = 0x04;
    f.data[6] = 0xA0;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0xFD, mock.sent[0].data[0]);
    TEST_ASSERT_EQUAL_HEX8(0xE7, mock.sent[0].data[7]);
}

// --- No sends on unrelated CAN IDs ---

void test_hw3_ignores_unrelated_can_id()
{
    CanFrame f = {.id = 999};
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_hw3_gw_autopilot_mux2_updates_state_without_send()
{
    CanFrame f = {.id = 2047};
    f.data[0] = 0x02;
    f.data[5] = 0x08; // ENHANCED = 2
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(2, handler.gatewayAutopilot);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// --- Send counts ---

void test_hw3_AD_enabled_mux0_sends_exactly_1()
{
    CanFrame f = {.id = 1021};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
}

void test_hw3_mux1_sends_exactly_1()
{
    CanFrame f = {.id = 1021};
    f.data[0] = 0x01;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
}

// --- Filter IDs ---

void test_hw3_filter_ids_count()
{
    TEST_ASSERT_EQUAL_UINT8(4, handler.filterIdCount());
}

void test_hw3_filter_ids_values()
{
    const uint32_t *ids = handler.filterIds();
    TEST_ASSERT_EQUAL_UINT32(787, ids[0]);
    TEST_ASSERT_EQUAL_UINT32(1016, ids[1]);
    TEST_ASSERT_EQUAL_UINT32(1021, ids[2]);
    TEST_ASSERT_EQUAL_UINT32(2047, ids[3]);
}

int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_hw3_filter_ids_count);
    RUN_TEST(test_hw3_filter_ids_values);

    RUN_TEST(test_hw3_follow_distance_1_sets_profile_2);
    RUN_TEST(test_hw3_follow_distance_2_sets_profile_1);
    RUN_TEST(test_hw3_follow_distance_3_sets_profile_0);
    RUN_TEST(test_hw3_follow_distance_0_keeps_default);
    RUN_TEST(test_hw3_follow_distance_profile_survives_mux0_without_injection);

    RUN_TEST(test_hw3_AD_enabled_only_set_on_mux0);
    RUN_TEST(test_hw3_AD_disabled_on_mux0_prevents_mux2_send);

    RUN_TEST(test_hw3_AD_mux0_sends_with_bit46);
    RUN_TEST(test_hw3_nag_suppression_clears_bit19_on_mux1);
    RUN_TEST(test_hw3_nag_suppression_skips_mux1_changes_when_eap_runtime_disabled);
    RUN_TEST(test_hw3_mux1_sets_track_labels_bit46);
    RUN_TEST(test_hw3_track_mode_request_forces_on_and_updates_checksum);
    RUN_TEST(test_hw3_ignores_unrelated_can_id);
    RUN_TEST(test_hw3_gw_autopilot_mux2_updates_state_without_send);

    RUN_TEST(test_hw3_AD_enabled_mux0_sends_exactly_1);
    RUN_TEST(test_hw3_mux1_sends_exactly_1);

    return UNITY_END();
}
