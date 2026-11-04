#ifndef INFRA_TIME_SYSTEM_CLOCK_H
#define INFRA_TIME_SYSTEM_CLOCK_H

#include "i_clock.h"

#include <chrono>

namespace infra
{

class SystemClock final : public IClock
{
  public:
    std::chrono::steady_clock::time_point steadyNow() const override { return std::chrono::steady_clock::now(); }
    std::chrono::system_clock::time_point now() const override { return std::chrono::system_clock::now(); }
};

} // namespace infra

#endif // INFRA_TIME_SYSTEM_CLOCK_H
