#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include "shared_types.h"

struct LogRingBuffer
{
    static constexpr size_t kCapacity = 32;
    static constexpr size_t kMaxMsgLen = 128;

    struct Entry
    {
        char msg[kMaxMsgLen];
        uint32_t timestamp_ms;
    };

    Entry entries[kCapacity] = {};
    Shared<uint32_t> head{0};

    void push(const char *msg, uint32_t ts)
    {
        uint32_t h = head;
        Entry &e = entries[h % kCapacity];
        strncpy(e.msg, msg, kMaxMsgLen - 1);
        e.msg[kMaxMsgLen - 1] = '\0';
        e.timestamp_ms = ts;
        head = h + 1;
    }

    uint32_t currentHead() const { return head; }

    const Entry &at(uint32_t index) const
    {
        return entries[index % kCapacity];
    }

    int readSince(uint32_t since, Entry *out, int maxOut) const
    {
        uint32_t h = head;
        uint32_t oldest = (h > kCapacity) ? (h - kCapacity) : 0;
        uint32_t start = (since > oldest) ? since : oldest;

        int count = 0;
        for (uint32_t i = start; i < h && count < maxOut; i++)
        {
            out[count++] = entries[i % kCapacity];
        }
        return count;
    }
};
