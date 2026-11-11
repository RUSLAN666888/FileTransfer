#include "domain/fsm/session_fsm.h"

#include <gtest/gtest.h>

namespace
{

using domain::handle;
using domain::SessionCloseReason;
using domain::SessionEffect;
using domain::SessionEffectType;
using domain::SessionEvent;
using domain::SessionEventType;
using domain::SessionState;

SessionEvent ev(SessionEventType t, SessionCloseReason r = SessionCloseReason::UserClosed) { return SessionEvent{t, r}; }

bool has_effect(const std::vector<SessionEffect>& effects, SessionEffectType t)
{
    for (const auto& e : effects)
        if (e.type == t)
            return true;
    return false;
}

const SessionEffect* find_effect(const std::vector<SessionEffect>& effects, SessionEffectType t)
{
    for (const auto& e : effects)
        if (e.type == t)
            return &e;
    return nullptr;
}

void expect_closing(const std::pair<SessionState, std::vector<SessionEffect>>& result, SessionCloseReason expected_reason)
{
    EXPECT_EQ(result.first, SessionState::Closing);
    EXPECT_TRUE(has_effect(result.second, SessionEffectType::CloseTransport));
    const auto* sc = find_effect(result.second, SessionEffectType::SessionClosing);
    ASSERT_NE(sc, nullptr);
    EXPECT_EQ(sc->close_reason, expected_reason);
}

// --- IDLE ---

TEST(SessionFsm, IdleConnectRequested)
{
    auto r = handle(SessionState::Idle, ev(SessionEventType::ConnectRequested));
    EXPECT_EQ(r.first, SessionState::Handshaking);
    EXPECT_TRUE(has_effect(r.second, SessionEffectType::StartHandshake));
    EXPECT_EQ(r.second.size(), 1u);
}

TEST(SessionFsm, IdleIncomingRequestAccepted)
{
    auto r = handle(SessionState::Idle, ev(SessionEventType::IncomingRequestAccepted));
    EXPECT_EQ(r.first, SessionState::Handshaking);
    EXPECT_TRUE(has_effect(r.second, SessionEffectType::StartHandshake));
}

TEST(SessionFsm, IdleIgnoresUnrelatedEvents)
{
    for (auto t : {SessionEventType::HandshakeOk, SessionEventType::PingReceived, SessionEventType::UserClose, SessionEventType::TransportClosed})
    {
        auto r = handle(SessionState::Idle, ev(t));
        EXPECT_EQ(r.first, SessionState::Idle);
        EXPECT_TRUE(r.second.empty());
    }
}

// --- HANDSHAKING ---

TEST(SessionFsm, HandshakingOk)
{
    auto r = handle(SessionState::Handshaking, ev(SessionEventType::HandshakeOk));
    EXPECT_EQ(r.first, SessionState::Established);
    EXPECT_TRUE(has_effect(r.second, SessionEffectType::StartLivenessTimer));
}

TEST(SessionFsm, HandshakingFailedForwardsReason)
{
    // The reason comes from the caller, not from SessionFsm.
    expect_closing(handle(SessionState::Handshaking, ev(SessionEventType::HandshakeFailed, SessionCloseReason::TransportError)), SessionCloseReason::TransportError);
}

TEST(SessionFsm, HandshakingUserRejectedForwardsReason)
{
    expect_closing(handle(SessionState::Handshaking, ev(SessionEventType::UserRejected, SessionCloseReason::UserClosed)), SessionCloseReason::UserClosed);
}

TEST(SessionFsm, HandshakingTimeoutForwardsReason)
{
    expect_closing(handle(SessionState::Handshaking, ev(SessionEventType::HandshakeTimeout, SessionCloseReason::PingTimeout)), SessionCloseReason::PingTimeout);
}

TEST(SessionFsm, HandshakingIgnoresUnrelatedEvents)
{
    for (auto t : {SessionEventType::PingReceived, SessionEventType::PongReceived, SessionEventType::UserClose, SessionEventType::TransportClosed})
    {
        auto r = handle(SessionState::Handshaking, ev(t));
        EXPECT_EQ(r.first, SessionState::Handshaking);
        EXPECT_TRUE(r.second.empty());
    }
}

// --- ESTABLISHED ---

TEST(SessionFsm, EstablishedPingReceived)
{
    auto r = handle(SessionState::Established, ev(SessionEventType::PingReceived));
    EXPECT_EQ(r.first, SessionState::Established);
    EXPECT_TRUE(has_effect(r.second, SessionEffectType::SendPong));
    EXPECT_EQ(r.second.size(), 1u);
}

TEST(SessionFsm, EstablishedPongReceived)
{
    auto r = handle(SessionState::Established, ev(SessionEventType::PongReceived));
    EXPECT_EQ(r.first, SessionState::Established);
    EXPECT_TRUE(has_effect(r.second, SessionEffectType::ResetLivenessTimer));
}

TEST(SessionFsm, EstablishedPingTimeoutForwardsReason)
{
    expect_closing(handle(SessionState::Established, ev(SessionEventType::PingTimeout, SessionCloseReason::PingTimeout)), SessionCloseReason::PingTimeout);
}

TEST(SessionFsm, EstablishedProtocolViolationForwardsReason)
{
    expect_closing(handle(SessionState::Established, ev(SessionEventType::ProtocolViolation, SessionCloseReason::ProtocolViolation)), SessionCloseReason::ProtocolViolation);
}

TEST(SessionFsm, EstablishedUserCloseForwardsReason)
{
    expect_closing(handle(SessionState::Established, ev(SessionEventType::UserClose, SessionCloseReason::UserClosed)), SessionCloseReason::UserClosed);
}

TEST(SessionFsm, EstablishedTransportErrorForwardsReason)
{
    expect_closing(handle(SessionState::Established, ev(SessionEventType::TransportError, SessionCloseReason::TransportError)), SessionCloseReason::TransportError);
}

TEST(SessionFsm, EstablishedIgnoresUnrelatedEvents)
{
    for (auto t : {SessionEventType::HandshakeOk, SessionEventType::ConnectRequested, SessionEventType::TransportClosed})
    {
        auto r = handle(SessionState::Established, ev(t));
        EXPECT_EQ(r.first, SessionState::Established);
        EXPECT_TRUE(r.second.empty());
    }
}

// --- CLOSING ---

TEST(SessionFsm, ClosingTransportClosed)
{
    auto r = handle(SessionState::Closing, ev(SessionEventType::TransportClosed));
    EXPECT_EQ(r.first, SessionState::Closed);
    EXPECT_TRUE(r.second.empty());
}

TEST(SessionFsm, ClosingIgnoresOtherEvents)
{
    for (auto t : {SessionEventType::PingReceived, SessionEventType::PongReceived, SessionEventType::UserClose, SessionEventType::TransportError})
    {
        auto r = handle(SessionState::Closing, ev(t));
        EXPECT_EQ(r.first, SessionState::Closing);
        EXPECT_TRUE(r.second.empty());
    }
}

// --- CLOSED ---

TEST(SessionFsm, ClosedIsTerminal)
{
    for (auto t :
         {SessionEventType::ConnectRequested, SessionEventType::HandshakeOk, SessionEventType::PingReceived, SessionEventType::UserClose, SessionEventType::TransportClosed})
    {
        auto r = handle(SessionState::Closed, ev(t));
        EXPECT_EQ(r.first, SessionState::Closed);
        EXPECT_TRUE(r.second.empty());
    }
}

// --- reason forwarding ---

TEST(SessionFsm, ReasonIsForwardedVerbatim)
{
    // The FSM never creates a reason; it forwards whatever the caller supplied.
    for (auto reason : {SessionCloseReason::UserClosed, SessionCloseReason::PingTimeout, SessionCloseReason::ProtocolViolation, SessionCloseReason::TransportError})
    {
        auto r = handle(SessionState::Established, ev(SessionEventType::TransportError, reason));
        const auto* sc = find_effect(r.second, SessionEffectType::SessionClosing);
        ASSERT_NE(sc, nullptr);
        EXPECT_EQ(sc->close_reason, reason);
    }
}

} // namespace
