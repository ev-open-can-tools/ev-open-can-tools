#pragma once

#include <memory>
#include <algorithm>
#include <cmath>
#include "can_frame_types.h"
#include "drivers/can_driver.h"
#include "can_helpers.h"
#include "injection_policy.h"
#include "shared_types.h"
#include "log_buffer.h"

#ifndef NATIVE_BUILD
#ifdef ESP_PLATFORM
#include "platform/espidf_runtime.h"
#else
#include <Arduino.h>
#endif
#endif

inline LogRingBuffer logRing;

// Nag suppression torque is safety-clamped in firmware. Configuration and
// dashboard values must never expand these bounds.
[[maybe_unused]] static const float TORQUE_NM_MAX = +1.80f;
[[maybe_unused]] static const float TORQUE_NM_MIN = -1.80f;
static const uint16_t TORQUE_RAW_MAX = 0x08B6;
static const uint16_t TORQUE_RAW_MIN = 0x074E;

static uint16_t clampNagTorqueRaw(uint16_t value)
{
    return std::max(TORQUE_RAW_MIN, std::min(TORQUE_RAW_MAX, value));
}

enum class NagMode : uint8_t
{
    Disabled = 0,
    ModeA = 1,
    ModeB = 2,
    ModeC = 3,
};

static uint8_t clampNagMode(uint8_t mode)
{
    return mode <= static_cast<uint8_t>(NagMode::ModeC)
               ? mode
               : static_cast<uint8_t>(NagMode::Disabled);
}

static const char *nagModeName(uint8_t mode)
{
    switch (static_cast<NagMode>(clampNagMode(mode)))
    {
    case NagMode::ModeA:
        return "Mode A";
    case NagMode::ModeB:
        return "Mode B";
    case NagMode::ModeC:
        return "Mode C";
    default:
        return "Disabled";
    }
}

static bool nagModeAllowedForHardware(uint8_t mode, uint8_t hardwareMode)
{
    NagMode selected = static_cast<NagMode>(clampNagMode(mode));
    return hardwareMode != 2 || selected != NagMode::ModeC;
}

struct CarManagerBase
{
    Shared<int> speedProfile{1};
    Shared<bool> speedProfileAuto{true};
    Shared<bool> ADEnabled{false};
    Shared<bool> APActive{false};
    // Default Parked=true so the AP Injection Gate opens immediately on
    // module boot when the DI is asleep (e.g. car locked with Sentry on,
    // CAN ID 280 not broadcast). Once live DI/DIF gear frames arrive,
    // isVehicleParked() only keeps this true for definitive Park. INVALID
    // and SNA fail closed so unknown live gear cannot open the gate while AP
    // is inactive.
    Shared<bool> Parked{true};
    Shared<bool> Summoning{false};
    Shared<int> gatewayAutopilot{-1};
    Shared<bool> enablePrint{true};
    Shared<uint32_t> frameCount{0};
    Shared<uint32_t> framesSent{0};
    Shared<int> speedOffset{0};
    Shared<uint32_t> last921Ms{0};
    Shared<uint32_t> last923Ms{0};
    Shared<uint32_t> last280Ms{0};
    Shared<uint32_t> last390Ms{0};
    Shared<uint32_t> last599Ms{0};
    Shared<uint32_t> last1016Ms{0};
    Shared<uint32_t> last1021Ms{0};
    Shared<int> dasAutopilotStatus{-1};
    Shared<bool> summonPolicyDiSeen{false};
    Shared<bool> summonPolicySpeedSeen{false};
    Shared<bool> summonPolicyApSeen{false};
    Shared<bool> summonPolicyUiSeen{false};
    Shared<bool> summonPolicySecondaryGearSeen{false};
    Shared<uint8_t> summonPolicyDiGear{0};
    Shared<uint8_t> summonPolicySecondaryGear{0};
    Shared<uint8_t> summonPolicyApState{15};
    Shared<uint8_t> summonPolicySelfParkRequest{15};
    Shared<uint16_t> summonPolicyVehicleSpeedRaw{kDiVehicleSpeedSnaRaw};
    Shared<bool> summonPolicyAca{false};
    Shared<bool> summonPolicyRequestConfirmed{false};
    Shared<uint32_t> summonPolicyDiMs{0};
    Shared<uint32_t> summonPolicySpeedMs{0};
    Shared<uint32_t> summonPolicyApMs{0};
    Shared<uint32_t> summonPolicyUiMs{0};
    Shared<uint32_t> summonPolicySecondaryGearMs{0};
    Shared<uint32_t> summonPolicyRequestMs{0};

    unsigned long lastSummonActivityMs = 0;
    // Summon-vs-AP/TACC discrimination state. ACA (DI_autonomyControlActive)
    // alone is set during AP, TACC, and Smart Summon, so it cannot be the
    // sole gate signal. We only treat ACA as "summon active" when we have
    // also observed UI_selfParkRequest go non-zero during the current
    // autonomy episode. ACA falling edge clears sprSeen so the next ACA
    // rising edge (e.g. user engaging TACC after a completed summon) does
    // not falsely keep the gate open.
    bool sprSeen = false;
    bool lastAca = false;

    void (*onFrame)(const CanFrame &) = nullptr;
    void (*onSend)(uint8_t mux, bool ok) = nullptr;
    bool (*checkAD)() = nullptr;
    bool (*checkNag)() = nullptr;
    bool (*checkSummon)() = nullptr;
    bool (*checkIsa)() = nullptr;
    bool (*checkEvd)() = nullptr;

    bool injectionGateOpen() const
    {
        return (bool)APActive || (bool)Parked || (bool)Summoning;
    }

    static uint32_t diagnosticMillis()
    {
#ifndef NATIVE_BUILD
        return millis();
#else
        return 0;
#endif
    }

