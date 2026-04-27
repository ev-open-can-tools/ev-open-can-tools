#pragma once

#include <cstdint>
#include <cstring>

#include "can_frame_types.h"
#include "drivers/can_driver.h"

#ifndef ISOTP_PAYLOAD_MAX
#define ISOTP_PAYLOAD_MAX 32
#endif

enum IsoTpPciType : uint8_t
{
    ISOTP_PCI_SINGLE = 0,
    ISOTP_PCI_FIRST = 1,
    ISOTP_PCI_CONSECUTIVE = 2,
    ISOTP_PCI_FLOW_CONTROL = 3,
};

enum IsoTpError : uint8_t
{
    ISOTP_ERR_NONE = 0,
    ISOTP_ERR_BAD_LENGTH = 1,
    ISOTP_ERR_OVERFLOW = 2,
    ISOTP_ERR_WRONG_SEQUENCE = 3,
    ISOTP_ERR_UNEXPECTED_CF = 4,
    ISOTP_ERR_UNEXPECTED_FC = 5,
    ISOTP_ERR_FC_OVERFLOW = 6,
    ISOTP_ERR_FC_WAIT = 7,
    ISOTP_ERR_TX_TOO_LARGE = 8,
    ISOTP_ERR_SEND_FAILED = 9,
    ISOTP_ERR_RX_TIMEOUT = 10,
};

struct IsoTpLink
{
    uint8_t rxPayload[ISOTP_PAYLOAD_MAX];
    uint16_t rxLength;
    uint16_t rxReceived;
    uint8_t rxNextSeq;
    bool rxActive;
    bool rxComplete;

    uint8_t txPayload[ISOTP_PAYLOAD_MAX];
    uint16_t txLength;
    uint16_t txOffset;
    uint8_t txNextSeq;
    uint8_t txBlockSize;
    uint8_t txBlockRemaining;
    uint8_t txStMin;
    bool txActive;
    bool txWaitingFlowControl;

    uint8_t lastRxPci;
    uint8_t lastTxPci;
    uint8_t lastError;
    uint16_t lastRxLength;
    uint16_t lastTxLength;
    uint16_t lastCompleteRxLength;
    uint16_t lastCompleteTxLength;
    uint8_t flowControlRxCount;
    uint8_t flowControlTxCount;
};

static const char *isoTpPciName(uint8_t pci)
{
    switch (pci)
    {
    case ISOTP_PCI_SINGLE:
        return "single";
    case ISOTP_PCI_FIRST:
        return "first";
    case ISOTP_PCI_CONSECUTIVE:
        return "consecutive";
    case ISOTP_PCI_FLOW_CONTROL:
        return "flow_control";
    }
    return "unknown";
}

static const char *isoTpErrorName(uint8_t error)
{
    switch (error)
    {
    case ISOTP_ERR_NONE:
        return "none";
    case ISOTP_ERR_BAD_LENGTH:
        return "bad_length";
    case ISOTP_ERR_OVERFLOW:
        return "overflow";
    case ISOTP_ERR_WRONG_SEQUENCE:
        return "wrong_sequence";
    case ISOTP_ERR_UNEXPECTED_CF:
        return "unexpected_consecutive";
    case ISOTP_ERR_UNEXPECTED_FC:
        return "unexpected_flow_control";
    case ISOTP_ERR_FC_OVERFLOW:
        return "flow_control_overflow";
    case ISOTP_ERR_FC_WAIT:
        return "flow_control_wait";
    case ISOTP_ERR_TX_TOO_LARGE:
        return "tx_too_large";
    case ISOTP_ERR_SEND_FAILED:
        return "send_failed";
    case ISOTP_ERR_RX_TIMEOUT:
        return "rx_timeout";
    }
    return "unknown";
}

static void isoTpClearRx(IsoTpLink &link)
{
    link.rxLength = 0;
    link.rxReceived = 0;
    link.rxNextSeq = 0;
    link.rxActive = false;
    link.rxComplete = false;
}

static void isoTpClearTx(IsoTpLink &link)
{
    link.txLength = 0;
    link.txOffset = 0;
    link.txNextSeq = 0;
    link.txBlockSize = 0;
    link.txBlockRemaining = 0;
    link.txStMin = 0;
    link.txActive = false;
    link.txWaitingFlowControl = false;
}

