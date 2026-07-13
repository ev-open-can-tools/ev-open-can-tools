#pragma once

#include <stdint.h>

#include "shared_types.h"

inline constexpr uint32_t kSummonInjectionFreshnessMs = 500;
inline constexpr uint32_t kSummonInjectionRequestLeadMs = 2000;
inline constexpr uint16_t kDiVehicleSpeedStationaryRaw = 500;
inline constexpr uint16_t kDiVehicleSpeedMaxValidRaw = 4062;
inline constexpr uint16_t kDiVehicleSpeedSnaRaw = 4095;

inline Shared<bool> summonOnlyInjectionRuntime{false};

enum class SummonInjectionState : uint8_t
{
    Disabled,
    AllowedParked,
    AllowedSummon,
    MissingDiState,
    MissingSpeed,
    MissingAutopilotState,
    MissingSummonState,
    StaleDiState,
    StaleSpeed,
    StaleAutopilotState,
    StaleSummonState,
    InvalidGear,
    InvalidSpeed,
    InvalidAutopilotState,
    InvalidSummonState,
    ContradictoryGear,
    ParkedButMoving,
    AutopilotActive,
    AutonomyUnconfirmed,
    ManualDriving,
    MovingWithoutSummon,
};

struct SummonInjectionSnapshot
{
    bool diSeen = false;
    bool speedSeen = false;
    bool autopilotSeen = false;
    bool summonStateSeen = false;
    bool secondaryGearSeen = false;
    uint32_t diTimestampMs = 0;
    uint32_t speedTimestampMs = 0;
    uint32_t autopilotTimestampMs = 0;
    uint32_t summonStateTimestampMs = 0;
    uint32_t secondaryGearTimestampMs = 0;
    uint8_t diGear = 0;
    uint8_t secondaryGear = 0;
    uint8_t autopilotState = 15;
    uint8_t selfParkRequest = 15;
    uint16_t vehicleSpeedRaw = kDiVehicleSpeedSnaRaw;
    bool autonomyControlActive = false;
    bool summonRequestConfirmed = false;
};

struct SummonInjectionDecision
{
    bool allowed;
    SummonInjectionState state;
};

inline bool summonInjectionFresh(bool seen, uint32_t timestampMs, uint32_t nowMs)
{
    return seen && nowMs - timestampMs <= kSummonInjectionFreshnessMs;
}

inline bool summonInjectionKnownGear(uint8_t gear)
{
    return gear >= 1 && gear <= 4;
}

inline bool summonInjectionApActive(uint8_t state)
{
    return state >= 3 && state <= 6;
}

inline bool summonInjectionApInactive(uint8_t state)
{
    return state <= 2;
}

inline bool summonInjectionValidRequest(uint8_t request)
{
    return request <= 12;
}

inline bool summonInjectionRequestConfirmsSession(uint8_t request)
{
    switch (request)
    {
    case 1:  // SELF_PARK_FORWARD
    case 2:  // SELF_PARK_REVERSE
    case 4:  // PRIME
    case 5:  // PAUSE
    case 6:  // RESUME
    case 7:  // AUTO_SUMMON_FORWARD
    case 8:  // AUTO_SUMMON_REVERSE
    case 10: // AUTO_SUMMON_PRIMED
    case 11: // SMART_SUMMON
    case 12: // SMART_SUMMON_NO_OP
        return true;
    default:
        return false;
    }
}

inline bool summonInjectionRequestCancelsSession(uint8_t request)
{
    return request == 3 || request == 9;
}

