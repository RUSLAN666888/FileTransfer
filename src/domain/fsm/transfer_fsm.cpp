#include "domain/fsm/transfer_fsm.h"

namespace domain
{

namespace
{

bool is_terminal(TransferState s)
{
    switch (s)
    {
    case TransferState::Done:
    case TransferState::Rejected:
    case TransferState::Cancelled:
    case TransferState::Failed:
        return true;
    default:
        return false;
    }
}

// "R: keep .part + checkpoint" — checkpoint is written by TransferManager;
// the FSM only signals KeepPart for the receiver.
std::vector<TransferEffect> keep_part_if_receiver(TransferSide side)
{
    if (side == TransferSide::Receiver)
    {
        return {{TransferEffectType::KeepPart}};
    }
    return {};
}

// ACTIVE / PAUSED: recv CANCEL or user cancel.
// "on user cancel also send CANCEL; R: keep .part + checkpoint".
std::pair<TransferState, std::vector<TransferEffect>> to_cancelled(TransferSide side, bool user_initiated)
{
    std::vector<TransferEffect> effects;
    if (user_initiated)
    {
        effects.push_back({TransferEffectType::SendCancel});
    }
    for (auto& e : keep_part_if_receiver(side))
    {
        effects.push_back(std::move(e));
    }
    return {TransferState::Cancelled, std::move(effects)};
}

// ACTIVE: recv ERROR.
// "if PROTOCOL_VIOLATION, also emit CloseSession; R: keep .part + checkpoint".
std::pair<TransferState, std::vector<TransferEffect>> active_error(TransferSide side, ReasonCode code)
{
    std::vector<TransferEffect> effects;
    if (code == ReasonCode::ProtocolViolation)
    {
        effects.push_back({TransferEffectType::SendError, std::nullopt, code});
        effects.push_back({TransferEffectType::CloseSession});
    }
    for (auto& e : keep_part_if_receiver(side))
    {
        effects.push_back(std::move(e));
    }
    return {TransferState::Failed, std::move(effects)};
}

// ACTIVE / PAUSED / RESUME_PENDING: recv SessionClosing.
// "R: keep .part + checkpoint".
std::pair<TransferState, std::vector<TransferEffect>> session_closing(TransferSide side, SessionCloseReason reason)
{
    auto next = (reason == SessionCloseReason::UserClosed) ? TransferState::Cancelled : TransferState::Failed;
    return {next, keep_part_if_receiver(side)};
}

// Any non-terminal state: message not valid here.
// "otherwise -> ERROR{PROTOCOL_VIOLATION}".
std::pair<TransferState, std::vector<TransferEffect>> protocol_violation()
{
    return {TransferState::Failed,
            {
                {TransferEffectType::SendError, std::nullopt, ReasonCode::ProtocolViolation},
                {TransferEffectType::CloseSession},
            }};
}

} // namespace

std::pair<TransferState, std::vector<TransferEffect>> handle(TransferState state, TransferSide side, const TransferEvent& event)
{
    if (is_terminal(state))
    {
        return {state, {}};
    }

    switch (state)
    {

        // ---------------------------------------------------------------- IDLE
    case TransferState::Idle:
        if (side == TransferSide::Sender)
        {
            switch (event.type)
            {
            // "user adds file -> OFFERED, S, send FILE_OFFER"
            case TransferEventType::UserAddFile:
                return {TransferState::Offered,
                        {
                            {TransferEffectType::SendFileOffer},
                        }};
            // "recv RESUME_REQUEST -> ACTIVE, S, send RESUME_ACCEPT"
            case TransferEventType::ResumeRequestReceived:
                return {TransferState::Active,
                        {
                            {TransferEffectType::SendResumeAccept},
                        }};
            // "recv RESUME_REQUEST, transfer/file missing or changed ->
            //  REJECTED, S, send RESUME_REJECT{reason}"
            case TransferEventType::ResumeRequestFileChanged:
                return {TransferState::Rejected,
                        {
                            {TransferEffectType::SendResumeReject, std::nullopt, ReasonCode::FileChanged},
                        }};
            default:
                return {state, {}};
            }
        }
        else
        {
            switch (event.type)
            {
            // "recv FILE_OFFER{chunks_number} -> OFFERED, R, validate path / disk"
            case TransferEventType::FileOfferReceived:
                return {TransferState::Offered, {}};
            default:
                return {state, {}};
            }
        }

        // ------------------------------------------------------------- OFFERED
    case TransferState::Offered:
        if (side == TransferSide::Sender)
        {
            switch (event.type)
            {
            // "recv FILE_ACCEPT -> ACTIVE, S, start sending CHUNK"
            case TransferEventType::FileAcceptReceived:
                return {TransferState::Active,
                        {
                            {TransferEffectType::SendChunk},
                        }};
            // "recv FILE_REJECT -> REJECTED, S, notify user"
            case TransferEventType::FileRejectReceived:
                return {TransferState::Rejected,
                        {
                            {TransferEffectType::NotifyUser, UserMessage::PeerRejectedFile},
                        }};
            // "timeout (default 30 s) -> FAILED, both"
            case TransferEventType::OfferTimeout:
                return {TransferState::Failed, {}};
            // "recv CANCEL / user cancel -> CANCELLED, both"
            // case TransferEventType::CancelReceived:
            //     return to_cancelled(side, /*user_initiated=*/false);
            // case TransferEventType::UserCancel:
            //     return to_cancelled(side, /*user_initiated=*/true);
            default:
                return protocol_violation();
            }
        }
        else
        {
            switch (event.type)
            {
            // "accept -> ACTIVE, R, send FILE_ACCEPT, open .part"
            case TransferEventType::UserAccept:
                return {TransferState::Active,
                        {
                            {TransferEffectType::SendFileAccept},
                        }};
            // "reject -> REJECTED, R, send FILE_REJECT{reason}"
            case TransferEventType::UserReject:
                return {TransferState::Rejected,
                        {
                            {TransferEffectType::SendFileReject, std::nullopt, ReasonCode::UserRejected},
                        }};
            // "timeout (default 30 s) -> FAILED, both"
            case TransferEventType::OfferTimeout:
                return {TransferState::Failed, {}};
            // "recv CANCEL / user cancel -> CANCELLED, both"
            // case TransferEventType::CancelReceived:
            //     return to_cancelled(side, /*user_initiated=*/false);
            // case TransferEventType::UserCancel:
            //     return to_cancelled(side, /*user_initiated=*/true);
            default:
                return protocol_violation();
            }
        }

        // -------------------------------------------------------------- ACTIVE
    case TransferState::Active:
        if (side == TransferSide::Sender)
        {
            switch (event.type)
            {
            // "recv CHUNK_ACK -> ACTIVE, S, update progress"
            case TransferEventType::ChunkAckReceived:
                return {TransferState::Active, {}};
            // "last CHUNK_ACK received -> DONE, S, send COMPLETE"
            case TransferEventType::LastChunkAckReceived:
                return {TransferState::Done,
                        {
                            {TransferEffectType::SendComplete},
                        }};
            // "recv PAUSE / user pause -> PAUSED, both, stop; on user pause also send PAUSE"
            case TransferEventType::PauseReceived:
                return {TransferState::Paused, {}};
            case TransferEventType::UserPause:
                return {TransferState::Paused,
                        {
                            {TransferEffectType::SendPause},
                        }};
            // "recv CANCEL / user cancel -> CANCELLED, both; R: keep .part + checkpoint"
            case TransferEventType::CancelReceived:
                return to_cancelled(side, /*user_initiated=*/false);
            case TransferEventType::UserCancel:
                return to_cancelled(side, /*user_initiated=*/true);
            // "recv ERROR -> FAILED, both; if PROTOCOL_VIOLATION, also emit CloseSession"
            case TransferEventType::ErrorReceived:
                return active_error(side, event.error_code);
            // "recv SessionClosing{...} -> CANCELLED / FAILED, both"
            case TransferEventType::SessionClosing:
                return session_closing(side, event.close_reason);
            default:
                return protocol_violation();
            }
        }
        else
        {
            switch (event.type)
            {
            // "recv CHUNK -> ACTIVE, R, write to disk, send CHUNK_ACK"
            case TransferEventType::ChunkReceived:
                return {TransferState::Active,
                        {
                            {TransferEffectType::SendChunkAck},
                        }};
            // "last CHUNK written -> DONE, R, rename .part"
            case TransferEventType::LastChunkWritten:
                return {TransferState::Done,
                        {
                            {TransferEffectType::RenamePartToFinal},
                        }};
            // "recv PAUSE / user pause -> PAUSED, both"
            case TransferEventType::PauseReceived:
                return {TransferState::Paused, {}};
            case TransferEventType::UserPause:
                return {TransferState::Paused,
                        {
                            {TransferEffectType::SendPause},
                        }};
            // "recv CANCEL / user cancel -> CANCELLED, both"
            case TransferEventType::CancelReceived:
                return to_cancelled(side, /*user_initiated=*/false);
            case TransferEventType::UserCancel:
                return to_cancelled(side, /*user_initiated=*/true);
            // "recv ERROR -> FAILED, both"
            case TransferEventType::ErrorReceived:
                return active_error(side, event.error_code);
            // "recv SessionClosing{...} -> CANCELLED / FAILED, both"
            case TransferEventType::SessionClosing:
                return session_closing(side, event.close_reason);
            // "COMPLETE: only in ACTIVE (R); informational (R is already DONE)"
            case TransferEventType::CompleteReceived:
                return {TransferState::Active, {}};
            default:
                return protocol_violation();
            }
        }

        // -------------------------------------------------------------- PAUSED
    case TransferState::Paused:
        switch (event.type)
        {
        // "recv RESUME / user resume -> ACTIVE, both; on user resume also send RESUME"
        case TransferEventType::ResumeReceived:
            return {TransferState::Active, {}};
        case TransferEventType::UserResume:
            return {TransferState::Active,
                    {
                        {TransferEffectType::SendResume},
                    }};
        // "recv CANCEL / user cancel -> CANCELLED, both"
        case TransferEventType::CancelReceived:
            return to_cancelled(side, /*user_initiated=*/false);
        case TransferEventType::UserCancel:
            return to_cancelled(side, /*user_initiated=*/true);
        // "recv SessionClosing{...} -> CANCELLED / FAILED, both"
        case TransferEventType::SessionClosing:
            return session_closing(side, event.close_reason);
        // In-flight messages: ignored, not violations.
        case TransferEventType::ChunkReceived:
        case TransferEventType::ChunkAckReceived:
            return {state, {}};
        default:
            return protocol_violation();
        }

        // ------------------------------------------------------ RESUME_PENDING
    case TransferState::ResumePending:
        if (side != TransferSide::Receiver)
        {
            return protocol_violation();
        }
        switch (event.type)
        {
        // "recv RESUME_ACCEPT -> ACTIVE, R, seek to chunk_offset, wait for CHUNK"
        case TransferEventType::ResumeAcceptReceived:
            return {TransferState::Active, {}};
        // "recv RESUME_REJECT -> REJECTED, R, notify user"
        case TransferEventType::ResumeRejectReceived:
            return {TransferState::Rejected,
                    {
                        {TransferEffectType::NotifyUser, UserMessage::PeerRejectedFile},
                    }};
        // "timeout -> FAILED, R"
        case TransferEventType::ResumeTimeout:
            return {TransferState::Failed, {}};
        // "recv SessionClosing{...} -> CANCELLED / FAILED, R"
        case TransferEventType::SessionClosing:
            return session_closing(side, event.close_reason);
        default:
            return protocol_violation();
        }

    case TransferState::Done:
    case TransferState::Rejected:
    case TransferState::Cancelled:
    case TransferState::Failed:
        return {state, {}};
    }
    return {state, {}};
}

} // namespace domain
