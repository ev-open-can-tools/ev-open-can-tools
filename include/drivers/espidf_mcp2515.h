#pragma once

#include <cstring>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include "platform/espidf_runtime.h"

#define CAN_EFF_FLAG 0x80000000UL
#define CAN_RTR_FLAG 0x40000000UL
#define CAN_SFF_MASK 0x000007FFUL
#define CAN_EFF_MASK 0x1FFFFFFFUL
#define CAN_MAX_DLEN 8

struct can_frame
{
    uint32_t can_id = 0;
    uint8_t can_dlc = 0;
    uint8_t data[CAN_MAX_DLEN] = {};
};

enum CAN_CLOCK
{
    MCP_20MHZ,
    MCP_16MHZ,
    MCP_8MHZ
};

enum CAN_SPEED
{
    CAN_500KBPS
};

class MCP2515
{
public:
    enum ERROR
    {
        ERROR_OK = 0,
        ERROR_FAIL = 1,
        ERROR_ALLTXBUSY = 2,
        ERROR_FAILINIT = 3,
        ERROR_FAILTX = 4,
        ERROR_NOMSG = 5
    };

    enum MASK
    {
        MASK0,
        MASK1
    };

    enum RXF
    {
        RXF0 = 0,
        RXF1,
        RXF2,
        RXF3,
        RXF4,
        RXF5
    };

    enum EFLG : uint8_t
    {
        EFLG_TXBO = (1 << 5)
    };

    explicit MCP2515(uint8_t csPin) : csPin_(csPin) {}

    ERROR reset()
    {
        ioError_ = false;
        if (!ensureBus())
            return ERROR_FAILINIT;
        uint8_t cmd = INSTRUCTION_RESET;
        if (!transfer(&cmd, nullptr, 1))
            return ERROR_FAILINIT;
        delay(10);
        writeReg(MCP_CANINTE, CANINTF_RX0IF | CANINTF_RX1IF | CANINTF_ERRIF | CANINTF_MERRF);
        bitModify(MCP_RXB0CTRL, RXBnCTRL_RXM_MASK | RXB0CTRL_BUKT | RXB0CTRL_FILHIT_MASK, RXBnCTRL_RXM_STDEXT | RXB0CTRL_BUKT | RXB0CTRL_FILHIT);
        bitModify(MCP_RXB1CTRL, RXBnCTRL_RXM_MASK | RXB1CTRL_FILHIT_MASK, RXBnCTRL_RXM_STDEXT | RXB1CTRL_FILHIT);
        return ioError_ ? ERROR_FAILINIT : ERROR_OK;
    }

    ERROR setBitrate(CAN_SPEED, CAN_CLOCK clock)
    {
        ERROR err = setConfigMode();
        if (err != ERROR_OK)
            return err;
        uint8_t cfg1 = 0, cfg2 = 0, cfg3 = 0;
        if (clock == MCP_16MHZ)
        {
            cfg1 = 0x00;
            cfg2 = 0xF0;
            cfg3 = 0x86;
        }
        else
        {
            cfg1 = 0x00;
            cfg2 = 0x90;
            cfg3 = 0x82;
        }
        writeReg(MCP_CNF1, cfg1);
        writeReg(MCP_CNF2, cfg2);
        writeReg(MCP_CNF3, cfg3);
        return ioError_ ? ERROR_FAILINIT : ERROR_OK;
    }

    ERROR setConfigMode() { return setMode(CANCTRL_REQOP_CONFIG); }
    ERROR setNormalMode() { return setMode(CANCTRL_REQOP_NORMAL); }

    ERROR setFilterMask(MASK mask, bool ext, uint32_t id)
    {
        ERROR err = setConfigMode();
        if (err != ERROR_OK)
            return err;
        uint8_t data[4];
        prepareId(data, ext, id);
        writeRegs(mask == MASK0 ? MCP_RXM0SIDH : MCP_RXM1SIDH, data, sizeof(data));
        return ioError_ ? ERROR_FAIL : ERROR_OK;
    }

    ERROR setFilter(RXF filter, bool ext, uint32_t id)
    {
        static constexpr uint8_t regs[] = {MCP_RXF0SIDH, MCP_RXF1SIDH, MCP_RXF2SIDH, MCP_RXF3SIDH, MCP_RXF4SIDH, MCP_RXF5SIDH};
        if (filter < RXF0 || filter > RXF5)
            return ERROR_FAIL;
        ERROR err = setConfigMode();
        if (err != ERROR_OK)
            return err;
        uint8_t data[4];
        prepareId(data, ext, id);
        writeRegs(regs[filter], data, sizeof(data));
        return ioError_ ? ERROR_FAIL : ERROR_OK;
    }

