#include <unity.h>
#include <cstdint>

// Extracted filter computation logic from TWAIDriver::setFilters() so we can
// unit-test the math without the ESP-IDF TWAI hardware API.
struct TwaiFilterResult
{
    uint32_t acceptance_code;
    uint32_t acceptance_mask;
};

static TwaiFilterResult computeTwaiFilter(const uint32_t *ids, uint8_t count)
{
    TwaiFilterResult r = {};
    if (count == 0)
        return r;

    uint32_t differ = 0;
    for (uint8_t i = 1; i < count; i++)
    {
        differ |= ids[0] ^ ids[i];
    }
    uint32_t base = ids[0] & ~differ;
    r.acceptance_code = base << 21;
    r.acceptance_mask = (differ << 21) | 0x001FFFFF;
    return r;
}

static bool filterAccepts(const TwaiFilterResult &f, uint32_t id)
{
    uint32_t rx = id << 21;
    return (rx & ~f.acceptance_mask) == (f.acceptance_code & ~f.acceptance_mask);
}

void setUp() {}
void tearDown() {}

// --- Single ID filter (Legacy-like: 2 IDs far apart) ---

void test_twai_filter_legacy_accepts_id_69()
{
    uint32_t ids[] = {69, 1006};
    auto f = computeTwaiFilter(ids, 2);
    TEST_ASSERT_TRUE(filterAccepts(f, 69));
}

void test_twai_filter_legacy_accepts_id_1006()
{
    uint32_t ids[] = {69, 1006};
    auto f = computeTwaiFilter(ids, 2);
    TEST_ASSERT_TRUE(filterAccepts(f, 1006));
}

// --- HW3 filter (Autopilot IDs) ---

void test_twai_filter_hw3_accepts_id_1016()
{
    uint32_t ids[] = {1016, 1021, 2047};
    auto f = computeTwaiFilter(ids, 3);
    TEST_ASSERT_TRUE(filterAccepts(f, 1016));
}

void test_twai_filter_hw3_accepts_id_1021()
{
    uint32_t ids[] = {1016, 1021, 2047};
    auto f = computeTwaiFilter(ids, 3);
    TEST_ASSERT_TRUE(filterAccepts(f, 1021));
}

void test_twai_filter_hw3_accepts_id_2047()
{
    uint32_t ids[] = {1016, 1021, 2047};
    auto f = computeTwaiFilter(ids, 3);
    TEST_ASSERT_TRUE(filterAccepts(f, 2047));
}

void test_twai_filter_hw3_rejects_unrelated_id()
{
    uint32_t ids[] = {1016, 1021, 2047};
    auto f = computeTwaiFilter(ids, 3);
    TEST_ASSERT_FALSE(filterAccepts(f, 1000));
}

// --- HW4 filter (ISA + Autopilot IDs) ---

void test_twai_filter_hw4_accepts_id_921()
{
    uint32_t ids[] = {921, 1016, 1021, 2047};
    auto f = computeTwaiFilter(ids, 4);
    TEST_ASSERT_TRUE(filterAccepts(f, 921));
}

void test_twai_filter_hw4_accepts_id_1016()
{
    uint32_t ids[] = {921, 1016, 1021, 2047};
    auto f = computeTwaiFilter(ids, 4);
    TEST_ASSERT_TRUE(filterAccepts(f, 1016));
}

void test_twai_filter_hw4_accepts_id_1021()
{
    uint32_t ids[] = {921, 1016, 1021, 2047};
    auto f = computeTwaiFilter(ids, 4);
    TEST_ASSERT_TRUE(filterAccepts(f, 1021));
}

void test_twai_filter_hw4_accepts_id_2047()
{
    uint32_t ids[] = {921, 1016, 1021, 2047};
    auto f = computeTwaiFilter(ids, 4);
    TEST_ASSERT_TRUE(filterAccepts(f, 2047));
}

void test_twai_filter_hw4_rejects_distant_id()
{
    uint32_t ids[] = {921, 1016, 1021, 2047};
    auto f = computeTwaiFilter(ids, 4);
    TEST_ASSERT_FALSE(filterAccepts(f, 100));
}

