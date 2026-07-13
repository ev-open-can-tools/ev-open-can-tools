#pragma once

#ifdef ESP_PLATFORM
#include "drivers/espidf_mcp2515.h"
#include <freertos/semphr.h>
#else
#include <SPI.h>
#include <mcp2515.h>
#endif
#include <limits.h>
#include "../can_frame_types.h"
#include "can_driver.h"

class ESP32_MCP2515Driver : public CanDriver
{
public:
    static constexpr bool kSupportsISR = false;

    explicit ESP32_MCP2515Driver(uint8_t csPin) : mcp_(csPin), csPin_(csPin)
    {
#ifdef ESP_PLATFORM
        mutex_ = xSemaphoreCreateMutex();
#endif
    }

    ~ESP32_MCP2515Driver() override
    {
#ifdef ESP_PLATFORM
        if (mutex_)
            vSemaphoreDelete(mutex_);
#endif
    }

    bool init() override
    {
#ifdef ESP_PLATFORM
        if (!mutex_)
            return false;
#endif
        lock();
        if (mcp_.reset() != MCP2515::ERROR_OK)
        {
#ifdef ESP_PLATFORM
            initialized_ = false;
#endif
            unlock();
            return false;
        }
#ifndef MCP_CRYSTAL_FREQ
#define MCP_CRYSTAL_FREQ MCP_8MHZ
#endif
        MCP2515::ERROR e = mcp_.setBitrate(CAN_500KBPS, MCP_CRYSTAL_FREQ);
        if (e != MCP2515::ERROR_OK)
        {
#ifdef ESP_PLATFORM
            initialized_ = false;
#endif
            unlock();
            return false;
        }
        bool ok = mcp_.setNormalMode() == MCP2515::ERROR_OK;
#ifdef ESP_PLATFORM
        initialized_ = ok;
#endif
        unlock();
        return ok;
    }

    void setFilters(const uint32_t *ids, uint8_t count) override
    {
        if (!ids || count == 0)
            return;
#ifdef ESP_PLATFORM
        if (!mutex_)
            return;
#endif
        lock();
#ifdef ESP_PLATFORM
        exactFilterCount_ = 0;
        for (uint8_t i = 0; i < count && exactFilterCount_ < kMaxExactFilters; i++)
        {
            uint32_t id = ids[i] & 0x7FF;
            bool duplicate = false;
            for (uint8_t j = 0; j < exactFilterCount_; j++)
                duplicate |= exactFilterIds_[j] == id;
            if (!duplicate)
                exactFilterIds_[exactFilterCount_++] = id;
        }
        if (!initialized_)
        {
            unlock();
            return;
        }
#endif
        if (mcp_.setConfigMode() != MCP2515::ERROR_OK)
        {
#ifdef ESP_PLATFORM
            initialized_ = false;
#endif
            unlock();
            return;
        }
#ifdef ESP_PLATFORM
        if (monitorAll_)
        {
            bool filtersOk = configureAcceptAll();
            initialized_ = filtersOk && mcp_.setNormalMode() == MCP2515::ERROR_OK;
            unlock();
            return;
        }
        if (exactFilterCount_ > 6)
        {
            bool filtersOk = configureGroupedFilters();
            initialized_ = filtersOk && mcp_.setNormalMode() == MCP2515::ERROR_OK;
            unlock();
            return;
        }
        ids = exactFilterIds_;
        count = exactFilterCount_;
#endif
        bool filtersOk = mcp_.setFilterMask(MCP2515::MASK0, false, 0x7FF) == MCP2515::ERROR_OK;
        filtersOk &= mcp_.setFilter(MCP2515::RXF0, false, ids[0]) == MCP2515::ERROR_OK;
        filtersOk &= mcp_.setFilter(MCP2515::RXF1, false, count > 1 ? ids[1] : ids[0]) == MCP2515::ERROR_OK;
        filtersOk &= mcp_.setFilterMask(MCP2515::MASK1, false, 0x7FF) == MCP2515::ERROR_OK;
        filtersOk &= mcp_.setFilter(MCP2515::RXF2, false, count > 2 ? ids[2] : ids[0]) == MCP2515::ERROR_OK;
        filtersOk &= mcp_.setFilter(MCP2515::RXF3, false, count > 3 ? ids[3] : ids[0]) == MCP2515::ERROR_OK;
        filtersOk &= mcp_.setFilter(MCP2515::RXF4, false, count > 4 ? ids[4] : ids[0]) == MCP2515::ERROR_OK;
        filtersOk &= mcp_.setFilter(MCP2515::RXF5, false, count > 5 ? ids[5] : ids[0]) == MCP2515::ERROR_OK;
#ifdef ESP_PLATFORM
        initialized_ = filtersOk && mcp_.setNormalMode() == MCP2515::ERROR_OK;
#else
        mcp_.setNormalMode();
#endif
        unlock();
    }

    bool enableInterrupt(void (*)(void)) override
    {
        return false;
    }

    bool ready() const override
    {
#ifdef ESP_PLATFORM
        if (!mutex_)
            return false;
        lock();
        bool value = initialized_;
        unlock();
        return value;
#else
        return true;
#endif
    }

