#ifndef INFRA_CRYPTO_IRANDOMSOURCE_H
#define INFRA_CRYPTO_IRANDOMSOURCE_H

#include <cstddef>
#include <cstdint>

namespace infra
{

/**
 * @brief Source of cryptographically secure random bytes.
 *
 * @details
 * Used for keypair generation, nonces, etc. Implementations must be
 * backed by a CSPRNG.
 * Not used for non-security randomness
 */
class IRandomSource
{
  public:
    virtual ~IRandomSource() = default;

    /// @brief Fills `out` with `n` random bytes.
    virtual void fill(std::uint8_t* out, std::size_t n) = 0;
};

} // namespace infra

#endif
