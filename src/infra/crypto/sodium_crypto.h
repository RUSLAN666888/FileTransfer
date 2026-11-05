#ifndef INFRA_CRYPTO_SODIUMCRYPTO_H
#define INFRA_CRYPTO_SODIUMCRYPTO_H

#include "infra/crypto/i_crypto.h"

namespace infra
{

class SodiumCrypto final : public ICrypto
{
  public:
    void sha256(std::uint8_t* out, const std::uint8_t* in, std::size_t n) override;
};

} // namespace infra

#endif