inline SummonInjectionDecision evaluateSummonInjectionPolicy(const SummonInjectionSnapshot &s,
                                                             uint32_t nowMs)
{
    if (!s.diSeen)
        return {false, SummonInjectionState::MissingDiState};
    if (!s.speedSeen)
        return {false, SummonInjectionState::MissingSpeed};
    if (!s.autopilotSeen)
        return {false, SummonInjectionState::MissingAutopilotState};
    if (!s.summonStateSeen)
        return {false, SummonInjectionState::MissingSummonState};
    if (!summonInjectionFresh(true, s.diTimestampMs, nowMs))
        return {false, SummonInjectionState::StaleDiState};
    if (!summonInjectionFresh(true, s.speedTimestampMs, nowMs))
        return {false, SummonInjectionState::StaleSpeed};
    if (!summonInjectionFresh(true, s.autopilotTimestampMs, nowMs))
        return {false, SummonInjectionState::StaleAutopilotState};
    if (!summonInjectionFresh(true, s.summonStateTimestampMs, nowMs))
        return {false, SummonInjectionState::StaleSummonState};
    if (!summonInjectionKnownGear(s.diGear))
        return {false, SummonInjectionState::InvalidGear};
    if (s.vehicleSpeedRaw > kDiVehicleSpeedMaxValidRaw)
        return {false, SummonInjectionState::InvalidSpeed};
    if (summonInjectionApActive(s.autopilotState))
        return {false, SummonInjectionState::AutopilotActive};
    if (!summonInjectionApInactive(s.autopilotState))
        return {false, SummonInjectionState::InvalidAutopilotState};
    if (!summonInjectionValidRequest(s.selfParkRequest))
        return {false, SummonInjectionState::InvalidSummonState};

    if (s.secondaryGearSeen &&
        summonInjectionFresh(true, s.secondaryGearTimestampMs, nowMs))
    {
        if (!summonInjectionKnownGear(s.secondaryGear))
            return {false, SummonInjectionState::InvalidGear};
        if (s.secondaryGear != s.diGear)
            return {false, SummonInjectionState::ContradictoryGear};
    }

    const bool stationary = s.vehicleSpeedRaw == kDiVehicleSpeedStationaryRaw;
    const bool summonActive = s.autonomyControlActive && s.summonRequestConfirmed;

    if (s.diGear == 1 && !stationary)
        return {false, SummonInjectionState::ParkedButMoving};
    if (summonActive && s.diGear == 3)
        return {false, SummonInjectionState::ContradictoryGear};
    if (summonActive)
        return {true, SummonInjectionState::AllowedSummon};
    if (s.autonomyControlActive)
        return {false, SummonInjectionState::AutonomyUnconfirmed};
    if (s.diGear == 1)
        return {true, SummonInjectionState::AllowedParked};
    if (!stationary)
        return {false, SummonInjectionState::MovingWithoutSummon};
    return {false, SummonInjectionState::ManualDriving};
}

inline SummonInjectionDecision summonOnlyInjectionDecision(bool enabled,
                                                           const SummonInjectionSnapshot &snapshot,
                                                           uint32_t nowMs)
{
    if (!enabled)
        return {true, SummonInjectionState::Disabled};
    return evaluateSummonInjectionPolicy(snapshot, nowMs);
}

inline const char *summonInjectionStateName(SummonInjectionState state)
{
    switch (state)
    {
    case SummonInjectionState::Disabled:
        return "disabled";
    case SummonInjectionState::AllowedParked:
        return "allowed: parked and stationary";
    case SummonInjectionState::AllowedSummon:
        return "allowed: Summon confirmed active";
    case SummonInjectionState::MissingDiState:
        return "blocked: DI state missing";
    case SummonInjectionState::MissingSpeed:
        return "blocked: vehicle speed missing";
    case SummonInjectionState::MissingAutopilotState:
        return "blocked: Autopilot state missing";
    case SummonInjectionState::MissingSummonState:
        return "blocked: Summon request state missing";
    case SummonInjectionState::StaleDiState:
        return "blocked: DI state stale";
    case SummonInjectionState::StaleSpeed:
        return "blocked: vehicle speed stale";
    case SummonInjectionState::StaleAutopilotState:
        return "blocked: Autopilot state stale";
    case SummonInjectionState::StaleSummonState:
        return "blocked: Summon request state stale";
    case SummonInjectionState::InvalidGear:
        return "blocked: gear invalid";
    case SummonInjectionState::InvalidSpeed:
        return "blocked: vehicle speed invalid";
    case SummonInjectionState::InvalidAutopilotState:
        return "blocked: Autopilot state invalid";
    case SummonInjectionState::InvalidSummonState:
        return "blocked: Summon request state invalid";
    case SummonInjectionState::ContradictoryGear:
        return "blocked: gear signals contradictory";
    case SummonInjectionState::ParkedButMoving:
        return "blocked: Park conflicts with vehicle speed";
    case SummonInjectionState::AutopilotActive:
        return "blocked: Autopilot active";
    case SummonInjectionState::AutonomyUnconfirmed:
        return "blocked: autonomy active without confirmed Summon";
    case SummonInjectionState::ManualDriving:
        return "blocked: manual driving state";
    case SummonInjectionState::MovingWithoutSummon:
        return "blocked: moving without confirmed Summon";
    }
    return "blocked: unknown policy state";
}
