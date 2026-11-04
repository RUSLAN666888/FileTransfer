#ifndef INFRA_TIME_FAKECLOCK_H
#define INFRA_TIME_FAKECLOCK_H

#include "i_clock.h"

namespace infra
{

/**
 * @brief Test implementation of `IClock` with manually controlled time.
 *
 * @details
 * Both clocks start at their epoch and advance only via `advance()` or
 * `setSystem()`.
 */
class FakeClock final : public IClock
{
  public:
    /// @brief Monotonic time; advances only via `advance()`.
    std::chrono::steady_clock::time_point steadyNow() const override { return steady_; }

    /// @brief Wall-clock time; advances via `advance()` or jumps via `setSystem()`.
    std::chrono::system_clock::time_point now() const override { return system_; }

    /// @brief Moves both clocks forward by `d`.
    void advance(std::chrono::steady_clock::duration d)
    {
        steady_ += d;
        system_ += d;
    }

    /// @brief Jumps the wall-clock only; `steadyNow()` is unaffected.
    void setSystem(std::chrono::system_clock::time_point t) { system_ = t; }

  private:
    std::chrono::steady_clock::time_point steady_{};
    std::chrono::system_clock::time_point system_{};
};

} // namespace infra

#endif // INFRA_TIME_FAKECLOCK_H
