#include "infra/crypto/sodium_crypto.h"
#include <array>
#include <cstdint>
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

TEST(SodiumCrypto, Ed25519RoundTrip)
{
    infra::SodiumCrypto c;
    std::array<std::uint8_t, 32> pub{};
    std::array<std::uint8_t, 64> priv{};
    c.ed25519Keypair(pub.data(), priv.data());

    const std::array<std::uint8_t, 5> msg = {'h', 'e', 'l', 'l', 'o'};
    std::array<std::uint8_t, 64> sig{};
    c.ed25519Sign(sig.data(), msg.data(), msg.size(), priv.data());

    EXPECT_TRUE(c.ed25519Verify(sig.data(), msg.data(), msg.size(), pub.data()));

    // Tamper: flip a bit in the signature
    sig[0] ^= 0x01;
    EXPECT_FALSE(c.ed25519Verify(sig.data(), msg.data(), msg.size(), pub.data()));

    // Tamper: flip a bit in the message
    sig[0] ^= 0x01; // restore
    auto bad = msg;
    bad[0] ^= 0x01;
    EXPECT_FALSE(c.ed25519Verify(sig.data(), bad.data(), bad.size(), pub.data()));
}

TEST(SodiumCrypto, Ed25519WrongKeyRejected)
{
    infra::SodiumCrypto c;
    std::array<std::uint8_t, 32> pub1{}, pub2{};
    std::array<std::uint8_t, 64> priv1{}, priv2{};
    c.ed25519Keypair(pub1.data(), priv1.data());
    c.ed25519Keypair(pub2.data(), priv2.data());

    const std::array<std::uint8_t, 3> msg = {'a', 'b', 'c'};
    std::array<std::uint8_t, 64> sig{};
    c.ed25519Sign(sig.data(), msg.data(), msg.size(), priv1.data());

    EXPECT_TRUE(c.ed25519Verify(sig.data(), msg.data(), msg.size(), pub1.data()));
    EXPECT_FALSE(c.ed25519Verify(sig.data(), msg.data(), msg.size(), pub2.data()));
}
