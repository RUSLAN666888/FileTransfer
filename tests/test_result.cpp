#include "infra/result.h"
#include <gtest/gtest.h>

TEST(Result, OkHasValue)
{
    infra::Result<int> r = 42;
    EXPECT_TRUE(r);
    EXPECT_EQ(r.value(), 42);
}

TEST(Result, ErrorHasNoValue)
{
    infra::Result<int> r = infra::Error{"boom"};
    EXPECT_FALSE(r);
    EXPECT_EQ(r.error().message, "boom");
}

TEST(Result, ValueOnRvalueMoves)
{
    infra::Result<std::vector<int>> r = std::vector<int>{1, 2, 3};
    auto v = std::move(r).value();
    EXPECT_EQ(v, (std::vector<int>{1, 2, 3}));
}

TEST(ResultVoid, DefaultIsOk)
{
    infra::Result<void> r;
    EXPECT_TRUE(r);
}
