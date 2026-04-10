#include <unity.h>
#include "log_buffer.h"

static LogRingBuffer ring;

void setUp() { ring = LogRingBuffer(); }
void tearDown() {}

void test_empty_buffer_head_is_zero()
{
    TEST_ASSERT_EQUAL_UINT32(0, ring.currentHead());
}

void test_push_increments_head()
{
    ring.push("hello", 100);
    TEST_ASSERT_EQUAL_UINT32(1, ring.currentHead());
}

void test_push_stores_message()
{
    ring.push("test msg", 200);
    TEST_ASSERT_EQUAL_STRING("test msg", ring.at(0).msg);
    TEST_ASSERT_EQUAL_UINT32(200, ring.at(0).timestamp_ms);
}

void test_read_since_returns_new_entries()
{
    ring.push("first", 100);
    ring.push("second", 200);
    ring.push("third", 300);

    LogRingBuffer::Entry out[8];
    int count = ring.readSince(1, out, 8);
    TEST_ASSERT_EQUAL_INT(2, count);
    TEST_ASSERT_EQUAL_STRING("second", out[0].msg);
    TEST_ASSERT_EQUAL_STRING("third", out[1].msg);
}

void test_read_since_zero_returns_all()
{
    ring.push("a", 10);
    ring.push("b", 20);

    LogRingBuffer::Entry out[8];
    int count = ring.readSince(0, out, 8);
    TEST_ASSERT_EQUAL_INT(2, count);
}

void test_wraparound_drops_oldest()
{
    for (uint32_t i = 0; i < LogRingBuffer::kCapacity + 5; i++)
    {
        char msg[16];
        snprintf(msg, sizeof(msg), "msg%u", i);
        ring.push(msg, i * 100);
    }

    TEST_ASSERT_EQUAL_UINT32(LogRingBuffer::kCapacity + 5, ring.currentHead());

    LogRingBuffer::Entry out[64];
    int count = ring.readSince(0, out, 64);
    TEST_ASSERT_EQUAL_INT(LogRingBuffer::kCapacity, count);
    TEST_ASSERT_EQUAL_STRING("msg5", out[0].msg);
}

void test_read_respects_max_out()
{
    ring.push("a", 10);
    ring.push("b", 20);
    ring.push("c", 30);

    LogRingBuffer::Entry out[2];
    int count = ring.readSince(0, out, 2);
    TEST_ASSERT_EQUAL_INT(2, count);
}

void test_truncates_long_message()
{
    char longMsg[256];
    memset(longMsg, 'A', sizeof(longMsg) - 1);
    longMsg[255] = '\0';
    ring.push(longMsg, 100);

    TEST_ASSERT_EQUAL_UINT32(LogRingBuffer::kMaxMsgLen - 1,
                             strlen(ring.at(0).msg));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_empty_buffer_head_is_zero);
    RUN_TEST(test_push_increments_head);
    RUN_TEST(test_push_stores_message);
    RUN_TEST(test_read_since_returns_new_entries);
    RUN_TEST(test_read_since_zero_returns_all);
    RUN_TEST(test_wraparound_drops_oldest);
    RUN_TEST(test_read_respects_max_out);
    RUN_TEST(test_truncates_long_message);
    return UNITY_END();
}
