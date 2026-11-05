#include "infra/crypto/sodium_crypto.h"
#include <sodium.h>

namespace infra
{

void SodiumCrypto::sha256(std::uint8_t* out, const std::uint8_t* in, std::size_t n) { crypto_hash_sha256(out, in, n); }

void SodiumCrypto::ed25519Keypair(std::uint8_t* pub, std::uint8_t* priv) { crypto_sign_keypair(pub, priv); }

void SodiumCrypto::ed25519Sign(std::uint8_t* sig, const std::uint8_t* msg, std::size_t msgLen, const std::uint8_t* priv) { crypto_sign_detached(sig, NULL, msg, msgLen, priv); }

bool SodiumCrypto::ed25519Verify(const std::uint8_t* sig, const std::uint8_t* msg, std::size_t msgLen, const std::uint8_t* pub)
{
    return crypto_sign_verify_detached(sig, msg, msgLen, pub) == 0;
}

} // namespace infra
