#ifndef INFRA_CRYPTO_I_CRYPTO_H
#define INFRA_CRYPTO_I_CRYPTO_H

#include <cstdint>

namespace infra
{

class ICrypto
{
  public:
    virtual ~ICrypto() = default;

    /// @brief Computes SHA-256 over `in[0..n)`.
    /// @param out 32-byte output buffer.
    virtual void sha256(std::uint8_t* out, const std::uint8_t* in, std::size_t n) = 0;
};

} // namespace infra

#endif
