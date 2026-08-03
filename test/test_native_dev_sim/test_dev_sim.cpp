#include <unity.h>
#include "dev_sim.h"

static DevSim sim;

void setUp() { sim = DevSim(); }
void tearDown() {}

// Before a step has elapsed, no frame is due.
void test_no_frame_before_step()
{
    CanFrame f;
    sim.reset(1000);
    TEST_ASSERT_FALSE(sim.read(f, 1000));
    TEST_ASSERT_FALSE(sim.read(f, 1000 + DevSim::kStepMs - 1));
}

// A frame becomes due once a full step has elapsed.
void test_frame_after_step()
{
    CanFrame f;
    sim.reset(1000);
    TEST_ASSERT_TRUE(sim.read(f, 1000 + DevSim::kStepMs));
    TEST_ASSERT_EQUAL_UINT8(8, f.dlc);
}

// The first cycle emits exactly the scripted IDs the handlers filter on.
void test_script_covers_handler_ids()
{
    CanFrame f;
    uint32_t t = 0;
    sim.reset(t);
    bool saw280 = false, saw390 = false, saw921 = false, saw1016 = false, saw1021 = false;
    for (int i = 0; i < 7; i++)
    {
        t += DevSim::kStepMs;
        TEST_ASSERT_TRUE(sim.read(f, t));
        if (f.id == 280)
            saw280 = true;
        if (f.id == 390)
            saw390 = true;
        if (f.id == 921)
            saw921 = true;
        if (f.id == 1016)
            saw1016 = true;
        if (f.id == 1021)
            saw1021 = true;
    }
    TEST_ASSERT_TRUE(saw280);
    TEST_ASSERT_TRUE(saw390);
    TEST_ASSERT_TRUE(saw921);
    TEST_ASSERT_TRUE(saw1016);
    TEST_ASSERT_TRUE(saw1021);
}

// The 280 frame decodes to Park and the 1021 mux0 frame flags AD-selected,
// so the pipeline reacts (Parked gate open + HW3 injection path).
void test_frames_are_meaningful()
{
    CanFrame f;
    uint32_t t = 0;
    sim.reset(t);
    bool checked280 = false, checkedInject = false;
    for (int i = 0; i < 7; i++)
    {
        t += DevSim::kStepMs;
        sim.read(f, t);
        if (f.id == 280)
        {
            TEST_ASSERT_EQUAL_UINT8(1, (f.data[2] >> 5) & 0x07); // gear = P
            checked280 = true;
        }
        if (f.id == 1021 && (f.data[0] & 0x07) == 0)
        {
            TEST_ASSERT_EQUAL_UINT8(1, (f.data[4] >> 5) & 0x01); // AD selected
            checkedInject = true;
        }
    }
    TEST_ASSERT_TRUE(checked280);
    TEST_ASSERT_TRUE(checkedInject);
}

// data[6] advances once per full cycle so the sniffer shows live data.
void test_rolling_counter_advances()
{
    CanFrame f;
    uint32_t t = 0;
    sim.reset(t);
    t += DevSim::kStepMs;
    sim.read(f, t); // first frame of cycle 0
    uint8_t c0 = f.data[6];
    for (int i = 0; i < 7; i++) // advance a full cycle
    {
        t += DevSim::kStepMs;
        sim.read(f, t);
    }
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(c0 + 1), f.data[6]);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_no_frame_before_step);
    RUN_TEST(test_frame_after_step);
    RUN_TEST(test_script_covers_handler_ids);
    RUN_TEST(test_frames_are_meaningful);
    RUN_TEST(test_rolling_counter_advances);
    return UNITY_END();
}
