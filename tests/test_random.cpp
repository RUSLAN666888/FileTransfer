#include "infra/crypto/fake_random.h"
#include <array>
#include <cstdint>
#include <gtest/gtest.h>

TEST(FakeRandom, Deterministic)
{
    infra::FakeRandom a(42), b(42);
    std::array<std::uint8_t, 32> x{}, y{};
    a.fill(x.data(), x.size());
    b.fill(y.data(), y.size());
    EXPECT_EQ(x, y);
}

TEST(FakeRandom, DifferentSeedsDifferentOutput)
{
    infra::FakeRandom a(1), b(2);
    std::array<std::uint8_t, 32> x{}, y{};
    a.fill(x.data(), x.size());
    b.fill(y.data(), y.size());
    EXPECT_NE(x, y);
}

TEST(FakeRandom, FillsRequestedLength)
{
    infra::FakeRandom r(7);
    std::array<std::uint8_t, 16> buf{};
    r.fill(buf.data(), buf.size());
    // just check if not all values are zerp (in LCG with zerp seed it's impossible)
    bool all_zero = true;
    for (auto b : buf)
        if (b != 0)
        {
            all_zero = false;
            break;
        }
    EXPECT_FALSE(all_zero);
}
