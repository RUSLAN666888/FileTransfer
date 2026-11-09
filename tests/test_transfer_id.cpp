#include "domain/transfer_id.h"

#include <gtest/gtest.h>

namespace
{

using domain::TransferId;

std::array<std::uint8_t, 10> random_bytes(std::uint8_t seed)
{
    std::array<std::uint8_t, 10> r{};
    for (std::size_t i = 0; i < r.size(); ++i)
        r[i] = static_cast<std::uint8_t>(seed + i);
    return r;
}

} // namespace

TEST(TransferId, GenerateSetsVersion7)
{
    const auto rb = random_bytes(0x11);
    const auto id = TransferId::generate(0x0000'1234'5678'9ABCull, rb);

    // Version 7 in byte 6 high nibble.
    EXPECT_EQ(id.raw()[6] & 0xF0, 0x70);
}

TEST(TransferId, GenerateSetsVariant)
{
    const auto rb = random_bytes(0x22);
    const auto id = TransferId::generate(0, rb);

    // Variant 0b10 in byte 8 high two bits.
    EXPECT_EQ(id.raw()[8] & 0xC0, 0x80);
}

TEST(TransferId, GenerateEncodesTimestampBigEndian)
{
    const std::uint64_t ts = 0x0000'1234'5678'9ABCull;
    const auto rb = random_bytes(0);
    const auto id = TransferId::generate(ts, rb);

    std::uint64_t decoded = 0;
    for (int i = 0; i < 6; ++i)
    {
        decoded = (decoded << 8) | id.raw()[i];
    }
    EXPECT_EQ(decoded, ts & 0x0000'FFFF'FFFF'FFFFull);
}

TEST(TransferId, GeneratePreservesRandomBits)
{
    const auto rb = random_bytes(0);
    const auto id = TransferId::generate(0, rb);

    // Byte 6 low nibble = rb[0] & 0x0F.
    EXPECT_EQ(id.raw()[6] & 0x0F, rb[0] & 0x0F);
    // Byte 7 = rb[1].
    EXPECT_EQ(id.raw()[7], rb[1]);
    // Byte 8 low 6 bits = rb[2] & 0x3F.
    EXPECT_EQ(id.raw()[8] & 0x3F, rb[2] & 0x3F);
    // Bytes 9..15 = rb[3..9].
    for (std::size_t i = 9; i < 16; ++i)
    {
        EXPECT_EQ(id.raw()[i], rb[3 + (i - 9)]) << "i=" << i;
    }
}

TEST(TransferId, StringRoundTrip)
{
    const auto rb = random_bytes(0x33);
    const auto id = TransferId::generate(0x0000'1234'5678'9ABCull, rb);

    const auto s = id.to_string();
    ASSERT_EQ(s.size(), 36u);
    EXPECT_EQ(s[8], '-');
    EXPECT_EQ(s[13], '-');
    EXPECT_EQ(s[18], '-');
    EXPECT_EQ(s[23], '-');

    auto parsed = TransferId::from_string(s);
    ASSERT_TRUE(parsed) << parsed.error().message;
    EXPECT_EQ(parsed.value(), id);
}

TEST(TransferId, FromStringWrongLengthFails)
{
    EXPECT_FALSE(TransferId::from_string(""));
    EXPECT_FALSE(TransferId::from_string("00000000-0000-0000-0000-00000000000"));
    EXPECT_FALSE(TransferId::from_string("00000000-0000-0000-0000-0000000000000"));
}

TEST(TransferId, FromStringInvalidCharFails) { EXPECT_FALSE(TransferId::from_string("zzzzzzzz-zzzz-zzzz-zzzz-zzzzzzzzzzzz")); }

TEST(TransferId, FromStringMisplacedHyphenFails) { EXPECT_FALSE(TransferId::from_string("000000000-000-0000-0000-000000000000")); }

TEST(TransferId, EqualityAndOrdering)
{
    const auto a = TransferId::generate(1000, random_bytes(0));
    const auto b = TransferId::generate(1000, random_bytes(0));
    const auto c = TransferId::generate(2000, random_bytes(0));

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);

    std::map<TransferId, int> m;
    m[a] = 1;
    m[c] = 2;
    EXPECT_EQ(m.size(), 2u);
}