    void setMonitorAll(bool enabled) override
    {
#ifdef ESP_PLATFORM
        if (!mutex_)
            return;
        uint32_t filters[kMaxExactFilters] = {};
        uint8_t filterCount = 0;
        bool restore = false;
        lock();
        if (monitorAll_ != enabled)
        {
            monitorAll_ = enabled;
            filterCount = exactFilterCount_;
            memcpy(filters, exactFilterIds_, filterCount * sizeof(filters[0]));
            if (initialized_ && enabled)
            {
                bool ok = mcp_.setConfigMode() == MCP2515::ERROR_OK && configureAcceptAll();
                initialized_ = ok && mcp_.setNormalMode() == MCP2515::ERROR_OK;
            }
            else
                restore = initialized_ && !enabled;
        }
        unlock();
        if (restore && filterCount)
            setFilters(filters, filterCount);
#else
        (void)enabled;
#endif
    }

    bool read(CanFrame &frame) override
    {
#ifdef ESP_PLATFORM
        if (!mutex_)
            return false;
#endif
        lock();
#ifdef ESP_PLATFORM
        if (!initialized_)
        {
            uint32_t now = millis();
            bool retry = now - lastInitAttemptMs_ >= kInitRetryMs;
            if (retry)
                lastInitAttemptMs_ = now;
            uint8_t filterCount = exactFilterCount_;
            uint32_t filters[kMaxExactFilters];
            memcpy(filters, exactFilterIds_, filterCount * sizeof(filters[0]));
            unlock();
            if (retry && init() && filterCount)
                setFilters(filters, filterCount);
            return false;
        }
#endif
        for (uint8_t attempt = 0; attempt < 8; attempt++)
        {
            can_frame raw;
            MCP2515::ERROR readError = mcp_.readMessage(&raw);
            if (readError != MCP2515::ERROR_OK)
            {
#ifdef ESP_PLATFORM
                if (readError != MCP2515::ERROR_NOMSG)
                    initialized_ = false;
#endif
                unlock();
                return false;
            }
#ifdef ESP_PLATFORM
            if ((raw.can_id & (CAN_EFF_FLAG | CAN_RTR_FLAG)) != 0 || !exactFilterMatches(raw.can_id))
                continue;
#endif
            frame.id = raw.can_id;
            frame.dlc = raw.can_dlc <= 8 ? raw.can_dlc : 8;
            memcpy(frame.data, raw.data, 8);
            unlock();
            return true;
        }
        unlock();
        return false;
    }

    bool send(const CanFrame &frame) override
    {
        if (!sendAllowed(frame) || frame.id > 0x7FF || frame.dlc > 8)
        {
            if (onSendFrame)
                onSendFrame(frame, false);
            return false;
        }
#ifdef ESP_PLATFORM
        if (!mutex_)
        {
            if (onSendFrame)
                onSendFrame(frame, false);
            return false;
        }
#endif
        lock();
#ifdef ESP_PLATFORM
        if (!initialized_)
        {
            unlock();
            if (onSendFrame)
                onSendFrame(frame, false);
            return false;
        }
#endif
        can_frame raw;
        raw.can_id = frame.id;
        raw.can_dlc = frame.dlc;
        memcpy(raw.data, frame.data, 8);
        bool ok = mcp_.sendMessage(&raw) == MCP2515::ERROR_OK;
        unlock();
        if (onSendFrame)
            onSendFrame(frame, ok);
        return ok;
    }

    void clearPendingTransmit() override
    {
#ifdef ESP_PLATFORM
        if (!mutex_)
            return;
        lock();
        if (initialized_)
            mcp_.abortPendingTransmissions();
        unlock();
#endif
    }

    MCP2515 &mcp() { return mcp_; }

    void configurationSummary(char *out, size_t outLen) const override
    {
        if (!out || outLen == 0)
            return;
        snprintf(out, outLen, "bitrate=500000 mcp2515CsGPIO=%u",
                 static_cast<unsigned int>(csPin_));
    }

#ifdef ESP_PLATFORM
    uint8_t errorFlags()
    {
        if (!mutex_)
            return 0;
        lock();
        uint8_t flags = mcp_.getErrorFlags();
        unlock();
        return flags;
    }

    bool recover()
    {
        if (!mutex_)
            return false;
        lock();
        bool ok = mcp_.reset() == MCP2515::ERROR_OK &&
                  mcp_.setBitrate(CAN_500KBPS, MCP_CRYSTAL_FREQ) == MCP2515::ERROR_OK &&
                  mcp_.setNormalMode() == MCP2515::ERROR_OK;
        initialized_ = ok;
        unlock();
        return ok;
    }
#endif

private:
#ifdef ESP_PLATFORM
    static constexpr uint8_t kMaxExactFilters = 160;
    static constexpr uint32_t kInitRetryMs = 5000;

