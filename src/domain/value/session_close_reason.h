#ifndef DOMAIN_SESSION_CLOSE_REASON_H
#define DOMAIN_SESSION_CLOSE_REASON_H

namespace domain
{

/// @brief One enum for both transfer and session FSMs
enum class SessionCloseReason
{
    UserClosed,
    PingTimeout,
    ProtocolViolation,
    TransportError,
};

} // namespace domain

#endif
