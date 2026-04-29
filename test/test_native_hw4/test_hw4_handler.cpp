#include <unity.h>
#include "can_frame_types.h"
#include "drivers/can_driver.h"
#include "can_helpers.h"
#include "handlers.h"
#include "drivers/mock_driver.h"

static MockDriver mock;
static HW4Handler handler;

static bool denyInjection()
{
    return false;
}

void setUp()
{
    mock.reset();
    handler = HW4Handler();
    handler.enablePrint = false;
    bypassTlsscRequirementRuntime = kBypassTlsscRequirementDefaultEnabled;
    isaSpeedChimeSuppressRuntime = kIsaSpeedChimeSuppressDefaultEnabled;
    emergencyVehicleDetectionRuntime = kEmergencyVehicleDetectionDefaultEnabled;
    enhancedAutopilotRuntime = true;
}

void tearDown() {}

// --- Speed profile from follow distance (CAN ID 1016) ---

void test_hw4_follow_distance_1_sets_profile_3()
{
    CanFrame f = {.id = 1016};
    f.data[5] = 0b00100000; // fd = 1
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(3, handler.speedProfile);
}

void test_hw4_follow_distance_2_sets_profile_2()
{
    CanFrame f = {.id = 1016};
    f.data[5] = 0b01000000; // fd = 2
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(2, handler.speedProfile);
}

void test_hw4_follow_distance_3_sets_profile_1()
{
    CanFrame f = {.id = 1016};
    f.data[5] = 0b01100000; // fd = 3
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(1, handler.speedProfile);
}

void test_hw4_follow_distance_4_sets_profile_0()
{
    CanFrame f = {.id = 1016};
    f.data[5] = 0b10000000; // fd = 4
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(0, handler.speedProfile);
}

void test_hw4_follow_distance_5_sets_profile_4()
{
    CanFrame f = {.id = 1016};
    f.data[5] = 0b10100000; // fd = 5
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(4, handler.speedProfile);
}

// --- AD shadowing fix regression test ---

void test_hw4_AD_enabled_only_set_on_mux0()
{
    CanFrame f0 = {.id = 1021};
    f0.data[0] = 0x00;
    f0.data[4] = 0x40;
    handler.handleMessage(f0, mock);
    TEST_ASSERT_TRUE(handler.ADEnabled);

    mock.reset();
    CanFrame f2 = {.id = 1021};
    f2.data[0] = 0x02;
    f2.data[4] = 0x00; // AD bit not set in mux 2
    handler.handleMessage(f2, mock);
    TEST_ASSERT_TRUE(handler.ADEnabled);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// --- AD activation (mux 0) ---

void test_hw4_AD_mux0_sets_bits_46_and_60()
{
    CanFrame f = {.id = 1021};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x40, mock.sent[0].data[5] & 0x40); // bit 46
    TEST_ASSERT_EQUAL_HEX8(0x10, mock.sent[0].data[7] & 0x10); // bit 60
}

void test_hw4_AD_mux0_sets_emergency_bit59()
{
    CanFrame f = {.id = 1021};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x08, mock.sent[0].data[7] & 0x08); // bit 59
}

void test_hw4_AD_mux0_skips_emergency_bit59_when_runtime_disabled()
{
    emergencyVehicleDetectionRuntime = false;
    CanFrame f = {.id = 1021};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x00, mock.sent[0].data[7] & 0x08);
}

