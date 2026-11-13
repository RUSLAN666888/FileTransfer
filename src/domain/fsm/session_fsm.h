#ifndef DOMAIN_SESSION_FSM_H
#define DOMAIN_SESSION_FSM_H

#include "domain/value/session_close_reason.h"

#include <utility>
#include <vector>

namespace domain
{

enum class SessionState
{
    Idle,
    Handshaking,
    Established,
    Closing,
    Closed,
};

enum class SessionEventType
{
    ConnectRequested,
    IncomingRequestAccepted,
    HandshakeOk,
    HandshakeFailed,
    UserRejected,
    HandshakeTimeout,
    PingReceived,
    PongReceived,
    PingTimeout,
    ProtocolViolation,
    UserClose,
    TransportError,
    TransportClosed,
};

struct SessionEvent
{
    SessionEventType type;
    // Meaningful only for events that transition to CLOSING.
    // The caller (app::SessionManager) knows why the session is closing
    // and supplies the reason; SessionFsm only forwards it.
    SessionCloseReason reason = SessionCloseReason::UserClosed;
};

enum class SessionEffectType
{
    StartHandshake,
    StartLivenessTimer,
    ResetLivenessTimer,
    SendPong,
    CloseTransport,
    SessionClosing,
};

struct SessionEffect
{
    SessionEffectType type;
    // Only meaningful when type == SessionClosing.
    SessionCloseReason close_reason = SessionCloseReason::UserClosed;
};

/// @brief Session lifecycle FSM.
///
/// Pure function. The caller supplies the close reason in the event;
/// SessionFsm forwards it via `SessionClosing{reason}` and does not decide
/// what the reason means for transfers, logging, or the GUI. Those are
/// listeners on EventBus.
std::pair<SessionState, std::vector<SessionEffect>> handle(SessionState state, const SessionEvent& event);

} // namespace domain

#endif
