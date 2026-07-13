#include <unity.h>

#include "can_helpers.h"
#include "drivers/mock_driver.h"
#include "handlers.h"
#include "injection_policy.h"

static SummonInjectionSnapshot freshParkedSnapshot()
{
    SummonInjectionSnapshot s;
    s.diSeen = true;
    s.speedSeen = true;
    s.autopilotSeen = true;
    s.summonStateSeen = true;
    s.diGear = 1;
    s.autopilotState = 0;
    s.selfParkRequest = 0;
    s.vehicleSpeedRaw = kDiVehicleSpeedStationaryRaw;
    return s;
}

static void assertState(const SummonInjectionDecision &decision, bool allowed,
                        SummonInjectionState state)
{
    TEST_ASSERT_EQUAL(allowed, decision.allowed);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(state), static_cast<uint8_t>(decision.state));
}

void setUp()
{
    summonOnlyInjectionRuntime = false;
}

void tearDown() {}

void test_parked_and_stationary_allows_injection()
{
    assertState(evaluateSummonInjectionPolicy(freshParkedSnapshot(), 0), true,
                SummonInjectionState::AllowedParked);
}

void test_parked_before_summon_begins_allows_injection()
{
    SummonInjectionSnapshot s = freshParkedSnapshot();
    s.selfParkRequest = 11;
    s.summonRequestConfirmed = true;
    assertState(evaluateSummonInjectionPolicy(s, 0), true,
                SummonInjectionState::AllowedParked);
}

void test_summon_active_and_moving_allows_injection()
{
    SummonInjectionSnapshot s = freshParkedSnapshot();
    s.diGear = 4;
    s.vehicleSpeedRaw = 625;
    s.autonomyControlActive = true;
    s.summonRequestConfirmed = true;
    s.selfParkRequest = 11;
    assertState(evaluateSummonInjectionPolicy(s, 0), true,
                SummonInjectionState::AllowedSummon);
}

void test_manual_driving_blocks_injection()
{
    SummonInjectionSnapshot s = freshParkedSnapshot();
    s.diGear = 4;
    assertState(evaluateSummonInjectionPolicy(s, 0), false,
                SummonInjectionState::ManualDriving);
}

void test_autopilot_active_blocks_injection()
{
    SummonInjectionSnapshot s = freshParkedSnapshot();
    s.autopilotState = 3;
    assertState(evaluateSummonInjectionPolicy(s, 0), false,
                SummonInjectionState::AutopilotActive);
}

void test_moving_without_confirmed_summon_blocks_injection()
{
    SummonInjectionSnapshot s = freshParkedSnapshot();
    s.diGear = 4;
    s.vehicleSpeedRaw = 625;
    assertState(evaluateSummonInjectionPolicy(s, 0), false,
                SummonInjectionState::MovingWithoutSummon);
}

void test_transition_from_summon_to_manual_stops_immediately()
{
    SummonInjectionSnapshot s = freshParkedSnapshot();
    s.diGear = 4;
    s.vehicleSpeedRaw = 625;
    s.autonomyControlActive = true;
    s.summonRequestConfirmed = true;
    s.selfParkRequest = 11;
    TEST_ASSERT_TRUE(evaluateSummonInjectionPolicy(s, 0).allowed);

    s.autonomyControlActive = false;
    s.summonRequestConfirmed = false;
    assertState(evaluateSummonInjectionPolicy(s, 0), false,
                SummonInjectionState::MovingWithoutSummon);
}

void test_transition_from_summon_to_autopilot_stops_immediately()
{
    SummonInjectionSnapshot s = freshParkedSnapshot();
    s.diGear = 4;
    s.vehicleSpeedRaw = 625;
    s.autonomyControlActive = true;
    s.summonRequestConfirmed = true;
    s.selfParkRequest = 11;
    TEST_ASSERT_TRUE(evaluateSummonInjectionPolicy(s, 0).allowed);

    s.autopilotState = 6;
    assertState(evaluateSummonInjectionPolicy(s, 0), false,
                SummonInjectionState::AutopilotActive);
}

