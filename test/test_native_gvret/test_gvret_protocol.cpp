#include <unity.h>

#include "gvret_protocol.h"

void setUp() {}
void tearDown() {}

void test_encodes_reference_frame_layout()
{
    CanFrame frame = {.id = 0x370, .dlc = 3, .data = {0x12, 0x34, 0x56}};
    uint8_t output[19] = {};
    size_t size = GvretProtocol::encodeCanFrame(frame, 0x12345678, output, sizeof(output));
    const uint8_t expected[] = {
        0xF1, 0x00, 0x78, 0x56, 0x34, 0x12,
        0x70, 0x03, 0x00, 0x00, 0x03, 0x12, 0x34, 0x56};
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), size);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, output, sizeof(expected));
}

void test_rejects_invalid_frame_or_short_output()
{
    uint8_t output[19] = {};
    CanFrame extended = {.id = 0x800, .dlc = 8};
    TEST_ASSERT_EQUAL_size_t(0, GvretProtocol::encodeCanFrame(extended, 0, output, sizeof(output)));
    CanFrame badDlc = {.id = 0x370, .dlc = 9};
    TEST_ASSERT_EQUAL_size_t(0, GvretProtocol::encodeCanFrame(badDlc, 0, output, sizeof(output)));
    CanFrame valid = {.id = 0x370, .dlc = 8};
    TEST_ASSERT_EQUAL_size_t(0, GvretProtocol::encodeCanFrame(valid, 0, output, 18));
}

void test_can_params_reply_describes_one_500k_bus()
{
    TEST_ASSERT_EQUAL_size_t(19, sizeof(GvretProtocol::kCanParamsResponse));
    TEST_ASSERT_EQUAL_HEX8(0xF1, GvretProtocol::kCanParamsResponse[0]);
    TEST_ASSERT_EQUAL_HEX8(0x06, GvretProtocol::kCanParamsResponse[1]);
    TEST_ASSERT_EQUAL_UINT8(1, GvretProtocol::kCanParamsResponse[2]);
    TEST_ASSERT_EQUAL_HEX8(0x20, GvretProtocol::kCanParamsResponse[3]);
    TEST_ASSERT_EQUAL_HEX8(0xA1, GvretProtocol::kCanParamsResponse[4]);
    TEST_ASSERT_EQUAL_HEX8(0x07, GvretProtocol::kCanParamsResponse[5]);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_encodes_reference_frame_layout);
    RUN_TEST(test_rejects_invalid_frame_or_short_output);
    RUN_TEST(test_can_params_reply_describes_one_500k_bus);
    return UNITY_END();
}
