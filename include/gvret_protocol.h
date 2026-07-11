#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "can_frame_types.h"

namespace GvretProtocol
{
static constexpr uint8_t kProtocolStart = 0xF1;
static constexpr uint8_t kBinaryMode = 0xE7;
static constexpr uint8_t kBuildFrame = 0x00;
static constexpr uint8_t kGetParams = 0x06;
static constexpr uint8_t kGetDeviceInfo = 0x07;
static constexpr uint8_t kKeepAlive = 0x09;
static constexpr uint8_t kGetBusCount = 0x0C;

static constexpr uint8_t kCanParamsResponse[19] = {
    kProtocolStart, kGetParams, 1, 0x20, 0xA1, 0x07, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0};

inline size_t encodeCanFrame(const CanFrame &frame, uint32_t timestampUs,
                             uint8_t *output, size_t capacity)
{
    const size_t outputSize = 11U + frame.dlc;
    if (!output || frame.id > 0x7FF || frame.dlc > 8 || capacity < outputSize)
        return 0;

    size_t index = 0;
    output[index++] = kProtocolStart;
    output[index++] = kBuildFrame;
    for (uint8_t shift = 0; shift < 4; shift++)
        output[index++] = static_cast<uint8_t>(timestampUs >> (shift * 8));
    for (uint8_t shift = 0; shift < 4; shift++)
        output[index++] = static_cast<uint8_t>(frame.id >> (shift * 8));
    output[index++] = static_cast<uint8_t>(frame.dlc & 0x0F); // bus 0
    memcpy(output + index, frame.data, frame.dlc);
    return outputSize;
}
} // namespace GvretProtocol
