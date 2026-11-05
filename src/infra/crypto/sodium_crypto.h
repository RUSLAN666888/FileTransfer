#ifndef INFRA_CRYPTO_SODIUMCRYPTO_H
#define INFRA_CRYPTO_SODIUMCRYPTO_H

#include "infra/crypto/i_crypto.h"
#include <cstddef>
#include <cstdint>

namespace infra
{

/**
 * @brief `ICrypto` backed by libsodium.
 */
class SodiumCrypto final : public ICrypto
{
  public:
    void sha256(std::uint8_t* out, const std::uint8_t* in, std::size_t n) override;

    void ed25519Keypair(std::uint8_t* pub, std::uint8_t* priv) override;
    void ed25519Sign(std::uint8_t* sig, const std::uint8_t* msg, std::size_t msgLen, const std::uint8_t* priv) override;
    bool ed25519Verify(const std::uint8_t* sig, const std::uint8_t* msg, std::size_t msgLen, const std::uint8_t* pub) override;
};

} // namespace infra

#endif
