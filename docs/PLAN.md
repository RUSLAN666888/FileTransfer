# Roadmap

High-level milestones. Each milestone is a working, testable result — not a task list. 

---

## Milestone 0 — Skeleton & CI

**Goal:** empty project builds and tests pass on Linux and Windows.

- CMake targets: 5 libraries (`infra`, `transport`, `protocol`, `domain`, `app`) + 1 executable, links downward only.
- One dummy GoogleTest per layer, as separate executable targets under `libs/<layer>/tests/`.
- GitHub Actions: build + `ctest` on `{ubuntu-latest, windows-latest}`.
- `.clang-format` + format check in CI.

**Done:** CI green on both OS; `cmake --build` works locally on both.

---

## Milestone 1 — infra

**Goal:** all external-world wrappers exist and are unit-tested.

- Crypto primitives (keypair, sign, verify, hash, AEAD, random bytes) verified against known test vectors.
- File system operations incl. safe open (no symlink following).
- Clock abstraction (testable timers).
- Keystore: keypair persisted on first launch; reload returns the same key.
- Logging: console and in-memory implementations. Database logging is deferred until the schema exists (see Milestone 2.5).

**Done:** unit tests for crypto vectors, file ops, keystore round-trip.

---

## Milestone 2 — domain (pure FSMs)

**Goal:** all three state machines implemented as pure functions, fully covered by table-driven tests.

- Peer identity: Ed25519 keypair generated on first launch, stored in keystore, derived PeerId (base32, 26 chars); reload yields the same PeerId.
- Transfer identifiers (UUIDv7) and checkpoint offsets.
- Session, transfer, and relay-client state machines as pure functions; every transition row from `protocol.md` (TransferFsm), `session.md` (SessionFsm), `relay.md` (RelayClientFsm) has a test.
- `PROTOCOL_VIOLATION` cases covered.

**Done:** every transition from the three specs has a test; identity bootstrap test passes (generate → store → reload → same PeerId).

---

## Milestone 2.5 — storage

**Goal:** schema and repositories work against `:memory:` SQLite.

- `docs/storage.md`: tables (`peers`, `transfers`, `checkpoints`, `blocklist`, `logs`, `history`), indexes, migration policy.
- CRUD for peers, transfers, checkpoints, blocklist, history, logs — all against `:memory:` SQLite.
- Database logging: writes to and reads back from the logs table.

**Done:** all repository CRUD tests pass; database logging writes and reads back a log entry.

---

## Milestone 3 — protocol

**Goal:** frames and secure channel work end-to-end in-process.

- Frame encoding/decoding for all message types; round-trip and fuzz tests.
- Discovery packet structure and verification (signature, PeerId derivation, timestamp, sequence).
- Secure channel: Noise XX handshake over an abstract byte stream.
- In-process handshake test: two secure channels establish and exchange an encrypted frame.

**Done:** round-trip tests pass; two in-process secure channels complete handshake and exchange data.

---

## Milestone 4 — LAN end-to-end

**Goal:** two processes on loopback discover each other, establish a secure session, and transfer a single file.

- LAN discovery: peers on the same host find each other via UDP broadcast.
- Session: Noise handshake completes; peer identity verified.
- Single file transfer: no queue, no pause/resume; hash matches on the receiver.
- `scripts/demo-lan.sh` reproduces the transfer.

**Done:** integration test — two processes, 10 MiB file, hash matches.

---

## Milestone 5 — Full transfer semantics

**Goal:** all transfer semantics from the spec work.

- Multiple files: queued per direction (max 1 active per direction); priority up/down reorders the queue.
- Pause/resume: either side can pause and resume; progress freezes and continues from the same offset.
- Cancel: retains `.part` and checkpoint by default; user can delete partial file locally.
- Crash recovery: kill process mid-transfer, restart, resume to completion; hash matches.
- Progress is based on bytes durably written by the receiver, not bytes sent.

**Done:** integration tests for pause/resume and crash-recovery; both hash-match.

---

## Milestone 6 — Relay

**Goal:** file transfer through the relay works end-to-end. Design and implement relay server

- Relay connection: peer connects to relay over TLS, verifies server fingerprint, registers with its PeerId.
- Liveness: dead connections release their PeerId within the configured timeout.
- Rendezvous: two registered peers connect through the relay; payload is end-to-end encrypted (relay sees only opaque bytes).
- File transfer through relay works end-to-end.
- Integration test with an in-process mock relay; the mock defines the wire contract.

**Done:** two clients register, rendezvous, transfer a file; `ALREADY_REGISTERED`, `PEER_OFFLINE`, and PING timeout are handled.


---

## Milestone 7 — GUI

**Goal:** end-to-end flow through Qt6.

- Peer list (discovered, manual, history), connect, block, fingerprint/details.
- Session window: drag-and-drop + file picker, queue view with progress/speed/ETA, pause/resume/cancel, priority.
- Settings, active peers, history (with Reveal in file manager), blocklist management, identity panel, network diagnostics.
- System tray notifications for incoming session, completion, failure.


---

## Milestone 8 — Documentation polish

**Goal:** separated, reviewable documentation.

- Split the working spec into `docs/requirements.md` ("what") and `docs/design.md` ("how").
- Update `architecture.md` with anything that changed during implementation.

**Done:** docs are internally consistent; no duplicated source of truth; `architecture.md` reflects the code as shipped.

---

## Cross-cutting (applied throughout, not a milestone)

- CI stays green on Linux + Windows at every commit.
- New interface appears only when a substitution scenario exists.
- Architecture changes are recorded in `architecture.md`, not silently in code.
