#ifndef INFRA_TIME_ICLOCK_H
#define INFRA_TIME_ICLOCK_H

#include <chrono>

namespace infra
{

/**
 * @brief Read-only source of the current time.
 *
 * @details
 * `IClock` does not sleep, does not schedule timers, and does not invoke
 * callbacks. Waiting is the responsibility of the event loop (asio) or of a
 * test using `FakeClock`.
 *
 * Two clocks, on purpose:
 *  - `steadyNow()` is monotonic and must be used for durations: timeouts
 *    (PING, handshake, RESUME_PENDING), intervals (ACK cadence), and rate
 *    windows. It is immune to NTP jumps and manual clock changes.
 *  - `now()` is wall-clock and must be used for timestamps: log entries,
 *    `DiscoveryPacket::timestamp`, history records. It may jump.
 *
 */
class IClock
{
  public:
    virtual ~IClock() = default;

    /**
     * @brief Monotonic clock for measuring durations.
     * @return Current steady time point. Never decreases between calls.
     * @note Used for timeouts, intervals, and rate windows.
     */
    virtual std::chrono::steady_clock::time_point steadyNow() const = 0;

    /**
     * @brief Wall-clock for calendar timestamps.
     * @return Current system time point. May jump (NTP, manual changes).
     * @note Used for log entries, `DiscoveryPacket::timestamp`, history.
     */
    virtual std::chrono::system_clock::time_point now() const = 0;
};

} // namespace infra

#endif // INFRA_TIME_ICLOCK_H
