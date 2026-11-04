#ifndef INFRA_CRYPTO_SODIUMRANDOM_H
#define INFRA_CRYPTO_SODIUMRANDOM_H

#include "infra/crypto/i_random_source.h"

namespace infra
{

/**
 * @brief `IRandomSource` backed by libsodium's `randombytes_buf`.
 *
 * @details
 * Production implementation. Uses the OS CSPRNG via libsodium; never
 * deterministic.
 */
class SodiumRandom final : public IRandomSource
{
  public:
    void fill(std::uint8_t* out, std::size_t n) override;
};

} // namespace infra

#endif