    void updateGateFrameDiagnostics(const CanFrame &frame)
    {
        uint32_t now = diagnosticMillis();
        if (frame.id == 921)
        {
            last921Ms = now;
        }
        else if (frame.id == 923)
        {
            last923Ms = now;
        }
        else if (frame.id == 280)
            last280Ms = now;
        else if (frame.id == 390)
            last390Ms = now;
        else if (frame.id == 599)
            last599Ms = now;
        else if (frame.id == 1016)
            last1016Ms = now;
        else if (frame.id == 1021)
            last1021Ms = now;
    }

    // Recompute Summoning from current sprSeen + lastAca state. Summoning
    // requires both: ACA bit currently set AND we have seen at least one
    // UI_selfParkRequest non-zero command in the current autonomy episode.
    // This excludes plain TACC (ACA=1, no spr) and post-AP ACA tail
    // (ACA blip with no fresh spr) from latching the gate.
    void recomputeSummoning()
    {
        Summoning = lastAca && sprSeen;
    }

    // Update summon state from UI_driverAssistControl (CAN ID 1016).
    // Tesla DBC: UI_selfParkRequest at byte 3 bits 4-7 (4=PRIME, 5=PAUSE,
    // 7/8=AUTO_SUMMON_FWD/REV, 11=SMART_SUMMON, 0=NONE). Records that a
    // summon command has been issued during the current autonomy episode.
    void updateSummonFrom1016(const CanFrame &frame)
    {
        if (frame.dlc < 4)
            return;
        uint8_t spr = static_cast<uint8_t>((frame.data[3] >> 4) & 0x0F);
        if (spr != 0)
            sprSeen = true;
        if (frame.dlc == 8)
        {
            summonPolicyUiSeen = true;
            summonPolicyUiMs = diagnosticMillis();
            summonPolicySelfParkRequest = spr;
            if (summonInjectionRequestConfirmsSession(spr))
            {
                summonPolicyRequestConfirmed = true;
                summonPolicyRequestMs = diagnosticMillis();
            }
            else if (summonInjectionRequestCancelsSession(spr))
                summonPolicyRequestConfirmed = false;
        }
        recomputeSummoning();
    }

    // Update summon state from DI_systemStatus (CAN ID 280).
    // Tesla DBC: DI_autonomyControlActive at bit 50 (byte 6 bit 2). Held
    // high while the DI is being driven by AP, TACC, Smart Summon, etc.
    // ACA falling edge ends the autonomy episode and clears sprSeen so a
    // subsequent TACC engagement (ACA=1 again) does not re-latch the gate.
    void updateSummonFromDISystemStatus(const CanFrame &frame)
    {
        if (frame.dlc < 7)
            return;
        bool aca = (frame.data[6] & 0x04) != 0;
        uint32_t now = diagnosticMillis();
        if (!lastAca && aca &&
            (!summonPolicyRequestConfirmed ||
             now - (uint32_t)summonPolicyRequestMs > kSummonInjectionRequestLeadMs))
            summonPolicyRequestConfirmed = false;
        if (lastAca && !aca)
        {
            sprSeen = false;
            summonPolicyRequestConfirmed = false;
        }
        lastAca = aca;
        if (frame.dlc == 8)
        {
            summonPolicyDiSeen = true;
            summonPolicyDiMs = now;
            summonPolicyDiGear = readDIGear(frame);
            summonPolicyAca = aca;
        }
        recomputeSummoning();
    }

    void updateSummonPolicySpeed(const CanFrame &frame)
    {
        if (frame.dlc != 8)
            return;
        summonPolicySpeedSeen = true;
        summonPolicySpeedMs = diagnosticMillis();
        summonPolicyVehicleSpeedRaw = readDIVehicleSpeedRaw(frame);
    }

    void updateSummonPolicySecondaryGear(uint8_t gear)
    {
        summonPolicySecondaryGearSeen = true;
        summonPolicySecondaryGearMs = diagnosticMillis();
        summonPolicySecondaryGear = gear;
    }

    void updateSummonPolicyAutopilot(const CanFrame &frame, uint8_t state)
    {
        if (frame.dlc != 8)
            return;
        summonPolicyApSeen = true;
        summonPolicyApMs = diagnosticMillis();
        summonPolicyApState = state;
    }

    void resetSummonOnlyPolicyState()
    {
        summonPolicyDiSeen = false;
        summonPolicySpeedSeen = false;
        summonPolicyApSeen = false;
        summonPolicyUiSeen = false;
        summonPolicySecondaryGearSeen = false;
        summonPolicyDiGear = 0;
        summonPolicySecondaryGear = 0;
        summonPolicyApState = 15;
        summonPolicySelfParkRequest = 15;
        summonPolicyVehicleSpeedRaw = kDiVehicleSpeedSnaRaw;
        summonPolicyAca = false;
        summonPolicyRequestConfirmed = false;
        summonPolicyDiMs = 0;
        summonPolicySpeedMs = 0;
        summonPolicyApMs = 0;
        summonPolicyUiMs = 0;
        summonPolicySecondaryGearMs = 0;
        summonPolicyRequestMs = 0;
    }

    SummonInjectionSnapshot summonOnlyInjectionSnapshot() const
    {
        SummonInjectionSnapshot snapshot;
        snapshot.diSeen = summonPolicyDiSeen;
        snapshot.speedSeen = summonPolicySpeedSeen;
        snapshot.autopilotSeen = summonPolicyApSeen;
        snapshot.summonStateSeen = summonPolicyUiSeen;
        snapshot.secondaryGearSeen = summonPolicySecondaryGearSeen;
        snapshot.diTimestampMs = summonPolicyDiMs;
        snapshot.speedTimestampMs = summonPolicySpeedMs;
        snapshot.autopilotTimestampMs = summonPolicyApMs;
        snapshot.summonStateTimestampMs = summonPolicyUiMs;
        snapshot.secondaryGearTimestampMs = summonPolicySecondaryGearMs;
        snapshot.diGear = summonPolicyDiGear;
        snapshot.secondaryGear = summonPolicySecondaryGear;
        snapshot.autopilotState = summonPolicyApState;
        snapshot.selfParkRequest = summonPolicySelfParkRequest;
        snapshot.vehicleSpeedRaw = summonPolicyVehicleSpeedRaw;
        snapshot.autonomyControlActive = summonPolicyAca;
        snapshot.summonRequestConfirmed = summonPolicyRequestConfirmed;
        return snapshot;
    }

