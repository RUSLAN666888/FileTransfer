#include "domain/transfer_id.h"

#include <cstdio>

namespace domain
{

namespace
{

char hex_digit(std::uint8_t v) { return v < 10 ? static_cast<char>('0' + v) : static_cast<char>('a' + v - 10); }

int hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

} // namespace

TransferId TransferId::generate(std::uint64_t unix_ms, std::span<const std::uint8_t, 10> random_bytes)
{
    std::array<std::uint8_t, kRawBytes> raw{};

    // Bytes 0..5: unix_ms big-endian (48 bits).
    raw[0] = static_cast<std::uint8_t>((unix_ms >> 40) & 0xFF);
    raw[1] = static_cast<std::uint8_t>((unix_ms >> 32) & 0xFF);
    raw[2] = static_cast<std::uint8_t>((unix_ms >> 24) & 0xFF);
    raw[3] = static_cast<std::uint8_t>((unix_ms >> 16) & 0xFF);
    raw[4] = static_cast<std::uint8_t>((unix_ms >> 8) & 0xFF);
    raw[5] = static_cast<std::uint8_t>((unix_ms) & 0xFF);

    // Byte 6: version 7 in high nibble, random in low nibble.
    raw[6] = static_cast<std::uint8_t>(0x70 | (random_bytes[0] & 0x0F));

    // Byte 7: random.
    raw[7] = random_bytes[1];

    // Byte 8: variant 0b10 in high two bits, random in low six.
    raw[8] = static_cast<std::uint8_t>(0x80 | (random_bytes[2] & 0x3F));

    // Bytes 9..15: random.
    for (std::size_t i = 9; i < kRawBytes; ++i)
    {
        raw[i] = random_bytes[3 + (i - 9)];
    }

    return TransferId::from_bytes(raw);
}

TransferId TransferId::from_bytes(const std::array<std::uint8_t, kRawBytes>& bytes)
{
    TransferId id;
    id.raw_ = bytes;
    return id;
}

infra::Result<TransferId> TransferId::from_string(std::string_view s)
{
    // Canonical form: 8-4-4-4-12 hex chars, with hyphens. 36 chars.
    if (s.size() != 36)
    {
        return infra::Error{"UUID must be 36 chars, got " + std::to_string(s.size())};
    }
    if (s[8] != '-' || s[13] != '-' || s[18] != '-' || s[23] != '-')
    {
        return infra::Error{"UUID has misplaced hyphens"};
    }

    std::array<std::uint8_t, kRawBytes> raw{};
    std::size_t out = 0;
    for (std::size_t i = 0; i < 36;)
    {
        if (s[i] == '-')
        {
            ++i;
            continue;
        }
        const int hi = hex_value(s[i]);
        const int lo = hex_value(s[i + 1]);
        if (hi < 0 || lo < 0)
        {
            return infra::Error{"invalid hex digit in UUID"};
        }
        raw[out++] = static_cast<std::uint8_t>((hi << 4) | lo);
        i += 2;
    }

    return TransferId::from_bytes(raw);
}

std::string TransferId::to_string() const
{
    std::string out;
    out.reserve(36);
    for (std::size_t i = 0; i < kRawBytes; ++i)
    {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            out.push_back('-');
        out.push_back(hex_digit(raw_[i] >> 4));
        out.push_back(hex_digit(raw_[i] & 0x0F));
    }
    return out;
}

} // namespace domain
