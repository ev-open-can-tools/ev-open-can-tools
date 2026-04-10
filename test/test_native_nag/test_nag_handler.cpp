#include <unity.h>
#include "can_frame_types.h"
#include "drivers/can_driver.h"
#include "can_helpers.h"
#include "handlers.h"
#include "drivers/mock_driver.h"

static MockDriver mock;
static NagHandler handler;

// Helper: build a realistic CAN 880 frame
static CanFrame makeEpasFrame(uint8_t handsOn, float torqueNm, uint8_t counter, uint8_t eacStatus = 2)
{
    CanFrame f = {.id = 880, .dlc = 8};
    // bytes 0-1: steeringRackForce (arbitrary realistic values)
    f.data[0] = 0x12;
    f.data[1] = 0x00;
    // bytes 2-3: torsionBarTorque = (torque + 20.5) / 0.01
    uint16_t tRaw = static_cast<uint16_t>((torqueNm + 20.5) / 0.01);
    f.data[2] = 0x08 | ((tRaw >> 8) & 0x0F); // upper nibble = flags (0x08)
    f.data[3] = tRaw & 0xFF;
    // byte 4: handsOnLevel in bits 7:6, internalSAS bits in lower
    f.data[4] = static_cast<uint8_t>((handsOn & 0x03) << 6) | 0x1F;
    // byte 5: internalSAS LSB
    f.data[5] = 0x89;
    // byte 6: upper nibble = eacStatus/tireID, lower nibble = counter
    f.data[6] = static_cast<uint8_t>((eacStatus << 5) | (counter & 0x0F));
    // byte 7: checksum = sum(b0..b6) + 0x73
    uint16_t sum = 0;
    for (int i = 0; i < 7; i++)
        sum += f.data[i];
    f.data[7] = static_cast<uint8_t>((sum + 0x73) & 0xFF);
    return f;
}

// Helper: verify checksum of a frame
static bool verifyChecksum(const CanFrame &f)
{
    uint16_t sum = 0;
    for (int i = 0; i < 7; i++)
        sum += f.data[i];
    return f.data[7] == static_cast<uint8_t>((sum + 0x73) & 0xFF);
}

void setUp()
{
    mock.reset();
    handler = NagHandler();
    handler.enablePrint = false;
}

void tearDown() {}

// ============================================================
// Filter IDs
// ============================================================

void test_nag_filter_ids_count()
{
    TEST_ASSERT_EQUAL_UINT8(1, handler.filterIdCount());
}

void test_nag_filter_ids_value()
{
    const uint32_t *ids = handler.filterIds();
    TEST_ASSERT_EQUAL_UINT32(880, ids[0]);
}

// ============================================================
// Basic echo behavior
// ============================================================

void test_nag_echoes_when_handson_0()
{
    CanFrame f = makeEpasFrame(0, 0.33, 0x0C);
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
}