    SummonInjectionDecision summonOnlyInjectionDecisionAt(uint32_t nowMs) const
    {
        return ::summonOnlyInjectionDecision(summonOnlyInjectionRuntime,
                                             summonOnlyInjectionSnapshot(), nowMs);
    }

    // Force Summoning off and reset sprSeen when the vehicle is observed
    // in Park with no active autonomy episode, so a manual P->D shift
    // afterwards correctly waits for AP. During Smart Summon startup the
    // DI can report ACA=1 while gear is still P; keep sprSeen latched so
    // it survives the pending shift out of Park.
    void clearSummonOnPark()
    {
        Summoning = false;
        sprSeen = false;
#ifndef NATIVE_BUILD
        lastSummonActivityMs = 0;
#endif
    }

    void clearSummonOnParkIfAcaInactive(uint8_t gear)
    {
        if (gear == 1 && !lastAca)
            clearSummonOnPark();
    }

    bool shouldInjectSpeedProfile() const
    {
#if defined(ESP32_DASHBOARD)
        return !speedProfileAuto;
#else
        return true;
#endif
    }

    virtual void handleMessage(CanFrame &frame, CanDriver &driver) = 0;
    virtual const uint32_t *filterIds() const = 0;
    virtual uint8_t filterIdCount() const = 0;
    virtual ~CarManagerBase() = default;
};

/**
 * SummonUnlockHandler — minimal "Summon EU Unlock for all cars" (HW3).
 *
 * On UI_autopilotControl (CAN 1021 / 0x3FD) multiplexer 1 it clears the summon
 * EU restriction bit (bit 19) and sets the summon enable bit (bit 46 for HW3, or
 * bit 47 when built with -D SUMMON_HW4), then sends the modified frame. Injects
 * nothing else.
 *
 * The injection can be gated four ways at build time:
 *   -D SUMMON_GATE_NONE  inject on every 1021 mux 1 frame (no gate)
 *   -D SUMMON_GATE_AP    inject when injectionGateOpen() — AP active OR parked
 *                        OR summoning (the upstream INJECTION_AFTER_AP gate)
 *   -D SUMMON_GATE_PARK  inject when Parked OR summoning, but NOT while AP is
 *                        active — avoids corrupting UI_autopilotControl during
 *                        live Autopilot (which faults AP / "red steering wheel")
 *   (default)            inject ONLY while a genuine Summon is active
 *
 * Summoning is the shared base-class discrimination: it requires both the ACA
 * bit (DI_autonomyControlActive, CAN 280) AND a UI_selfParkRequest command
 * (CAN 1016) during the current autonomy episode, so plain TACC/AP does not
 * open the summon-only gate. The gated modes observe 280/390(/921 for AP)/1016
 * to drive that state, but inject nothing on those frames.
 *
 * This is the compiled-in equivalent of the single-rule plugin for
 * non-dashboard boards that have no plugin engine (e.g. feather_rp2040_can).
 *
 * Enable the handler with build flag: -D SUMMON_UNLOCK_ONLY
 */
struct SummonUnlockHandler : public CarManagerBase
{
#if defined(SUMMON_GATE_NONE)
    const uint32_t *filterIds() const override
    {
        static constexpr uint32_t ids[] = {1021};
        return ids;
    }
    uint8_t filterIdCount() const override { return 1; }
#elif defined(SUMMON_GATE_AP)
    const uint32_t *filterIds() const override
    {
        static constexpr uint32_t ids[] = {280, 390, 921, 1016, 1021};
        return ids;
    }
    uint8_t filterIdCount() const override { return 5; }
#else
    const uint32_t *filterIds() const override
    {
        static constexpr uint32_t ids[] = {280, 390, 1016, 1021};
        return ids;
    }
    uint8_t filterIdCount() const override { return 4; }
#endif

    void handleMessage(CanFrame &frame, CanDriver &driver) override
    {
        if (onFrame)
            onFrame(frame);
        updateGateFrameDiagnostics(frame);

#if !defined(SUMMON_GATE_NONE)
        // Observe-only frames that drive the Summoning / AP-gate state.
        if (frame.id == 280)
        {
            if (frame.dlc < 3)
                return;
            uint8_t diGear = readDIGear(frame);
            Parked = isVehicleParked(diGear);
            updateSummonFromDISystemStatus(frame);
            clearSummonOnParkIfAcaInactive(diGear);
            return;
        }
        if (frame.id == 390)
        {
            if (frame.dlc < 8)
                return;
            uint8_t difGear = readVehicleGear(frame);
            Parked = isVehicleParked(difGear);
            clearSummonOnParkIfAcaInactive(difGear);
            return;
        }
#if defined(SUMMON_GATE_AP)
        if (frame.id == 921)
        {
            if (frame.dlc < 1)
                return;
            APActive = isDASAutopilotActive(readDASAutopilotStatus(frame));
            return;
        }
#endif
        if (frame.id == 1016)
        {
            if (frame.dlc < 6)
                return;
            updateSummonFrom1016(frame);
            return;
        }
#endif

        // Summon EU Unlock injection on 1021 mux 1.
        if (frame.id != 1021)
            return;
        if (frame.dlc < 8)
            return;
        if (readMuxID(frame) != 1)
            return;
#if defined(SUMMON_GATE_NONE)
        // ungated: always inject
#elif defined(SUMMON_GATE_AP)
        if (!injectionGateOpen())
            return;
#elif defined(SUMMON_GATE_PARK)
        if (!((bool)Parked || (bool)Summoning))
            return;
#else
        if (!Summoning)
            return;
#endif
        setBit(frame, 19, false); // clear summon EU restriction bit
#if defined(SUMMON_HW4)
        setBit(frame, 47, true); // set summon enable bit (HW4)
#else
        setBit(frame, 46, true); // set summon enable bit (HW3)
#endif
        framesSent++;
        driver.send(frame);
        if (onSend)
            onSend(1, true);
    }
};

