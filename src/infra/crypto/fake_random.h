#ifndef INFRA_CRYPTO_FAKERANDOM_H
#define INFRA_CRYPTO_FAKERANDOM_H

#include "i_random_source.h"
#include <cstdint>

namespace infra
{

/// @brief Deterministic `IRandomSource` for tests. NOT for production.
class FakeRandom final : public IRandomSource
{
  public:
    explicit FakeRandom(std::uint8_t seed = 0) : state_(seed) {}

    void fill(std::uint8_t* out, std::size_t n) override
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            state_ = state_ * 6364136223846793005ULL + 1442695040888963407ULL;
            out[i] = static_cast<std::uint8_t>(state_ >> 56);
        }
    }

  private:
    std::uint64_t state_;
};

} // namespace infra

#endif
