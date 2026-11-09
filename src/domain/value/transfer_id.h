#ifndef DOMAIN_TRANSFER_ID_H
#define DOMAIN_TRANSFER_ID_H

#include "infra/result.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace domain
{

/// @brief Transfer identity (UUIDv7, RFC 9562).
///
/// UUIDv7 layout:
///   - bits  0..47: unix timestamp in milliseconds (big-endian).
///   - bits 48..51: version (0b0111 = 7).
///   - bits 52..63: random (12 bits).
///   - bits 64..65: variant (0b10).
///   - bits 66..127: random (62 bits).
///
/// The timestamp prefix makes UUIDv7 roughly sortable by creation time, which
/// matters for the transfer history table.
class TransferId
{
  public:
    static constexpr std::size_t kRawBytes = 16;

    /// @brief Generate a new UUIDv7 using the current time and the RNG.
    /// @param unix_ms  Current unix time in milliseconds.
    /// @param rng      Filled with 10 random bytes (80 random bits total).
    static TransferId generate(std::uint64_t unix_ms, std::span<const std::uint8_t, 10> random_bytes);

    /// @brief Construct from 16 raw bytes without validation.
    ///        Intended for decoding from the wire / database; the caller is
    ///        responsible for having validated the bytes.
    static TransferId from_bytes(const std::array<std::uint8_t, kRawBytes>& bytes);

    /// @brief Parse the canonical UUID string form `xxxxxxxx-xxxx-...`.
    static infra::Result<TransferId> from_string(std::string_view s);

    /// @brief Canonical UUID string form (36 chars, lowercase).
    std::string to_string() const;

    const std::array<std::uint8_t, kRawBytes>& raw() const noexcept { return raw_; }

    bool operator==(const TransferId&) const = default;
    bool operator<(const TransferId& other) const { return raw_ < other.raw_; }

  private:
    std::array<std::uint8_t, kRawBytes> raw_{};
};

} // namespace domain

#endif
