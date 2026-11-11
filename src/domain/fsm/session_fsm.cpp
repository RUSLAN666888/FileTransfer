#include "domain/fsm/session_fsm.h"

namespace domain
{

namespace
{

std::pair<SessionState, std::vector<SessionEffect>> to_closing(SessionCloseReason reason)
{
    return {SessionState::Closing,
            {
                {SessionEffectType::CloseTransport, reason},
                {SessionEffectType::SessionClosing, reason},
            }};
}

} // namespace

std::pair<SessionState, std::vector<SessionEffect>> handle(SessionState state, const SessionEvent& event)
{
    switch (state)
    {
    case SessionState::Idle:
        switch (event.type)
        {
        case SessionEventType::ConnectRequested:
        case SessionEventType::IncomingRequestAccepted:
            return {SessionState::Handshaking,
                    {
                        {SessionEffectType::StartHandshake},
                    }};
        default:
            return {state, {}};
        }

    case SessionState::Handshaking:
        switch (event.type)
        {
        case SessionEventType::HandshakeOk:
            return {SessionState::Established,
                    {
                        {SessionEffectType::StartLivenessTimer},
                    }};
        case SessionEventType::HandshakeFailed:
        case SessionEventType::UserRejected:
        case SessionEventType::HandshakeTimeout:
            return to_closing(event.reason);
        default:
            return {state, {}};
        }

    case SessionState::Established:
        switch (event.type)
        {
        case SessionEventType::PingReceived:
            return {SessionState::Established,
                    {
                        {SessionEffectType::SendPong},
                    }};
        case SessionEventType::PongReceived:
            return {SessionState::Established,
                    {
                        {SessionEffectType::ResetLivenessTimer},
                    }};
        case SessionEventType::PingTimeout:
        case SessionEventType::ProtocolViolation:
        case SessionEventType::UserClose:
        case SessionEventType::TransportError:
            return to_closing(event.reason);
        default:
            return {state, {}};
        }

    case SessionState::Closing:
        if (event.type == SessionEventType::TransportClosed)
        {
            return {SessionState::Closed, {}};
        }
        return {state, {}};

    case SessionState::Closed:
        return {state, {}};
    }
    return {state, {}};
}

} // namespace domain
