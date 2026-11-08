#pragma once
#include <array>
#include <cstdint>
#include <cstring>

namespace PhotoCrap::Storage
{
struct BinaryHeader {
    static constexpr char          Magic[9] = "PCRBIN\r\n";
    static constexpr std::uint64_t Size = 64, MaxPreview = 512 * 1024;
    std::uint64_t                  preview = 0, index = 0, packed = 0, unpacked = 0, length = 0;
    static std::uint64_t           number(const char *p)
    {
        std::uint64_t value = 0;
        for (int i = 7; i >= 0; --i)
            value = (value << 8) | static_cast<unsigned char>(p[i]);
        return value;
    }
    static void number(char *p, std::uint64_t value)
    {
        for (int i = 0; i < 8; ++i) {
            p[i] = char(value);
            value >>= 8;
        }
    }
    std::array<char, Size> bytes() const
    {
        std::array<char, Size> result{};
        std::memcpy(result.data(), Magic, 8);
        number(result.data() + 8, 14);
        number(result.data() + 16, preview);
        number(result.data() + 24, index);
        number(result.data() + 32, packed);
        number(result.data() + 40, unpacked);
        number(result.data() + 48, length);
        return result;
    }
    bool read(const char *p)
    {
        if (std::memcmp(p, Magic, 8) || number(p + 8) != 14 || number(p + 56))
            return false;
        preview  = number(p + 16);
        index    = number(p + 24);
        packed   = number(p + 32);
        unpacked = number(p + 40);
        length   = number(p + 48);
        return preview <= MaxPreview && index >= Size + preview && packed > 0 &&
               packed <= 32 * 1024 * 1024 && unpacked > 0 && unpacked <= 32 * 1024 * 1024 &&
               index <= length && packed == length - index && length <= 8ULL * 1024 * 1024 * 1024;
    }
};
} // namespace PhotoCrap::Storage
