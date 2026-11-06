# Protocol Specification

## Session Model

A single session between two peers. Within a session, both sides can simultaneously act as sender and receiver. Each transfer is independent and has a direction (send / recv from the local peer's perspective).

At most one active transfer per direction per session (maximum 2 concurrent). The rest are queued. This allows sending and receiving at the same time.

## Framing

Every message (control or data) is carried in a single frame:

   `[u32 length][u8 type][u64 transfer_id][u8 nonce[24]][payload][tag]`

- length — total frame length after this field (little-endian).
- type — message type).
- transfer_id — 0 for session-level messages, `UUIDv7` for transfer-level.
- payload — depends on type.
- tag — AEAD tag (Poly1305, 16 bytes), covers `[type][transfer_id][payload]`.

## Message Types

| Name | Direction | Payload |
|---|---|---|
| FILE_OFFER | sender→receiver | transfer_id, file name, total chunks count |
| FILE_ACCEPT | receiver→sender | — |
| FILE_REJECT | receiver→sender | reason |
| CHUNK | sender→receiver | transfer_id, data |
| CHUNK_ACK | receiver→sender | transfer_id, chunk number |
| PAUSE | both | transfer_id, reason |
| RESUME | both | transfer_id |
| CANCEL | both | transfer_id, reason |
| RESUME_REQUEST | receiver→sender | transfer_id, chunk_offset |
| RESUME_ACCEPT | sender→receiver | transfer_id |
| RESUME_REJECT | sender→receiver | transfer_id, reason |
| COMPLETE | sender→receiver | transfer_id |
| ERROR | both | message |
| PING | both | — |
| PONG | both | — |

RELAY_PING / RELAY_PONG are a separate protocol on the TLS hop, not part of session framing.

## Resumable Transfers

Each transfer checkpoint (stored in the database) is identified by `transfer_id`:

   ```checkpoint(transfer_id, chunk_offset)```

`chunk_offset` is the number of the next chunk the receiver expects. It is the highest contiguous chunk durably written to disk, plus one.

**Resume flow.**

1. On session establishment (or reconnect), the receiver checks its database for transfers with a valid checkpoint for this peer and sends `RESUME_REQUEST{transfer_id, chunk_offset}`.
2. The sender loads its state for `transfer_id`. If missing or the source file has changed → `RESUME_REJECT{reason}`.
3. Otherwise the sender seeks to `chunk_offset * CHUNK_SIZE` and replies `RESUME_ACCEPT{transfer_id}`, then resumes sending `CHUNK` messages from that offset.
4. The receiver writes incoming chunks starting at `chunk_offset` and updates the checkpoint.

**Checkpoint write policy.** The receiver updates the in-memory `chunk_offset` after every successfully written chunk. The database row is persisted:

- after every 32 chunks, or
- after every 5 seconds, or
- on `PAUSE`, `CANCEL`, `COMPLETE`, and clean session close.

On crash, at most 32 chunks (32 MiB at 1 MiB chunk size) are re-sent. The checkpoint is never written before the corresponding chunk is durably written to disk, so `chunk_offset` always points to a chunk boundary that is safe to resume from.

**File integrity.** The final file hash is verified by the receiver after the last chunk (before renaming `.part`). The hash algorithm is negotiated in `HELLO`. On mismatch, the transfer is marked `FAILED` and the `.part` file is retained for inspection.



## Rules

1. One active transfer per direction. `A→B` and `B→A` may run concurrently.
2. `CHUNK_ACK` is sent after the chunk is written to disk, not after recv().
3. `transfer_id = 0` only for session-level messages (`HELLO`, `HELLO_ACK`, `PING`, `PONG`, `ERROR` without a transfer).
7. `RESUME_REQUEST` is only valid for an existing checkpoint. No checkpoint → `RESUME_REJECT`.
8. Unknown type → `ERROR{PROTOCOL_VIOLATION}`, session is closed.


## File Transfer Protocol — State Machines

One FSM for every `tranfer_id`. States and transitions are the same for sender and receiver; the difference in messenges being sent by sender or receiver.

## States

| State | Meaning |
|---|---|
| `IDLE` | Transfer created, `FILE_OFFER` not yet sent. |
| `OFFERED` | `FILE_OFFER` sent (S) or received (R). Awaiting `FILE_ACCEPT` / `FILE_REJECT`. |
| `ACTIVE` | Transfer in progress: `CHUNK` / `CHUNK_ACK`. |
| `PAUSED` | Transfer paused (locally or by `PAUSE` from the other side). |
| `RESUME_PENDING` | `RESUME_REQUEST` sent (R), awaiting `RESUME_ACCEPT` / `RESUME_REJECT`. |
| `DONE` | Last chunk processed, file complete. |
| `REJECTED` | Receiver rejected `FILE_OFFER` or `RESUME_REQUEST`. |
| `CANCELLED` | Transfer cancelled by either side. |
| `FAILED` | Error (timeout, disk error, session disconnect). |

## Transitions

