#pragma once

#include "../can_frame_types.h"
#include "can_driver.h"
#include <driver/twai.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class TWAIDriver : public CanDriver
{
public:
    static constexpr bool kSupportsISR = false;

    TWAIDriver(gpio_num_t txPin, gpio_num_t rxPin)
        : txPin_(txPin), rxPin_(rxPin) {}

    bool init() override
    {
        if (!mutex_)
            mutex_ = xSemaphoreCreateMutex();
        if (!mutex_)
            return false;

        g_config_ = TWAI_GENERAL_CONFIG_DEFAULT(txPin_, rxPin_, TWAI_MODE_NORMAL);
        g_config_.rx_queue_len = 32;
        g_config_.tx_queue_len = 16;

        t_config_ = TWAI_TIMING_CONFIG_500KBITS();
        f_config_ = TWAI_FILTER_CONFIG_ACCEPT_ALL();

        lock();
        driverOK_ = installAndStartLocked();
        unlock();
        return driverOK_;
    }

    void setFilters(const uint32_t *ids, uint8_t count) override
    {
        if (count == 0)
            return;

        uint32_t differ = 0;
        for (uint8_t i = 1; i < count; i++)
        {
            differ |= ids[0] ^ ids[i];
        }

        uint32_t base = ids[0] & ~differ;
        twai_filter_config_t nextFilter = f_config_;
        nextFilter.acceptance_code = base << 21;
        nextFilter.acceptance_mask = (differ << 21) | 0x001FFFFF;
        nextFilter.single_filter = true;

        lock();
        // TWAI only has a mask filter; sparse ID sets can pass false positives.
        exactFilterCount_ = (count < kMaxExactFilters) ? count : kMaxExactFilters;
        for (uint8_t i = 0; i < exactFilterCount_; i++)
            exactFilterIds_[i] = ids[i];
        f_config_ = nextFilter;
        stopAndUninstallLocked();
        driverOK_ = installAndStartLocked();
        unlock();
    }

    bool enableInterrupt(void (* /*onReady*/)()) override { return false; }

    bool read(CanFrame &frame) override
    {
        for (uint8_t attempt = 0; attempt < kReadDrainBudget; attempt++)
        {
            lock();
            if (!driverOK_)
            {
                tryRecover();
                unlock();
                return false;
            }

            twai_message_t msg;
            if (twai_receive(&msg, 0) != ESP_OK)
            {
                if (isBusOff())
                    recoverWithCooldown();
                unlock();
                return false;
            }
            bool accepted = exactFilterMatchesLocked(msg.identifier);
            unlock();

            if (!accepted)
                continue;

            frame.id = msg.identifier;
            frame.dlc = (msg.data_length_code <= 8) ? msg.data_length_code : 8;
            memset(frame.data, 0, 8);
            memcpy(frame.data, msg.data, frame.dlc);
            return true;
        }

        return false;
    }

    bool send(const CanFrame &frame) override
    {
        lock();
        if (!driverOK_)
        {
            unlock();
            if (onSendFrame)
                onSendFrame(frame, false);
            return false;
        }

        twai_message_t msg = {};
        uint8_t dlc = (frame.dlc <= 8) ? frame.dlc : 8;
        msg.identifier = frame.id;
        msg.data_length_code = dlc;
        memcpy(msg.data, frame.data, dlc);

        // Short timeout (2ms): modified frames should not be dropped, but
        // long blocks (10ms) risk overflowing the 32-deep RX queue.
        // At 500kbps, ~8 frames arrive in 2ms — queue handles this fine.
        bool ok = twai_transmit(&msg, pdMS_TO_TICKS(2)) == ESP_OK;
        if (!ok)
        {
            if (isBusOff())
                recoverWithCooldown();
        }
        unlock();
        if (onSendFrame)
            onSendFrame(frame, ok);
        return ok;
    }

private:
    static constexpr uint8_t kMaxExactFilters = 32;
    static constexpr uint8_t kReadDrainBudget = 8;
    static constexpr uint32_t BUSOFF_COOLDOWN_MS = 1000;

    bool exactFilterMatchesLocked(uint32_t id) const
    {
        if (exactFilterCount_ == 0)
            return true;
        for (uint8_t i = 0; i < exactFilterCount_; i++)
        {
            if (exactFilterIds_[i] == id)
                return true;
        }
        return false;
    }

    bool isBusOff()
    {
        if (!driverInstalled_)
            return false;
        twai_status_info_t status;
        if (twai_get_status_info(&status) != ESP_OK)
            return false;
        return status.state == TWAI_STATE_BUS_OFF;
    }

    void recoverWithCooldown()
    {
        uint32_t now = millis();
        if (now - lastRecovery_ < BUSOFF_COOLDOWN_MS)
            return;
        lastRecovery_ = now;

        stopAndUninstallLocked();
        driverOK_ = installAndStartLocked();
    }

    void tryRecover()
    {
        uint32_t now = millis();
        if (now - lastRecovery_ < BUSOFF_COOLDOWN_MS * 10)
            return;
        lastRecovery_ = now;

        stopAndUninstallLocked();
        driverOK_ = installAndStartLocked();
    }

    void lock()
    {
        if (mutex_)
            xSemaphoreTake(mutex_, portMAX_DELAY);
    }

    void unlock()
    {
        if (mutex_)
            xSemaphoreGive(mutex_);
    }

    bool installAndStartLocked()
    {
        if (twai_driver_install(&g_config_, &t_config_, &f_config_) != ESP_OK)
        {
            driverInstalled_ = false;
            return false;
        }
        driverInstalled_ = true;
        if (twai_start() != ESP_OK)
        {
            twai_driver_uninstall();
            driverInstalled_ = false;
            return false;
        }
        return true;
    }

    void stopAndUninstallLocked()
    {
        if (!driverInstalled_)
            return;
        twai_stop();
        twai_driver_uninstall();
        driverInstalled_ = false;
        driverOK_ = false;
    }

    gpio_num_t txPin_;
    gpio_num_t rxPin_;
    twai_general_config_t g_config_;
    twai_timing_config_t t_config_;
    twai_filter_config_t f_config_;
    SemaphoreHandle_t mutex_ = nullptr;
    bool driverInstalled_ = false;
    bool driverOK_ = false;
    uint32_t lastRecovery_ = 0;
    uint32_t exactFilterIds_[kMaxExactFilters] = {};
    uint8_t exactFilterCount_ = 0;
};
