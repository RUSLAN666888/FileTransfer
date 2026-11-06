#ifndef INFRA_RESULT_H
#define INFRA_RESULT_H

#include <string>
#include <utility>
#include <variant>

namespace infra
{

/// @brief Error carried by `Result<T>` on failure.
struct Error
{
    std::string message;
};

/// @brief Minimal `expected<T, Error>` for C++20.
///
/// Invariants:
///  - `has_value() == true`  => `value()` is valid, `error()` is not.
///  - `has_value() == false` => `error()` is valid, `value()` is not.
template <typename T> class Result
{
  public:
    Result(T value) : storage_(std::in_place_index<0>, std::move(value)) {}
    Result(Error err) : storage_(std::in_place_index<1>, std::move(err)) {}

    bool has_value() const noexcept { return storage_.index() == 0; }
    explicit operator bool() const noexcept { return has_value(); }

    T& value() & { return std::get<0>(storage_); }
    const T& value() const& { return std::get<0>(storage_); }
    T&& value() && { return std::get<0>(std::move(storage_)); }

    const Error& error() const& { return std::get<1>(storage_); }
    Error&& error() && { return std::get<1>(std::move(storage_)); }

  private:
    std::variant<T, Error> storage_;
};

/// @brief `Result<void>` specialization.
template <> class Result<void>
{
  public:
    Result() = default;
    Result(Error err) : error_(std::move(err)), ok_(false) {}

    bool has_value() const noexcept { return ok_; }
    explicit operator bool() const noexcept { return ok_; }

    const Error& error() const& { return error_; }
    Error&& error() && { return std::move(error_); }

  private:
    Error error_{};
    bool ok_ = true;
};

} // namespace infra

#endif
