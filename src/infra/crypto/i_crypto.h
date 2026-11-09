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
    static constexpr std::size_t kSha256Bytes = 32;

    static constexpr std::size_t kEd25519PublicKeyBytes = 32;
    static constexpr std::size_t kEd25519PrivateKeyBytes = 64; // seed || pk
    static constexpr std::size_t kEd25519SeedBytes = 32;
    static constexpr std::size_t kEd25519SignatureBytes = 64;

    static constexpr std::size_t kAeadKeyBytes = 32;   // crypto_aead_xchacha20poly1305_ietf_KEYBYTES
    static constexpr std::size_t kAeadNonceBytes = 24; // crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
    static constexpr std::size_t kAeadTagBytes = 16;   // crypto_aead_xchacha20poly1305_ietf_ABYTES

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

    /// @brief Encrypts `plaintext` with XChaCha20-Poly1305 (detached tag).
    /// @param ciphertext Output, must hold `plaintextLen` bytes.
    /// @param tag Output, 16 bytes.
    /// @param aad Additional authenticated data (may be nullptr if aadLen == 0).
    virtual void aeadEncrypt(std::uint8_t* ciphertext, std::uint8_t* tag, const std::uint8_t* plaintext, std::size_t plaintextLen, const std::uint8_t* aad, std::size_t aadLen,
                             const std::uint8_t* nonce, const std::uint8_t* key) = 0;

    /// @brief Decrypts and verifies a detached XChaCha20-Poly1305 ciphertext.
    /// @return true if authentication succeeds and `plaintext` is written; false otherwise.
    virtual bool aeadDecrypt(std::uint8_t* plaintext, const std::uint8_t* ciphertext, std::size_t ciphertextLen, const std::uint8_t* tag, const std::uint8_t* aad,
                             std::size_t aadLen, const std::uint8_t* nonce, const std::uint8_t* key) = 0;
};

} // namespace infra

#endif
