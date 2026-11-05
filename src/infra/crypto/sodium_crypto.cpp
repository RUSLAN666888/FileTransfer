#include "infra/crypto/sodium_crypto.h"
#include <sodium.h>

namespace infra
{

void SodiumCrypto::sha256(std::uint8_t* out, const std::uint8_t* in, std::size_t n) { crypto_hash_sha256(out, in, n); }

} // namespace infra