static CanFrame isoTpMakeFrame(uint32_t id, uint8_t bus)
{
    CanFrame frame;
    frame.id = id;
    frame.dlc = 8;
    frame.bus = bus;
    return frame;
}

static void isoTpPadFrame(CanFrame &frame, uint8_t start)
{
    for (uint8_t i = start; i < 8; i++)
        frame.data[i] = 0x00;
}

static bool isoTpSendFrame(IsoTpLink &link, CanDriver &driver, const CanFrame &frame,
                           uint8_t pci)
{
    link.lastTxPci = pci;
    if (!driver.send(frame))
    {
        link.lastError = ISOTP_ERR_SEND_FAILED;
        return false;
    }
    link.lastError = ISOTP_ERR_NONE;
    return true;
}

static bool isoTpSendConsecutiveFrames(IsoTpLink &link, CanDriver &driver, uint32_t txId,
                                       uint8_t bus)
{
    uint8_t sentInBlock = 0;
    while (link.txActive && link.txOffset < link.txLength)
    {
        if (link.txBlockSize != 0 && sentInBlock >= link.txBlockSize)
        {
            link.txWaitingFlowControl = true;
            return true;
        }

        CanFrame frame = isoTpMakeFrame(txId, bus);
        frame.data[0] = 0x20 | (link.txNextSeq & 0x0F);
        uint8_t copied = 0;
        while (copied < 7 && link.txOffset < link.txLength)
        {
            frame.data[1 + copied] = link.txPayload[link.txOffset++];
            copied++;
        }
        isoTpPadFrame(frame, 1 + copied);

        if (!isoTpSendFrame(link, driver, frame, ISOTP_PCI_CONSECUTIVE))
            return false;

        link.txNextSeq = (link.txNextSeq + 1) & 0x0F;
        sentInBlock++;
    }

    link.lastCompleteTxLength = link.txLength;
    isoTpClearTx(link);
    return true;
}

static bool isoTpSendPayload(IsoTpLink &link, CanDriver &driver, uint32_t txId,
                             uint8_t bus, const uint8_t *payload, uint16_t len)
{
    link.rxComplete = false;
    link.lastTxLength = len;

    if (len > ISOTP_PAYLOAD_MAX)
    {
        link.lastError = ISOTP_ERR_TX_TOO_LARGE;
        return false;
    }

    if (len <= 7)
    {
        CanFrame frame = isoTpMakeFrame(txId, bus);
        frame.data[0] = len & 0x0F;
        for (uint8_t i = 0; i < len; i++)
            frame.data[1 + i] = payload[i];
        isoTpPadFrame(frame, 1 + len);
        link.lastCompleteTxLength = len;
        isoTpClearTx(link);
        return isoTpSendFrame(link, driver, frame, ISOTP_PCI_SINGLE);
    }

    std::memcpy(link.txPayload, payload, len);
    link.txLength = len;
    link.txOffset = 6;
    link.txNextSeq = 1;
    link.txBlockSize = 0;
    link.txBlockRemaining = 0;
    link.txStMin = 0;
    link.txActive = true;
    link.txWaitingFlowControl = true;
    link.lastCompleteTxLength = 0;

    CanFrame frame = isoTpMakeFrame(txId, bus);
    frame.data[0] = 0x10 | ((len >> 8) & 0x0F);
    frame.data[1] = len & 0xFF;
    for (uint8_t i = 0; i < 6; i++)
        frame.data[2 + i] = payload[i];

    return isoTpSendFrame(link, driver, frame, ISOTP_PCI_FIRST);
}

static bool isoTpSendFlowControl(IsoTpLink &link, CanDriver &driver, uint32_t txId,
                                 uint8_t bus, uint8_t status)
{
    CanFrame frame = isoTpMakeFrame(txId, bus);
    frame.data[0] = 0x30 | (status & 0x0F);
    frame.data[1] = 0x00; // block size: unlimited
    frame.data[2] = 0x00; // STmin: no delay request
    isoTpPadFrame(frame, 3);
    link.flowControlTxCount++;
    return isoTpSendFrame(link, driver, frame, ISOTP_PCI_FLOW_CONTROL);
}