    ERROR readMessage(can_frame *frame)
    {
        if (!frame)
            return ERROR_FAIL;
        ioError_ = false;
        uint8_t status = readStatus();
        if (ioError_)
            return ERROR_FAIL;
        if (status & STAT_RX0IF)
            return readRx(MCP_RXB0CTRL, MCP_RXB0SIDH, MCP_RXB0DATA, CANINTF_RX0IF, frame);
        if (status & STAT_RX1IF)
            return readRx(MCP_RXB1CTRL, MCP_RXB1SIDH, MCP_RXB1DATA, CANINTF_RX1IF, frame);
        return ERROR_NOMSG;
    }

    ERROR sendMessage(const can_frame *frame)
    {
        if (!frame)
            return ERROR_FAILTX;
        ioError_ = false;
        static constexpr uint8_t ctrl[] = {MCP_TXB0CTRL, MCP_TXB1CTRL, MCP_TXB2CTRL};
        static constexpr uint8_t sidh[] = {MCP_TXB0SIDH, MCP_TXB1SIDH, MCP_TXB2SIDH};
        for (int i = 0; i < 3; i++)
        {
            if ((readReg(ctrl[i]) & TXB_TXREQ) == 0)
                return sendTx(ctrl[i], sidh[i], frame);
        }
        return ERROR_ALLTXBUSY;
    }

    uint8_t getErrorFlags()
    {
        ioError_ = false;
        return readReg(MCP_EFLG);
    }

private:
    static constexpr uint8_t INSTRUCTION_WRITE = 0x02;
    static constexpr uint8_t INSTRUCTION_READ = 0x03;
    static constexpr uint8_t INSTRUCTION_BITMOD = 0x05;
    static constexpr uint8_t INSTRUCTION_READ_STATUS = 0xA0;
    static constexpr uint8_t INSTRUCTION_RESET = 0xC0;
    static constexpr uint8_t TXB_EXIDE_MASK = 0x08;
    static constexpr uint8_t DLC_MASK = 0x0F;
    static constexpr uint8_t RTR_MASK = 0x40;
    static constexpr uint8_t MCP_SIDH = 0;
    static constexpr uint8_t MCP_SIDL = 1;
    static constexpr uint8_t MCP_EID8 = 2;
    static constexpr uint8_t MCP_EID0 = 3;
    static constexpr uint8_t MCP_DLC = 4;
    static constexpr uint8_t MCP_DATA = 5;
    static constexpr uint8_t STAT_RX0IF = 0x01;
    static constexpr uint8_t STAT_RX1IF = 0x02;
    static constexpr uint8_t CANINTF_RX0IF = 0x01;
    static constexpr uint8_t CANINTF_RX1IF = 0x02;
    static constexpr uint8_t CANINTF_ERRIF = 0x20;
    static constexpr uint8_t CANINTF_MERRF = 0x80;
    static constexpr uint8_t RXBnCTRL_RXM_MASK = 0x60;
    static constexpr uint8_t RXBnCTRL_RXM_STDEXT = 0x00;
    static constexpr uint8_t RXBnCTRL_RTR = 0x08;
    static constexpr uint8_t RXB0CTRL_BUKT = 0x04;
    static constexpr uint8_t RXB0CTRL_FILHIT_MASK = 0x03;
    static constexpr uint8_t RXB1CTRL_FILHIT_MASK = 0x07;
    static constexpr uint8_t RXB0CTRL_FILHIT = 0x00;
    static constexpr uint8_t RXB1CTRL_FILHIT = 0x01;
    static constexpr uint8_t TXB_TXREQ = 0x08;
    static constexpr uint8_t TXB_ABTF = 0x40;
    static constexpr uint8_t TXB_MLOA = 0x20;
    static constexpr uint8_t TXB_TXERR = 0x10;
    static constexpr uint8_t CANCTRL_REQOP = 0xE0;
    static constexpr uint8_t CANSTAT_OPMOD = 0xE0;
    static constexpr uint8_t CANCTRL_REQOP_NORMAL = 0x00;
    static constexpr uint8_t CANCTRL_REQOP_CONFIG = 0x80;
    static constexpr uint8_t MCP_RXF0SIDH = 0x00;
    static constexpr uint8_t MCP_RXF1SIDH = 0x04;
    static constexpr uint8_t MCP_RXF2SIDH = 0x08;
    static constexpr uint8_t MCP_RXF3SIDH = 0x10;
    static constexpr uint8_t MCP_RXF4SIDH = 0x14;
    static constexpr uint8_t MCP_RXF5SIDH = 0x18;
    static constexpr uint8_t MCP_RXM0SIDH = 0x20;
    static constexpr uint8_t MCP_RXM1SIDH = 0x24;
    static constexpr uint8_t MCP_CNF3 = 0x28;
    static constexpr uint8_t MCP_CNF2 = 0x29;
    static constexpr uint8_t MCP_CNF1 = 0x2A;
    static constexpr uint8_t MCP_CANINTE = 0x2B;
    static constexpr uint8_t MCP_CANINTF = 0x2C;
    static constexpr uint8_t MCP_EFLG = 0x2D;
    static constexpr uint8_t MCP_TXB0CTRL = 0x30;
    static constexpr uint8_t MCP_TXB0SIDH = 0x31;
    static constexpr uint8_t MCP_TXB1CTRL = 0x40;
    static constexpr uint8_t MCP_TXB1SIDH = 0x41;
    static constexpr uint8_t MCP_TXB2CTRL = 0x50;
    static constexpr uint8_t MCP_TXB2SIDH = 0x51;
    static constexpr uint8_t MCP_RXB0CTRL = 0x60;
    static constexpr uint8_t MCP_RXB0SIDH = 0x61;
    static constexpr uint8_t MCP_RXB0DATA = 0x66;
    static constexpr uint8_t MCP_RXB1CTRL = 0x70;
    static constexpr uint8_t MCP_RXB1SIDH = 0x71;
    static constexpr uint8_t MCP_RXB1DATA = 0x76;
    static constexpr uint8_t MCP_CANSTAT = 0x0E;
    static constexpr uint8_t MCP_CANCTRL = 0x0F;

