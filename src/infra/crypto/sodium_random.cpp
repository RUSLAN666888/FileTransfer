#include "infra/crypto/sodium_random.h"

#include <sodium.h>

namespace infra
{

void SodiumRandom::fill(std::uint8_t* out, std::size_t n) { randombytes_buf(out, n); }

} // namespace infra
