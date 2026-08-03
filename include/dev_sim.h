#pragma once

#include "can_frame_types.h"
#include <cstdint>

// Synthetic CAN traffic generator for the dev/test mode.
//
// When the dev mode is active the firmware sources frames from here instead of
// the real CAN driver, so the full RX -> handler -> injection pipeline runs on
// the bench with no transceiver attached: the sniffer, RX/TX/fps counters, gate
// diagnostics and (for the HW3 handler) speed-profile injection all come alive.
//
// The script cycles through the IDs the dashboard handlers filter on, plus one
// unfiltered ID for sniffer variety, and stamps a rolling byte into data[6] so
// the on-screen data visibly changes. Kept pure and time-injected (nowMs passed
// in) so it can be unit-tested natively without a clock.
class DevSim
{
public:
    static constexpr uint32_t kStepMs = 20; // ~50 simulated frames/sec aggregate

    void reset(uint32_t nowMs)
    {
        idx_ = 0;
        counter_ = 0;
        lastMs_ = nowMs;
        primed_ = true;
    }

    // Returns true and fills `frame` when the next scripted frame is due at
    // `nowMs`; returns false otherwise (poll again on the next loop).
    bool read(CanFrame &frame, uint32_t nowMs)
    {
        if (!primed_)
            reset(nowMs);
        if ((uint32_t)(nowMs - lastMs_) < kStepMs)
            return false;
        lastMs_ = nowMs;

        buildFrame(frame, idx_);
        idx_ = (uint8_t)((idx_ + 1) % kScriptLen);
        if (idx_ == 0)
            counter_++;
        return true;
    }

private:
    static constexpr uint8_t kScriptLen = 7;

    void buildFrame(CanFrame &f, uint8_t i)
    {
        f = CanFrame{};
        f.dlc = 8;
        switch (i)
        {
        case 0: // DI_systemStatus (280): DI_gear = P (byte2 bits5-7 = 1) -> Parked
            f.id = 280;
            f.data[2] = 0x20;
            break;
        case 1: // 390: vehicle gear = P (byte7 bits3-5 = 1) -> Parked
            f.id = 390;
            f.data[7] = 0x08;
            break;
        case 2: // DAS_status (921): autopilot inactive (byte0 low nibble = 0)
            f.id = 921;
            f.data[0] = 0x00;
            break;
        case 3: // UI_driverAssistControl (1016): follow distance = 2 (byte5 bits5-7)
            f.id = 1016;
            f.data[5] = 0x40;
            break;
        case 4: // UI_autopilotControl (1021) mux 0 with "AD selected in UI"
                // (byte4 bit5) -> HW3 handler enables + injects the speed profile
            f.id = 1021;
            f.data[0] = 0x00;      // mux 0
            f.data[3] = (40 << 1); // arbitrary speed field
            f.data[4] = 0x20;      // AD selected in UI
            break;
        case 5: // UI_autopilotControl (1021) mux 1: summon-control frame
            f.id = 1021;
            f.data[0] = 0x01; // mux 1
            break;
        default: // an ID no handler filters on, for sniffer variety
            f.id = 0x230;
            f.data[0] = 0xAA;
            break;
        }
        f.data[6] = counter_; // rolling byte -> visibly live data in the sniffer
    }

    uint8_t idx_ = 0;
    uint8_t counter_ = 0;
    uint32_t lastMs_ = 0;
    bool primed_ = false;
};
