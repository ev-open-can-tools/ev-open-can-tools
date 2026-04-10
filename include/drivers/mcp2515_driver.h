#pragma once

#include <SPI.h>
#include <mcp2515.h>
#include <memory>
#include "../can_frame_types.h"
#include "can_driver.h"

class MCP2515Driver : public CanDriver
{
public:
    static constexpr bool kSupportsISR = true;

    explicit MCP2515Driver(uint8_t csPin) : mcp_(csPin) {}

    bool init() override
    {
        mcp_.reset();
        MCP2515::ERROR e = mcp_.setBitrate(CAN_500KBPS, MCP_16MHZ);
        if (e != MCP2515::ERROR_OK)
            return false;
        mcp_.setNormalMode();
        return true;
    }

    void setFilters(const uint32_t *ids, uint8_t count) override
    {
        mcp_.setConfigMode();
        mcp_.setFilterMask(MCP2515::MASK0, false, 0x7FF);
        mcp_.setFilter(MCP2515::RXF0, false, ids[0]);
        mcp_.setFilter(MCP2515::RXF1, false, count > 1 ? ids[1] : ids[0]);
        mcp_.setFilterMask(MCP2515::MASK1, false, 0x7FF);
        mcp_.setFilter(MCP2515::RXF2, false, count > 2 ? ids[2] : ids[0]);
        mcp_.setFilter(MCP2515::RXF3, false, count > 3 ? ids[3] : ids[0]);
        mcp_.setFilter(MCP2515::RXF4, false, count > 4 ? ids[4] : ids[0]);
        mcp_.setFilter(MCP2515::RXF5, false, count > 5 ? ids[5] : ids[0]);
        mcp_.setNormalMode();
    }

    bool enableInterrupt(void (*onReady)()) override
    {
        pinMode(PIN_CAN_INTERRUPT, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(PIN_CAN_INTERRUPT), onReady, FALLING);
        return true;
    }

    bool read(CanFrame &frame) override
    {
        can_frame raw;
        if (mcp_.readMessage(&raw) != MCP2515::ERROR_OK)
            return false;
        frame.id = raw.can_id;
        frame.dlc = raw.can_dlc;
        memcpy(frame.data, raw.data, 8);
        return true;
    }

    void send(const CanFrame &frame) override
    {
        can_frame raw;
        raw.can_id = frame.id;
        raw.can_dlc = frame.dlc;
        memcpy(raw.data, frame.data, 8);
        mcp_.sendMessage(&raw);
    }

private:
    MCP2515 mcp_;
};
