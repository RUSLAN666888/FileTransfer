#ifndef DOMAIN_PEER_ID_H
#define DOMAIN_PEER_ID_H

#include "infra/result.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace domain
{

/// @brief Self-certifying peer identity.
///
/// Derived as `SHA-256(public_key)` truncated to 128 bits. On the wire it is
/// transmitted as 16 raw bytes; for SQLite, GUI, and logs it is encoded as
/// base32 (RFC 4648).
///
/// This is a value type. It knows nothing about discovery state (sequence
/// numbers, timestamps, endpoints) — that lives in `DiscoveredPeer` in the
/// discovery service.
class PeerId
{
  public:
    static constexpr std::size_t kRawBytes = 16;

    /// @brief Base32 length without padding for `kRawBytes` octets.
    ///
    /// RFC 4648 §6 encodes 5 octets (40 bits) as 8 base32 chars. The final
    /// partial block (1..4 octets) is padded on the right with zero bits to
    /// the next 5-bit boundary. For 16 octets: 3 full blocks (15 octets,
    /// 24 chars) + 1 octet (2 chars) = 26 chars. No `=` padding.
    static constexpr std::size_t kBase32Chars = (kRawBytes * 8 + 4) / 5;

    /// @brief Construct from the first 16 bytes of `SHA-256(public_key)`.
    ///        The caller is responsible for computing the hash.
    static PeerId from_sha256_truncated(const std::array<std::uint8_t, 32>& sha256);

    /// @brief Parse a base32-encoded PeerId (26 chars, uppercase, no padding).
    static infra::Result<PeerId> from_base32(std::string_view s);

    /// @brief Encode as base32 (26 chars, uppercase, no padding).
    std::string to_base32() const;

    const std::array<std::uint8_t, kRawBytes>& raw() const noexcept { return raw_; }

    bool operator==(const PeerId&) const = default;
    bool operator<(const PeerId& other) const { return raw_ < other.raw_; }

  private:
    std::array<std::uint8_t, kRawBytes> raw_{};
};

} // namespace domain

#endif
