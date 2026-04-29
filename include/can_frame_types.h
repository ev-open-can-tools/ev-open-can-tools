#pragma once

#include <cstdint>
#include <cstring>

struct CanFrame
{
    uint32_t id = 0;
    uint8_t dlc = 8;
    uint8_t data[8] = {};
};
