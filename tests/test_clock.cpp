#include "infra/time/fake_clock.h"
#include <gtest/gtest.h>

/// @brief `advance()` must move both clocks by the same duration.
TEST(FakeClock, AdvanceMovesBoth)
{
    infra::FakeClock c;
    auto s0 = c.steadyNow();
    auto w0 = c.now();
    c.advance(std::chrono::seconds(5));
    EXPECT_EQ(c.steadyNow() - s0, std::chrono::seconds(5));
    EXPECT_EQ(c.now() - w0, std::chrono::seconds(5));
}

/// @brief `setSystem()` models an NTP jump and must not touch steady time.
TEST(FakeClock, SetSystemDoesNotAffectSteady)
{
    infra::FakeClock c;
    auto s0 = c.steadyNow();
    c.setSystem(std::chrono::system_clock::time_point{std::chrono::hours(1000)});
    EXPECT_EQ(c.steadyNow(), s0);
}
