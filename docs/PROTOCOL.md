# Protocol Specification

## Session Model

A single session between two peers. Within a session, both sides can simultaneously act as sender and receiver. Each transfer is independent and has a direction (send / recv from the local peer's perspective).

At most one active transfer per direction per session (maximum 2 concurrent). The rest are queued. This allows sending and receiving at the same time.

## Framing

Every message (control or data) is carried in a single frame:

   `[u8 version][u32 length][u8 type][u64 transfer_id][u8 nonce[24]][payload][tag]`

- version — protocol version (currently `1`). Frames with an unknown
  version are rejected with `ERROR{PROTOCOL_VIOLATION}` and the session
  is closed.
- length — total frame length after this field (little-endian).
- type — message type.
- transfer_id — 0 for session-level messages, `UUIDv7` for transfer-level.
- payload — depends on type.
- tag — AEAD tag (Poly1305, 16 bytes), covers
  `[version][length][type][transfer_id][payload]`.

The version byte is not needed for v1 behaviour, but it is reserved now
so that future optimizations can be negotiated without breaking older peers.

## Message Types

The table below is the single source of truth for all message types used
by any FSM in this document. `SessionFsm` uses session-level messages
(`transfer_id = 0`); `TransferFsm` uses transfer-level messages
(`transfer_id != 0`); `RelayClientFsm` uses
the relay control messages on the TLS hop, which are outside this frame
format.

| Name | Level | Direction | Payload | Used by |
|---|---|---|---|---|
| `PING` | session | both | — | `SessionFsm` |
| `PONG` | session | both | — | `SessionFsm` |
| `ERROR` | session / transfer | both | code, message | `SessionFsm`, `TransferFsm` |
| `FILE_OFFER` | transfer | S→R | file name, total chunks | `TransferFsm` |
| `FILE_ACCEPT` | transfer | R→S | — | `TransferFsm` |
| `FILE_REJECT` | transfer | R→S | reason | `TransferFsm` |
| `CHUNK` | transfer | S→R | data | `TransferFsm` |
| `CHUNK_ACK` | transfer | R→S | chunk number | `TransferFsm` |
| `PAUSE` | transfer | both | reason | `TransferFsm` |
| `RESUME` | transfer | both | — | `TransferFsm` |
| `CANCEL` | transfer | both | reason | `TransferFsm` |
| `RESUME_REQUEST` | transfer | R→S | file size, chunk_offset | `TransferFsm` |
| `RESUME_ACCEPT` | transfer | S→R | — | `TransferFsm` |
| `RESUME_REJECT` | transfer | S→R | reason | `TransferFsm` |
| `COMPLETE` | transfer | S→R | — | `TransferFsm` |

`RELAY_PING` / `RELAY_PONG` are relay control messages on the TLS hop,
not part of this frame format. They are listed in the Relay State
Machine.

`transfer_id = 0` is used for session-level messages (`PING`, `PONG`, and `ERROR` without a transfer). Any other
message must carry a non-zero `transfer_id`.

## Resumable Transfers

Each transfer checkpoint (stored in the database) is identified by
`transfer_id`:

   ```checkpoint(transfer_id, file_size, chunk_offset)```

`chunk_offset` is the number of the next chunk the receiver expects. It
is the highest contiguous chunk durably written to disk, plus one.

**Resume flow.**

1. On session establishment (or reconnect), the receiver checks its
   database for transfers with a valid checkpoint for this peer and
   sends `RESUME_REQUEST{transfer_id, chunk_offset}`.
2. The sender loads its state for `transfer_id`. If missing, or the
   source file has changed (size), or the source file does not
   exist → `RESUME_REJECT{reason}`. The sender does not attempt to
   detect file changes by hashing: size checking are sufficient for
   v1, and a full-file hash on multi-GiB files would cost minutes.
3. Otherwise the sender seeks to `chunk_offset * CHUNK_SIZE` and
   replies `RESUME_ACCEPT{transfer_id}`, then resumes sending `CHUNK`
   messages from that offset.
4. The receiver writes incoming chunks starting at `chunk_offset` and
   updates the checkpoint.

**Checkpoint write policy.** The receiver updates the in-memory
`chunk_offset` after every successfully written chunk. The database row
is persisted:

- after every 32 chunks, or
- after every 5 seconds, or
- on `PAUSE`, `CANCEL`, `COMPLETE`, and clean session close.

The checkpoint is never written before the corresponding chunk is
durably written to disk, so `chunk_offset` always points to a chunk
boundary that is safe to resume from.

On graceful shutdown (user closes the window), the current in-memory
checkpoint is flushed to the database before the database is closed, so
the next start resumes from the exact position. On a crash (power loss,
process kill), the last persisted checkpoint is used; 

**File integrity.** Each chunk is authenticated by the Noise AEAD; the
receiver verifies the tag before writing. The FSM enforces sequential
chunk numbers: a `CHUNK` whose `chunk_number` is not the next expected
one is `ERROR{PROTOCOL_VIOLATION}`.


## File Transfer Protocol — State Machines

One FSM per `transfer_id`. States and transitions are the same for sender and
receiver; only the messages sent differ. `TransferFsm` is a pure function:
`handle(state, event) -> (new_state, effects)`. Session closes arrive as
`SessionClosing{reason}` (see Session State Machine) and map here to
`CANCELLED` (`UserClosed`) or `FAILED` (any other reason).

## States

| State | Meaning |
|---|---|
| `IDLE` | Created; `FILE_OFFER` not yet sent (S) / `RESUME_REQUEST` not yet received (R). |
| `OFFERED` | `FILE_OFFER` sent (S) or received (R); awaiting accept/reject. |
| `ACTIVE` | Transfer in progress: `CHUNK` / `CHUNK_ACK`. |
| `PAUSED` | Paused locally or by `PAUSE`. |
| `RESUME_PENDING` | `RESUME_REQUEST` sent (R); awaiting accept/reject. |
| `DONE` | Last chunk processed, file complete. |
| `REJECTED` | Receiver rejected `FILE_OFFER`, or sender rejected `RESUME_REQUEST`. |
| `CANCELLED` | Cancelled by either side, or by `SessionClosing{UserClosed}`. |
| `FAILED` | Error: timeout, disk error, `PROTOCOL_VIOLATION`, or `SessionClosing{other}`. |

Terminal: `DONE`, `REJECTED`, `CANCELLED`, `FAILED`. A retry creates a new `transfer_id`.

## Transitions

| State | Event | To | Side | Action |
|---|---|---|---|---|
| `IDLE` | user adds file | `OFFERED` | S | send `FILE_OFFER` |
| `IDLE` | recv `FILE_OFFER{chunks_number}` | `OFFERED` | R | validate path / disk |
| `IDLE` | recv `RESUME_REQUEST` | `ACTIVE` | S | send `RESUME_ACCEPT`, resume from `chunk_offset` |
| `IDLE` | recv `RESUME_REQUEST`, transfer/file missing or changed | `REJECTED` | S | send `RESUME_REJECT{reason}` |
| `OFFERED` | recv `FILE_ACCEPT` | `ACTIVE` | S | start sending `CHUNK` |
| `OFFERED` | recv `FILE_REJECT` | `REJECTED` | S | notify user |
| `OFFERED` | accept | `ACTIVE` | R | send `FILE_ACCEPT`, open `.part` |
| `OFFERED` | reject | `REJECTED` | R | send `FILE_REJECT{reason}` |
| `OFFERED` | timeout (default 30 s) | `FAILED` | both | — |
| `ACTIVE` | recv `CHUNK` | `ACTIVE` | R | write to disk, send `CHUNK_ACK` |
| `ACTIVE` | recv `CHUNK_ACK` | `ACTIVE` | S | update progress |
| `ACTIVE` | recv `PAUSE` / user pause | `PAUSED` | both | stop; on user pause also send `PAUSE` |
| `ACTIVE` | recv `CANCEL` / user cancel | `CANCELLED` | both | on user cancel also send `CANCEL`; R: keep `.part` + checkpoint |
| `ACTIVE` | last `CHUNK_ACK` received | `DONE` | S | send `COMPLETE` |
| `ACTIVE` | last `CHUNK` written | `DONE` | R | rename `.part` |
| `ACTIVE` | recv `ERROR` | `FAILED` | both | if `PROTOCOL_VIOLATION`, also emit `CloseSession`; R: keep `.part` + checkpoint |
| `ACTIVE` | recv `SessionClosing{UserClosed}` | `CANCELLED` | both | R: keep `.part` + checkpoint |
| `ACTIVE` | recv `SessionClosing{other}` | `FAILED` | both | R: keep `.part` + checkpoint |
| `PAUSED` | recv `RESUME` / user resume | `ACTIVE` | both | on user resume also send `RESUME` |
| `PAUSED` | recv `CANCEL` / user cancel | `CANCELLED` | both | on user cancel also send `CANCEL`; R: keep `.part` + checkpoint |
| `PAUSED` | recv `SessionClosing{UserClosed}` | `CANCELLED` | both | R: keep `.part` + checkpoint |
| `PAUSED` | recv `SessionClosing{other}` | `FAILED` | both | R: keep `.part` + checkpoint |
| `RESUME_PENDING` | recv `RESUME_ACCEPT` | `ACTIVE` | R | seek to `chunk_offset`, wait for `CHUNK` |
| `RESUME_PENDING` | recv `RESUME_REJECT` | `REJECTED` | R | notify user |
| `RESUME_PENDING` | timeout (default 30 s) | `FAILED` | R | — |
| `RESUME_PENDING` | recv `SessionClosing{UserClosed}` | `CANCELLED` | R | — |
| `RESUME_PENDING` | recv `SessionClosing{other}` | `FAILED` | R | — |

## Special messages

| Message | Handling |
|---|---|
| `PING` / `PONG` | Session-level; ignored by transfer FSM. |
| `ERROR` | Any non-terminal → `FAILED`. `PROTOCOL_VIOLATION` also emits `CloseSession`. |
| `CANCEL` | Any non-terminal → `CANCELLED`. Ignored in terminal states. |
| `PAUSE` | Only in `ACTIVE`; otherwise → `ERROR{PROTOCOL_VIOLATION}`. |
| `RESUME` | Only in `PAUSED`; otherwise → `ERROR{PROTOCOL_VIOLATION}`. |
| `CHUNK` | Only in `ACTIVE` (R); otherwise → `ERROR{PROTOCOL_VIOLATION}`. |
| `CHUNK_ACK` | Only in `ACTIVE` (S); otherwise → `ERROR{PROTOCOL_VIOLATION}`. |
| `FILE_ACCEPT` / `FILE_REJECT` | Only in `OFFERED` (S); otherwise → `ERROR{PROTOCOL_VIOLATION}`. |
| `RESUME_ACCEPT` / `RESUME_REJECT` | Only in `RESUME_PENDING` (R); otherwise → `ERROR{PROTOCOL_VIOLATION}`. |
| `RESUME_REQUEST` | Only in `IDLE` (S); otherwise → `ERROR{PROTOCOL_VIOLATION}`. |
| `COMPLETE` | Only in `ACTIVE` (R); informational (R is already `DONE`). Ignored elsewhere. |

## In-flight messages

After leaving `ACTIVE`, late `CHUNK` / `CHUNK_ACK` are ignored: no `CHUNK_ACK` is
sent, progress is not updated. Only events valid in the current state change it.


# Session State Machine

## Scope

The Session FSM describes the session lifecycle from "no connection" to
"closed". It is transport-agnostic: LAN (TCP) and WAN (Relay) use the same
Session FSM, because `ITransport` abstracts the byte stream.

## States

| State | Meaning |
|---|---|
| `IDLE` | No session, transport not established. |
| `HANDSHAKING` | Transport ready, Noise handshake in progress (+ user confirmation). |
| `ESTABLISHED` | Handshake complete |
| `CLOSING` | Close initiated (user, `ERROR`, PING timeout, transport error). |
| `CLOSED` | Session closed, resources released. |

## Close reasons

`SessionCloseReason` is carried in the `SessionClosing` effect:

| Reason | Meaning | `TransferFsm` reaction |
|---|---|---|
| `UserClosed` | User pressed "Close session". | `CANCELLED` |
| `PingTimeout` | Liveness timer expired; peer is unreachable. | `FAILED` |
| `ProtocolViolation` | Peer sent an invalid frame or event. | `FAILED` |
| `TransportError` | Transport reported `CONNECTION_LOST`. | `FAILED` |
| `HandshakeFailed` | Handshake failed, user rejected, or handshake timed out. | `FAILED` (no active transfers exist in `HANDSHAKING`) |

## Transitions

| From | Event | To | Action |
|---|---|---|---|
| `IDLE` | user initiates connect / incoming request accepted | `HANDSHAKING` | start Noise handshake |
| `HANDSHAKING` | handshake OK + user confirmed | `ESTABLISHED` | start liveness timer, accept transfers |
| `HANDSHAKING` | handshake failed / user rejected / timeout | `CLOSING` | emit `SessionClosing{HandshakeFailed}` |
| `ESTABLISHED` | recv `PING` | `ESTABLISHED` | send `PONG` |
| `ESTABLISHED` | recv `PONG` | `ESTABLISHED` | reset liveness timer |
| `ESTABLISHED` | PING timeout | `CLOSING` | close transport; emit `SessionClosing{PingTimeout}` |
| `ESTABLISHED` | recv `ERROR{PROTOCOL_VIOLATION}` | `CLOSING` | close transport; emit `SessionClosing{ProtocolViolation}` |
| `ESTABLISHED` | user closes session | `CLOSING` | emit `SessionClosing{UserClosed}` |
| `ESTABLISHED` | transport error (`CONNECTION_LOST`) | `CLOSING` | emit `SessionClosing{TransportError}` |
| `CLOSING` | transport closed | `CLOSED` | — |
| `CLOSED` | — | — | terminal |

## Effect ordering

Effects from a single transition are executed in the order returned:

1. `SendError{...}` (if any) — best-effort, transport may already be broken.
2. `CloseTransport` (if any).
3. `NotifyActiveTransfers{close_reason}` — always last, so `TransferFsm`
   sees the final reason after the transport is already being torn down.

# Relay State Machine

## Scope

The Relay Client FSM prepares the transport for WAN mode: TLS to the relay, registration, rendezvous, `PIPE_OPEN`. It **precedes** the Session FSM: once `PIPE_OPEN` is received, control is handed to the Session FSM (`IDLE → HANDSHAKING`).

The Relay Client FSM is **not** used in LAN mode. LAN mode uses only the Session FSM (TCP is ready immediately).

## States

| State | Meaning |
|---|---|
| `DISCONNECTED` | No connection to the relay. |
| `TLS_CONNECTING` | TLS to the relay is being established. |
| `REGISTERING` | `REGISTER` sent, awaiting confirmation. |
| `REGISTERED` | Registered, `RENDEZVOUS_REQUEST` may be sent. |
| `IN_SESSION` | `PIPE_OPEN` received, session in progress (control is with the Session FSM). |
| `CLOSING` | Closing (user, error, `RELAY_PING` timeout). |
| `CLOSED` | Connection closed. |

## Transitions

| From | Event | To | Action |
|---|---|---|---|
| `DISCONNECTED` | user enables relay / app start | `TLS_CONNECTING` | start TLS handshake |
| `TLS_CONNECTING` | TLS OK, fingerprint matches | `REGISTERING` | send `REGISTER` |
| `TLS_CONNECTING` | TLS fail / fingerprint mismatch | `CLOSING` | notify user |
| `REGISTERING` | recv `REGISTER_OK` | `REGISTERED` | start `RELAY_PING` timer |
| `REGISTERING` | recv `ERROR{ALREADY_REGISTERED}` | `CLOSING` | notify user |
| `REGISTERING` | timeout | `CLOSING` | notify user |
| `REGISTERED` | user sends `RENDEZVOUS_REQUEST` | `REGISTERED` | send request |
| `REGISTERED` | recv `PIPE_OPEN` | `IN_SESSION` | hand transport to Session FSM |
| `REGISTERED` | recv `RELAY_PING` | `REGISTERED` | send `RELAY_PONG` |
| `REGISTERED` | recv `ERROR{PEER_NOT_FOUND}` / `PEER_OFFLINE` / `PEER_BUSY` | `REGISTERED` | notify user, stay registered |
| `REGISTERED` | `RELAY_PING` timeout | `CLOSING` | close TLS |
| `IN_SESSION` | Session FSM → `CLOSED` | `REGISTERED` | return to registered state |
| `IN_SESSION` | `RELAY_PING` timeout | `CLOSING` | close TLS, notify Session FSM |
| `CLOSING` | TLS closed | `CLOSED` | — |
| `CLOSED` | user reconnects | `TLS_CONNECTING` | — |

