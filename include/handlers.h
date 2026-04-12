#pragma once

#include <memory>
#include <algorithm>
#include "can_frame_types.h"
#include "drivers/can_driver.h"
#include "can_helpers.h"
#include "shared_types.h"
#include "log_buffer.h"

#ifndef NATIVE_BUILD
#include <Arduino.h>
#endif

inline LogRingBuffer logRing;

struct CarManagerBase
{
    Shared<int> speedProfile{1};
    Shared<bool> ADEnabled{false};
    Shared<int> gatewayAutopilot{-1};
    Shared<bool> enablePrint{true};
    Shared<uint32_t> frameCount{0};
    Shared<uint32_t> framesSent{0};
    Shared<int> speedOffset{0};

    void (*onFrame)(const CanFrame &) = nullptr;
    void (*onSend)(uint8_t mux, bool ok) = nullptr;
    bool (*checkAD)() = nullptr;
    bool (*checkNag)() = nullptr;
    bool (*checkSummon)() = nullptr;
    bool (*checkIsa)() = nullptr;
    bool (*checkEvd)() = nullptr;

    virtual void handleMessage(CanFrame &frame, CanDriver &driver) = 0;
    virtual const uint32_t *filterIds() const = 0;
    virtual uint8_t filterIdCount() const = 0;
    virtual ~CarManagerBase() = default;
};

struct LegacyHandler : public CarManagerBase
{
    const uint32_t *filterIds() const override
    {
        static constexpr uint32_t ids[] = {69, 1006};
        return ids;
    }
    uint8_t filterIdCount() const override { return 2; }