struct LegacyHandler : public CarManagerBase
{
    const uint32_t *filterIds() const override
    {
        static constexpr uint32_t ids[] = {69, 280, 390, 599, 921, 1006, 1016};
        return ids;
    }
    uint8_t filterIdCount() const override { return 7; }

    void handleMessage(CanFrame &frame, CanDriver &driver) override
    {
        if (onFrame)
            onFrame(frame);
        updateGateFrameDiagnostics(frame);
        // STW_ACTN_RQ (0x045 = 69): Follow-Distance-Stalk as Source for Profile Mapping
        // byte[1]: 0x00=Pos1, 0x21=Pos2, 0x42=Pos3, 0x64=Pos4, 0x85=Pos5, 0xA6=Pos6, 0xC8=Pos7
        if (frame.id == 69)
        {
            if (frame.dlc < 2)
                return;
            if (!speedProfileAuto)
                return;
            uint8_t pos = frame.data[1] >> 5;
            if (pos <= 1)
                speedProfile = 2;
            else if (pos == 2)
                speedProfile = 1;
            else
                speedProfile = 0;
            return;
        }
        if (frame.id == 280)
        {
            if (frame.dlc < 3)
                return;
            {
                uint8_t diGear = readDIGear(frame);
                Parked = isVehicleParked(diGear);
                // Only clear Summoning on a *definitive* Park (gear==1).
                // SNA (7) and INVALID (0) can blip during gear transitions
                // (e.g. during a Summon shift to Reverse) and would
                // otherwise drop the gate mid-summon.
                updateSummonFromDISystemStatus(frame);
                clearSummonOnParkIfAcaInactive(diGear);
            }
            return;
        }
        if (frame.id == 390)
        {
            if (frame.dlc < 8)
                return;
            {
                uint8_t difGear = readVehicleGear(frame);
                Parked = isVehicleParked(difGear);
                updateSummonPolicySecondaryGear(difGear);
                // Only clear Summoning on a *definitive* Park (gear==1).
                // SNA (7) and INVALID (0) can blip during gear transitions.
                clearSummonOnParkIfAcaInactive(difGear);
            }
            return;
        }
        if (frame.id == 599)
        {
            updateSummonPolicySpeed(frame);
            return;
        }
        if (frame.id == 921)
        {
            if (frame.dlc < 1)
                return;
            uint8_t status = readDASAutopilotStatus(frame);
            dasAutopilotStatus = status;
            APActive = isDASAutopilotActive(status);
            updateSummonPolicyAutopilot(frame, status);
            return;
        }
        if (frame.id == 1016)
        {
            updateSummonFrom1016(frame);
            return;
        }
        if (frame.id == 1006)
        {
            if (frame.dlc < 8)
                return;
            auto index = readMuxID(frame);
            if (index == 0)
                ADEnabled = isADSelectedInUI(frame) && (!checkAD || checkAD());
            if (index == 0 && ADEnabled && shouldInjectSpeedProfile() && (!checkAD || checkAD()))
            {
                setSpeedProfileV12V13(frame, speedProfile);
                setBit(frame, 46, true);
                bool ok = driver.send(frame);
                if (ok)
                    framesSent++;
                if (onSend)
                    onSend(0, ok);
            }
            if (index == 1 && (!checkNag || checkNag()))
            {
#if !defined(ESP32_DASHBOARD)
                setBit(frame, 19, false);
                framesSent++;
                driver.send(frame);
                if (onSend)
                    onSend(1, true);
#endif
            }
            if (index == 0 && enablePrint)
            {
                char buf[LogRingBuffer::kMaxMsgLen];
                snprintf(buf, sizeof(buf), "LegacyHandler: AD: %d, Profile: %d",
                         (bool)ADEnabled, (int)speedProfile);
                logRing.push(buf,
#ifndef NATIVE_BUILD
                             millis()
#else
                             0
#endif
                );
#ifndef NATIVE_BUILD
                Serial.println(buf);
#endif
            }
        }
    }
};

struct HW3Handler : public CarManagerBase
{
    const uint32_t *filterIds() const override
    {
        static constexpr uint32_t ids[] = {280, 390, 599, 921, 1016, 1021, 2047};
        return ids;
    }
    uint8_t filterIdCount() const override { return 7; }