    bool ensureBus()
    {
        if (device_)
            return true;
        spi_bus_config_t bus = {};
        bus.mosi_io_num = SPI_MOSI;
        bus.miso_io_num = SPI_MISO;
        bus.sclk_io_num = SPI_SCK;
        bus.quadwp_io_num = -1;
        bus.quadhd_io_num = -1;
        esp_err_t err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
            return false;
        spi_device_interface_config_t dev = {};
        dev.clock_speed_hz = 8000000;
        dev.mode = 0;
        dev.spics_io_num = csPin_;
        dev.queue_size = 1;
        return spi_bus_add_device(SPI2_HOST, &dev, &device_) == ESP_OK;
    }

    bool transfer(const uint8_t *tx, uint8_t *rx, size_t len)
    {
        if (!ensureBus())
            return false;
        spi_transaction_t t = {};
        t.length = len * 8;
        t.tx_buffer = tx;
        t.rx_buffer = rx;
        return spi_device_transmit(device_, &t) == ESP_OK;
    }

    uint8_t readReg(uint8_t reg)
    {
        uint8_t tx[3] = {INSTRUCTION_READ, reg, 0};
        uint8_t rx[3] = {};
        if (!transfer(tx, rx, sizeof(tx)))
            ioError_ = true;
        return rx[2];
    }

    void readRegs(uint8_t reg, uint8_t *data, size_t len)
    {
        if (!data || len > 14)
        {
            ioError_ = true;
            return;
        }
        uint8_t tx[16] = {};
        uint8_t rx[16] = {};
        tx[0] = INSTRUCTION_READ;
        tx[1] = reg;
        if (!transfer(tx, rx, len + 2))
        {
            ioError_ = true;
            memset(data, 0, len);
            return;
        }
        memcpy(data, rx + 2, len);
    }

    void writeReg(uint8_t reg, uint8_t value)
    {
        uint8_t tx[3] = {INSTRUCTION_WRITE, reg, value};
        if (!transfer(tx, nullptr, sizeof(tx)))
            ioError_ = true;
    }

    void writeRegs(uint8_t reg, const uint8_t *data, size_t len)
    {
        if (!data || len > 14)
        {
            ioError_ = true;
            return;
        }
        uint8_t tx[16] = {};
        tx[0] = INSTRUCTION_WRITE;
        tx[1] = reg;
        memcpy(tx + 2, data, len);
        if (!transfer(tx, nullptr, len + 2))
            ioError_ = true;
    }

    void bitModify(uint8_t reg, uint8_t mask, uint8_t data)
    {
        uint8_t tx[4] = {INSTRUCTION_BITMOD, reg, mask, data};
        if (!transfer(tx, nullptr, sizeof(tx)))
            ioError_ = true;
    }