    bool exactFilterMatches(uint32_t id) const
    {
        if (monitorAll_)
            return true;
        id &= 0x7FF;
        for (uint8_t i = 0; i < exactFilterCount_; i++)
        {
            if (exactFilterIds_[i] == id)
                return true;
        }
        return exactFilterCount_ == 0;
    }

    void groupPattern(uint8_t splitBit, bool splitValue, uint32_t &base, uint32_t &mask,
                      uint8_t &wildcards) const
    {
        bool first = true;
        uint32_t differ = 0;
        base = 0;
        for (uint8_t i = 0; i < exactFilterCount_; i++)
        {
            if (splitBit < 11 && (((exactFilterIds_[i] >> splitBit) & 1U) != splitValue))
                continue;
            if (first)
            {
                base = exactFilterIds_[i];
                first = false;
            }
            else
                differ |= base ^ exactFilterIds_[i];
        }
        mask = (~differ) & 0x7FF;
        base &= mask;
        wildcards = static_cast<uint8_t>(__builtin_popcount(differ & 0x7FF));
    }

    bool configureGroupedFilters()
    {
        uint8_t bestBit = 0xFF;
        uint32_t bestCost = UINT_MAX;
        for (uint8_t bit = 0; bit < 11; bit++)
        {
            bool hasZero = false, hasOne = false;
            for (uint8_t i = 0; i < exactFilterCount_; i++)
            {
                hasZero |= ((exactFilterIds_[i] >> bit) & 1U) == 0;
                hasOne |= ((exactFilterIds_[i] >> bit) & 1U) != 0;
            }
            if (!hasZero || !hasOne)
                continue;
            uint32_t base0, mask0, base1, mask1;
            uint8_t wild0, wild1;
            groupPattern(bit, false, base0, mask0, wild0);
            groupPattern(bit, true, base1, mask1, wild1);
            uint32_t cost = (1U << wild0) + (1U << wild1);
            if (cost < bestCost)
            {
                bestCost = cost;
                bestBit = bit;
            }
        }

        uint32_t base0, mask0, base1, mask1;
        uint8_t wild0, wild1;
        if (bestBit == 0xFF)
        {
            groupPattern(0xFF, false, base0, mask0, wild0);
            base1 = base0;
            mask1 = mask0;
        }
        else
        {
            groupPattern(bestBit, false, base0, mask0, wild0);
            groupPattern(bestBit, true, base1, mask1, wild1);
        }
        bool ok = mcp_.setFilterMask(MCP2515::MASK0, false, mask0) == MCP2515::ERROR_OK;
        ok &= mcp_.setFilter(MCP2515::RXF0, false, base0) == MCP2515::ERROR_OK;
        ok &= mcp_.setFilter(MCP2515::RXF1, false, base0) == MCP2515::ERROR_OK;
        ok &= mcp_.setFilterMask(MCP2515::MASK1, false, mask1) == MCP2515::ERROR_OK;
        ok &= mcp_.setFilter(MCP2515::RXF2, false, base1) == MCP2515::ERROR_OK;
        ok &= mcp_.setFilter(MCP2515::RXF3, false, base1) == MCP2515::ERROR_OK;
        ok &= mcp_.setFilter(MCP2515::RXF4, false, base1) == MCP2515::ERROR_OK;
        ok &= mcp_.setFilter(MCP2515::RXF5, false, base1) == MCP2515::ERROR_OK;
        return ok;
    }

    bool configureAcceptAll()
    {
        bool ok = mcp_.setFilterMask(MCP2515::MASK0, false, 0) == MCP2515::ERROR_OK;
        ok &= mcp_.setFilter(MCP2515::RXF0, false, 0) == MCP2515::ERROR_OK;
        ok &= mcp_.setFilter(MCP2515::RXF1, false, 0) == MCP2515::ERROR_OK;
        ok &= mcp_.setFilterMask(MCP2515::MASK1, false, 0) == MCP2515::ERROR_OK;
        ok &= mcp_.setFilter(MCP2515::RXF2, false, 0) == MCP2515::ERROR_OK;
        ok &= mcp_.setFilter(MCP2515::RXF3, false, 0) == MCP2515::ERROR_OK;
        ok &= mcp_.setFilter(MCP2515::RXF4, false, 0) == MCP2515::ERROR_OK;
        ok &= mcp_.setFilter(MCP2515::RXF5, false, 0) == MCP2515::ERROR_OK;
        return ok;
    }
#endif

    void lock() const
    {
#ifdef ESP_PLATFORM
        if (mutex_)
            xSemaphoreTake(mutex_, portMAX_DELAY);
#endif
    }

    void unlock() const
    {
#ifdef ESP_PLATFORM
        if (mutex_)
            xSemaphoreGive(mutex_);
#endif
    }

    MCP2515 mcp_;
    uint8_t csPin_ = 0;
#ifdef ESP_PLATFORM
    mutable SemaphoreHandle_t mutex_ = nullptr;
    uint32_t exactFilterIds_[kMaxExactFilters] = {};
    uint8_t exactFilterCount_ = 0;
    bool initialized_ = false;
    uint32_t lastInitAttemptMs_ = 0;
    bool monitorAll_ = false;
#endif
};