void test_hw4_no_send_when_AD_disabled_mux0()
{
    CanFrame f = {.id = 1021};
    f.data[0] = 0x00;
    f.data[4] = 0x00;
    handler.handleMessage(f, mock);
    TEST_ASSERT_FALSE(handler.ADEnabled);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_hw4_checkAD_blocks_mux0_and_mux2_send()
{
    handler.checkAD = denyInjection;

    CanFrame f0 = {.id = 1021};
    f0.data[0] = 0x00;
    f0.data[4] = 0x40;
    handler.handleMessage(f0, mock);
    TEST_ASSERT_FALSE(handler.ADEnabled);
    TEST_ASSERT_EQUAL(0, mock.sent.size());

    mock.reset();
    handler.ADEnabled = true;
    CanFrame f2 = {.id = 1021};
    f2.data[0] = 0x02;
    handler.handleMessage(f2, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// --- Nag suppression (mux 1) ---

void test_hw4_nag_suppression_clears_bit19_sets_bit47()
{
    CanFrame f = {.id = 1021};
    f.data[0] = 0x01;
    setBit(f, 19, true);
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_FALSE((mock.sent[0].data[2] >> 3) & 0x01);     // bit 19 cleared
    TEST_ASSERT_EQUAL_HEX8(0x80, mock.sent[0].data[5] & 0x80); // bit 47 set
}

void test_hw4_nag_suppression_skips_mux1_changes_when_eap_runtime_disabled()
{
    enhancedAutopilotRuntime = false;
    CanFrame f = {.id = 1021};
    f.data[0] = 0x01;
    setBit(f, 19, true);
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size()); // frame are not sent when runtime disabled
}

// --- Profile is observed but not injected (mux 2 stays silent) ---
void test_hw4_mux2_does_not_inject_speed_profile()
{
    handler.speedProfile = 3;
    CanFrame f0 = {.id = 1021};
    f0.data[0] = 0x00;
    f0.data[4] = 0x40;
    handler.handleMessage(f0, mock);
    mock.reset();
    CanFrame f = {.id = 1021};
    f.data[0] = 0x02;
    f.data[7] = 0x00;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_hw4_mux2_preserves_old_profile_bits_by_not_sending()
{
    handler.speedProfile = 0;
    CanFrame f0 = {.id = 1021};
    f0.data[0] = 0x00;
    f0.data[4] = 0x40;
    handler.handleMessage(f0, mock);
    mock.reset();
    CanFrame f = {.id = 1021};
    f.data[0] = 0x02;
    f.data[7] = 0x70; // old profile bits all set
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}
// --- Send counts ---

void test_hw4_mux0_AD_enabled_sends_1()
{
    CanFrame f = {.id = 1021};
    f.data[0] = 0x00;
    f.data[4] = 0x40;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
}

void test_hw4_mux1_sends_1()
{
    CanFrame f = {.id = 1021};
    f.data[0] = 0x01;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
}

void test_hw4_mux2_sends_0()
{
    CanFrame f0 = {.id = 1021};
    f0.data[0] = 0x00;
    f0.data[4] = 0x40;
    handler.handleMessage(f0, mock);
    mock.reset();
    CanFrame f = {.id = 1021};
    f.data[0] = 0x02;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}
void test_hw4_ignores_unrelated_can_id()
{
    CanFrame f = {.id = 999};
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// --- ISA speed chime suppression (CAN ID 921) ---

void test_hw4_isa_suppress_sets_bit5_of_data1()
{
    CanFrame f = {.id = 921};
    f.data[1] = 0x00;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x20, mock.sent[0].data[1] & 0x20);
}

void test_hw4_isa_suppress_preserves_existing_data1_bits()
{
    CanFrame f = {.id = 921};
    f.data[1] = 0xC3;
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_HEX8(0xE3, mock.sent[0].data[1]); // 0xC3 | 0x20
}

void test_hw4_isa_suppress_checksum_correct()
{
    CanFrame f = {.id = 921};
    f.data[0] = 0x10;
    f.data[1] = 0x05;
    f.data[2] = 0x00;
    f.data[3] = 0x00;
    f.data[4] = 0x00;
    f.data[5] = 0x00;
    f.data[6] = 0x00;
    handler.handleMessage(f, mock);
    // After OR: data[1] = 0x25
    // sum of data[0..6] = 0x10 + 0x25 = 0x35
    // sum += (921 & 0xFF) + (921 >> 8) = 0x99 + 0x03 = 0x9C
    // total = 0x35 + 0x9C = 0xD1
    TEST_ASSERT_EQUAL_HEX8(0xD1, mock.sent[0].data[7]);
}

void test_hw4_isa_suppress_returns_early_no_further_processing()
{
    handler.ADEnabled = true;
    CanFrame f = {.id = 921};
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size()); // only the ISA send, not any AD logic
}

void test_hw4_isa_suppress_runtime_off_skips_send()
{
    isaSpeedChimeSuppressRuntime = false;
    CanFrame f = {.id = 921};
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_hw4_gw_autopilot_mux2_updates_state_without_send()
{
    CanFrame f = {.id = 2047};
    f.data[0] = 0x02;
    f.data[5] = 0x0C; // SELF_DRIVING = 3
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_INT(3, handler.gatewayAutopilot);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// --- Filter IDs ---

void test_hw4_filter_ids_count()
{
    TEST_ASSERT_EQUAL_UINT8(4, handler.filterIdCount());
}

void test_hw4_filter_ids_values()
{
    const uint32_t *ids = handler.filterIds();
    TEST_ASSERT_EQUAL_UINT32(921, ids[0]);
    TEST_ASSERT_EQUAL_UINT32(1016, ids[1]);
    TEST_ASSERT_EQUAL_UINT32(1021, ids[2]);
    TEST_ASSERT_EQUAL_UINT32(2047, ids[3]);
}

int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_hw4_filter_ids_count);
    RUN_TEST(test_hw4_filter_ids_values);

    RUN_TEST(test_hw4_follow_distance_1_sets_profile_3);
    RUN_TEST(test_hw4_follow_distance_2_sets_profile_2);
    RUN_TEST(test_hw4_follow_distance_3_sets_profile_1);
    RUN_TEST(test_hw4_follow_distance_4_sets_profile_0);
    RUN_TEST(test_hw4_follow_distance_5_sets_profile_4);

    RUN_TEST(test_hw4_AD_enabled_only_set_on_mux0);
    RUN_TEST(test_hw4_AD_mux0_sets_bits_46_and_60);
    RUN_TEST(test_hw4_AD_mux0_sets_emergency_bit59);
    RUN_TEST(test_hw4_AD_mux0_skips_emergency_bit59_when_runtime_disabled);
    RUN_TEST(test_hw4_no_send_when_AD_disabled_mux0);
    RUN_TEST(test_hw4_checkAD_blocks_mux0_and_mux2_send);

    RUN_TEST(test_hw4_nag_suppression_clears_bit19_sets_bit47);
    RUN_TEST(test_hw4_nag_suppression_skips_mux1_changes_when_eap_runtime_disabled);

    RUN_TEST(test_hw4_mux2_does_not_inject_speed_profile);
    RUN_TEST(test_hw4_mux2_preserves_old_profile_bits_by_not_sending);

    RUN_TEST(test_hw4_mux0_AD_enabled_sends_1);
    RUN_TEST(test_hw4_mux1_sends_1);
    RUN_TEST(test_hw4_mux2_sends_0);
    RUN_TEST(test_hw4_ignores_unrelated_can_id);

    RUN_TEST(test_hw4_isa_suppress_sets_bit5_of_data1);
    RUN_TEST(test_hw4_isa_suppress_preserves_existing_data1_bits);
    RUN_TEST(test_hw4_isa_suppress_checksum_correct);
    RUN_TEST(test_hw4_isa_suppress_returns_early_no_further_processing);
    RUN_TEST(test_hw4_isa_suppress_runtime_off_skips_send);
    RUN_TEST(test_hw4_gw_autopilot_mux2_updates_state_without_send);

    return UNITY_END();
}