    void handleMessage(CanFrame &frame, CanDriver &driver) override
    {
        if (onFrame)
            onFrame(frame);
        updateGateFrameDiagnostics(frame);
        if (frame.id == 280)
        {
            if (frame.dlc < 3)
                return;
            {
                uint8_t diGear = readDIGear(frame);
                Parked = isVehicleParked(diGear);
                // Only clear Summoning on a *definitive* Park (gear==1).
                // SNA (7) and INVALID (0) can blip during gear transitions
                // (e.g. during a Summon shift to Reverse) and would
                // otherwise drop the gate mid-summon.
                updateSummonFromDISystemStatus(frame);
                clearSummonOnParkIfAcaInactive(diGear);
            }
            return;
        }
        if (frame.id == 390)
        {
            if (frame.dlc < 8)
                return;
            {
                uint8_t difGear = readVehicleGear(frame);
                Parked = isVehicleParked(difGear);
                updateSummonPolicySecondaryGear(difGear);
                // Only clear Summoning on a *definitive* Park (gear==1).
                // SNA (7) and INVALID (0) can blip during gear transitions.
                clearSummonOnParkIfAcaInactive(difGear);
            }
            return;
        }
        if (frame.id == 599)
        {
            updateSummonPolicySpeed(frame);
            return;
        }
        if (frame.id == 1016)
        {
            if (frame.dlc < 6)
                return;
            updateSummonFrom1016(frame);
            if (!speedProfileAuto)
                return;
            uint8_t followDistance = (frame.data[5] & 0b11100000) >> 5;
            switch (followDistance)
            {
            case 1:
                speedProfile = 2;
                break;
            case 2:
                speedProfile = 1;
                break;
            case 3:
                speedProfile = 0;
                break;
            default:
                break;
            }
            return;
        }
        if (frame.id == 921)
        {
            if (frame.dlc < 1)
                return;
            uint8_t status = readDASAutopilotStatus(frame);
            dasAutopilotStatus = status;
            APActive = isDASAutopilotActive(status);
            updateSummonPolicyAutopilot(frame, status);
            return;
        }
        if (frame.id == 2047)
        {
            if (frame.dlc < 6)
                return;
            if (readMuxID(frame) != 2)
                return;

            uint8_t next = readGTWAutopilot(frame);
            int prev = gatewayAutopilot;
            gatewayAutopilot = next;

            if (enablePrint && prev != next)
            {
                char buf[LogRingBuffer::kMaxMsgLen];
                snprintf(buf, sizeof(buf), "HW3Handler: GTW_autopilot: %d -> %u (%s)",
                         prev, (unsigned int)next, describeGTWAutopilot(next));
                logRing.push(buf,
#ifndef NATIVE_BUILD
                             millis()
#else
                             0
#endif
                );
#ifndef NATIVE_BUILD
                Serial.println(buf);
#endif
            }
            return;
        }
        if (frame.id == 1021)
        {
            if (frame.dlc < 8)
                return;
            auto index = readMuxID(frame);
            if (index == 0)
                ADEnabled = isADSelectedInUI(frame) && (!checkAD || checkAD());
            if (index == 0 && ADEnabled && (!checkAD || checkAD()))
            {
                speedOffset = std::max(std::min(((uint8_t)((frame.data[3] >> 1) & 0x3F) - 30) * 5, 100), 0);
#if defined(ESP32_DASHBOARD)
                if (shouldInjectSpeedProfile())
                {
                    setSpeedProfileV12V13(frame, speedProfile);
                    setBit(frame, 46, true);
                    bool ok = driver.send(frame);
                    if (ok)
                        framesSent++;
                    if (onSend)
                        onSend(0, ok);
                }
#else
                setBit(frame, 46, true);
                framesSent++;
                driver.send(frame);
                if (onSend)
                    onSend(0, true);
#endif
            }
            if (index == 1)
            {
#if !defined(ESP32_DASHBOARD)
                bool modified = false;
#if defined(ENHANCED_AUTOPILOT)
                if (enhancedAutopilotRuntime && enhancedAutopilotInjectionAllowed(injectionGateOpen()))
                {
                    setBit(frame, 19, false);
                    setBit(frame, 46, true);
                    modified = true;
                }
#else
                // No dashboard: nag suppression always-on (RP2040, M4)
                setBit(frame, 19, false);
                modified = true;
#endif
                if (modified)
                {
                    framesSent++;
                    driver.send(frame);
                    if (onSend)
                        onSend(1, true);
                }
#endif
            }
            if (index == 0 && enablePrint)
            {
                char buf[LogRingBuffer::kMaxMsgLen];
                snprintf(buf, sizeof(buf), "HW3Handler: AD: %d, Profile: %d, Offset: %d",
                         (bool)ADEnabled, (int)speedProfile, (int)speedOffset);
                logRing.push(buf,
#ifndef NATIVE_BUILD
                             millis()
#else
                             0
#endif
                );
#ifndef NATIVE_BUILD
                Serial.println(buf);
#endif
            }
        }
    }
};

/**
 * NagHandler — Autosteer nag suppression (counter+1 echo method)
 *
 * Built-in, runtime-selectable nag suppression. This path does not use the
 * plugin engine. Modes mirror the reviewed nag_echo implementation:
 * - A: fixed +1.80 Nm echo on every eligible 0x370 frame.
 * - B: +1.80/+1.50/-1.50/-1.80 Nm cycle with 1 s burst / 1.5 s pause.
 * - C: DAS hands-on state machine, gated by fresh DAS status (0x399) and
 *   0x129 steering context. Dashboard HW4 selection fails closed to Off.
 *
 * Common echo behavior:
 * - Listens for CAN 880 (0x370) = EPAS3P_sysStatus
 * - When handsOnLevel = 0, or Mode C receives an eligible real level-1 frame:
 *   1. Copies the real frame
 *   2. Sets byte 3 = 0xB6 (fixed torsionBarTorque = 1.80 Nm)
 *   3. Sets byte 4 |= 0x40 (handsOnLevel = 1)
 *   4. Increments counter (byte 6 lower nibble + 1)
 *   5. Recalculates checksum (byte 7)
 * - The real EPAS frame with the same counter arrives AFTER -> rejected as duplicate
 *
 * Tested: Model Y Performance 2022 HW3, Basic Autopilot
 * Bus: Party CAN only. Connector/pins vary by vehicle; see docs/nag-killer.md.
 *
 * Enable with build flag: -D NAG_KILLER
 */
struct NagHandler : public CarManagerBase
{
    Shared<bool> nagKillerActive{true};
    Shared<uint8_t> nagMode{static_cast<uint8_t>(NagMode::ModeA)};
    Shared<uint8_t> nagHardwareMode{0};
    Shared<uint32_t> nagEchoCount{0};

    static constexpr uint32_t kTargetId = 0x370;
    static constexpr uint32_t kLegacyApStateId = 0x399;
    static constexpr uint32_t kHw4ApStateId = 0x39B;
    static constexpr uint32_t kSteeringId = 0x129;
    static constexpr uint32_t kContextFreshMs = 1000;
    static constexpr uint32_t kModeBBurstMs = 1000;
    static constexpr uint32_t kModeBPauseMs = 1500;
    static constexpr uint32_t kModeBTorqueStepMs = 200;

