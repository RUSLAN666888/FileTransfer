#ifndef INFRA_CRYPTO_KEYPAIR_H
#define INFRA_CRYPTO_KEYPAIR_H

#include <array>
#include <cstdint>

#include "infra/crypto/i_crypto.h" // for ICrypto::kEd25519*Bytes

#include <array>
#include <cstdint>

namespace infra
{

/// @brief Ed25519 keypair.
///
/// `private_key` follows the libsodium convention: the first 32 bytes are
/// the seed, the last 32 bytes are the public key. This is the format
/// `crypto_sign_ed25519_*` expects.
struct Keypair
{
    std::array<std::uint8_t, ICrypto::kEd25519PublicKeyBytes> public_key{};
    std::array<std::uint8_t, ICrypto::kEd25519PrivateKeyBytes> private_key{};

    bool operator==(const Keypair&) const = default;
};

} // namespace infra

#endif