| From | Event | To | Side | Action |
|---|---|---|---|---|
| `IDLE` | user adds file | `OFFERED` | S | send `FILE_OFFER` |
| `IDLE` | recv `FILE_OFFER` | `OFFERED` | R | validate path / disk |
| `IDLE` | recv `RESUME_REQUEST` | `ACTIVE` | S | send `RESUME_ACCEPT`, resume from `chunk_offset` |
| `IDLE` | recv `RESUME_REQUEST`, file changed / not found | `REJECTED` | S | send `RESUME_REJECT{reason}` |
| `OFFERED` | recv `FILE_ACCEPT` | `ACTIVE` | S | start sending `CHUNK` |
| `OFFERED` | recv `FILE_REJECT` | `REJECTED` | S | notify user |
| `OFFERED` | accept | `ACTIVE` | R | send `FILE_ACCEPT`, open `.part` |
| `OFFERED` | reject | `REJECTED` | R | send `FILE_REJECT{reason}` |
| `OFFERED` | timeout | `FAILED` | both | — |
| `ACTIVE` | recv `CHUNK` | `ACTIVE` | R | write to disk, send `CHUNK_ACK` |
| `ACTIVE` | recv `CHUNK_ACK` | `ACTIVE` | S | update progress |
| `ACTIVE` | recv `PAUSE` | `PAUSED` | both | stop sending / writing |
| `ACTIVE` | user pause | `PAUSED` | both | send `PAUSE` |
| `ACTIVE` | recv `CANCEL` | `CANCELLED` | both | R: keep `.part` + checkpoint |
| `ACTIVE` | user cancel | `CANCELLED` | both | send `CANCEL`, R: keep `.part` + checkpoint |
| `ACTIVE` | last chunk processed | `DONE` | both | S: send `COMPLETE`; rename `.part` |
| `ACTIVE` | error / session closing | `FAILED` | both | R: keep `.part` + checkpoint |
| `PAUSED` | recv `RESUME` | `ACTIVE` | both | continue sending / writing |
| `PAUSED` | user resume | `ACTIVE` | both | send `RESUME` |
| `PAUSED` | recv `CANCEL` | `CANCELLED` | both | R: keep `.part` + checkpoint |
| `PAUSED` | user cancel | `CANCELLED` | both | send `CANCEL`, R: keep `.part` + checkpoint |
| `PAUSED` | session closing | `FAILED` | both | — |
| `RESUME_PENDING` | recv `RESUME_ACCEPT` | `ACTIVE` | R | seek to `chunk_offset`, wait for `CHUNK` |
| `RESUME_PENDING` | recv `RESUME_REJECT` | `REJECTED` | R | notify user |
| `RESUME_PENDING` | timeout | `FAILED` | R | — |
| `PAUSED` | recv `CANCEL` | `CANCELLED` | R | keep `.part` + checkpoint |
| `PAUSED` | user cancel | `CANCELLED` | both | send `CANCEL`, keep `.part` + checkpoint |

**Terminal States**: DONE, REJECTED, CANCELLED, FAILED

## Special Messages

| Message | Handling |
|---|---|
| `PING` / `PONG` | Session-level liveness; not part of transfer state machine. Ignored by transfer FSM. |
| `ERROR` | Received in any state → transfer transitions to `FAILED`; if `code = PROTOCOL_VIOLATION`, session closes. |
| `CANCEL` | Accepted in any non-terminal state → `CANCELLED`. |
| `PAUSE` | Only valid in `ACTIVE`; in any other state → `ERROR{PROTOCOL_VIOLATION}`. |
| `RESUME` | Only valid in `PAUSED`; in any other state → `ERROR{PROTOCOL_VIOLATION}`. |
| `CHUNK` | Only valid in `ACTIVE` (receiver side); otherwise → `ERROR{PROTOCOL_VIOLATION}`. |
| `CHUNK_ACK` | Only valid in `ACTIVE` (sender side); otherwise → `ERROR{PROTOCOL_VIOLATION}`. |
| `FILE_ACCEPT` / `FILE_REJECT` | Only valid in `OFFERED` (sender side); otherwise → `ERROR{PROTOCOL_VIOLATION}`. |
| `RESUME_ACCEPT` / `RESUME_REJECT` | Only valid in `RESUME_PENDING` (receiver side); otherwise → `ERROR{PROTOCOL_VIOLATION}`. |
| `RESUME_REQUEST` | Valid in `IDLE` (sender side, from checkpoint); otherwise → `ERROR{PROTOCOL_VIOLATION}`. |


# Session State Machine

## Scope

The Session FSM describes the session lifecycle from "no connection" to "closed". It is transport-agnostic: LAN (TCP) and WAN (Relay) use the same Session FSM, because `ITransport` abstracts the byte stream.


## States

| State | Meaning |
|---|---|
| `IDLE` | No session, transport not established. |
| `HANDSHAKING` | Transport ready, Noise handshake in progress (+ user confirmation). |
| `ESTABLISHED` | Handshake complete |
| `CLOSING` | Close initiated (user, `ERROR`, PING timeout, transport error). |
| `CLOSED` | Session closed, resources released. |

## Transitions

| From | Event | To | Action |
|---|---|---|---|
| `IDLE` | user initiates connect / incoming request accepted | `HANDSHAKING` | start Noise handshake |
| `HANDSHAKING` | handshake OK + user confirmed | `ESTABLISHED` | start liveness timer, accept transfers |
| `HANDSHAKING` | handshake failed / user rejected / timeout | `CLOSING` | send `ERROR{AUTH_FAILED}` if possible |
| `ESTABLISHED` | recv `PING` | `ESTABLISHED` | send `PONG` |
| `ESTABLISHED` | recv `PONG` | `ESTABLISHED` | reset liveness timer |
| `ESTABLISHED` | PING timeout | `CLOSING` | close transport |
| `ESTABLISHED` | recv `ERROR{PROTOCOL_VIOLATION}` | `CLOSING` | close transport |
| `ESTABLISHED` | user closes session | `CLOSING` | send `ERROR{USER_CLOSED}` if possible |
| `ESTABLISHED` | transport error (`CONNECTION_LOST`) | `CLOSING` | — |
| `CLOSING` | transport closed | `CLOSED` | notify  active transfer → `FAILED` |
| `CLOSED` | — | — | terminal |


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

