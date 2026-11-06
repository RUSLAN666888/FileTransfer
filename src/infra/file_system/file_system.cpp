#include "infra/file_system/file_system.h"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace infra
{

namespace
{

Error to_error(const std::error_code& ec, const std::string& context) { return Error{context + ": " + ec.message()}; }

std::string tmp_path(const std::string& path) { return path + ".tmp"; }

// --- IReadHandle on std::ifstream ---------------------------------------

class FstreamReadHandle final : public IReadHandle
{
  public:
    explicit FstreamReadHandle(std::ifstream in) : in_(std::move(in)) {}

    Result<std::size_t> read_at(std::uint64_t offset, std::span<std::uint8_t> buf) override
    {
        in_.clear(); // clear any prior eof/fail flags
        in_.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        if (!in_)
            return Error{"seekg failed at offset " + std::to_string(offset)};

        if (buf.empty())
            return std::size_t{0};

        in_.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        const auto got = static_cast<std::size_t>(in_.gcount());
        if (in_.bad())
            return Error{"read failed at offset " + std::to_string(offset)};
        // eof with partial read is fine; caller sees `got < buf.size()`.
        return got;
    }

    Result<std::uint64_t> size() override
    {
        std::error_code ec;
        in_.clear();
        in_.seekg(0, std::ios::end);
        if (!in_)
            return Error{"seekg(end) failed"};
        const auto end = in_.tellg();
        if (end < 0)
            return Error{"tellg failed"};
        return static_cast<std::uint64_t>(end);
    }

  private:
    std::ifstream in_;
};

// --- IWriteHandle on std::fstream ---------------------------------------

class FstreamWriteHandle final : public IWriteHandle
{
  public:
    explicit FstreamWriteHandle(std::fstream out) : out_(std::move(out)) {}

    Result<void> write_at(std::uint64_t offset, std::span<const std::uint8_t> data) override
    {
        out_.clear();
        out_.seekp(static_cast<std::streamoff>(offset), std::ios::beg);

        if (!out_)
            return Error{"seekp failed at offset " + std::to_string(offset)};

        if (!data.empty())
        {
            out_.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        }

        if (!out_)
            return Error{"write failed at offset " + std::to_string(offset)};
        return {};
    }

    Result<void> flush() override
    {
        out_.flush();
        if (!out_)
            return Error{"flush failed"};
        return {};
    }

  private:
    std::fstream out_;
};

} // namespace

// --- IFileSystem --------------------------------------------------------

Result<std::vector<std::uint8_t>> FileSystem::read_file(const std::string& path)
{
    auto h = open_for_read(path);
    if (!h)
        return h.error();

    auto sz = h.value()->size();
    if (!sz)
        return sz.error();

    std::vector<std::uint8_t> buf(static_cast<std::size_t>(sz.value()));
    if (buf.empty())
        return buf;

    auto n = h.value()->read_at(0, buf);
    if (!n)
        return n.error();

    if (n.value() != buf.size())
    {
        return Error{"short read: " + path};
    }
    return buf;
}

Result<void> FileSystem::write_file_atomic(const std::string& path, std::span<const std::uint8_t> data)
{
    const auto tmp = tmp_path(path);

    // Write to `.tmp` first.
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out)
            return Error{"cannot open for writing: " + tmp};
        if (!data.empty())
        {
            out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        }
        out.flush();
        if (!out)
        {
            std::error_code ignored;
            fs::remove(tmp, ignored);
            return Error{"short write: " + tmp};
        }
    }

    auto r = rename(tmp, path);
    if (!r)
    {
        std::error_code ignored;
        fs::remove(tmp, ignored);
        return r;
    }
    return {};
}

Result<std::unique_ptr<IReadHandle>> FileSystem::open_for_read(const std::string& path)
{
    std::error_code ec;
    if (!fs::is_regular_file(path, ec))
    {
        if (ec)
            return to_error(ec, "is_regular_file(" + path + ")");

        return Error{"not a regular file: " + path};
    }

    std::ifstream in(path, std::ios::binary);
    if (!in)
        return Error{"cannot open for reading: " + path};

    return std::unique_ptr<IReadHandle>(new FstreamReadHandle(std::move(in)));
}

Result<std::unique_ptr<IWriteHandle>> FileSystem::open_for_write(const std::string& path)
{

    std::error_code ec;
    if (!fs::is_regular_file(path, ec))
    {
        if (ec)
            return to_error(ec, "is_regular_file(" + path + ")");
        return Error{"not a regular file: " + path};
    }

    // `in | out` mode: do not truncate, allow reading back if needed.
    std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
    if (!f)
        return Error{"cannot open for writing: " + path};

    return std::unique_ptr<IWriteHandle>(new FstreamWriteHandle(std::move(f)));
}

bool FileSystem::exists(const std::string& path)
{
    std::error_code ec;
    return fs::exists(path, ec);
}

Result<void> FileSystem::remove(const std::string& path)
{
    std::error_code ec;
    if (!fs::is_regular_file(path, ec))
    {
        if (ec)
            return to_error(ec, "is_regular_file(" + path + ")");
        return Error{"not a regular file: " + path};
    }
    fs::remove(path, ec);
    if (ec)
        return to_error(ec, "remove(" + path + ")");
    return {};
}

Result<void> FileSystem::rename(const std::string& from, const std::string& to)
{
    std::error_code ec;

    fs::rename(from, to, ec);
    if (ec)
        return to_error(ec, "rename(" + from + " -> " + to + ")");
    return {};
}

} // namespace infra