// --- HW3: tight filter rejects multiple unrelated IDs ---

void test_twai_filter_hw3_rejects_id_500()
{
    uint32_t ids[] = {1016, 1021, 2047};
    auto f = computeTwaiFilter(ids, 3);
    TEST_ASSERT_FALSE(filterAccepts(f, 500));
}

void test_twai_filter_hw3_rejects_id_0()
{
    uint32_t ids[] = {1016, 1021, 2047};
    auto f = computeTwaiFilter(ids, 3);
    TEST_ASSERT_FALSE(filterAccepts(f, 0));
}

// --- HW4: another rejection ---

void test_twai_filter_hw4_rejects_id_500()
{
    uint32_t ids[] = {921, 1016, 1021, 2047};
    auto f = computeTwaiFilter(ids, 4);
    TEST_ASSERT_FALSE(filterAccepts(f, 500));
}

// --- Legacy: wide gap means wider mask (false positives expected) ---

void test_twai_filter_legacy_rejects_id_500()
{
    uint32_t ids[] = {69, 1006};
    auto f = computeTwaiFilter(ids, 2);
    TEST_ASSERT_FALSE(filterAccepts(f, 500));
}

// --- Single ID ---

void test_twai_filter_single_id_exact_match()
{
    uint32_t ids[] = {500};
    auto f = computeTwaiFilter(ids, 1);
    TEST_ASSERT_TRUE(filterAccepts(f, 500));
    TEST_ASSERT_FALSE(filterAccepts(f, 501));
    TEST_ASSERT_FALSE(filterAccepts(f, 499));
}

// --- Mask correctness ---

void test_twai_filter_single_id_mask_is_exact()
{
    uint32_t ids[] = {500};
    auto f = computeTwaiFilter(ids, 1);
    TEST_ASSERT_EQUAL_HEX32(500u << 21, f.acceptance_code);
    TEST_ASSERT_EQUAL_HEX32(0x001FFFFF, f.acceptance_mask);
}

void test_twai_filter_hw4_mask_bits()
{
    uint32_t ids[] = {921, 1016, 1021, 2047};
    auto f = computeTwaiFilter(ids, 4);
    uint32_t expected_mask = (0x467u << 21) | 0x001FFFFF;
    TEST_ASSERT_EQUAL_HEX32(expected_mask, f.acceptance_mask);
}

void test_twai_filter_empty_count_returns_zero()
{
    auto f = computeTwaiFilter(nullptr, 0);
    TEST_ASSERT_EQUAL_HEX32(0, f.acceptance_code);
    TEST_ASSERT_EQUAL_HEX32(0, f.acceptance_mask);
}

int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_twai_filter_legacy_accepts_id_69);
    RUN_TEST(test_twai_filter_legacy_accepts_id_1006);
    RUN_TEST(test_twai_filter_legacy_rejects_id_500);

    RUN_TEST(test_twai_filter_hw3_accepts_id_1016);
    RUN_TEST(test_twai_filter_hw3_accepts_id_1021);
    RUN_TEST(test_twai_filter_hw3_accepts_id_2047);
    RUN_TEST(test_twai_filter_hw3_rejects_unrelated_id);
    RUN_TEST(test_twai_filter_hw3_rejects_id_500);
    RUN_TEST(test_twai_filter_hw3_rejects_id_0);

    RUN_TEST(test_twai_filter_hw4_accepts_id_921);
    RUN_TEST(test_twai_filter_hw4_accepts_id_1016);
    RUN_TEST(test_twai_filter_hw4_accepts_id_1021);
    RUN_TEST(test_twai_filter_hw4_accepts_id_2047);
    RUN_TEST(test_twai_filter_hw4_rejects_distant_id);
    RUN_TEST(test_twai_filter_hw4_rejects_id_500);

    RUN_TEST(test_twai_filter_single_id_exact_match);
    RUN_TEST(test_twai_filter_single_id_mask_is_exact);
    RUN_TEST(test_twai_filter_hw4_mask_bits);
    RUN_TEST(test_twai_filter_empty_count_returns_zero);

    return UNITY_END();
}
