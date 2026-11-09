#include "infra/crypto/fake_random.h"
#include "infra/crypto/sodium_crypto.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

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

namespace
{

using infra::FakeRandom;
using infra::ICrypto;
using infra::SodiumCrypto;

using Key = std::array<std::uint8_t, ICrypto::kAeadKeyBytes>;
using Nonce = std::array<std::uint8_t, ICrypto::kAeadNonceBytes>;
using Tag = std::array<std::uint8_t, ICrypto::kAeadTagBytes>;

Key make_key(std::uint8_t seed)
{
    Key k{};
    for (std::size_t i = 0; i < k.size(); ++i)
        k[i] = static_cast<std::uint8_t>(seed + i);
    return k;
}

std::vector<std::uint8_t> make_bytes(std::size_t size)
{
    std::vector<std::uint8_t> v(size);
    for (std::size_t i = 0; i < size; ++i)
        v[i] = static_cast<std::uint8_t>(i * 31 + 7);
    return v;
}

class AeadTest : public ::testing::Test
{
  protected:
    SodiumCrypto crypto;
    FakeRandom rng{0xAB}; // fixed seed -> reproducible failures

    Nonce fresh_nonce()
    {
        Nonce n{};
        rng.fill(n.data(), n.size());
        return n;
    }

    std::pair<std::vector<std::uint8_t>, Tag> seal(const std::vector<std::uint8_t>& pt, const Key& key, const Nonce& nonce)
    {
        std::vector<std::uint8_t> ct(pt.size());
        Tag tag{};
        crypto.aeadEncrypt(ct.data(), tag.data(), pt.data(), pt.size(), nullptr, 0, nonce.data(), key.data());
        return {std::move(ct), tag};
    }

    bool open(const std::vector<std::uint8_t>& ct, const Tag& tag, std::vector<std::uint8_t>& out, const Key& key, const Nonce& nonce)
    {
        out.assign(ct.size(), 0);
        return crypto.aeadDecrypt(out.data(), ct.data(), ct.size(), tag.data(), nullptr, 0, nonce.data(), key.data());
    }
};

} // namespace

TEST_F(AeadTest, RoundTrip)
{
    const auto key = make_key(1);
    const auto nonce = fresh_nonce();

    for (const std::size_t size : {0u, 1u, 16u, 1024u, 1u << 20})
    {
        const auto pt = make_bytes(size);
        const auto [ct, tag] = seal(pt, key, nonce);
        ASSERT_EQ(ct.size(), pt.size());

        std::vector<std::uint8_t> out;
        ASSERT_TRUE(open(ct, tag, out, key, nonce)) << "size=" << size;
        EXPECT_EQ(out, pt) << "size=" << size;
    }
}

TEST_F(AeadTest, Tamper)
{
    const auto key = make_key(3);
    const auto nonce = fresh_nonce();
    const auto pt = make_bytes(32);
    const auto [ct, tag] = seal(pt, key, nonce);

    std::vector<std::uint8_t> out;

    for (std::size_t i = 0; i < ct.size(); ++i)
    {
        auto t = ct;
        t[i] ^= 1;
        EXPECT_FALSE(open(t, tag, out, key, nonce)) << "ct byte " << i;
    }
    for (std::size_t i = 0; i < tag.size(); ++i)
    {
        auto t = tag;
        t[i] ^= 1;
        EXPECT_FALSE(open(ct, t, out, key, nonce)) << "tag byte " << i;
    }
}

TEST_F(AeadTest, WrongKey)
{
    const auto key = make_key(5);
    const auto wrong_key = make_key(6);
    const auto nonce = fresh_nonce();
    const auto pt = make_bytes(64);
    const auto [ct, tag] = seal(pt, key, nonce);

    std::vector<std::uint8_t> out;
    EXPECT_FALSE(open(ct, tag, out, wrong_key, nonce));
}
