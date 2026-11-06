#ifndef INFRA_CRYPTO_SODIUMCRYPTO_H
#define INFRA_CRYPTO_SODIUMCRYPTO_H

#include "infra/crypto/i_crypto.h"
#include <cstddef>
#include <cstdint>

namespace infra
{

/**
 * @brief `ICrypto` backed by libsodium.
 * @throws std::runtime_error from the constructor if `sodium_init()` fails.
 *         All other methods are `noexcept`.
 */
class SodiumCrypto final : public ICrypto
{
  public:
    SodiumCrypto();

    void sha256(std::uint8_t* out, const std::uint8_t* in, std::size_t n) override;

    void ed25519Keypair(std::uint8_t* pub, std::uint8_t* priv) override;
    void ed25519Sign(std::uint8_t* sig, const std::uint8_t* msg, std::size_t msgLen, const std::uint8_t* priv) override;
    bool ed25519Verify(const std::uint8_t* sig, const std::uint8_t* msg, std::size_t msgLen, const std::uint8_t* pub) override;

    /// @brief XChaCha20-Poly1305, detached tag. Tag authenticates `aad ‖ plaintext`.
    /// @param ciphertext Output, `plaintextLen` bytes.
    /// @param tag        Output, `kAeadTagBytes` (16) bytes.
    /// @param aad        May be `nullptr` with `aadLen == 0`.
    void aeadEncrypt(std::uint8_t* ciphertext, std::uint8_t* tag, const std::uint8_t* plaintext, std::size_t plaintextLen, const std::uint8_t* aad, std::size_t aadLen,
                     const std::uint8_t* nonce, const std::uint8_t* key) override;

    /// @brief XChaCha20-Poly1305 detached decrypt + verify.
    /// @param plaintext Output, `ciphertextLen` bytes. Unspecified on `false` - do not read.
    /// @param aad       May be `nullptr` with `aadLen == 0`.
    /// @return `true` on success; `false` on any auth failure. Never throws.
    bool aeadDecrypt(std::uint8_t* plaintext, const std::uint8_t* ciphertext, std::size_t ciphertextLen, const std::uint8_t* tag, const std::uint8_t* aad, std::size_t aadLen,
                     const std::uint8_t* nonce, const std::uint8_t* key) override;
};

} // namespace infra

#endif
