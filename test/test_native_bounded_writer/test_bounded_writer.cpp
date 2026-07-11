#include <unity.h>

#include "bounded_text_writer.h"

void setUp() {}
void tearDown() {}

void test_appends_text_and_formatted_values_without_allocation()
{
    char buffer[64];
    BoundedTextWriter writer(buffer, sizeof(buffer));
    TEST_ASSERT_TRUE(writer.append("frames="));
    TEST_ASSERT_TRUE(writer.appendf("%lu state=%s", 123UL, "running"));
    TEST_ASSERT_TRUE(writer.complete());
    TEST_ASSERT_EQUAL_STRING("frames=123 state=running", writer.data());
    TEST_ASSERT_EQUAL_size_t(24, writer.size());
}

void test_overflow_fails_closed_and_preserves_valid_prefix()
{
    char buffer[8];
    BoundedTextWriter writer(buffer, sizeof(buffer));
    TEST_ASSERT_TRUE(writer.append("abc"));
    TEST_ASSERT_FALSE(writer.append("12345"));
    TEST_ASSERT_FALSE(writer.complete());
    TEST_ASSERT_EQUAL_STRING("abc", writer.data());
    TEST_ASSERT_EQUAL_size_t(3, writer.size());
}

void test_safe_text_replaces_controls_and_honors_limit()
{
    char buffer[16];
    BoundedTextWriter writer(buffer, sizeof(buffer));
    TEST_ASSERT_TRUE(writer.appendSafe("ab\ncdEF", 5));
    TEST_ASSERT_TRUE(writer.complete());
    TEST_ASSERT_EQUAL_STRING("ab?cd", writer.data());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_appends_text_and_formatted_values_without_allocation);
    RUN_TEST(test_overflow_fails_closed_and_preserves_valid_prefix);
    RUN_TEST(test_safe_text_replaces_controls_and_honors_limit);
    return UNITY_END();
}
