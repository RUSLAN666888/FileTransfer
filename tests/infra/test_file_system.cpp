#ifndef TEST_FILE_SYSTEM_H
#define TEST_FILE_SYSTEM_H

#include "infra/file_system/file_system.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{

class FileSystemTest : public ::testing::Test
{
  protected:
    infra::FileSystem fs;
    fs::path dir;

    void SetUp() override
    {
        dir = fs::temp_directory_path() / ("ft_test_" + std::to_string(std::rand()));
        fs::create_directories(dir);
    }

    void TearDown() override
    {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    std::string p(const std::string& name) const { return (dir / name).string(); }
};

std::vector<std::uint8_t> bytes(std::size_t n, std::uint8_t salt = 0)
{
    std::vector<std::uint8_t> v(n);
    for (std::size_t i = 0; i < n; ++i)
        v[i] = static_cast<std::uint8_t>((i + salt) & 0xFF);
    return v;
}

} // namespace

TEST_F(FileSystemTest, ReadFileRoundTrip)
{
    const auto data = bytes(1024);
    ASSERT_TRUE(fs.write_file_atomic(p("a.bin"), data));

    auto r = fs.read_file(p("a.bin"));
    ASSERT_TRUE(r) << r.error().message;
    EXPECT_EQ(r.value(), data);
}

TEST_F(FileSystemTest, ReadFileMissingFails) { EXPECT_FALSE(fs.read_file(p("missing.bin"))); }

TEST_F(FileSystemTest, WriteFileAtomicLeavesNoTmp)
{
    ASSERT_TRUE(fs.write_file_atomic(p("b.bin"), bytes(16)));
    EXPECT_TRUE(fs.exists(p("b.bin")));
    EXPECT_FALSE(fs.exists(p("b.bin.tmp")));
}

TEST_F(FileSystemTest, OpenForReadReadAtOffsets)
{
    const auto data = bytes(256);
    ASSERT_TRUE(fs.write_file_atomic(p("c.bin"), data));

    auto h = fs.open_for_read(p("c.bin"));
    ASSERT_TRUE(h) << h.error().message;

    auto sz = h.value()->size();
    ASSERT_TRUE(sz);
    EXPECT_EQ(sz.value(), data.size());

    std::vector<std::uint8_t> buf(64);
    auto n = h.value()->read_at(64, buf);
    ASSERT_TRUE(n);
    ASSERT_EQ(n.value(), 64u);
    for (std::size_t i = 0; i < 64; ++i)
    {
        EXPECT_EQ(buf[i], data[64 + i]);
    }
}

TEST_F(FileSystemTest, OpenForReadPastEofReturnsZero)
{
    ASSERT_TRUE(fs.write_file_atomic(p("d.bin"), bytes(10)));

    auto h = fs.open_for_read(p("d.bin"));
    ASSERT_TRUE(h);

    std::vector<std::uint8_t> buf(16);
    auto n = h.value()->read_at(100, buf);
    ASSERT_TRUE(n);
    EXPECT_EQ(n.value(), 0u);
}

TEST_F(FileSystemTest, OpenForWriteWriteAtOffsets)
{
    const auto original = bytes(128, 0);
    ASSERT_TRUE(fs.write_file_atomic(p("e.bin"), original));

    const auto patch = bytes(32, 0xAA);
    {
        auto h = fs.open_for_write(p("e.bin"));
        ASSERT_TRUE(h) << h.error().message;

        auto w = h.value()->write_at(64, patch);
        ASSERT_TRUE(w) << w.error().message;
        ASSERT_TRUE(h.value()->flush());
    }

    auto r = fs.read_file(p("e.bin"));
    ASSERT_TRUE(r);
    const auto& out = r.value();
    ASSERT_EQ(out.size(), original.size());

    // Bytes before/after the patch are untouched.
    for (std::size_t i = 0; i < 64; ++i)
        EXPECT_EQ(out[i], original[i]);
    for (std::size_t i = 64; i < 96; ++i)
        EXPECT_EQ(out[i], patch[i - 64]);
    for (std::size_t i = 96; i < 128; ++i)
        EXPECT_EQ(out[i], original[i]);

    std::cout << "done" << std::endl;
}

TEST_F(FileSystemTest, RenameReplacesTarget)
{
    ASSERT_TRUE(fs.write_file_atomic(p("old.bin"), bytes(4, 1)));
    ASSERT_TRUE(fs.write_file_atomic(p("new.bin"), bytes(8, 2)));

    ASSERT_TRUE(fs.rename(p("new.bin"), p("old.bin")));
    EXPECT_FALSE(fs.exists(p("new.bin")));

    auto r = fs.read_file(p("old.bin"));
    ASSERT_TRUE(r);
    EXPECT_EQ(r.value(), bytes(8, 2));
}

TEST_F(FileSystemTest, RemoveFile)
{
    ASSERT_TRUE(fs.write_file_atomic(p("f.bin"), {}));
    ASSERT_TRUE(fs.remove(p("f.bin")));
    EXPECT_FALSE(fs.exists(p("f.bin")));
}

TEST_F(FileSystemTest, RemoveMissingFails) { EXPECT_FALSE(fs.remove(p("nope.bin"))); }

TEST_F(FileSystemTest, OpenForReadMissingFails) { EXPECT_FALSE(fs.open_for_read(p("missing.bin"))); }

TEST_F(FileSystemTest, OpenForWriteMissingFails) { EXPECT_FALSE(fs.open_for_write(p("missing.bin"))); }

TEST_F(FileSystemTest, RenameMissingFails) { EXPECT_FALSE(fs.rename(p("missing.bin"), p("dst.bin"))); }

TEST_F(FileSystemTest, ReadFileOnDirectoryFails)
{
    fs::create_directories(p("subdir"));
    EXPECT_FALSE(fs.read_file(p("subdir")));
}

TEST_F(FileSystemTest, OpenForWriteOnDirectoryFails)
{
    fs::create_directories(p("subdir"));
    EXPECT_FALSE(fs.open_for_write(p("subdir")));
}

TEST_F(FileSystemTest, RemoveOnDirectoryFails)
{
    fs::create_directories(p("subdir"));
    EXPECT_FALSE(fs.remove(p("subdir")));
}

TEST_F(FileSystemTest, ReadEmptyFileReturnsEmptyVector)
{
    ASSERT_TRUE(fs.write_file_atomic(p("empty.bin"), {}));

    auto r = fs.read_file(p("empty.bin"));
    ASSERT_TRUE(r);
    EXPECT_TRUE(r.value().empty());
}

TEST_F(FileSystemTest, OpenForReadEmptyFileSizeZero)
{
    ASSERT_TRUE(fs.write_file_atomic(p("empty.bin"), {}));

    auto h = fs.open_for_read(p("empty.bin"));
    ASSERT_TRUE(h);

    auto sz = h.value()->size();
    ASSERT_TRUE(sz);
    EXPECT_EQ(sz.value(), 0u);
}

#endif