static bool isoTpHandleFrame(IsoTpLink &link, CanDriver &driver, const CanFrame &frame,
                             uint32_t flowControlTxId)
{
    if (frame.dlc < 1)
    {
        link.lastError = ISOTP_ERR_BAD_LENGTH;
        return false;
    }

    link.rxComplete = false;
    uint8_t pci = frame.data[0] >> 4;
    link.lastRxPci = pci;

    switch (pci)
    {
    case ISOTP_PCI_SINGLE:
    {
        uint8_t len = frame.data[0] & 0x0F;
        if (len > 7 || frame.dlc < 1 + len)
        {
            link.lastError = ISOTP_ERR_BAD_LENGTH;
            isoTpClearRx(link);
            return true;
        }
        for (uint8_t i = 0; i < len; i++)
            link.rxPayload[i] = frame.data[1 + i];
        link.rxLength = len;
        link.rxReceived = len;
        link.lastRxLength = len;
        link.lastCompleteRxLength = len;
        link.rxActive = false;
        link.rxComplete = true;
        link.lastError = ISOTP_ERR_NONE;
        return true;
    }

    case ISOTP_PCI_FIRST:
    {
        uint16_t len = ((frame.data[0] & 0x0F) << 8) | frame.data[1];
        link.lastRxLength = len;
        if (len <= 7)
        {
            link.lastError = ISOTP_ERR_BAD_LENGTH;
            isoTpClearRx(link);
            return true;
        }
        if (len > ISOTP_PAYLOAD_MAX)
        {
            link.lastError = ISOTP_ERR_OVERFLOW;
            isoTpClearRx(link);
            bool sent = isoTpSendFlowControl(link, driver, flowControlTxId, frame.bus, 0x02);
            if (sent)
                link.lastError = ISOTP_ERR_OVERFLOW;
            return true;
        }

        link.rxLength = len;
        link.rxReceived = 0;
        link.lastCompleteRxLength = 0;
        uint8_t copied = 0;
        while (copied < 6 && link.rxReceived < link.rxLength)
        {
            link.rxPayload[link.rxReceived++] = frame.data[2 + copied];
            copied++;
        }
        link.rxNextSeq = 1;
        link.rxActive = true;
        link.rxComplete = false;
        link.lastError = ISOTP_ERR_NONE;
        isoTpSendFlowControl(link, driver, flowControlTxId, frame.bus, 0x00);
        return true;
    }

    case ISOTP_PCI_CONSECUTIVE:
    {
        if (!link.rxActive)
        {
            link.lastError = ISOTP_ERR_UNEXPECTED_CF;
            return true;
        }
        uint8_t seq = frame.data[0] & 0x0F;
        if (seq != link.rxNextSeq)
        {
            link.lastError = ISOTP_ERR_WRONG_SEQUENCE;
            isoTpClearRx(link);
            return true;
        }

        uint8_t copied = 0;
        while (copied < 7 && link.rxReceived < link.rxLength)
        {
            link.rxPayload[link.rxReceived++] = frame.data[1 + copied];
            copied++;
        }
        link.rxNextSeq = (link.rxNextSeq + 1) & 0x0F;
        link.lastError = ISOTP_ERR_NONE;

        if (link.rxReceived >= link.rxLength)
        {
            link.lastCompleteRxLength = link.rxLength;
            link.rxActive = false;
            link.rxComplete = true;
        }
        return true;
    }

    case ISOTP_PCI_FLOW_CONTROL:
    {
        link.flowControlRxCount++;
        if (!link.txActive)
        {
            link.lastError = ISOTP_ERR_UNEXPECTED_FC;
            return true;
        }

        uint8_t status = frame.data[0] & 0x0F;
        if (status == 0x00)
        {
            link.txWaitingFlowControl = false;
            link.txBlockSize = frame.data[1];
            link.txBlockRemaining = frame.data[1];
            link.txStMin = frame.data[2];
            link.lastError = ISOTP_ERR_NONE;
            return isoTpSendConsecutiveFrames(link, driver, flowControlTxId, frame.bus);
        }
        if (status == 0x01)
        {
            link.lastError = ISOTP_ERR_FC_WAIT;
            link.txWaitingFlowControl = true;
            return true;
        }

        link.lastError = ISOTP_ERR_FC_OVERFLOW;
        isoTpClearTx(link);
        return true;
    }
    }

    link.lastError = ISOTP_ERR_BAD_LENGTH;
    return true;
}

static void isoTpMarkRxTimeout(IsoTpLink &link)
{
    if (link.rxActive)
        link.lastError = ISOTP_ERR_RX_TIMEOUT;
    isoTpClearRx(link);
}