    uint8_t readStatus()
    {
        uint8_t tx[2] = {INSTRUCTION_READ_STATUS, 0};
        uint8_t rx[2] = {};
        if (!transfer(tx, rx, sizeof(tx)))
            ioError_ = true;
        return rx[1];
    }

    ERROR setMode(uint8_t mode)
    {
        ioError_ = false;
        bitModify(MCP_CANCTRL, CANCTRL_REQOP, mode);
        uint32_t started = millis();
        while (millis() - started < 10)
        {
            if (!ioError_ && (readReg(MCP_CANSTAT) & CANSTAT_OPMOD) == mode)
                return ERROR_OK;
            if (ioError_)
                return ERROR_FAIL;
        }
        return ERROR_FAIL;
    }

    void prepareId(uint8_t *buffer, bool ext, uint32_t id)
    {
        uint16_t canid = static_cast<uint16_t>(id & 0x0FFFF);
        if (ext)
        {
            buffer[MCP_EID0] = static_cast<uint8_t>(canid & 0xFF);
            buffer[MCP_EID8] = static_cast<uint8_t>(canid >> 8);
            canid = static_cast<uint16_t>(id >> 16);
            buffer[MCP_SIDL] = static_cast<uint8_t>(canid & 0x03);
            buffer[MCP_SIDL] += static_cast<uint8_t>((canid & 0x1C) << 3);
            buffer[MCP_SIDL] |= TXB_EXIDE_MASK;
            buffer[MCP_SIDH] = static_cast<uint8_t>(canid >> 5);
        }
        else
        {
            buffer[MCP_SIDH] = static_cast<uint8_t>(canid >> 3);
            buffer[MCP_SIDL] = static_cast<uint8_t>((canid & 0x07) << 5);
            buffer[MCP_EID0] = 0;
            buffer[MCP_EID8] = 0;
        }
    }

    ERROR sendTx(uint8_t ctrl, uint8_t sidh, const can_frame *frame)
    {
        if (frame->can_dlc > CAN_MAX_DLEN)
            return ERROR_FAILTX;
        uint8_t data[13] = {};
        bool ext = (frame->can_id & CAN_EFF_FLAG) != 0;
        bool rtr = (frame->can_id & CAN_RTR_FLAG) != 0;
        uint32_t id = frame->can_id & (ext ? CAN_EFF_MASK : CAN_SFF_MASK);
        prepareId(data, ext, id);
        data[MCP_DLC] = rtr ? (frame->can_dlc | RTR_MASK) : frame->can_dlc;
        memcpy(&data[MCP_DATA], frame->data, frame->can_dlc);
        writeRegs(sidh, data, 5 + frame->can_dlc);
        bitModify(ctrl, TXB_TXREQ, TXB_TXREQ);
        uint8_t result = readReg(ctrl);
        return ioError_ || (result & (TXB_ABTF | TXB_MLOA | TXB_TXERR)) ? ERROR_FAILTX : ERROR_OK;
    }

    ERROR readRx(uint8_t ctrlReg, uint8_t sidhReg, uint8_t dataReg, uint8_t flag, can_frame *frame)
    {
        uint8_t header[5] = {};
        readRegs(sidhReg, header, sizeof(header));
        if (ioError_)
            return ERROR_FAIL;
        uint32_t id = (header[MCP_SIDH] << 3) + (header[MCP_SIDL] >> 5);
        if ((header[MCP_SIDL] & TXB_EXIDE_MASK) == TXB_EXIDE_MASK)
        {
            id = (id << 2) + (header[MCP_SIDL] & 0x03);
            id = (id << 8) + header[MCP_EID8];
            id = (id << 8) + header[MCP_EID0];
            id |= CAN_EFF_FLAG;
        }
        uint8_t dlc = header[MCP_DLC] & DLC_MASK;
        if (dlc > CAN_MAX_DLEN)
        {
            bitModify(MCP_CANINTF, flag, 0);
            return ERROR_FAIL;
        }
        if (readReg(ctrlReg) & RXBnCTRL_RTR)
            id |= CAN_RTR_FLAG;
        frame->can_id = id;
        frame->can_dlc = dlc;
        memset(frame->data, 0, sizeof(frame->data));
        readRegs(dataReg, frame->data, dlc);
        bitModify(MCP_CANINTF, flag, 0);
        return ioError_ ? ERROR_FAIL : ERROR_OK;
    }

    uint8_t csPin_;
    spi_device_handle_t device_ = nullptr;
    bool ioError_ = false;
};