    void setMode(uint8_t mode) { nagMode = clampNagMode(mode); }
    void setHardwareMode(uint8_t mode) { nagHardwareMode = mode <= 2 ? mode : 0; }

    const uint32_t *filterIds() const override
    {
        static constexpr uint32_t ids[] = {kTargetId};
        return ids;
    }
    uint8_t filterIdCount() const override { return 1; }

    const uint32_t *modeFilterIds() const
    {
        static constexpr uint32_t legacyIds[] = {kTargetId, kLegacyApStateId, kSteeringId};
        static constexpr uint32_t hw4Ids[] = {kTargetId, kHw4ApStateId, kSteeringId};
        return static_cast<uint8_t>(nagHardwareMode) == 2 ? hw4Ids : legacyIds;
    }

    uint8_t modeFilterIdCount(uint8_t mode) const
    {
        return clampNagMode(mode) == static_cast<uint8_t>(NagMode::ModeC) ? 3 : 1;
    }

    void handleMessage(CanFrame &frame, CanDriver &driver) override
    {
        handleMessageAt(frame, driver, diagnosticMillis());
    }

    void handleMessageAt(CanFrame &frame, CanDriver &driver, uint32_t now,
                         bool transmissionAllowed = true)
    {
        uint8_t selectedMode = clampNagMode(nagMode);
        uint8_t selectedHardwareMode = static_cast<uint8_t>(nagHardwareMode);
        if (!nagModeAllowedForHardware(selectedMode, selectedHardwareMode))
            selectedMode = static_cast<uint8_t>(NagMode::Disabled);
        if (selectedMode != activeMode_ || selectedHardwareMode != activeHardwareMode_)
            resetModeState(selectedMode, selectedHardwareMode, now);

        if (frame.id == apStateId(selectedHardwareMode))
        {
            updateApState(frame, now);
            if (onFrame)
                onFrame(frame);
            return;
        }
        if (frame.id == kSteeringId)
        {
            updateSteering(frame, now);
            if (onFrame)
                onFrame(frame);
            return;
        }
        if (frame.id != kTargetId)
            return;
        if (onFrame)
            onFrame(frame);
        if (frame.dlc < 8)
            return;

        uint8_t handsOn = (frame.data[4] >> 6) & 0x03;
        uint16_t torqueRaw = static_cast<uint16_t>((frame.data[2] & 0x0F) << 8) |
                             frame.data[3];

        // Compare the complete configured 12-bit torque value. A byte-3-only
        // comparison can mistake unrelated negative/positive values for our
        // own echo.
        bool isOwnEcho = (handsOn == 1 && torqueRaw == TORQUE_RAW_MAX) ||
                         matchesLastEcho(frame);

        bool handsOnEligible = handsOn == 0 ||
                               (selectedMode == static_cast<uint8_t>(NagMode::ModeC) &&
                                handsOn == 1);
        if (!transmissionAllowed || !nagKillerActive || !nagKillerRuntime || selectedMode == 0 || isOwnEcho ||
            !handsOnEligible)
            return;

        uint16_t torque = TORQUE_RAW_MAX;
        bool setHandsOn = true;
        if (!decideInjection(selectedMode, now, torque, setHandsOn))
            return;

        CanFrame echo;
        echo.id = kTargetId;
        echo.dlc = 8;

        echo.data[0] = frame.data[0];
        echo.data[1] = frame.data[1];
        torque = clampNagTorqueRaw(torque);
        echo.data[2] = static_cast<uint8_t>((frame.data[2] & 0xF0) | ((torque >> 8) & 0x0F));
        echo.data[5] = frame.data[5];

        echo.data[3] = static_cast<uint8_t>(torque & 0xFF);

        echo.data[4] = setHandsOn ? static_cast<uint8_t>(frame.data[4] | 0x40)
                                  : frame.data[4];

        // Counter + 1
        uint8_t cnt = (frame.data[6] & 0x0F);
        cnt = (cnt + 1) & 0x0F;
        echo.data[6] = (frame.data[6] & 0xF0) | cnt;

        // Checksum: sum(byte0..byte6) + 0x73
        uint16_t sum = echo.data[0] + echo.data[1] + echo.data[2] + echo.data[3] + echo.data[4] + echo.data[5] + echo.data[6];
        echo.data[7] = static_cast<uint8_t>((sum + 0x73) & 0xFF);

        bool ok = driver.send(echo);
        if (ok)
        {
            framesSent++;
            nagEchoCount++;
            lastEcho_ = echo;
            lastEchoValid_ = true;
        }
        if (onSend)
            onSend(0, ok);

        if (enablePrint && (nagEchoCount % 500 == 1))
        {
            char buf[LogRingBuffer::kMaxMsgLen];
            snprintf(buf, sizeof(buf), "NagHandler: echo=%u",
                     (unsigned int)(uint32_t)nagEchoCount);
            logRing.push(buf,
#ifndef NATIVE_BUILD
                         millis()
#else
                         0
#endif
            );
#ifndef NATIVE_BUILD
            Serial.println(buf);
#endif
        }
    }

private:
    uint8_t activeMode_ = 0xFF;
    uint8_t activeHardwareMode_ = 0xFF;
    uint8_t modeBTorqueIndex_ = 0;
    uint32_t modeEnteredMs_ = 0;
    uint32_t modeBLastStepMs_ = 0;
    uint8_t apState_ = 0;
    uint8_t handsOnState_ = 0;
    float steeringAngleDeg_ = 0.0f;
    uint32_t lastApStateMs_ = 0;
    uint32_t lastSteeringMs_ = 0;
    uint32_t handsOnStateEnteredMs_ = 0;
    bool apStateSeen_ = false;
    bool steeringSeen_ = false;
    bool handsOnStateSeen_ = false;
    uint16_t walkSeed_ = 0;
    float lastModeCTorqueNm_ = 0.0f;
    CanFrame lastEcho_{};
    bool lastEchoValid_ = false;

