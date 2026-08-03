#pragma once

#include "../can_frame_types.h"
#include "can_driver.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#include <driver/twai.h>
#pragma GCC diagnostic pop
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>
#include "../platform/espidf_runtime.h"

class TWAIDriver : public CanDriver
{
public:
    static constexpr bool kSupportsISR = false;

    TWAIDriver(gpio_num_t txPin, gpio_num_t rxPin)
        : txPin_(txPin), rxPin_(rxPin), mutex_(xSemaphoreCreateMutex()) {}

    ~TWAIDriver() override
    {
        if (mutex_)
            vSemaphoreDelete(mutex_);
    }

    bool init() override
    {
        if (!mutex_)
            return false;

        initRequested_ = true;

        g_config_ = TWAI_GENERAL_CONFIG_DEFAULT(txPin_, rxPin_, TWAI_MODE_NORMAL);
        g_config_.rx_queue_len = 32;
        g_config_.tx_queue_len = 16;

        t_config_ = TWAI_TIMING_CONFIG_500KBITS();
        f_config_ = TWAI_FILTER_CONFIG_ACCEPT_ALL();
        normalFilter_ = f_config_;

        lock();
        driverOK_ = installAndStartLocked();
        unlock();
        return driverOK_;
    }

    void setFilters(const uint32_t *ids, uint8_t count) override
    {
        if (!mutex_ || !ids || count == 0)
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
        normalFilter_ = nextFilter;
        twai_filter_config_t acceptAll = TWAI_FILTER_CONFIG_ACCEPT_ALL();
        f_config_ = monitorAll_ ? acceptAll : normalFilter_;
        if (!initRequested_)
        {
            unlock();
            return;
        }
        stopAndUninstallLocked();
        driverOK_ = installAndStartLocked();
        unlock();
    }

    void setMonitorAll(bool enabled) override
    {
        if (!mutex_)
            return;
        lock();
        if (monitorAll_ == enabled)
        {
            unlock();
            return;
        }
        monitorAll_ = enabled;
        twai_filter_config_t acceptAll = TWAI_FILTER_CONFIG_ACCEPT_ALL();
        f_config_ = monitorAll_ ? acceptAll : normalFilter_;
        if (driverInstalled_)
        {
            stopAndUninstallLocked();
            driverOK_ = installAndStartLocked();
        }
        unlock();
    }

    bool enableInterrupt(void (* /*onReady*/)()) override { return false; }

    bool ready() const override
    {
        lock();
        bool value = driverOK_;
        unlock();
        return value;
    }

    void setSimLoopback(bool enabled) override { simLoopback_ = enabled; }

