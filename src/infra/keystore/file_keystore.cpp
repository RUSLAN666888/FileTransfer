#include "infra/keystore/file_keystore.h"

#include "infra/crypto/i_crypto.h"
#include "infra/file_system/i_file_system.h"

namespace infra
{

namespace
{

constexpr std::size_t kFileSize = ICrypto::kEd25519PublicKeyBytes + ICrypto::kEd25519PrivateKeyBytes;

std::vector<std::uint8_t> serialize(const Keypair& kp)
{
    std::vector<std::uint8_t> out;
    out.reserve(kFileSize);
    out.insert(out.end(), kp.public_key.begin(), kp.public_key.end());
    out.insert(out.end(), kp.private_key.begin(), kp.private_key.end());
    return out;
}

Result<Keypair> deserialize(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() != kFileSize)
    {
        return Error{"key file has wrong size: " + std::to_string(bytes.size())};
    }
    Keypair kp{};
    std::copy(bytes.begin(), bytes.begin() + 32, kp.public_key.begin());
    std::copy(bytes.begin() + 32, bytes.end(), kp.private_key.begin());
    return kp;
}

} // namespace

FileKeystore::FileKeystore(IFileSystem& fs, ICrypto& crypto, std::string path) : fs_(fs), crypto_(crypto), path_(std::move(path)) {}

Result<Keypair> FileKeystore::load_or_create()
{
    if (!fs_.exists(path_))
    {
        Keypair kp{};
        crypto_.ed25519Keypair(kp.public_key.data(), kp.private_key.data());
        auto r = store(kp);
        if (!r)
            return r.error();
        return kp;
    }
    auto bytes = fs_.read_file(path_);
    if (!bytes)
        return bytes.error();
    return deserialize(bytes.value());
}

Result<void> FileKeystore::store(const Keypair& kp) { return fs_.write_file_atomic(path_, serialize(kp)); }

} // namespace infra