    static uint16_t torqueNmToRaw(float torqueNm)
    {
        torqueNm = std::max(TORQUE_NM_MIN, std::min(TORQUE_NM_MAX, torqueNm));
        float scaled = (torqueNm + 20.5f) * 100.0f + 0.5f;
        return clampNagTorqueRaw(static_cast<uint16_t>(scaled));
    }

    static uint32_t apStateId(uint8_t hardwareMode)
    {
        return hardwareMode == 2 ? kHw4ApStateId : kLegacyApStateId;
    }

    void resetModeState(uint8_t mode, uint8_t hardwareMode, uint32_t now)
    {
        activeMode_ = mode;
        activeHardwareMode_ = hardwareMode;
        modeBTorqueIndex_ = 0;
        modeEnteredMs_ = now;
        modeBLastStepMs_ = now;
        apState_ = 0;
        handsOnState_ = 0;
        steeringAngleDeg_ = 0.0f;
        lastApStateMs_ = 0;
        lastSteeringMs_ = 0;
        handsOnStateEnteredMs_ = 0;
        apStateSeen_ = false;
        steeringSeen_ = false;
        handsOnStateSeen_ = false;
        walkSeed_ = 0;
        lastModeCTorqueNm_ = 0.0f;
        lastEchoValid_ = false;
    }

    bool matchesLastEcho(const CanFrame &frame) const
    {
        if (!lastEchoValid_ || frame.id != lastEcho_.id || frame.dlc != lastEcho_.dlc)
            return false;
        return std::equal(frame.data, frame.data + frame.dlc, lastEcho_.data);
    }

    void updateApState(const CanFrame &frame, uint32_t now)
    {
        if (frame.dlc < 6)
            return;
        uint8_t apState = readDASAutopilotStatus(frame);
        uint8_t handsOnState = readDASAutopilotHandsOnState(frame);
        apState_ = apState;
        lastApStateMs_ = now;
        apStateSeen_ = true;
        if (!handsOnStateSeen_ || handsOnState != handsOnState_)
        {
            handsOnState_ = handsOnState;
            handsOnStateEnteredMs_ = now;
            handsOnStateSeen_ = true;
        }
    }

    void updateSteering(const CanFrame &frame, uint32_t now)
    {
        if (frame.dlc < 4 || readSCCMSteeringAngleValidity(frame) != 1)
        {
            steeringSeen_ = false;
            return;
        }
        steeringAngleDeg_ = readSCCMSteeringAngle(frame);
        lastSteeringMs_ = now;
        steeringSeen_ = true;
    }

    bool decideInjection(uint8_t selectedMode, uint32_t now, uint16_t &torque,
                         bool &setHandsOn)
    {
        if (selectedMode == static_cast<uint8_t>(NagMode::ModeA))
        {
            torque = TORQUE_RAW_MAX;
            setHandsOn = true;
            return true;
        }

        if (selectedMode == static_cast<uint8_t>(NagMode::ModeB))
        {
            constexpr uint16_t kModeBTorques[] = {0x08B6, 0x0898, 0x076C, 0x074E};
            constexpr uint32_t kCycleMs = kModeBBurstMs + kModeBPauseMs;
            if ((now - modeEnteredMs_) % kCycleMs >= kModeBBurstMs)
                return false;
            if (now - modeBLastStepMs_ >= kModeBTorqueStepMs)
            {
                modeBTorqueIndex_ = static_cast<uint8_t>((modeBTorqueIndex_ + 1) % 4);
                modeBLastStepMs_ = now;
            }
            torque = kModeBTorques[modeBTorqueIndex_];
            setHandsOn = true;
            return true;
        }

        if (selectedMode != static_cast<uint8_t>(NagMode::ModeC) ||
            !apStateSeen_ || !steeringSeen_ || !handsOnStateSeen_ ||
            now - lastApStateMs_ > kContextFreshMs ||
            now - lastSteeringMs_ > kContextFreshMs ||
            apState_ < 3 || apState_ > 6 ||
            steeringAngleDeg_ < -5.0f || steeringAngleDeg_ > 5.0f)
            return false;

        float torqueNm = 0.0f;
        if (handsOnState_ == 2)
        {
            if (now - handsOnStateEnteredMs_ < 2000)
                return false;
            walkSeed_ = static_cast<uint16_t>(walkSeed_ * 1103u + 12345u);
            float delta = (static_cast<int>(walkSeed_ & 0x1F) - 16) * 0.05f;
            float magnitude = std::fabs(lastModeCTorqueNm_) + delta;
            magnitude = std::max(0.5f, std::min(TORQUE_NM_MAX, magnitude));
            torqueNm = steeringAngleDeg_ > 0.0f ? -magnitude : magnitude;
            lastModeCTorqueNm_ = torqueNm;
        }
        else if (handsOnState_ == 3)
        {
            if (now - handsOnStateEnteredMs_ < 1000)
                return false;
            uint32_t phase = (now - handsOnStateEnteredMs_ - 1000) % 1000;
            torqueNm = phase < 500 ? -1.8f + (phase / 500.0f) * 3.6f
                                   : 1.8f - ((phase - 500) / 500.0f) * 3.6f;
        }
        else
        {
            return false;
        }

        torque = torqueNmToRaw(torqueNm);
        setHandsOn = std::fabs(torqueNm) >= 1.0f;
        return true;
    }
};

struct HW4Handler : public CarManagerBase
{
    const uint32_t *filterIds() const override
    {
#if defined(ISA_SPEED_CHIME_SUPPRESS) && !defined(ESP32_DASHBOARD)
        // MCP2515 has six hardware filters. Keep 0x399 for ISA suppression
        // and use DI_systemStatus (0x118) as the gear source in this build.
        static constexpr uint32_t ids[] = {280, 599, 921, 923, 1016, 1021, 2047};
        return ids;
    }
    uint8_t filterIdCount() const override { return 7; }
#else
        static constexpr uint32_t ids[] = {280, 390, 599, 923, 1016, 1021, 2047};
        return ids;
    }
    uint8_t filterIdCount() const override { return 7; }
