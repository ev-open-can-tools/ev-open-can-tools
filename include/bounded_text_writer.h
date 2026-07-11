#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>

class BoundedTextWriter
{
public:
    BoundedTextWriter(char *buffer, size_t capacity) : buffer_(buffer), capacity_(capacity)
    {
        if (buffer_ && capacity_)
            buffer_[0] = '\0';
    }

    bool append(const char *text)
    {
        if (!text)
            text = "";
        return appendBytes(text, std::strlen(text));
    }

    bool appendf(const char *format, ...)
    {
        if (!buffer_ || capacity_ == 0 || length_ >= capacity_ - 1)
        {
            truncated_ = true;
            return false;
        }
        va_list args;
        va_start(args, format);
        int written = std::vsnprintf(buffer_ + length_, capacity_ - length_, format, args);
        va_end(args);
        if (written < 0 || static_cast<size_t>(written) >= capacity_ - length_)
        {
            buffer_[length_] = '\0';
            truncated_ = true;
            return false;
        }
        length_ += static_cast<size_t>(written);
        return true;
    }

    bool appendSafe(const char *text, size_t maxLength = 64)
    {
        if (!text)
            return append("unavailable");
        size_t count = 0;
        while (*text && count++ < maxLength)
        {
            unsigned char c = static_cast<unsigned char>(*text++);
            char safe = c >= 0x20 && c <= 0x7E ? static_cast<char>(c) : '?';
            if (!appendBytes(&safe, 1))
                return false;
        }
        return true;
    }

    const char *data() const { return buffer_ ? buffer_ : ""; }
    size_t size() const { return length_; }
    bool complete() const { return !truncated_; }

private:
    bool appendBytes(const char *data, size_t size)
    {
        if (!buffer_ || capacity_ == 0 || size >= capacity_ - length_)
        {
            if (buffer_ && capacity_)
                buffer_[length_ < capacity_ ? length_ : capacity_ - 1] = '\0';
            truncated_ = true;
            return false;
        }
        std::memcpy(buffer_ + length_, data, size);
        length_ += size;
        buffer_[length_] = '\0';
        return true;
    }

    char *buffer_ = nullptr;
    size_t capacity_ = 0;
    size_t length_ = 0;
    bool truncated_ = false;
};
