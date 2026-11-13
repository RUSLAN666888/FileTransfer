#ifndef DOMAIN_TRANSFER_FSM_H
#define DOMAIN_TRANSFER_FSM_H

#include "domain/value/session_close_reason.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace domain
{

enum class TransferState
{
    Idle,
    Offered,
    Active,
    Paused,
    ResumePending,
    Done,
    Rejected,
    Cancelled,
    Failed,
};

enum class TransferSide
{
    Sender,
    Receiver,
};

/// @brief Wire-level reason for FILE_REJECT, RESUME_REJECT, ERROR.
enum class ReasonCode : std::uint8_t
{
    FileChanged = 1,
    FileMissing = 2,
    DiskFull = 10,
    PathInvalid = 11,
    UserRejected = 12,
    ProtocolViolation = 20,
};

/// @brief UI-level message emitted by NotifyUser.
enum class UserMessage
{
    FileSent,
    FileReceived,
    PeerRejectedFile,
    ResumeRejectedFileChanged,
    PeerDidNotRespond,
    TransferCancelled,
    TransferFailed,
    ProtocolViolation,
};

enum class TransferEventType
{
    // User actions.
    UserAddFile,
    UserAccept,
    UserReject,
    UserPause,
    UserResume,
    UserCancel,

    // Received messages.
    FileOfferReceived,
    FileAcceptReceived,
    FileRejectReceived,
    ChunkReceived,
    ChunkAckReceived,
    PauseReceived,
    ResumeReceived,
    CancelReceived,
    ResumeRequestReceived,
    ResumeRequestFileChanged,
    ResumeAcceptReceived,
    ResumeRejectReceived,
    CompleteReceived,
    ErrorReceived,

    // Internal / timeout.
    LastChunkAckReceived,
    LastChunkWritten,
    OfferTimeout,
    ResumeTimeout,
    SessionClosing,
};

struct TransferEvent
{
    TransferEventType type;
    SessionCloseReason close_reason = SessionCloseReason::UserClosed;
    std::uint64_t chunk_offset = 0;                        // ResumeRequestReceived
    std::uint64_t chunk_number = 0;                        // ChunkAckReceived
    ReasonCode error_code = ReasonCode::ProtocolViolation; // ErrorReceived
};

enum class TransferEffectType
{
    SendFileOffer,
    SendFileAccept,
    SendFileReject,
    SendChunk,
    SendChunkAck,
    SendPause,
    SendResume,
    SendCancel,
    SendResumeAccept,
    SendResumeReject,
    SendComplete,
    SendError,
    CloseSession,
    RenamePartToFinal,
    KeepPart,
    NotifyUser,
};

struct TransferEffect
{
    TransferEffectType type;
    std::optional<UserMessage> user_message; // NotifyUser
    std::optional<ReasonCode> reason_code;   // Send*Reject / SendError
};

std::pair<TransferState, std::vector<TransferEffect>> handle(TransferState state, TransferSide side, const TransferEvent& event);

} // namespace domain

#endif
