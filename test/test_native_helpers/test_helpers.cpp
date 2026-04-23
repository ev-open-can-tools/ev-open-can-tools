#include <unity.h>
#include "can_frame_types.h"
#include "can_helpers.h"

void setUp()
{
    bypassTlsscRequirementRuntime = kBypassTlsscRequirementDefaultEnabled;
    isaSpeedChimeSuppressRuntime = kIsaSpeedChimeSuppressDefaultEnabled;
    emergencyVehicleDetectionRuntime = kEmergencyVehicleDetectionDefaultEnabled;
    enhancedAutopilotRuntime = kEnhancedAutopilotDefaultEnabled;
}
void tearDown() {}

// --- setBit ---

void test_setBit_sets_bit0_of_byte0()
{
    CanFrame f = {};
    setBit(f, 0, true);
    TEST_ASSERT_EQUAL_HEX8(0x01, f.data[0]);
}

void test_setBit_sets_bit7_of_byte0()
{
    CanFrame f = {};
    setBit(f, 7, true);
    TEST_ASSERT_EQUAL_HEX8(0x80, f.data[0]);
}

void test_setBit_sets_bit_in_byte5()
{
    CanFrame f = {};
    setBit(f, 46, true); // byte 5, bit 6
    TEST_ASSERT_EQUAL_HEX8(0x40, f.data[5]);
}

void test_setBit_sets_bit_in_byte7()
{
    CanFrame f = {};
    setBit(f, 60, true); // byte 7, bit 4
    TEST_ASSERT_EQUAL_HEX8(0x10, f.data[7]);
}

void test_setBit_clears_bit()
{
    CanFrame f = {};
    f.data[2] = 0xFF;
    setBit(f, 19, false); // byte 2, bit 3
    TEST_ASSERT_EQUAL_HEX8(0xF7, f.data[2]);
}

