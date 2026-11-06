#ifndef INFRA_I_FILE_SYSTEM_H
#define INFRA_I_FILE_SYSTEM_H

#include "infra/result.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace infra
{

/// @brief RAII handle for streaming reads from a file.
///
/// Used by `TransferManager` to read `CHUNK`-sized pieces at a given offset.
/// Not thread-safe: one handle per thread.
class IReadHandle
{
  public:
    virtual ~IReadHandle() = default;

    /// @brief Read up to `buf.size()` bytes starting at `offset`.
    /// @return Number of bytes actually read (may be less than `buf.size()`
    ///         at end of file). `Error` on I/O failure.
    virtual Result<std::size_t> read_at(std::uint64_t offset, std::span<std::uint8_t> buf) = 0;

    /// @brief Total file size in bytes.
    virtual Result<std::uint64_t> size() = 0;
};

/// @brief RAII handle for streaming writes to a file.
///
/// Used by `TransferManager` to write `CHUNK`-sized pieces at a given offset.
/// The file is created if it does not exist; existing contents are preserved
/// (writes overwrite the given range, the rest is untouched).
class IWriteHandle
{
  public:
    virtual ~IWriteHandle() = default;

    /// @brief Write `data` at `offset`. Extends the file if needed.
    virtual Result<void> write_at(std::uint64_t offset, std::span<const std::uint8_t> data) = 0;

    /// @brief Flush buffered data to the OS. Does not guarantee fsync.
    virtual Result<void> flush() = 0;
};

/// @brief File-system boundary.
///
/// All operations are synchronous and return `Result<T>`; exceptions from
/// `std::filesystem` / `std::fstream` are converted to `Error`.
///
/// Path sanitization is **not** part of this interface: defending against
/// path traversal / symlinks is out of scope per the threat model. Callers
/// that accept filenames from the wire are responsible for choosing a safe
/// final path themselves.
class IFileSystem
{
  public:
    virtual ~IFileSystem() = default;

    // --- Small-file helpers (keystore, config) ---

    /// @brief Read entire file into memory. Not for large files.
    virtual Result<std::vector<std::uint8_t>> read_file(const std::string& path) = 0;

    /// @brief Write `data` to `path` atomically via `.tmp` + `rename`.
    ///
    /// If the process dies mid-write, the destination is either the old
    /// content or the new content, never a partial file.
    ///
    /// Intended for small files (keystore, config). Not for streaming
    /// transfers: `TransferManager` writes chunks via `IWriteHandle` and
    /// renames `.part` to the final name itself.
    ///
    /// Atomicity relies on `rename` being atomic, which holds on POSIX and
    /// on Windows when source and destination are on the same volume.
    /// `path.tmp` must be on the same volume as `path` for this to work —
    /// currently the implementation always uses `path + ".tmp"`, so this
    /// is satisfied.
    virtual Result<void> write_file_atomic(const std::string& path, std::span<const std::uint8_t> data) = 0;

    // --- Streaming handles (file transfer) ---

    /// @brief Open `path` for streaming reads. Fails if the path does not
    ///        exist or is not a regular file.
    virtual Result<std::unique_ptr<IReadHandle>> open_for_read(const std::string& path) = 0;

    /// @brief Open `path` for streaming writes. Creates the file if missing.
    ///        Existing contents are preserved; writes overwrite the given
    ///        ranges. The caller is responsible for renaming `.part` to the
    ///        final name via `rename`.
    virtual Result<std::unique_ptr<IWriteHandle>> open_for_write(const std::string& path) = 0;

    // --- Metadata / lifecycle ---

    /// @brief True if `path` exists (file, directory, or symlink).
    virtual bool exists(const std::string& path) = 0;

    /// @brief Remove a regular file. Fails if the path does not exist or is
    ///        a directory.
    virtual Result<void> remove(const std::string& path) = 0;

    /// @brief Rename `from` to `to`, replacing `to` if it exists.
    ///
    /// Used for `.part` -> final name. Atomic on POSIX and on Windows when
    /// source and destination are on the same volume.
    virtual Result<void> rename(const std::string& from, const std::string& to) = 0;
};

} // namespace infra

#endif
