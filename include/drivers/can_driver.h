#pragma once

#include "../can_frame_types.h"
#include <stddef.h>
#include <stdio.h>

struct CanDriver
{
    void (*onSendFrame)(const CanFrame &, bool ok) = nullptr;
    bool (*allowSendFrame)(const CanFrame &) = nullptr;

    virtual bool init() = 0;
    virtual void setFilters(const uint32_t *ids, uint8_t count) = 0;
    virtual bool enableInterrupt(void (*onReady)()) = 0;
    virtual bool read(CanFrame &frame) = 0;
    virtual bool send(const CanFrame &frame) = 0;
    virtual bool ready() const { return true; }
    virtual void setMonitorAll(bool) {}
    virtual void clearPendingTransmit() {}

    bool sendAllowed(const CanFrame &frame) const
    {
        return !allowSendFrame || allowSendFrame(frame);
    }

    // Dev/test mode: route transmitted frames to a loopback (fire onSendFrame
    // only) instead of the bus, so a board with no transceiver stays quiet and
    // does not accumulate TX errors. No-op by default; drivers that talk to
    // hardware override this.
    virtual void setSimLoopback(bool /*enabled*/) {}

    virtual void diagnosticsJson(char *out, size_t outLen) const
    {
        if (!out || outLen == 0)
            return;
        snprintf(out, outLen, "{\"type\":\"generic\"}");
    }

    virtual void diagnosticsSummary(char *out, size_t outLen) const
    {
        if (!out || outLen == 0)
            return;
        snprintf(out, outLen, "CAN driver diagnostics unavailable");
    }

    virtual void configurationSummary(char *out, size_t outLen) const
    {
        if (!out || outLen == 0)
            return;
        snprintf(out, outLen, "bitrate=500000 pins=unavailable");
    }

    virtual ~CanDriver() = default;
};
