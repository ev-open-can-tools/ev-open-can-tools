#pragma once

#include "../can_frame_types.h"
#include "can_driver.h"
#include <driver/twai.h>

class TWAIDriver : public CanDriver
{
public:
    static constexpr bool kSupportsISR = false;

    TWAIDriver(gpio_num_t txPin, gpio_num_t rxPin)
        : txPin_(txPin), rxPin_(rxPin) {}

    bool init() override
    {
        g_config_ = TWAI_GENERAL_CONFIG_DEFAULT(txPin_, rxPin_, TWAI_MODE_NORMAL);
        g_config_.rx_queue_len = 32;
        g_config_.tx_queue_len = 16;

        t_config_ = TWAI_TIMING_CONFIG_500KBITS();
        f_config_ = TWAI_FILTER_CONFIG_ACCEPT_ALL();

        if (twai_driver_install(&g_config_, &t_config_, &f_config_) != ESP_OK)
            return false;
        if (twai_start() != ESP_OK)
            return false;
        driverOK_ = true;
        return true;
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
        f_config_.acceptance_code = base << 21;
        f_config_.acceptance_mask = (differ << 21) | 0x001FFFFF;
        f_config_.single_filter = true;

        // twai_stop() disables the TWAI ISR before uninstall
        twai_stop();
        twai_driver_uninstall();
        if (twai_driver_install(&g_config_, &t_config_, &f_config_) != ESP_OK ||
            twai_start() != ESP_OK)
        {
            driverOK_ = false;
        }
    }

    bool enableInterrupt(void (* /*onReady*/)()) override { return false; }

    bool read(CanFrame &frame) override
    {
        if (!driverOK_)
        {
            tryRecover();
            return false;
        }

        twai_message_t msg;
        if (twai_receive(&msg, 0) != ESP_OK)
        {
            if (isBusOff())
                recoverWithCooldown();
            return false;
        }
        frame.id = msg.identifier;
        frame.dlc = (msg.data_length_code <= 8) ? msg.data_length_code : 8;
        memset(frame.data, 0, 8);
        memcpy(frame.data, msg.data, frame.dlc);
        return true;
    }

    void send(const CanFrame &frame) override
    {
        if (!driverOK_)
            return;

        twai_message_t msg = {};
        uint8_t dlc = (frame.dlc <= 8) ? frame.dlc : 8;
        msg.identifier = frame.id;
        msg.data_length_code = dlc;
        memcpy(msg.data, frame.data, dlc);

        // Short timeout (2ms): modified frames should not be dropped, but
        // long blocks (10ms) risk overflowing the 32-deep RX queue.
        // At 500kbps, ~8 frames arrive in 2ms — queue handles this fine.
        if (twai_transmit(&msg, pdMS_TO_TICKS(2)) != ESP_OK)
        {
            if (isBusOff())
                recoverWithCooldown();
        }
    }

private:
    static constexpr uint32_t BUSOFF_COOLDOWN_MS = 1000;

    bool isBusOff()
    {
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

        twai_stop();
        twai_driver_uninstall();
        if (twai_driver_install(&g_config_, &t_config_, &f_config_) != ESP_OK ||
            twai_start() != ESP_OK)
        {
            driverOK_ = false;
        }
    }

    void tryRecover()
    {
        uint32_t now = millis();
        if (now - lastRecovery_ < BUSOFF_COOLDOWN_MS * 10)
            return;
        lastRecovery_ = now;

        if (twai_driver_install(&g_config_, &t_config_, &f_config_) == ESP_OK &&
            twai_start() == ESP_OK)
        {
            driverOK_ = true;
        }
    }

    gpio_num_t txPin_;
    gpio_num_t rxPin_;
    twai_general_config_t g_config_;
    twai_timing_config_t t_config_;
    twai_filter_config_t f_config_;
    bool driverOK_ = false;
    uint32_t lastRecovery_ = 0;
};
