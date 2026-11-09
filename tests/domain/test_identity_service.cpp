#include "domain/identity_service.h"

#include "infra/crypto/sodium_crypto.h"
#include "infra/file_system/file_system.h"
#include "infra/keystore/file_keystore.h"

#include <gtest/gtest.h>

#include <filesystem>

namespace fs = std::filesystem;

namespace
{

using domain::IdentityService;
using domain::PeerId;
using infra::FileKeystore;
using infra::FileSystem;
using infra::SodiumCrypto;

class IdentityServiceTest : public ::testing::Test
{
  protected:
    FileSystem fs;
    SodiumCrypto crypto;
    fs::path dir;

    void SetUp() override
    {
        dir = fs::temp_directory_path() / ("ft_identity_" + std::to_string(std::rand()));
        fs::create_directories(dir);
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    std::string key_path() const { return (dir / "key").string(); }
};

} // namespace

TEST_F(IdentityServiceTest, LoadOrCreateIsStableAcrossInstances)
{
    PeerId first;
    {
        FileKeystore ks(fs, crypto, key_path());
        IdentityService svc(ks, crypto);

        auto id = svc.load_or_create();
        ASSERT_TRUE(id) << id.error().message;
        first = id.value();
    }

    // New keystore + service instance: reads the same keypair from disk,
    // derives the same PeerId.
    {
        FileKeystore ks(fs, crypto, key_path());
        IdentityService svc(ks, crypto);

        auto id = svc.load_or_create();
        ASSERT_TRUE(id) << id.error().message;
        EXPECT_EQ(id.value(), first);
    }
}

TEST_F(IdentityServiceTest, LoadOrCreateIsIdempotent)
{
    FileKeystore ks(fs, crypto, key_path());
    IdentityService svc(ks, crypto);

    auto a = svc.load_or_create();
    auto b = svc.load_or_create();
    ASSERT_TRUE(a);
    ASSERT_TRUE(b);
    EXPECT_EQ(a.value(), b.value());
}

TEST_F(IdentityServiceTest, PeerIdMatchesSha256OfPublicKey)
{
    FileKeystore ks(fs, crypto, key_path());
    IdentityService svc(ks, crypto);

    auto id = svc.load_or_create();
    ASSERT_TRUE(id);

    // Recompute the expected PeerId independently.
    auto kp = ks.load_or_create();
    ASSERT_TRUE(kp);

    std::array<std::uint8_t, 32> hash{};
    crypto.sha256(hash.data(), kp.value().public_key.data(), kp.value().public_key.size());

    const auto expected = PeerId::from_sha256_truncated(hash);
    EXPECT_EQ(id.value(), expected);
}
