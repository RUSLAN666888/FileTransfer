#include "infra/keystore/file_keystore.h"

#include "infra/crypto/i_crypto.h"
#include "infra/crypto/sodium_crypto.h"
#include "infra/file_system/file_system.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace
{

using infra::FileKeystore;
using infra::FileSystem;
using infra::ICrypto;
using infra::Keypair;
using infra::SodiumCrypto;

class FileKeystoreTest : public ::testing::Test
{
  protected:
    FileSystem fs;
    SodiumCrypto crypto;
    fs::path dir;

    void SetUp() override
    {
        dir = fs::temp_directory_path() / ("ft_keystore_" + std::to_string(std::rand()));
        fs::create_directories(dir);
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    std::string path() const { return (dir / "key").string(); }

    FileKeystore make_keystore() { return FileKeystore(fs, crypto, path()); }
};

} // namespace

TEST_F(FileKeystoreTest, LoadOrCreateGeneratesAndPersists)
{
    auto ks = make_keystore();

    auto kp = ks.load_or_create();
    ASSERT_TRUE(kp) << kp.error().message;

    EXPECT_TRUE(fs.exists(path()));

    // File size must be exactly public_key (32) + private_key (64) = 96.
    auto bytes = fs.read_file(path());
    ASSERT_TRUE(bytes);
    EXPECT_EQ(bytes.value().size(), ICrypto::kEd25519PublicKeyBytes + ICrypto::kEd25519PrivateKeyBytes);
}

TEST_F(FileKeystoreTest, LoadOrCreateIsIdempotent)
{
    auto ks = make_keystore();

    auto kp1 = ks.load_or_create();
    ASSERT_TRUE(kp1);

    auto kp2 = ks.load_or_create();
    ASSERT_TRUE(kp2);

    EXPECT_EQ(kp1.value(), kp2.value());
}

TEST_F(FileKeystoreTest, ReloadFromNewInstanceReturnsSameKeypair)
{
    Keypair first;
    {
        auto ks = make_keystore();
        auto kp = ks.load_or_create();
        ASSERT_TRUE(kp);
        first = kp.value();
    }

    {
        auto ks = make_keystore();
        auto kp = ks.load_or_create();
        ASSERT_TRUE(kp);
        EXPECT_EQ(kp.value(), first);
    }
}

TEST_F(FileKeystoreTest, StoreReplacesExistingKeypair)
{
    auto ks = make_keystore();

    auto kp1 = ks.load_or_create();
    ASSERT_TRUE(kp1);

    Keypair kp2{};
    crypto.ed25519Keypair(kp2.public_key.data(), kp2.private_key.data());

    ASSERT_TRUE(ks.store(kp2)) << "store failed";

    auto kp3 = ks.load_or_create();
    ASSERT_TRUE(kp3);
    EXPECT_EQ(kp3.value(), kp2);
    EXPECT_NE(kp3.value(), kp1.value());
}

TEST_F(FileKeystoreTest, StoreOnFreshPathCreatesFile)
{
    auto ks = make_keystore();

    Keypair kp{};
    crypto.ed25519Keypair(kp.public_key.data(), kp.private_key.data());

    ASSERT_TRUE(ks.store(kp));

    auto loaded = ks.load_or_create();
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded.value(), kp);
}

TEST_F(FileKeystoreTest, CorruptedFileWithWrongSizeReturnsError)
{
    // Write 10 bytes instead of 96.
    ASSERT_TRUE(fs.write_file_atomic(path(), std::vector<std::uint8_t>(10, 0)));

    auto ks = make_keystore();
    auto kp = ks.load_or_create();
    EXPECT_FALSE(kp);
    EXPECT_NE(kp.error().message.find("wrong size"), std::string::npos);
}

TEST_F(FileKeystoreTest, EmptyFileReturnsError)
{
    ASSERT_TRUE(fs.write_file_atomic(path(), {}));

    auto ks = make_keystore();
    auto kp = ks.load_or_create();
    EXPECT_FALSE(kp);
    EXPECT_NE(kp.error().message.find("wrong size"), std::string::npos);
}

TEST_F(FileKeystoreTest, SerializedFormatIsPublicThenPrivate)
{
    auto ks = make_keystore();

    auto kp = ks.load_or_create();
    ASSERT_TRUE(kp);

    auto bytes = fs.read_file(path());
    ASSERT_TRUE(bytes);
    ASSERT_EQ(bytes.value().size(), 96u);

    // First 32 bytes are public_key.
    for (std::size_t i = 0; i < ICrypto::kEd25519PublicKeyBytes; ++i)
    {
        EXPECT_EQ(bytes.value()[i], kp.value().public_key[i]) << "i=" << i;
    }
    // Next 64 bytes are private_key.
    for (std::size_t i = 0; i < ICrypto::kEd25519PrivateKeyBytes; ++i)
    {
        EXPECT_EQ(bytes.value()[ICrypto::kEd25519PublicKeyBytes + i], kp.value().private_key[i]) << "i=" << i;
    }
}
