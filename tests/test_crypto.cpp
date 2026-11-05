#include "infra/crypto/sodium_crypto.h"
#include <array>
#include <cstring>
#include <gtest/gtest.h>

TEST(SodiumCrypto, Sha256KnownVector)
{
    // NIST FIPS 180-2: SHA-256("abc") = ba7816bf...15ad
    std::array<std::uint8_t, 3> input{'a', 'b', 'c'};
    std::array<std::uint8_t, 32> out{};

    infra::SodiumCrypto c;
    c.sha256(out.data(), input.data(), input.size());

    std::array<std::uint8_t, 32> expected{0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
                                          0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
    EXPECT_EQ(expected, out);
}