    bool read(CanFrame &frame) override
    {
        for (uint8_t attempt = 0; attempt < kReadDrainBudget; attempt++)
        {
            lock();
            if (!driverOK_)
            {
                tryRecoverLocked();
                unlock();
                return false;
            }
            if (!serviceBusStateLocked())
            {
                unlock();
                return false;
            }

            twai_message_t msg;
            esp_err_t rxErr = twai_receive(&msg, 0);
            if (rxErr != ESP_OK)
            {
                lastReceiveErr_ = rxErr;
                if (rxErr != ESP_ERR_TIMEOUT)
                {
                    receiveErrors_++;
                    if (!serviceBusStateLocked())
                    {
                        unlock();
                        return false;
                    }
                    else
                        driverOK_ = false;
                }
                unlock();
                return false;
            }
            bool accepted = !msg.extd && !msg.rtr && exactFilterMatchesLocked(msg.identifier);
            if (!accepted)
                rejectedFrames_++;
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
        if (simLoopback_)
        {
            // Dev/test mode: pretend the frame went out so TX counters/sniffer
            // light up, but never touch the (absent) transceiver.
            if (onSendFrame)
                onSendFrame(frame, true);
            return true;
        }

        if (!sendAllowed(frame) || frame.id > 0x7FF || frame.dlc > 8)
        {
            if (onSendFrame)
                onSendFrame(frame, false);
            return false;
        }
        lock();
        if (!driverOK_)
        {
            unlock();
            if (onSendFrame)
                onSendFrame(frame, false);
            return false;
        }
        if (!serviceBusStateLocked())
        {
            unlock();
            if (onSendFrame)
                onSendFrame(frame, false);
            return false;
        }

        twai_message_t msg = {};
        uint8_t dlc = frame.dlc;
        msg.identifier = frame.id;
        msg.data_length_code = dlc;
        memcpy(msg.data, frame.data, dlc);

        // Short timeout (2ms): modified frames should not be dropped, but
        // long blocks (10ms) risk overflowing the 32-deep RX queue.
        // At 500kbps, ~8 frames arrive in 2ms — queue handles this fine.
        esp_err_t txErr = twai_transmit(&msg, pdMS_TO_TICKS(2));
        bool ok = txErr == ESP_OK;
        if (!ok)
        {
            lastTransmitErr_ = txErr;
            transmitErrors_++;
            serviceBusStateLocked();
            if (txErr != ESP_ERR_TIMEOUT && !recoveryInProgress_)
                driverOK_ = false;
            uint32_t now = millis();
            if (now - lastTxFailLogMs_ >= 2000)
            {
                lastTxFailLogMs_ = now;
                Serial.printf("[TWAI] TX failed: %s total=%lu\n", esp_err_to_name(txErr),
                              static_cast<unsigned long>(transmitErrors_));
            }
        }
        unlock();
        if (onSendFrame)
            onSendFrame(frame, ok);
        return ok;
    }

    void clearPendingTransmit() override
    {
        lock();
        if (driverInstalled_)
            twai_clear_transmit_queue();
        unlock();
    }

    void diagnosticsJson(char *out, size_t outLen) const override
    {
        if (!out || outLen == 0)
            return;

        lock();
        twai_status_info_t status = {};
        bool hasStatus = driverInstalled_ && twai_get_status_info(&status) == ESP_OK;
        const twai_status_info_t &s = hasStatus ? status : lastStatus_;
        snprintf(out, outLen,
                 "{\"type\":\"twai\",\"txPin\":%d,\"rxPin\":%d,\"ok\":%s,\"installed\":%s,\"state\":\"%s\",\"stateCode\":%d,\"msgsToTx\":%u,\"msgsToRx\":%u,\"maxMsgsToTx\":%u,\"maxMsgsToRx\":%u,\"txErrCounter\":%u,\"rxErrCounter\":%u,\"txFailed\":%u,\"rxMissed\":%u,\"rxOverrun\":%u,\"arbLost\":%u,\"busErrors\":%u,\"recoveries\":%u,\"rxErrors\":%u,\"txErrors\":%u,\"rejected\":%u,\"lastInstallErr\":%d,\"lastStartErr\":%d,\"lastRxErr\":%d,\"lastTxErr\":%d}",
                 static_cast<int>(txPin_), static_cast<int>(rxPin_), driverOK_ ? "true" : "false",
                 driverInstalled_ ? "true" : "false", twaiStateName(s.state), static_cast<int>(s.state),
                 static_cast<unsigned int>(s.msgs_to_tx), static_cast<unsigned int>(s.msgs_to_rx),
                 static_cast<unsigned int>(maxTxQueueDepth_),
                 static_cast<unsigned int>(maxRxQueueDepth_),
                 static_cast<unsigned int>(s.tx_error_counter), static_cast<unsigned int>(s.rx_error_counter),
                 static_cast<unsigned int>(s.tx_failed_count), static_cast<unsigned int>(s.rx_missed_count),
                 static_cast<unsigned int>(s.rx_overrun_count), static_cast<unsigned int>(s.arb_lost_count),
                 static_cast<unsigned int>(s.bus_error_count), static_cast<unsigned int>(recoveries_),
                 static_cast<unsigned int>(receiveErrors_), static_cast<unsigned int>(transmitErrors_),
                 static_cast<unsigned int>(rejectedFrames_), static_cast<int>(lastInstallErr_),
                 static_cast<int>(lastStartErr_), static_cast<int>(lastReceiveErr_),
                 static_cast<int>(lastTransmitErr_));
        unlock();
    }

    void diagnosticsSummary(char *out, size_t outLen) const override
    {
        if (!out || outLen == 0)
            return;

        lock();
        twai_status_info_t status = {};
        bool hasStatus = driverInstalled_ && twai_get_status_info(&status) == ESP_OK;
        const twai_status_info_t &s = hasStatus ? status : lastStatus_;
        snprintf(out, outLen,
                 "TWAI tx=%d rx=%d ok=%s installed=%s state=%s msgsToRx=%u msgsToTx=%u maxMsgsToRx=%u maxMsgsToTx=%u txErrCounter=%u rxErrCounter=%u txFailed=%u rxMissed=%u rxOverrun=%u arbLost=%u busErrors=%u recoveries=%u rxErrors=%u txErrors=%u rejected=%u lastInstallErr=%d lastStartErr=%d lastRxErr=%d lastTxErr=%d",
                 static_cast<int>(txPin_), static_cast<int>(rxPin_), driverOK_ ? "yes" : "no",
                 driverInstalled_ ? "yes" : "no", twaiStateName(s.state),
                 static_cast<unsigned int>(s.msgs_to_rx), static_cast<unsigned int>(s.msgs_to_tx),
                 static_cast<unsigned int>(maxRxQueueDepth_),
                 static_cast<unsigned int>(maxTxQueueDepth_),
                 static_cast<unsigned int>(s.tx_error_counter), static_cast<unsigned int>(s.rx_error_counter),
                 static_cast<unsigned int>(s.tx_failed_count), static_cast<unsigned int>(s.rx_missed_count),
                 static_cast<unsigned int>(s.rx_overrun_count), static_cast<unsigned int>(s.arb_lost_count),
                 static_cast<unsigned int>(s.bus_error_count), static_cast<unsigned int>(recoveries_),
                 static_cast<unsigned int>(receiveErrors_), static_cast<unsigned int>(transmitErrors_),
                 static_cast<unsigned int>(rejectedFrames_), static_cast<int>(lastInstallErr_),
                 static_cast<int>(lastStartErr_), static_cast<int>(lastReceiveErr_),
                 static_cast<int>(lastTransmitErr_));
        unlock();
    }

    void configurationSummary(char *out, size_t outLen) const override
    {
        if (!out || outLen == 0)
            return;
        snprintf(out, outLen, "bitrate=500000 txGPIO=%d rxGPIO=%d",
                 static_cast<int>(txPin_), static_cast<int>(rxPin_));
    }

private:
    static constexpr uint8_t kMaxExactFilters = 160;
    static constexpr uint8_t kReadDrainBudget = 8;
    static constexpr uint32_t BUSOFF_COOLDOWN_MS = 1000;

    static const char *twaiStateName(twai_state_t state)
    {
        switch (state)
        {
        case TWAI_STATE_STOPPED:
            return "stopped";
        case TWAI_STATE_RUNNING:
            return "running";
        case TWAI_STATE_BUS_OFF:
            return "bus_off";
        case TWAI_STATE_RECOVERING:
            return "recovering";
        default:
            return "unknown";
        }
    }

    bool exactFilterMatchesLocked(uint32_t id) const
    {
        if (monitorAll_)
            return true;
        if (exactFilterCount_ == 0)
            return true;
        for (uint8_t i = 0; i < exactFilterCount_; i++)
        {
            if (exactFilterIds_[i] == id)
                return true;
        }
        return false;
    }

    bool serviceBusStateLocked()
    {
        if (!driverInstalled_)
            return false;
        twai_status_info_t status = {};
        if (twai_get_status_info(&status) != ESP_OK)
            return true;
        lastStatus_ = status;
        if (status.msgs_to_rx > maxRxQueueDepth_)
            maxRxQueueDepth_ = status.msgs_to_rx;
        if (status.msgs_to_tx > maxTxQueueDepth_)
            maxTxQueueDepth_ = status.msgs_to_tx;
        if (status.state == TWAI_STATE_RUNNING)
        {
            recoveryInProgress_ = false;
            return true;
        }
        if (status.state == TWAI_STATE_BUS_OFF)
        {
            uint32_t now = millis();
            if (!recoveryInProgress_ && now - lastRecovery_ >= BUSOFF_COOLDOWN_MS)
            {
                lastRecovery_ = now;
                esp_err_t err = twai_initiate_recovery();
                if (err == ESP_OK)
                {
                    recoveryInProgress_ = true;
                    recoveries_++;
                    Serial.println("[TWAI] BUS_OFF; recovery initiated");
                }
                else
                    Serial.printf("[TWAI] recovery initiation failed: %s\n", esp_err_to_name(err));
            }
            return false;
        }
        if (status.state == TWAI_STATE_RECOVERING)
        {
            recoveryInProgress_ = true;
            return false;
        }
        if (status.state == TWAI_STATE_STOPPED && recoveryInProgress_)
        {
            lastStartErr_ = twai_start();
            recoveryInProgress_ = false;
            driverOK_ = lastStartErr_ == ESP_OK;
            if (driverOK_)
                Serial.println("[TWAI] recovery complete; driver restarted");
            return driverOK_;
        }
        driverOK_ = false;
        return false;
    }

    void tryRecoverLocked()
    {
        uint32_t now = millis();
        if (now - lastRecovery_ < BUSOFF_COOLDOWN_MS * 10)
            return;
        lastRecovery_ = now;
        recoveries_++;

        stopAndUninstallLocked();
        driverOK_ = installAndStartLocked();
    }

    void lock() const
    {
        if (mutex_)
            xSemaphoreTake(mutex_, portMAX_DELAY);
    }

    void unlock() const
    {
        if (mutex_)
            xSemaphoreGive(mutex_);
    }

    bool installAndStartLocked()
    {
        lastInstallErr_ = twai_driver_install(&g_config_, &t_config_, &f_config_);
        if (lastInstallErr_ != ESP_OK)
        {
            driverInstalled_ = false;
            return false;
        }
        driverInstalled_ = true;
        lastStartErr_ = twai_start();
        if (lastStartErr_ != ESP_OK)
        {
            twai_driver_uninstall();
            driverInstalled_ = false;
            return false;
        }
        lastInstallErr_ = ESP_OK;
        lastStartErr_ = ESP_OK;
        twai_get_status_info(&lastStatus_);
        recoveryInProgress_ = false;
        return true;
    }

    void stopAndUninstallLocked()
    {
        if (!driverInstalled_)
            return;
        twai_get_status_info(&lastStatus_);
        twai_stop();
        twai_driver_uninstall();
        driverInstalled_ = false;
        driverOK_ = false;
    }

    gpio_num_t txPin_;
    gpio_num_t rxPin_;
    twai_general_config_t g_config_ = {};
    twai_timing_config_t t_config_ = {};
    twai_filter_config_t f_config_ = {};
    twai_filter_config_t normalFilter_ = {};
    mutable SemaphoreHandle_t mutex_ = nullptr;
    bool driverInstalled_ = false;
    bool driverOK_ = false;
    bool simLoopback_ = false;
    uint32_t lastRecovery_ = 0;
    uint32_t exactFilterIds_[kMaxExactFilters] = {};
    uint8_t exactFilterCount_ = 0;
    twai_status_info_t lastStatus_ = {};
    esp_err_t lastInstallErr_ = ESP_OK;
    esp_err_t lastStartErr_ = ESP_OK;
    esp_err_t lastReceiveErr_ = ESP_OK;
    esp_err_t lastTransmitErr_ = ESP_OK;
    uint32_t receiveErrors_ = 0;
    uint32_t transmitErrors_ = 0;
    uint32_t rejectedFrames_ = 0;
    uint32_t recoveries_ = 0;
    uint32_t maxRxQueueDepth_ = 0;
    uint32_t maxTxQueueDepth_ = 0;
    bool recoveryInProgress_ = false;
    uint32_t lastTxFailLogMs_ = 0;
    bool monitorAll_ = false;
    bool initRequested_ = false;
};