    void handleMessage(CanFrame &frame, CanDriver &driver) override
    {
        if (onFrame)
            onFrame(frame);
        // STW_ACTN_RQ (0x045 = 69): Follow-Distance-Stalk as Source for Profile Mapping
        // byte[1]: 0x00=Pos1, 0x21=Pos2, 0x42=Pos3, 0x64=Pos4, 0x85=Pos5, 0xA6=Pos6, 0xC8=Pos7
        if (frame.id == 69)
        {
            if (frame.dlc < 2)
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
        if (frame.id == 1006)
        {
            if (frame.dlc < 8)
                return;
            auto index = readMuxID(frame);
            if (index == 0)
                ADEnabled = isADSelectedInUI(frame);
            if (index == 0 && ADEnabled && (!checkAD || checkAD()))
            {
                setBit(frame, 46, true);
                setSpeedProfileV12V13(frame, speedProfile);
                framesSent++;
                driver.send(frame);
                if (onSend)
                    onSend(0, true);
            }
            if (index == 1 && (!checkNag || checkNag()))
            {
                setBit(frame, 19, false);
                framesSent++;
                driver.send(frame);
                if (onSend)
                    onSend(1, true);
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
#if defined(ESP32_DASHBOARD)
        static constexpr uint32_t ids[] = {787, 880, 1016, 1021, 2047};
        return ids;
    }
    uint8_t filterIdCount() const override { return 5; }
#else
        static constexpr uint32_t ids[] = {787, 1016, 1021, 2047};
        return ids;
    }
    uint8_t filterIdCount() const override { return 4; }
#endif

    void handleMessage(CanFrame &frame, CanDriver &driver) override
    {
        if (onFrame)
            onFrame(frame);
#if defined(ESP32_DASHBOARD)
        if (nagKillerRuntime && frame.id == 880 && frame.dlc >= 8)
        {
            if ((frame.data[4] >> 6 & 0x03) == 0)
            {
                CanFrame echo;
                echo.id = 880;
                echo.dlc = 8;
                echo.data[0] = frame.data[0];
                echo.data[1] = frame.data[1];
                echo.data[2] = (frame.data[2] & 0xF0) | 0x08;
                echo.data[3] = 0xB6;
                echo.data[4] = frame.data[4] | 0x40;
                echo.data[5] = frame.data[5];
                echo.data[6] = (frame.data[6] & 0xF0) | ((frame.data[6] + 1) & 0x0F);
                uint16_t s = echo.data[0] + echo.data[1] + echo.data[2] + echo.data[3] + echo.data[4] + echo.data[5] + echo.data[6];
                echo.data[7] = static_cast<uint8_t>((s + 0x73) & 0xFF);
                framesSent++;
                driver.send(echo);
                if (onSend)
                    onSend(3, true);
            }
            return;
        }
#endif
        if (frame.id == 787)
        {
            if (frame.dlc < 8)
                return;
            setTrackModeRequest(frame, kTrackModeRequestOn);
            frame.data[7] = computeVehicleChecksum(frame);
            framesSent++;
            driver.send(frame);
            return;
        }
        if (frame.id == 1016)
        {
            if (frame.dlc < 6)
                return;
            if (!speedProfileLocked)
            {
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
            }
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
                ADEnabled = isADSelectedInUI(frame);
            if (index == 0 && ADEnabled && (!checkAD || checkAD()))
            {
                speedOffset = std::max(std::min(((uint8_t)((frame.data[3] >> 1) & 0x3F) - 30) * 5, 100), 0);
                setBit(frame, 46, true);
                setSpeedProfileV12V13(frame, speedProfile);
                framesSent++;
                driver.send(frame);
                if (onSend)
                    onSend(0, true);
            }
            if (index == 1)
            {
                bool modified = false;
#if defined(ENHANCED_AUTOPILOT) || defined(ESP32_DASHBOARD)
                if (enhancedAutopilotRuntime)
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
            }
            if (index == 2 && ADEnabled && (!checkAD || checkAD()))
            {
                frame.data[0] &= ~(0b11000000);
                frame.data[1] &= ~(0b00111111);
                frame.data[0] |= (speedOffset & 0x03) << 6;
                frame.data[1] |= (speedOffset >> 2);
                framesSent++;
                driver.send(frame);
                if (onSend)
                    onSend(2, true);
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
 * NagHandler — DAS-aware autosteer nag suppression (counter+1 echo method)
 *
 * Listens for two CAN frames:
 *   - 880  (0x370) EPAS3P_sysStatus  — steering torque / handsOnLevel source
 *   - 921  (0x399) DAS_status        — DAS hands-on demand state
 *
 * When DAS is actively requesting hands-on (dasHandsOnState != LC_HANDS_ON_NOT_REQD):
 *   1. Copies the real EPAS frame
 *   2. Sets byte 3 = 0xB6 (fixed torsionBarTorque = 1.80 Nm)
 *   3. Sets byte 4 |= 0x40 (handsOnLevel = 1)
 *   4. Increments counter (byte 6 lower nibble + 1)
 *   5. Recalculates checksum (byte 7 = sum(b0..b6) + 0x73)
 *
 * Improvement over the naive approach (echo whenever EPAS handsOnLevel=0):
 *   EPAS naturally reports handsOnLevel=0 during normal AP driving with no nag
 *   pending — that is the resting state. Echoing unconditionally injects ~25
 *   unsolicited frames/s onto the bus during normal driving. By gating on
 *   DAS_autopilotHandsOnState from 0x399, we only inject when DAS is actually
 *   demanding a response, producing zero spurious traffic during satisfied driving.
 *
 * DAS_autopilotHandsOnState (42|4@1+ LE in 0x399): (d[5] >> 2) & 0x0F
 *   0 = LC_HANDS_ON_NOT_REQD  — DAS satisfied, no nag: echo suppressed
 *   1 = LC_HANDS_ON_REQD_DETECTED
 *   2 = LC_HANDS_ON_REQD_NOT_DETECTED — orange nag: echo fires
 *   3 = LC_HANDS_ON_REQD_VISUAL
 *   4 = LC_HANDS_ON_REQD_CHIME_1
 *   5 = LC_HANDS_ON_REQD_CHIME_2
 *   6 = LC_HANDS_ON_REQD_SLOWING
 *   7 = LC_HANDS_ON_REQD_STRUCK_OUT
 *   8 = LC_HANDS_ON_SUSPENDED     — AP paused, not struck: echo suppressed
 *   9 = LC_HANDS_ON_REQD_ESCALATED_CHIME_1
 *  10 = LC_HANDS_ON_REQD_ESCALATED_CHIME_2
 *  15 = LC_HANDS_ON_SNA
 *
 * Tested: Model X HW3, Legacy platform, FW 2026.8.3
 * Bus: X179 pin 18/19 (Chassis CAN)
 *
 * Enable with build flag: -D NAG_KILLER
 */
struct NagHandler : public CarManagerBase
{
    Shared<bool>     nagKillerActive{true};
    Shared<uint32_t> nagEchoCount{0};
    // Last decoded DAS_autopilotHandsOnState from 0x399.
    // Initialised to 0xFF (unseen) so the handler echoes conservatively
    // until the first DAS_status frame arrives — identical to the pre-DAS
    // behaviour for the brief startup window.
    Shared<uint8_t>  dasHandsOnState{0xFF};

    // ── Organic torque variation state ────────────────────────────────────
    // Not atomic — only accessed from handleMessage() (serialised CAN context).
    // xorshift32 PRNG avoids Arduino's random() (safe for NATIVE_BUILD).
    uint32_t _prngState      = 0xDEADBEEF;
    int16_t  _torqWalk       = 2230;   // raw starting point = 1.80 Nm
    uint8_t  _excFrames      = 0;      // frames remaining in grip excursion
    uint16_t _framesUntilExc = 175;    // frames until next excursion (~7 s @ 25 Hz)

    const uint32_t *filterIds() const override
    {
        // 880 = 0x370 EPAS3P_sysStatus, 921 = 0x399 DAS_status
        static constexpr uint32_t ids[] = {880, 921};
        return ids;
    }
    uint8_t filterIdCount() const override { return 2; }

    void handleMessage(CanFrame &frame, CanDriver &driver) override
    {
        // ── 0x399 DAS_status: track DAS hands-on demand level ────────────────
        if (frame.id == 921)
        {
            if (frame.dlc >= 6)
                dasHandsOnState = (frame.data[5] >> 2) & 0x0F;
            return;
        }

        // ── 0x370 EPAS3P_sysStatus: conditional echo ─────────────────────────
        if (frame.id != 880 || frame.dlc < 8)
            return;

        uint8_t handsOn = (frame.data[4] >> 6) & 0x03;

        if (!nagKillerActive || !nagKillerRuntime || handsOn != 0)
            return;

        // Gate: only inject when DAS is actually requesting hands-on.
        // State 0 (NOT_REQD) and state 8 (SUSPENDED) mean DAS is satisfied —
        // no echo needed. 0xFF = no DAS frame seen yet, echo as fallback.
        uint8_t dasState = dasHandsOnState;
        if (dasState == 0 || dasState == 8)
            return;

        // ── Organic torque variation ──────────────────────────────────────
        // Constant torque is trivially detectable in Tesla telemetry — a flat
        // torsionBarTorque signal for 30+ minutes is a statistical impossibility
        // from a real hand. We simulate muscle tremor and slow grip drift using
        // a smooth random walk with brief "tighten grip" excursions every 5–9 s.
        //
        // torsionBarTorque encoding (19|12@0+ Motorola, 0.01 Nm/LSB, -20.5 offset):
        //   tRaw = (Nm + 20.5) / 0.01    e.g. 1.80 Nm → 0x08B6
        //   d[2] lower nibble = tRaw >> 8,   d[3] = tRaw & 0xFF
        //
        // Normal walk: [2150–2290] ≈ [1.00–2.40 Nm] — light resting touch, handsOnLevel 1
        // Excursion:   2350 ± 20  ≈  [3.10–3.30 Nm] — brief grip pulse every ~5–9 s
        {
            uint32_t r = _prngState;
            r ^= r << 13; r ^= r >> 17; r ^= r << 5;
            _prngState = r;

            if (_excFrames > 0) {
                // Grip excursion — elevated torque for a few frames
                _torqWalk = static_cast<int16_t>(2350 + static_cast<int16_t>(r % 41) - 20);
                _excFrames--;
            } else {
                // Normal walk: step ±15 raw units per frame (±0.15 Nm per 40 ms)
                _torqWalk += static_cast<int16_t>(r % 31) - 15;
                if (_torqWalk < 2150) _torqWalk = 2150;
                if (_torqWalk > 2290) _torqWalk = 2290;
                if (_framesUntilExc == 0) {
                    _excFrames      = 3 + static_cast<uint8_t>(r % 3);       // 3–5 frames ≈ 120–200 ms
                    _framesUntilExc = 125 + static_cast<uint16_t>(r % 101);  // 125–225 frames ≈ 5–9 s @ 25 Hz
                } else {
                    _framesUntilExc--;
                }
            }
        }
        uint16_t torqRaw = static_cast<uint16_t>(_torqWalk);

        CanFrame echo;
        echo.id = 880;
        echo.dlc = 8;

        echo.data[0] = frame.data[0];
        echo.data[1] = frame.data[1];
        echo.data[2] = (frame.data[2] & 0xF0) | static_cast<uint8_t>(torqRaw >> 8);
        echo.data[3] = static_cast<uint8_t>(torqRaw & 0xFF);
        echo.data[5] = frame.data[5];

        // handsOnLevel = 1 — torque stays in level-1 range throughout walk
        echo.data[4] = frame.data[4] | 0x40;

        // Counter + 1
        uint8_t cnt = (frame.data[6] & 0x0F);
        cnt = (cnt + 1) & 0x0F;
        echo.data[6] = (frame.data[6] & 0xF0) | cnt;

        // Checksum: sum(byte0..byte6) + 0x73
        uint16_t sum = echo.data[0] + echo.data[1] + echo.data[2] + echo.data[3] + echo.data[4] + echo.data[5] + echo.data[6];
        echo.data[7] = static_cast<uint8_t>((sum + 0x73) & 0xFF);

        framesSent++;
        nagEchoCount++;
        driver.send(echo);

        if (enablePrint && (nagEchoCount % 500 == 1))
        {
            char buf[LogRingBuffer::kMaxMsgLen];
            snprintf(buf, sizeof(buf), "NagHandler: echo=%u das=%u",
                     (unsigned int)(uint32_t)nagEchoCount, (unsigned int)dasState);
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
};

struct HW4Handler : public CarManagerBase
{
    const uint32_t *filterIds() const override
    {
#if defined(ESP32_DASHBOARD)
        static constexpr uint32_t ids[] = {880, 921, 1016, 1021, 2047};
        return ids;
    }
    uint8_t filterIdCount() const override { return 5; }
#elif defined(ISA_SPEED_CHIME_SUPPRESS)
        static constexpr uint32_t ids[] = {921, 1016, 1021, 2047};
        return ids;
    }
    uint8_t filterIdCount() const override { return 4; }
#else
            static constexpr uint32_t ids[] = {1016, 1021, 2047};
            return ids;
        }
        uint8_t filterIdCount() const override { return 3; }
#endif

    void handleMessage(CanFrame &frame, CanDriver &driver) override
    {
        if (onFrame)
            onFrame(frame);
#if defined(ESP32_DASHBOARD)
        if (nagKillerRuntime && frame.id == 880 && frame.dlc >= 8)
        {
            if ((frame.data[4] >> 6 & 0x03) == 0)
            {
                CanFrame echo;
                echo.id = 880;
                echo.dlc = 8;
                echo.data[0] = frame.data[0];
                echo.data[1] = frame.data[1];
                echo.data[2] = (frame.data[2] & 0xF0) | 0x08;
                echo.data[3] = 0xB6;
                echo.data[4] = frame.data[4] | 0x40;
                echo.data[5] = frame.data[5];
                echo.data[6] = (frame.data[6] & 0xF0) | ((frame.data[6] + 1) & 0x0F);
                uint16_t s = echo.data[0] + echo.data[1] + echo.data[2] + echo.data[3] + echo.data[4] + echo.data[5] + echo.data[6];
                echo.data[7] = static_cast<uint8_t>((s + 0x73) & 0xFF);
                framesSent++;
                driver.send(echo);
                if (onSend)
                    onSend(3, true);
            }
            return;
        }
#endif
#if defined(ISA_SPEED_CHIME_SUPPRESS) || defined(ESP32_DASHBOARD)
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
            if (!speedProfileLocked)
            {
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
                ADEnabled = isADSelectedInUI(frame);
            if (index == 0 && ADEnabled && (!checkAD || checkAD()))
            {
                setBit(frame, 46, true);
                setBit(frame, 60, true);
#if defined(EMERGENCY_VEHICLE_DETECTION) || defined(ESP32_DASHBOARD)
                if (emergencyVehicleDetectionRuntime)
                    setBit(frame, 59, true);
#endif
                framesSent++;
                driver.send(frame);
                if (onSend)
                    onSend(0, true);
            }
            if (index == 1)
            {
                bool modified = false;
#if defined(ENHANCED_AUTOPILOT) || defined(ESP32_DASHBOARD)
                if (enhancedAutopilotRuntime)
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
            }
            if (index == 2 && ADEnabled)
            {
                frame.data[7] &= ~(0x07 << 4);
                frame.data[7] |= (speedProfile & 0x07) << 4;
                uint8_t off = hw4OffsetRuntime;
                if (off > 0)
                    frame.data[1] = (frame.data[1] & 0xC0) | (off & 0x3F);
                framesSent++;
                driver.send(frame);
                if (onSend)
                    onSend(2, true);
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
