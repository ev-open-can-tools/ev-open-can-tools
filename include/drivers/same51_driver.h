#pragma once

#include <CANSAME5x.h>
#include "../can_frame_types.h"
#include "can_driver.h"

class SAME51Driver : public CanDriver
{
public:
    static constexpr bool kSupportsISR = false;

    bool init() override
    {
        pinMode(PIN_CAN_STANDBY, OUTPUT);
        digitalWrite(PIN_CAN_STANDBY, false);
        pinMode(PIN_CAN_BOOSTEN, OUTPUT);
        digitalWrite(PIN_CAN_BOOSTEN, true);

        if (!can_.begin(500000))
            return false;
        return true;
    }

    void setFilters(const uint32_t *ids, uint8_t count) override
    {
        if (count == 0)
            return;
        // CANSAME5x has only 1 standard filter element, so compute a combined
        // mask that accepts all requested IDs. Bits that differ between any
        // pair of IDs are masked out (don't-care).
        uint32_t differ = 0;
        for (uint8_t i = 1; i < count; i++)
        {
            differ |= ids[0] ^ ids[i];
        }
        uint32_t mask = ~differ & 0x7FF;
        uint32_t base = ids[0] & mask;
        can_.filter(static_cast<int>(base), static_cast<int>(mask));
    }

    // SAME51 uses register reads (no SPI overhead) and an 8-deep HW FIFO.
    // The library's onReceive() consumes frames inside the ISR, which is
    // incompatible with our "flag + drain" pattern. Keep polling with HW filters.
    bool enableInterrupt(void (* /*onReady*/)()) override { return false; }

    bool read(CanFrame &frame) override
    {
        int packetSize = can_.parsePacket();
        if (packetSize <= 0)
            return false;

        frame.id = can_.packetId();
        frame.dlc = static_cast<uint8_t>(packetSize);
        for (int i = 0; i < packetSize && i < 8; i++)
        {
            frame.data[i] = static_cast<uint8_t>(can_.read());
        }
        return true;
    }

    void send(const CanFrame &frame) override
    {
        can_.beginPacket(frame.id);
        can_.write(frame.data, frame.dlc);
        can_.endPacket();
    }

private:
    CANSAME5x can_;
};