void test_nag_does_not_echo_when_handson_1()
{
    CanFrame f = makeEpasFrame(1, 1.5, 0x0C);
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_nag_does_not_echo_when_handson_2()
{
    CanFrame f = makeEpasFrame(2, 2.5, 0x0C);
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_nag_does_not_echo_when_handson_3()
{
    CanFrame f = makeEpasFrame(3, 3.0, 0x0C);
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_nag_does_not_echo_when_disabled()
{
    handler.nagKillerActive = false;
    CanFrame f = makeEpasFrame(0, 0.33, 0x0C);
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_nag_ignores_non_880_id()
{
    CanFrame f = makeEpasFrame(0, 0.33, 0x0C);
    f.id = 881; // wrong ID
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

void test_nag_ignores_short_dlc()
{
    CanFrame f = makeEpasFrame(0, 0.33, 0x0C);
    f.dlc = 7; // too short
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(0, mock.sent.size());
}

// ============================================================
// Counter+1 logic
// ============================================================

void test_nag_counter_increments_by_1()
{
    CanFrame f = makeEpasFrame(0, 0.33, 0x0C);
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    uint8_t outCounter = mock.sent[0].data[6] & 0x0F;
    TEST_ASSERT_EQUAL_HEX8(0x0D, outCounter); // 0x0C + 1
}

void test_nag_counter_wraps_from_f_to_0()
{
    CanFrame f = makeEpasFrame(0, 0.33, 0x0F);
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    uint8_t outCounter = mock.sent[0].data[6] & 0x0F;
    TEST_ASSERT_EQUAL_HEX8(0x00, outCounter); // 0x0F + 1 wraps to 0
}

void test_nag_counter_preserves_upper_nibble()
{
    CanFrame f = makeEpasFrame(0, 0.33, 0x05, 2); // eacStatus=2 -> upper nibble = 0x40
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL(1, mock.sent.size());
    uint8_t upperNibble = mock.sent[0].data[6] & 0xF0;
    uint8_t expectedUpper = f.data[6] & 0xF0;
    TEST_ASSERT_EQUAL_HEX8(expectedUpper, upperNibble);
}

// ============================================================
// Modified field values
// ============================================================

void test_nag_sets_handson_to_1()
{
    CanFrame f = makeEpasFrame(0, 0.33, 0x0C);
    handler.handleMessage(f, mock);
    uint8_t outHandsOn = (mock.sent[0].data[4] >> 6) & 0x03;
    TEST_ASSERT_EQUAL_UINT8(1, outHandsOn);
}

void test_nag_preserves_byte4_lower_bits()
{
    CanFrame f = makeEpasFrame(0, 0.33, 0x0C);
    f.data[4] = 0x1F; // handsOn=0, lower bits = 0x1F
    handler.handleMessage(f, mock);
    uint8_t outLower = mock.sent[0].data[4] & 0x3F;
    TEST_ASSERT_EQUAL_HEX8(0x1F, outLower); // lower 6 bits preserved
}

void test_nag_sets_fixed_torque_0xB6()
{
    CanFrame f = makeEpasFrame(0, 0.10, 0x0C); // low torque
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_HEX8(0xB6, mock.sent[0].data[3]);
}

void test_nag_torque_value_is_1_80_nm()
{
    CanFrame f = makeEpasFrame(0, 0.33, 0x0C);
    handler.handleMessage(f, mock);
    // Decode torque from echoed frame
    uint16_t tRaw = ((mock.sent[0].data[2] & 0x0F) << 8) | mock.sent[0].data[3];
    float torque = tRaw * 0.01f - 20.5f;
    TEST_ASSERT_FLOAT_WITHIN(0.1, 1.80, torque);
}

void test_nag_copies_bytes_0_1_2_5_unchanged()
{
    CanFrame f = makeEpasFrame(0, 0.33, 0x0C);
    f.data[0] = 0xAB;
    f.data[1] = 0xCD;
    f.data[2] = 0x8E; // upper nibble has flags
    f.data[5] = 0x42;
    // Recompute checksum after manual changes
    uint16_t sum = 0;
    for (int i = 0; i < 7; i++)
        sum += f.data[i];
    f.data[7] = static_cast<uint8_t>((sum + 0x73) & 0xFF);

    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_HEX8(0xAB, mock.sent[0].data[0]);
    TEST_ASSERT_EQUAL_HEX8(0xCD, mock.sent[0].data[1]);
    TEST_ASSERT_EQUAL_HEX8(0x88, mock.sent[0].data[2]); // upper nibble preserved, lower nibble = 0x08 (fixed torque 0x08B6)
    TEST_ASSERT_EQUAL_HEX8(0x42, mock.sent[0].data[5]);
}

// ============================================================
// Checksum verification
// ============================================================

void test_nag_checksum_correct()
{
    CanFrame f = makeEpasFrame(0, 0.33, 0x0C);
    handler.handleMessage(f, mock);
    TEST_ASSERT_TRUE(verifyChecksum(mock.sent[0]));
}

void test_nag_checksum_correct_at_counter_boundary()
{
    CanFrame f = makeEpasFrame(0, 0.33, 0x0F); // counter wraps
    handler.handleMessage(f, mock);
    TEST_ASSERT_TRUE(verifyChecksum(mock.sent[0]));
}

void test_nag_checksum_correct_with_various_inputs()
{
    // Test across multiple counter values and torques
    for (uint8_t cnt = 0; cnt < 16; cnt++)
    {
        mock.reset();
        CanFrame f = makeEpasFrame(0, -5.0 + cnt * 0.7, cnt);
        handler.handleMessage(f, mock);
        TEST_ASSERT_EQUAL(1, mock.sent.size());
        TEST_ASSERT_TRUE_MESSAGE(verifyChecksum(mock.sent[0]), "Checksum failed for counter sweep");
    }
}

// ============================================================
// Canary: output torque must stay in safe range
// ============================================================

void test_nag_output_torque_never_exceeds_safe_range()
{
    // The fixed torque is 1.80 Nm. Verify it's always in [-5, 5] Nm range.
    for (uint8_t cnt = 0; cnt < 16; cnt++)
    {
        mock.reset();
        CanFrame f = makeEpasFrame(0, -20.0 + cnt * 2.5, cnt);
        handler.handleMessage(f, mock);
        TEST_ASSERT_EQUAL(1, mock.sent.size());

        uint16_t tRaw = ((mock.sent[0].data[2] & 0x0F) << 8) | mock.sent[0].data[3];
        float torque = tRaw * 0.01f - 20.5f;

        // Must be exactly 1.80 Nm (from fixed byte 3 = 0xB6)
        TEST_ASSERT_FLOAT_WITHIN(0.1, 1.80, torque);
        // Must never exceed safe range
        TEST_ASSERT_TRUE(torque >= -5.0f);
        TEST_ASSERT_TRUE(torque <= 5.0f);
    }
}

void test_nag_output_handson_never_exceeds_1()
{
    CanFrame f = makeEpasFrame(0, 0.33, 0x0C);
    handler.handleMessage(f, mock);
    uint8_t ho = (mock.sent[0].data[4] >> 6) & 0x03;
    TEST_ASSERT_EQUAL_UINT8(1, ho); // exactly 1, never 2 or 3
}

// ============================================================
// Frame count tracking
// ============================================================

void test_nag_increments_frames_sent()
{
    CanFrame f = makeEpasFrame(0, 0.33, 0x0C);
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_UINT32(1, handler.framesSent);
}

void test_nag_increments_echo_count()
{
    CanFrame f = makeEpasFrame(0, 0.33, 0x0C);
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_UINT32(1, handler.nagEchoCount);
}

void test_nag_multiple_frames_count_correctly()
{
    for (int i = 0; i < 10; i++)
    {
        CanFrame f = makeEpasFrame(0, 0.33, i & 0x0F);
        handler.handleMessage(f, mock);
    }
    TEST_ASSERT_EQUAL_UINT32(10, handler.nagEchoCount);
    TEST_ASSERT_EQUAL(10, mock.sent.size());
}

// ============================================================
// Edge case: mixed handsOn sequence
// ============================================================

void test_nag_echoes_only_handson_0_in_mixed_sequence()
{
    // Simulate: ho=0, ho=1, ho=0, ho=2, ho=0
    CanFrame f0a = makeEpasFrame(0, 0.33, 0x00);
    CanFrame f1 = makeEpasFrame(1, 1.50, 0x01);
    CanFrame f0b = makeEpasFrame(0, 0.10, 0x02);
    CanFrame f2 = makeEpasFrame(2, 2.50, 0x03);
    CanFrame f0c = makeEpasFrame(0, 0.05, 0x04);

    handler.handleMessage(f0a, mock);
    handler.handleMessage(f1, mock);
    handler.handleMessage(f0b, mock);
    handler.handleMessage(f2, mock);
    handler.handleMessage(f0c, mock);

    TEST_ASSERT_EQUAL(3, mock.sent.size()); // only 3 echoes for ho=0
}

// ============================================================
// Output ID is always 880
// ============================================================

void test_nag_output_id_is_880()
{
    CanFrame f = makeEpasFrame(0, 0.33, 0x0C);
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_UINT32(880, mock.sent[0].id);
}

void test_nag_output_dlc_is_8()
{
    CanFrame f = makeEpasFrame(0, 0.33, 0x0C);
    handler.handleMessage(f, mock);
    TEST_ASSERT_EQUAL_UINT8(8, mock.sent[0].dlc);
}

int main()
{
    UNITY_BEGIN();

    // Filter
    RUN_TEST(test_nag_filter_ids_count);
    RUN_TEST(test_nag_filter_ids_value);

    // Basic echo behavior
    RUN_TEST(test_nag_echoes_when_handson_0);
    RUN_TEST(test_nag_does_not_echo_when_handson_1);
    RUN_TEST(test_nag_does_not_echo_when_handson_2);
    RUN_TEST(test_nag_does_not_echo_when_handson_3);
    RUN_TEST(test_nag_does_not_echo_when_disabled);
    RUN_TEST(test_nag_ignores_non_880_id);
    RUN_TEST(test_nag_ignores_short_dlc);

    // Counter+1
    RUN_TEST(test_nag_counter_increments_by_1);
    RUN_TEST(test_nag_counter_wraps_from_f_to_0);
    RUN_TEST(test_nag_counter_preserves_upper_nibble);

    // Modified fields
    RUN_TEST(test_nag_sets_handson_to_1);
    RUN_TEST(test_nag_preserves_byte4_lower_bits);
    RUN_TEST(test_nag_sets_fixed_torque_0xB6);
    RUN_TEST(test_nag_torque_value_is_1_80_nm);
    RUN_TEST(test_nag_copies_bytes_0_1_2_5_unchanged);

    // Checksum
    RUN_TEST(test_nag_checksum_correct);
    RUN_TEST(test_nag_checksum_correct_at_counter_boundary);
    RUN_TEST(test_nag_checksum_correct_with_various_inputs);

    // Safety canary
    RUN_TEST(test_nag_output_torque_never_exceeds_safe_range);
    RUN_TEST(test_nag_output_handson_never_exceeds_1);

    // Counters
    RUN_TEST(test_nag_increments_frames_sent);
    RUN_TEST(test_nag_increments_echo_count);
    RUN_TEST(test_nag_multiple_frames_count_correctly);

    // Edge cases
    RUN_TEST(test_nag_echoes_only_handson_0_in_mixed_sequence);

    // Output frame
    RUN_TEST(test_nag_output_id_is_880);
    RUN_TEST(test_nag_output_dlc_is_8);

    return UNITY_END();
}