void test_missing_or_stale_can_state_blocks_injection()
{
    SummonInjectionSnapshot missing = freshParkedSnapshot();
    missing.speedSeen = false;
    assertState(evaluateSummonInjectionPolicy(missing, 0), false,
                SummonInjectionState::MissingSpeed);

    SummonInjectionSnapshot stale = freshParkedSnapshot();
    assertState(evaluateSummonInjectionPolicy(stale, kSummonInjectionFreshnessMs + 1), false,
                SummonInjectionState::StaleDiState);
}

void test_contradictory_state_signals_block_injection()
{
    SummonInjectionSnapshot s = freshParkedSnapshot();
    s.secondaryGearSeen = true;
    s.secondaryGear = 4;
    assertState(evaluateSummonInjectionPolicy(s, 0), false,
                SummonInjectionState::ContradictoryGear);

    s.secondaryGearSeen = false;
    s.vehicleSpeedRaw = 501;
    assertState(evaluateSummonInjectionPolicy(s, 0), false,
                SummonInjectionState::ParkedButMoving);
}

void test_invalid_sna_signals_block_injection()
{
    SummonInjectionSnapshot s = freshParkedSnapshot();
    s.selfParkRequest = 15;
    assertState(evaluateSummonInjectionPolicy(s, 0), false,
                SummonInjectionState::InvalidSummonState);

    s.selfParkRequest = 0;
    s.vehicleSpeedRaw = kDiVehicleSpeedSnaRaw;
    assertState(evaluateSummonInjectionPolicy(s, 0), false,
                SummonInjectionState::InvalidSpeed);
}

void test_toggle_disabled_preserves_existing_permission()
{
    SummonInjectionSnapshot empty;
    assertState(summonOnlyInjectionDecision(false, empty, 0), true,
                SummonInjectionState::Disabled);
}

void test_handler_combines_exact_can_signals_for_parked_permission()
{
    HW3Handler handler;
    MockDriver driver;
    handler.enablePrint = false;

    CanFrame di = {.id = 280, .dlc = 8};
    di.data[2] = static_cast<uint8_t>(1U << 5);
    handler.handleMessage(di, driver);

    CanFrame speed = {.id = 599, .dlc = 8};
    speed.data[1] = static_cast<uint8_t>((kDiVehicleSpeedStationaryRaw & 0x0F) << 4);
    speed.data[2] = static_cast<uint8_t>(kDiVehicleSpeedStationaryRaw >> 4);
    handler.handleMessage(speed, driver);

    CanFrame ap = {.id = 921, .dlc = 8};
    ap.data[0] = 0;
    handler.handleMessage(ap, driver);

    CanFrame ui = {.id = 1016, .dlc = 8};
    ui.data[3] = 0;
    handler.handleMessage(ui, driver);

    summonOnlyInjectionRuntime = true;
    assertState(handler.summonOnlyInjectionDecisionAt(0), true,
                SummonInjectionState::AllowedParked);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_parked_and_stationary_allows_injection);
    RUN_TEST(test_parked_before_summon_begins_allows_injection);
    RUN_TEST(test_summon_active_and_moving_allows_injection);
    RUN_TEST(test_manual_driving_blocks_injection);
    RUN_TEST(test_autopilot_active_blocks_injection);
    RUN_TEST(test_moving_without_confirmed_summon_blocks_injection);
    RUN_TEST(test_transition_from_summon_to_manual_stops_immediately);
    RUN_TEST(test_transition_from_summon_to_autopilot_stops_immediately);
    RUN_TEST(test_missing_or_stale_can_state_blocks_injection);
    RUN_TEST(test_contradictory_state_signals_block_injection);
    RUN_TEST(test_invalid_sna_signals_block_injection);
    RUN_TEST(test_toggle_disabled_preserves_existing_permission);
    RUN_TEST(test_handler_combines_exact_can_signals_for_parked_permission);
    return UNITY_END();
}