void test_setBit_does_not_affect_other_bytes()
{
    CanFrame f = {};
    f.data[0] = 0xAA;
    f.data[1] = 0xBB;
    setBit(f, 8, true); // byte 1, bit 0
    TEST_ASSERT_EQUAL_HEX8(0xAA, f.data[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBB, f.data[1]);
}

// --- readMuxID ---

void test_readMuxID_extracts_lower_3_bits()
{
    CanFrame f = {};
    f.data[0] = 0x05;
    TEST_ASSERT_EQUAL_UINT8(5, readMuxID(f));
}

void test_readMuxID_masks_upper_bits()
{
    CanFrame f = {};
    f.data[0] = 0xFA; // binary: 11111010 -> lower 3 = 010 = 2
    TEST_ASSERT_EQUAL_UINT8(2, readMuxID(f));
}

void test_readMuxID_zero()
{
    CanFrame f = {};
    f.data[0] = 0x00;
    TEST_ASSERT_EQUAL_UINT8(0, readMuxID(f));
}

void test_readMuxID_max_value()
{
    CanFrame f = {};
    f.data[0] = 0x07;
    TEST_ASSERT_EQUAL_UINT8(7, readMuxID(f));
}

// --- isADSelectedInUI ---

void test_isADSelectedInUI_true_when_bit6_set()
{
    CanFrame f = {};
    f.data[4] = 0x40; // bit 6 set
    TEST_ASSERT_TRUE(isADSelectedInUI(f));
}

void test_isADSelectedInUI_false_when_bit6_clear()
{
    CanFrame f = {};
    f.data[4] = 0x00;
    TEST_ASSERT_FALSE(isADSelectedInUI(f));
}

void test_isADSelectedInUI_ignores_other_bits()
{
    CanFrame f = {};
    f.data[4] = 0xBF; // all bits set except bit 6
    TEST_ASSERT_FALSE(isADSelectedInUI(f));
}

void test_isADSelectedInUI_true_with_other_bits()
{
    CanFrame f = {};
    f.data[4] = 0xFF;
    TEST_ASSERT_TRUE(isADSelectedInUI(f));
}

// --- readGTWAutopilot ---

void test_readGTWAutopilot_extracts_bits_42_to_44()
{
    CanFrame f = {};
    f.data[5] = 0x0C; // 0b011 at bits 42-44
    TEST_ASSERT_EQUAL_UINT8(3, readGTWAutopilot(f));
}

void test_readGTWAutopilot_masks_other_bits()
{
    CanFrame f = {};
    f.data[5] = 0xFF;
    TEST_ASSERT_EQUAL_UINT8(7, readGTWAutopilot(f));
}

// --- setSpeedProfileV12V13 ---

void test_setSpeedProfileV12V13_sets_profile_0()
{
    CanFrame f = {};
    f.data[6] = 0xFF;
    setSpeedProfileV12V13(f, 0);
    TEST_ASSERT_EQUAL_HEX8(0xF9, f.data[6]); // bits 1-2 cleared
}

void test_setSpeedProfileV12V13_sets_profile_1()
{
    CanFrame f = {};
    f.data[6] = 0x00;
    setSpeedProfileV12V13(f, 1);
    TEST_ASSERT_EQUAL_HEX8(0x02, f.data[6]);
}

void test_setSpeedProfileV12V13_sets_profile_2()
{
    CanFrame f = {};
    f.data[6] = 0x00;
    setSpeedProfileV12V13(f, 2);
    TEST_ASSERT_EQUAL_HEX8(0x04, f.data[6]);
}

void test_setSpeedProfileV12V13_preserves_other_bits()
{
    CanFrame f = {};
    f.data[6] = 0xF9; // bits 1-2 clear, rest set
    setSpeedProfileV12V13(f, 1);
    TEST_ASSERT_EQUAL_HEX8(0xFB, f.data[6]);
}

void test_computeVehicleChecksum_sums_payload_and_frame_id()
{
    CanFrame f = {.id = 1021, .dlc = 8};
    f.data[0] = 0xFD;
    f.data[1] = 0x10;
    f.data[2] = 0x20;
    f.data[3] = 0x04;
    f.data[4] = 0x00;
    f.data[5] = 0x00;
    f.data[6] = 0xA0;
    f.data[7] = 0x00;
    TEST_ASSERT_EQUAL_HEX8(0xD1, computeVehicleChecksum(f));
}

// --- Runtime BYPASS_TLSSC_REQUIREMENT ---

void test_runtime_bypass_tlssc_overrides_when_bit_clear()
{
    bypassTlsscRequirementRuntime = true;
    CanFrame f = {};
    f.data[4] = 0x00;
    TEST_ASSERT_TRUE(isADSelectedInUI(f));
    bypassTlsscRequirementRuntime = false;
}

void test_runtime_bypass_tlssc_off_reads_frame()
{
    bypassTlsscRequirementRuntime = false;
    CanFrame f = {};
    f.data[4] = 0x00;
    TEST_ASSERT_FALSE(isADSelectedInUI(f));
}

void test_runtime_bypass_tlssc_off_still_reads_real_bit()
{
    bypassTlsscRequirementRuntime = false;
    CanFrame f = {};
    f.data[4] = 0x40;
    TEST_ASSERT_TRUE(isADSelectedInUI(f));
}

void test_runtime_defaults_start_disabled()
{
    TEST_ASSERT_EQUAL(kBypassTlsscRequirementDefaultEnabled, bypassTlsscRequirementRuntime);
    TEST_ASSERT_EQUAL(kIsaSpeedChimeSuppressDefaultEnabled, isaSpeedChimeSuppressRuntime);
    TEST_ASSERT_EQUAL(kEmergencyVehicleDetectionDefaultEnabled, emergencyVehicleDetectionRuntime);
    TEST_ASSERT_EQUAL(kEnhancedAutopilotDefaultEnabled, enhancedAutopilotRuntime);
}

int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_setBit_sets_bit0_of_byte0);
    RUN_TEST(test_setBit_sets_bit7_of_byte0);
    RUN_TEST(test_setBit_sets_bit_in_byte5);
    RUN_TEST(test_setBit_sets_bit_in_byte7);
    RUN_TEST(test_setBit_clears_bit);
    RUN_TEST(test_setBit_does_not_affect_other_bytes);

    RUN_TEST(test_readMuxID_extracts_lower_3_bits);
    RUN_TEST(test_readMuxID_masks_upper_bits);
    RUN_TEST(test_readMuxID_zero);
    RUN_TEST(test_readMuxID_max_value);

    RUN_TEST(test_isADSelectedInUI_true_when_bit6_set);
    RUN_TEST(test_isADSelectedInUI_false_when_bit6_clear);
    RUN_TEST(test_isADSelectedInUI_ignores_other_bits);
    RUN_TEST(test_isADSelectedInUI_true_with_other_bits);
    RUN_TEST(test_readGTWAutopilot_extracts_bits_42_to_44);
    RUN_TEST(test_readGTWAutopilot_masks_other_bits);

    RUN_TEST(test_setSpeedProfileV12V13_sets_profile_0);
    RUN_TEST(test_setSpeedProfileV12V13_sets_profile_1);
    RUN_TEST(test_setSpeedProfileV12V13_sets_profile_2);
    RUN_TEST(test_setSpeedProfileV12V13_preserves_other_bits);
    RUN_TEST(test_computeVehicleChecksum_sums_payload_and_frame_id);

    RUN_TEST(test_runtime_bypass_tlssc_overrides_when_bit_clear);
    RUN_TEST(test_runtime_bypass_tlssc_off_reads_frame);
    RUN_TEST(test_runtime_bypass_tlssc_off_still_reads_real_bit);
    RUN_TEST(test_runtime_defaults_start_disabled);

    return UNITY_END();
}