#endif

    void handleMessage(CanFrame &frame, CanDriver &driver) override
    {
        if (onFrame)
            onFrame(frame);
        updateGateFrameDiagnostics(frame);
        if (frame.id == 280)
        {
            if (frame.dlc < 3)
                return;
            {
                uint8_t diGear = readDIGear(frame);
                Parked = isVehicleParked(diGear);
                // Only clear Summoning on a *definitive* Park (gear==1).
                // SNA (7) and INVALID (0) can blip during gear transitions
                // (e.g. during a Summon shift to Reverse) and would
                // otherwise drop the gate mid-summon.
                updateSummonFromDISystemStatus(frame);
                clearSummonOnParkIfAcaInactive(diGear);
            }
            return;
        }
        if (frame.id == 390)
        {
            if (frame.dlc < 8)
                return;
            {
                uint8_t difGear = readVehicleGear(frame);
                Parked = isVehicleParked(difGear);
                updateSummonPolicySecondaryGear(difGear);
                // Only clear Summoning on a *definitive* Park (gear==1).
                // SNA (7) and INVALID (0) can blip during gear transitions.
                clearSummonOnParkIfAcaInactive(difGear);
            }
            return;
        }
        if (frame.id == 599)
        {
            updateSummonPolicySpeed(frame);
            return;
        }
        if (frame.id == 923)
        {
            if (frame.dlc < 1)
                return;
            uint8_t status = readHW4DASAutopilotStatus(frame);
            dasAutopilotStatus = status;
            APActive = isDASAutopilotActive(status);
            updateSummonPolicyAutopilot(frame, status);
        }
#if defined(ISA_SPEED_CHIME_SUPPRESS) && !defined(ESP32_DASHBOARD)
        if (isaSpeedChimeSuppressRuntime && frame.id == 921)
        {
            if (frame.dlc < 8)
                return;
            if (!isaSpeedChimeSuppressRuntime)
                return;
            frame.data[1] |= 0x20;
            uint8_t sum = 0;
            for (int i = 0; i < 7; i++)
                sum += frame.data[i];
            sum += (921 & 0xFF) + (921 >> 8);
            frame.data[7] = sum & 0xFF;
            framesSent++;
            driver.send(frame);
            if (onSend)
                onSend(0, true);
            return;
        }
#endif
        if (frame.id == 1016)
        {
            if (frame.dlc < 6)
                return;
            updateSummonFrom1016(frame);
            if (!speedProfileAuto)
                return;
            auto fd = (frame.data[5] & 0b11100000) >> 5;
            switch (fd)
            {
            case 1:
                speedProfile = 3;
                break;
            case 2:
                speedProfile = 2;
                break;
            case 3:
                speedProfile = 1;
                break;
            case 4:
                speedProfile = 0;
                break;
            case 5:
                speedProfile = 4;
                break;
            }
        }
        if (frame.id == 2047)
        {
            if (frame.dlc < 6)
                return;
            if (readMuxID(frame) != 2)
                return;

            uint8_t next = readGTWAutopilot(frame);
            int prev = gatewayAutopilot;
            gatewayAutopilot = next;

            if (enablePrint && prev != next)
            {
                char buf[LogRingBuffer::kMaxMsgLen];
                snprintf(buf, sizeof(buf), "HW4Handler: GTW_autopilot: %d -> %u (%s)",
                         prev, (unsigned int)next, describeGTWAutopilot(next));
                logRing.push(buf,
#ifndef NATIVE_BUILD
                             millis()
#else
                             0
#endif
                );
#ifndef NATIVE_BUILD
                Serial.println(buf);
#endif
            }
            return;
        }
        if (frame.id == 1021)
        {
            if (frame.dlc < 8)
                return;
            auto index = readMuxID(frame);
            if (index == 0)
                ADEnabled = isADSelectedInUI(frame) && (!checkAD || checkAD());
            if (index == 0 && ADEnabled && (!checkAD || checkAD()))
            {
#if !defined(ESP32_DASHBOARD)
                setBit(frame, 46, true);
                setBit(frame, 60, true);
#if defined(EMERGENCY_VEHICLE_DETECTION) && !defined(ESP32_DASHBOARD)
                if (emergencyVehicleDetectionRuntime)
                    setBit(frame, 59, true);
#endif
                framesSent++;
                driver.send(frame);
                if (onSend)
                    onSend(0, true);
#endif
            }
            if (index == 2 && ADEnabled && !speedProfileAuto && (!checkAD || checkAD()))
            {
                setSpeedProfileHW4(frame, speedProfile);
                bool ok = driver.send(frame);
                if (ok)
                    framesSent++;
                if (onSend)
                    onSend(2, ok);
            }
            if (index == 1)
            {
#if !defined(ESP32_DASHBOARD)
                bool modified = false;
#if defined(ENHANCED_AUTOPILOT)
                if (enhancedAutopilotRuntime && enhancedAutopilotInjectionAllowed(injectionGateOpen()))
                {
                    setBit(frame, 19, false);
                    setBit(frame, 47, true);
                    modified = true;
                }
#else
                // No dashboard: nag suppression always-on (RP2040, M4)
                setBit(frame, 19, false);
                setBit(frame, 47, true);
                modified = true;
#endif
                if (modified)
                {
                    framesSent++;
                    driver.send(frame);
                    if (onSend)
                        onSend(1, true);
                }
#endif
            }
            if (index == 0 && enablePrint)
            {
                char buf[LogRingBuffer::kMaxMsgLen];
                snprintf(buf, sizeof(buf), "HW4Handler: AD: %d, Profile: %d",
                         (bool)ADEnabled, (int)speedProfile);
                logRing.push(buf,
#ifndef NATIVE_BUILD
                             millis()
#else
                             0
#endif
                );
#ifndef NATIVE_BUILD
                Serial.println(buf);
#endif
            }
        }
    }
};
