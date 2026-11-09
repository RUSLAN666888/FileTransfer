#include "domain/peer_id.h"

#include <gtest/gtest.h>

namespace
{

using domain::PeerId;

std::array<std::uint8_t, 32> make_hash(std::uint8_t seed)
{
    std::array<std::uint8_t, 32> h{};
    for (std::size_t i = 0; i < h.size(); ++i)
        h[i] = static_cast<std::uint8_t>(seed + i);
    return h;
}

} // namespace

TEST(PeerId, FromSha256TakesFirst16Bytes)
{
    const auto hash = make_hash(0);
    const auto id = PeerId::from_sha256_truncated(hash);
    for (std::size_t i = 0; i < PeerId::kRawBytes; ++i)
    {
        EXPECT_EQ(id.raw()[i], hash[i]) << "i=" << i;
    }
}

TEST(PeerId, Base32RoundTrip)
{
    const auto id = PeerId::from_sha256_truncated(make_hash(42));
    const auto encoded = id.to_base32();

    ASSERT_EQ(encoded.size(), PeerId::kBase32Chars);

    auto decoded = PeerId::from_base32(encoded);
    ASSERT_TRUE(decoded) << decoded.error().message;
    EXPECT_EQ(decoded.value(), id);
}

TEST(PeerId, FromBase32WrongLengthFails)
{
    EXPECT_FALSE(PeerId::from_base32(""));
    EXPECT_FALSE(PeerId::from_base32("ABC"));
    EXPECT_FALSE(PeerId::from_base32(std::string(25, 'A')));
    EXPECT_FALSE(PeerId::from_base32(std::string(27, 'A')));
}

TEST(PeerId, FromBase32InvalidCharFails)
{
    // '0', '1', '8', '9', '=', lowercase are not in RFC 4648 §6 alphabet.
    EXPECT_FALSE(PeerId::from_base32(std::string(26, '0')));
    EXPECT_FALSE(PeerId::from_base32(std::string(26, '1')));
    EXPECT_FALSE(PeerId::from_base32(std::string(26, 'a')));
    EXPECT_FALSE(PeerId::from_base32(std::string(26, '=')));
}
