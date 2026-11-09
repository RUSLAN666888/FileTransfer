#include "domain/peer_id.h"

#include <algorithm>
#include <span>

namespace domain
{

namespace
{

// RFC 4648 §6 alphabet: A-Z, 2-7.
constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

// Encode a byte buffer as RFC 4648 §6 base32 without padding.
//
// Bit layout: process 5 octets (40 bits) at a time, emit 8 chars. The final
// partial block (1..4 octets) is processed bit by bit; trailing bits in the
// last char are zero (canonical). No `=` padding is emitted.
std::string encode_base32(std::span<const std::uint8_t> in)
{
    std::string out;
    out.reserve((in.size() * 8 + 4) / 5);

    std::uint32_t buffer = 0;
    int bits = 0;
    for (auto byte : in)
    {
        buffer = (buffer << 8) | byte;
        bits += 8;
        while (bits >= 5)
        {
            bits -= 5;
            out.push_back(kAlphabet[(buffer >> bits) & 0x1F]);
        }
    }
    if (bits > 0)
    {
        // Left-align the remaining bits to a 5-bit boundary.
        out.push_back(kAlphabet[(buffer << (5 - bits)) & 0x1F]);
    }
    return out;
}

int decode_char(char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= '2' && c <= '7')
        return c - '2' + 26;
    return -1;
}

} // namespace

PeerId PeerId::from_sha256_truncated(const std::array<std::uint8_t, 32>& sha256)
{
    PeerId id;
    std::copy(sha256.begin(), sha256.begin() + kRawBytes, id.raw_.begin());
    return id;
}

infra::Result<PeerId> PeerId::from_base32(std::string_view s)
{
    if (s.size() != kBase32Chars)
    {
        return infra::Error{"PeerId must be " + std::to_string(kBase32Chars) + " base32 chars, got " + std::to_string(s.size())};
    }

    std::array<std::uint8_t, kRawBytes> raw{};
    std::uint32_t buffer = 0;
    int bits = 0;
    std::size_t out_idx = 0;

    for (char c : s)
    {
        const int v = decode_char(c);
        if (v < 0)
        {
            return infra::Error{std::string{"invalid base32 char: '"} + c + "'"};
        }
        buffer = (buffer << 5) | static_cast<std::uint32_t>(v);
        bits += 5;
        if (bits >= 8)
        {
            bits -= 8;
            if (out_idx >= kRawBytes)
            {
                return infra::Error{"PeerId base32 too long"};
            }
            raw[out_idx++] = static_cast<std::uint8_t>((buffer >> bits) & 0xFF);
        }
    }

    // 26 chars * 5 bits = 130 bits; 128 are meaningful, 2 are padding.
    // Canonical encodings have zero padding bits in the final char.
    if (bits > 0 && (buffer & ((1u << bits) - 1)) != 0)
    {
        return infra::Error{"PeerId base32 has non-zero trailing bits"};
    }
    if (out_idx != kRawBytes)
    {
        return infra::Error{"PeerId base32 decoded to wrong length"};
    }

    PeerId id;
    id.raw_ = raw;
    return id;
}

std::string PeerId::to_base32() const { return encode_base32(raw_); }

} // namespace domain
