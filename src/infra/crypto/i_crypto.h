#ifndef INFRA_CRYPTO_I_CRYPTO_H
#define INFRA_CRYPTO_I_CRYPTO_H

#include <cstddef>
#include <cstdint>

namespace infra
{

/**
 * @brief Cryptographic primitives used by the application.
 *
 * @details
 * Thin wrapper over a crypto library (libsodium in `SodiumCrypto`).
 * No state, no policy — only primitives. Higher-level protocols
 * (Noise handshake) are built on top of these.
 */
class ICrypto
{
  public:
    static constexpr std::size_t kEd25519PublicKeyBytes = 32;
    static constexpr std::size_t kEd25519PrivateKeyBytes = 64; // seed || pk
    static constexpr std::size_t kEd25519SeedBytes = 32;
    static constexpr std::size_t kEd25519SignatureBytes = 64;

    virtual ~ICrypto() = default;

    /// @brief Computes SHA-256 over `in[0..n)`.
    /// @param out 32-byte output buffer.
    virtual void sha256(std::uint8_t* out, const std::uint8_t* in, std::size_t n) = 0;

    /// @brief Generates a fresh Ed25519 keypair.
    /// @param pub 32-byte output for the public key.
    /// @param priv 64-byte output for the secret key (seed ‖ pk).
    virtual void ed25519Keypair(std::uint8_t* pub, std::uint8_t* priv) = 0;

    /// @brief Signs `msg[0..msgLen)` with `priv`, writes 64 bytes to `sig`.
    virtual void ed25519Sign(std::uint8_t* sig, const std::uint8_t* msg, std::size_t msgLen, const std::uint8_t* priv) = 0;

    /// @brief Verifies `sig` over `msg[0..msgLen)` against `pub`.
    /// @return true if the signature is valid, false otherwise.
    virtual bool ed25519Verify(const std::uint8_t* sig, const std::uint8_t* msg, std::size_t msgLen, const std::uint8_t* pub) = 0;
};

} // namespace infra

#endif
