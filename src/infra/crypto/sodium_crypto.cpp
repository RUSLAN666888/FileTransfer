#include "infra/crypto/sodium_crypto.h"
#include <sodium.h>
#include <stdexcept>

namespace infra
{

SodiumCrypto::SodiumCrypto()
{
    if (sodium_init() < 0)
    {
        throw std::runtime_error("sodium_init() failed");
    }
}

void SodiumCrypto::sha256(std::uint8_t* out, const std::uint8_t* in, std::size_t n) { crypto_hash_sha256(out, in, n); }

void SodiumCrypto::ed25519Keypair(std::uint8_t* pub, std::uint8_t* priv) { crypto_sign_keypair(pub, priv); }

void SodiumCrypto::ed25519Sign(std::uint8_t* sig, const std::uint8_t* msg, std::size_t msgLen, const std::uint8_t* priv) { crypto_sign_detached(sig, NULL, msg, msgLen, priv); }

bool SodiumCrypto::ed25519Verify(const std::uint8_t* sig, const std::uint8_t* msg, std::size_t msgLen, const std::uint8_t* pub)
{
    return crypto_sign_verify_detached(sig, msg, msgLen, pub) == 0;
}

void SodiumCrypto::aeadEncrypt(std::uint8_t* ciphertext, std::uint8_t* tag, const std::uint8_t* plaintext, std::size_t plaintextLen, const std::uint8_t* aad, std::size_t aadLen,
                               const std::uint8_t* nonce, const std::uint8_t* key)
{

    if (crypto_aead_xchacha20poly1305_ietf_encrypt_detached(ciphertext, tag, /*maclen_p=*/nullptr, plaintext, plaintextLen, aad, aadLen, /*nsec=*/nullptr, nonce, key) != 0)
    {
        // Only reachable if libsodium rejects the arguments (e.g. null
        // pointers with non-zero lengths). That is a caller bug, not a
        // runtime condition — surface it loudly rather than silently
        // producing a frame that fails to authenticate on the peer.
        throw std::runtime_error("AEAD detached encryption failed");
    }
}

bool SodiumCrypto::aeadDecrypt(std::uint8_t* plaintext, const std::uint8_t* ciphertext, std::size_t ciphertextLen, const std::uint8_t* tag, const std::uint8_t* aad,
                               std::size_t aadLen, const std::uint8_t* nonce, const std::uint8_t* key)
{
    // crypto_aead_*_decrypt_detached returns 0 on success, -1 on any
    // authentication failure (tampered ciphertext, wrong tag, wrong key,
    // wrong nonce, wrong AAD). Map both non-zero results to `false`.
    return crypto_aead_xchacha20poly1305_ietf_decrypt_detached(plaintext, /*nsec=*/nullptr, ciphertext, ciphertextLen, tag, aad, aadLen, nonce, key) == 0;
}

} // namespace infra
