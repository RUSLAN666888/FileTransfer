#ifndef INFRA_FILE_SYSTEM_H
#define INFRA_FILE_SYSTEM_H

#include "infra/file_system/i_file_system.h"
#include "infra/result.h"

namespace infra
{

/// @brief `IFileSystem` on top of `std::filesystem` and `std::fstream`.
///
/// Single implementation for all platforms. No `#ifdef`: the only
/// OS-sensitive operation is `rename`, which `std::filesystem::rename`
/// already implements correctly on both platforms (with an explicit
/// pre-remove of the destination on Windows, see `.cpp`).
class FileSystem final : public IFileSystem
{
  public:
    Result<std::vector<std::uint8_t>> read_file(const std::string& path) override;
    Result<void> write_file_atomic(const std::string& path, std::span<const std::uint8_t> data) override;
    Result<std::unique_ptr<IReadHandle>> open_for_read(const std::string& path) override;
    Result<std::unique_ptr<IWriteHandle>> open_for_write(const std::string& path) override;
    bool exists(const std::string& path) override;
    Result<void> remove(const std::string& path) override;
    Result<void> rename(const std::string& from, const std::string& to) override;
};

} // namespace infra

#endif
