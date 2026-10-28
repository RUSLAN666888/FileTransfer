Technical Specification: Secure P2P File Transfer Application

**Target stack:** C++20, CMake, SQLite, libsodium, Qt6, GoogleTest, boost.asio

---

## Overview

A cross-platform peer-to-peer file transfer application supporting multiple transport channels (TCP over LAN, relayed transfer over the Internet) with end-to-end encryption, peer authentication, resumable transfers.

### Scope

In scope for v1:

- One-to-one transfers between two peers (over LAN).
- LAN discovery via UDP broadcast.
- Relayed Internet transfer (relay-only; no NAT traversal).
- Persistent queue, resumable transfers, blocklist, history.


---

## Functional Requirements

### Peer Discovery

#### LAN Discovery

The application shall broadcast UDP discovery packets containing peer metadata and simultaneously listen for incoming discovery packets from other peers. Discovery operates on a configurable UDP port (default `45892`).

Broadcast is limited to the local subnet. Internet discovery in v1 is provided exclusively by the relay rendezvous protocol: the user supplies the peer's `peer_id` out-of-band (manually, via invite link), and the relay connects the two peers if both are online.

Discovery packet payload:

- Peer ID (`16 raw bytes`)
- Public key (`32 raw bytes`, Ed25519) — required for signature verification, since a hash cannot be inverted
- Listening TCP port (`u16`)
- Timestamp (`u64`, unix seconds) — packets older than 60 s (configurable) are dropped
- Sequence number (`u64`, monotonic per peer) — for deduplication and replay protection within the freshness window
- Ed25519 signature over the payload (excluding the signature field itself)

Receivers must verify `SHA-256(public_key)` truncated to 128 bits equals `peer_id`, and that the Ed25519 signature verifies against `public_key`. A mismatch in either check → the packet is rejected.

#### Peer Identity

Peer identity must be cryptographically bound to the host to prevent spoofing and impersonation (a prerequisite for DoS mitigation).

- On first launch, the application generates an **Ed25519 keypair**.
- The private key is stored in an OS-protected keystore where available (DPAPI on Windows, `libsecret`/`kwallet` or an encrypted file with a user-supplied passphrase on Linux). The file MUST be created with `0600` permissions.
- The **Peer ID** is derived as `SHA-256(public_key)` truncated to 128 bits. This provides a stable, self-certifying identifier that cannot be forged without the private key.

**Wire format.** On the wire (discovery packets, `REGISTER`, handshake), the Peer ID is transmitted as **16 raw bytes**. The public key (32 bytes) is transmitted alongside it — a hash cannot be inverted, so the key must be supplied explicitly.

**Display and storage format.** For SQLite, GUI, and logs, the Peer ID is encoded as a **base32 string without padding, uppercase, 26 characters** (RFC 4648 §6).

**Verification.** A receiver of any message containing `peer_id` and `public_key` must verify:

1. `SHA-256(public_key)` truncated to 128 bits equals `peer_id`.
2. The Ed25519 signature over the message payload verifies against `public_key`.

A mismatch in either check → the message is rejected.

**Blocklist.** Blocked peer IDs are stored in SQLite (base32 string form) and rejected at discovery, handshake, and transfer layers. The blocklist is keyed by Peer ID, not by public key; a blocked peer that regenerates its key gets a new Peer ID and must be re-blocked.


#### DoS Mitigation at Discovery Layer

- Incoming LAN discovery packets from unknown peers are rate-limited per source IP (token bucket): **60 packets/minute per source IP** (configurable).
- Signature verification is performed only after a cheap structural validation pass (length, version, field presence) to avoid CPU exhaustion.
- Connection requests are subject to a per-peer handshake limit (default 3 pending per minute), tracked by peer ID and source address.
- Discovery packets carry a monotonic **sequence number** per sender. The receiving side must drop packets whose sequence number is less than or equal to the last seen value from the same `peer_id` (replay and deduplication), and must drop packets older than the freshness window (default 60 s, based on the `timestamp` field).

---

### Connection Establishment

#### Session Handshake

Peer-to-peer connection establishment must follow a mutually authenticated handshake specified in Noise protocol (`XX` pattern). 

Mutual user confirmation — the application must require explicit user confirmation on the responder side before the session is elevated to file-transfer mode. 

#### Blocklist

A peer that has been blocked by the user must not:

- be shown in the discovered peers list;
- initiate or receive a session;
- have its discovery packets accepted.

Blocklist entries persist in SQLite keyed by Peer ID.

---

### Transport Abstraction

The session layer must be agnostic to the underlying physical transport. Every physical channel — TCP, relay — is exposed through a uniform **byte-stream transport** contract. Adding a new physical medium requires only implementing this contract and registering it in the transport factory; no changes to the session, encryption, queue, or logging layers are needed.

**Transport contract.** Every transport must provide:

- **Read** — obtain bytes from the remote side. Blocking or asynchronous (implementation-defined); errors are surfaced explicitly.
- **Write** — send bytes to the remote side. May be partial; the caller is responsible for retrying the remainder.
- **Close** — release the transport. After close, further reads/writes return a connection-lost error.
- **Remote endpoint** — an opaque identifier of the peer (address, port, or relay-assigned ID), used for logging and diagnostics.

**Declared capabilities.** Each transport declares:

**Concrete transports (v1):**

- **TCP** — ordered, reliable, not lossy, unlimited MTU. Baseline LAN transport.
- **Relay** — ordered, reliable; MTU and latency are implementation-defined (the relay may impose limits).

**Boundaries.** Transport implementations must not know about sessions, encryption, or file semantics.
Providing reliable data transfer protocol is out of scope (tcp and other transport protocols are responsible for it). 
---


### Internet Transfer via Relay

#### Model

A relay server mediates the session without access to plaintext, following the rendezvous + end-to-end encryption pattern. The relay is a standalone service with its own specification document; this section defines only the client-side contract.

The relay is semi-trusted: it must see the control plane to route, but it must not see the payload. Compromise of the relay does not break E2EE.

Flow:

1. Both peers maintain a persistent TLS 1.3 connection to the relay. TLS protects the hop between each peer and the relay; it is not part of the end-to-end trust model.
2. After TLS is established, each peer authenticates to the relay at the application layer with an Ed25519 signature. The relay verifies the signature before registering the peer.
3. Peer A sends `RENDEZVOUS_REQUEST{peer_id_B, session_nonce}`; the relay queues it.
4. Peer B, upon receiving a matching `RENDEZVOUS_MATCH`, agrees to the session.
5. The relay sends `PIPE_OPEN` to both peers and forwards opaque byte frames between their sockets.
6. The Noise handshake runs end-to-end inside the relayed pipe, independent of the TLS layer. The relay cannot decrypt it.

### Client-Side Contract

**Transport.** TLS 1.3. The relay's TLS identity is not part of the end-to-end trust model — it only protects the hop. Only self-hosted relays with self-signed certificates are assumed. Peer must verify server's certificate's SHA256 fingerprint against its local stored one.

**Authentication.** Application-level, on top of TLS, using the peer's Ed25519 identity key. The peer MUST include its public key in the registration message, because a public key cannot be derived from a hash:

1. After TLS is established, the peer sends:

  `REGISTER{peer_id, public_key, nonce, signature}`

  where:
  - `peer_id` = `SHA-256(public_key)` truncated to 128 bits, base32-encoded;
  - `public_key` = the peer's Ed25519 public key (32 bytes);
  - `nonce` = 32 random bytes, fresh for every `REGISTER`;
  - `signature` = `Ed25519_sign(peer_id ‖ nonce)`.

2. The relay verifies, in order:

  a. **ID binding:** `SHA-256(public_key)` truncated to 128 bits equals `peer_id`. This binds the supplied key to the self-certifying ID. If not, reject.
  
  b. **Signature:** `Ed25519_verify(signature, peer_id ‖ nonce, public_key) == true`. This proves possession of the private key. If not, reject.
  
  c. **Nonce freshness:** `nonce` has not been seen within a configurable replay window (default 5 minutes). If it has, reject as replay.

3. Only after all three checks pass does the relay register the mapping `peer_id → TLS connection`.

**Registration uniqueness.** A `peer_id` MUST have at most one active registration at a time. If a `REGISTER` arrives for an already-registered `peer_id`, the relay MUST reject it with `ERROR{ALREADY_REGISTERED}`. The relay does not replace the existing registration. A peer that believes it has been disconnected MUST close its own connection first and retry registration with a fresh nonce; the relay considers the old registration dead only after a PING timeout (default 30 s).

**Liveness: RELAY_PING / RELAY_PONG.** The relay MUST detect dead connections and release their `peer_id` registration; otherwise a half-open TCP connection (after a network drop, laptop sleep, or process crash without TLS close) would permanently occupy the `peer_id` slot.

- The relay sends `RELAY_PING` to each registered peer every **15 seconds** (configurable).
- The peer MUST reply with `RELAY_PONG` immediately.
- If no `RELAY_PONG` is received within **30 seconds** (configurable, default = 2 × PING interval), the relay marks the connection dead, closes it, and removes the `peer_id → connection` mapping.
- A peer that misses PINGs due to a transient network issue MAY reconnect and re-register with a fresh nonce; the relay treats the new `REGISTER` as authoritative once the old registration is marked dead.

`RELAY_PING` and `RELAY_PONG` are relay control messages, not Noise frames. They are sent on the TLS hop and are visible to the relay. They carry no payload and no session data. They are distinct from the session-level `PING`/`PONG`, which run end-to-end inside the Noise session and are not visible to the relay.

The exact PING interval and timeout are implementation details to be tuned during testing; the architectural invariant is that **a dead connection MUST eventually release its `peer_id`**, and the relay MUST NOT rely on TCP keepalive alone for this.

**Accepted residual risk: registration DoS.** An attacker who replays a captured `REGISTER` after the nonce replay window (default 5 min) and before the legitimate peer's PING timeout can occupy the `peer_id` slot and cause a temporary denial of service. This is accepted as a residual risk for v1. It does **not** affect confidentiality or integrity: the attacker cannot complete a Noise handshake without the private key, so no session can be established. The legitimate peer recovers by reconnecting with exponential backoff after the PING timeout.

---

### Logging

#### Log Domains

Logs are partitioned into sections in the GUI:

- Errors — all failures and warnings.
- Connections — discovery events, handshake outcomes, per-peer info (Peer ID, endpoint, fingerprint).
- Transfer history.

#### Storage

All log entries must be stored in the **SQLite database**, not in plain-text log files. This enables efficient filtering, sorting, and pagination in the GUI without parsing text.

#### Requirements

- All errors are logged unconditionally to the database.
- Log view supports sorting by timestamp and filtering by filename (via SQL queries, indexed).
- Each history entry provides a Reveal in file manager action. If the file no longer exists, the application displays a appropriate warning.

---

### GUI

Qt6. Layout is at the designer's discretion within the following constraints:

- Startup view: Discovered peers panel with per-peer actions (Connect, Block, Show fingerprint/details).
- Session view (active): File transfer panel with:
 - Drag-and-drop area for adding files (also folder drop).
 - Manual file picker.
 - Queue view with per-transfer widget: progress bar, speed, ETA, up/down priority buttons, Pause/Resume/Cancel.
- Additional panels required for usability:
 - Settings: default download folder, discovery port, transport selection, relay URL, log level, keystore path, etc.
 - Active peers / sessions: currently connected peers with live throughput and state.
 - Transfer history: persistent table backed by SQLite, with Reveal-in-file-manager action.
 - Blocklist management: view, unblock, export/import.
 - Identity panel: display own Peer ID, fingerprint, and public key; regeneration option with explicit warning.
 - Network diagnostics: session status, relay connection state.

### User Experience (UX) Flow

This section defines the user-facing flow of the application. The UX is built around a **single active session** model: a peer supports exactly one active session at a time, and the GUI is organized around a single session window, not a list of parallel queues.

#### Sending Flow

1. The user opens the application and sees a peer list: discovered peers (LAN), manually added peers, and peers from history.
2. The user connects to selected peer and file transfer panel appears.
3. The user initiates a transfer by adding files (either via drag & drop or `Browse` button).
4. Files are added to the outgoing queue. The first transfer starts automatically; no "Start" button is required.
5. No intermediate confirmation dialog is shown for sending. The only confirmation in the entire flow is for **incoming** sessions.

**Sender controls** (per transfer):

- **Pause / Resume** — active transfer only.
- **Cancel** — any transfer (active or queued).
- **Priority up/down** — queued (inactive) transfers only.
- **Add files** — appends to the queue at any time.

#### Receiving Flow

**File reception.** Once the session is established, incoming files are accepted **automatically** — no per-file confirmation dialog. The receiver sees each file in the incoming queue as it arrives (sender sends `FILE_OFFER{transfer_id, file_name, file_size, }` before the first `CHUNK`). Files are written to `.part` files with atomic rename on completion.

**Per-file controls** (receiver, in the session widget):
- **Pause / Resume** — active transfer only.
- **Cancel** — any transfer; the `.part` file and checkpoint are retained by default, with a "Delete partial file" action available locally.

#### Pause and Resume Semantics

Pause and Resume are **symmetric**: either peer can initiate, and the other peer honors it via `PAUSE{transfer_id, reason}` / `RESUME{transfer_id}`.

- **Sender pauses** → receiver sees "Paused by sender"; progress freezes.
- **Receiver pauses** → sender sees "Paused by receiver"; progress freezes.
- **Resume** works the same way: either side can resume, and the other side continues.


#### Cancel Semantics

Cancel is **symmetric** and affects both peers:

- Sender cancels → `CANCEL{reason=USER_CANCELLED}` → receiver stops receiving; the `.part` file and checkpoint are **retained** by default, with a "Delete partial file" action available locally.
- Receiver cancels → `CANCEL{reason=USER_CANCELLED}` → sender stops sending; the receiver keeps the `.part` file and checkpoint for a potential resume.
- Cancel does **not** tear down the session; other transfers on the same session (if any are queued) continue.
- Close session (a separate action) tears down the entire session, including all transfers.

#### Progress Display

Progress is shown consistently on both sides:

- **Sender:** driven by `CHUNK_ACK{received_up_to}`. The progress bar shows `received_up_to / file_size` — bytes durably written by the receiver. The sender's own `bytes_sent` MAY be shown as a secondary indicator (e.g., "sent 420 MiB, confirmed 400 MiB"), but it MUST NOT be used as the primary progress.
- **Receiver:** driven by `bytes_received` from the checkpoint.
- **Update cadence:** every `CHUNK_ACK` (every N chunks or T ms). Between ACKs, the bar shows the last confirmed value; a "waiting for ACK" indicator appears if no ACK is received for more than 2× the ACK interval.
- **Speed and ETA:** computed from the delta of `received_up_to` over a sliding window (default 5 s), smoothed with an exponential moving average.
- **On resume:** the progress bar starts at the offset returned in `RESUME_ACCEPT{offset}` (sender) or `bytes_received` (receiver).

#### Transfer States

Each transfer widget displays a state badge:

| State | Badge | Actions |
|---|---|---|
| `receiving/sending` | Progress bar + speed + ETA | Pause, Cancel, priority up/down (if queued) |
| `paused` | "Paused by you" / "Paused by peer" | Resume, Cancel |
| `cancelled` | "Cancelled" (greyed out) | Delete partial file |
| `failed` | Red bar, error message | Retry, Delete partial file |
| `done` | Full bar, "Completed" | Reveal in file manager |

#### Session Window

Because a peer supports exactly one active session at a time, the GUI shows a single session window with:

- Peer ID, fingerprint (TOFU), connection state, transport (TCP / laser / relay).
- Outgoing queue (sender) or incoming queue (receiver).
- Per-transfer progress, speed, ETA, and controls.
- Session-level status: total bytes confirmed / written, session duration.
- **Close session** action, which tears down the entire session.

If a new session is requested while one is active (incoming `RENDEZVOUS_MATCH` or outgoing attempt), it is **rejected automatically** with `SESSION_BUSY`. No user prompt is shown. The requesting peer surfaces a human-readable message and MAY retry later with exponential backoff.

#### Notifications

- **Incoming session** → system tray notification + optional sound.
- **Transfer completion** → notification ("File sent to <peer>" / "File received from <peer>").
- **Transfer failure** → notification + red badge in the GUI.
- **Peer busy / offline** → informational message in the session window; no modal dialog.


# Threat Model

## 1. Scope

Two deployment modes with separate threat models:

| | LAN Mode | WAN Mode |
|---|---|---|
| Topology | Direct peer-to-peer | Peer ↔ Relay ↔ Peer |
| Relay | No | Yes (semi-trusted) |
| Network | Trusted LAN | Untrusted Internet |
| Encryption | Noise (E2EE) | TLS (hop) + Noise (E2EE) |
| Metadata visible to | LAN hosts | Relay |

Out of scope: host OS compromise, physical access, side-channels, global adversaries, traffic analysis.

## 2. LAN Mode

### Assets

- Payload (file contents).
- Peer identity.
- Availability of the direct channel.

### Trust boundaries

One: the LAN segment. Both endpoints are trusted. No intermediary.

### Adversaries

- Passive LAN observer (mirrored port, ARP spoofing).
- Compromised peer with a valid key.
- External attacker if the LAN is routed.

### STRIDE

| STRIDE | Threat | Mitigation | Residual |
|---|---|---|---|
| **S**poofing | Forged `peer_id` in discovery | Ed25519 signature; `SHA-256(pubkey) == peer_id` checked before trust | Private key compromise |
| **S**poofing | Impersonation in handshake | Noise `XX` mutual auth; transcript binds both static keys | Static key compromise |
| **T**ampering | Modified file content | Noise AEAD per frame | — |
| **T**ampering | Modified discovery packet | Signature covers full payload; sequence + timestamp | — |
| **R**epudiation | Peer denies a transfer | SQLite transfer history with timestamps | No cryptographic non-repudiation |
| **I**nformation disclosure | Passive LAN capture | Noise E2EE | Metadata (endpoints, timing) visible in LAN |
| **I**nformation disclosure | Discovery metadata in broadcast | Unavoidable for UDP broadcast; minimal fields only | **Accepted:** peer_id, port, presence visible to LAN |
| **I**nformation disclosure | Key exfiltration | OS keystore; `0600` file fallback | Host OS compromise |
| **D**enial of service | Discovery flood | Per-source-IP token bucket; structural validation before signature; sequence/timestamp dedup | LAN-level DoS outside app control |
| **D**enial of service | Handshake flood | Per-peer handshake limit (3 pending/min) | OS resource exhaustion |
| **E**levation of privilege | Path traversal via filename | Sanitization: strip directories, reject `..`, reject reserved names, canonical path check | — |
| **E**levation of privilege | Symlink overwrite | `O_NOFOLLOW` / `FILE_FLAG_OPEN_REPARSE_POINT` | — |

### Out of scope (LAN)

- Switch or cabling access.
- Host OS compromise.
- Side-channels.
- Traffic analysis.

**Compromised peer with a valid key.** A peer whose host is compromised (malware, stolen key) or a malicious participant. Noise mutual auth does not protect against it: the peer proves possession of its key, because it has the key.

In v1, the application does **not** attempt to defend against a compromised peer at the protocol level beyond:
- Path sanitization for incoming filenames — prevents writing outside the download folder.
- State machine validation — invalid transitions are rejected with `PROTOCOL_VIOLATION`.

## 3. WAN Mode

### Assets

- Payload.
- Peer identity.
- Metadata (who talks to whom, when, volume).
- Control-plane integrity (`REGISTER`, `RENDEZVOUS_REQUEST`, `PIPE_OPEN`).
- Relay availability.

### Trust boundaries

1. Peer ↔ relay — untrusted; TLS 1.3.
2. Relay — **semi-trusted**: sees control plane and metadata, must not see payload.
3. Relay ↔ peer — untrusted; TLS 1.3.

Compromise of the relay does not break E2EE.

### Adversaries

- Passive WAN observer.
- Active WAN MITM.
- Malicious or compromised relay.
- Compromised peer.
- DoS attacker.
- Unauthenticated relay abuser.

### STRIDE

| STRIDE | Threat | Mitigation | Residual |
|---|---|---|---|
| **S**poofing | Forged identity at relay registration | Ed25519 signature over `peer_id ‖ nonce`; relay checks `SHA-256(pubkey) == peer_id` | Private key compromise |
| **S**poofing | Relay impersonation | TLS 1.3 + SHA-256 fingerprint pinning (self-signed relay cert) | Leaked or misconfigured fingerprint |
| **S**poofing | Peer impersonation end-to-end | Noise `XX` mutual auth; relay sees only ciphertext | Static key compromise |
| **S**poofing | Key substitution for a known `peer_id` | Noise binds the channel to whatever key is presented | **Accepted:** substitution is not detected |
| **T**ampering | Modified control messages | TLS 1.3 AEAD; Ed25519 over `REGISTER` | Malicious relay may drop/reorder (detected by timeouts, not prevented) |
| **T**ampering | Modified payload | Noise AEAD per frame | — |
| **R**epudiation | Peer denies a transfer | SQLite history | No cryptographic non-repudiation; relay logs untrusted |
| **I**nformation disclosure | Relay reads payload | Noise E2EE; relay forwards opaque frames | — |
| **I**nformation disclosure | Metadata visible to relay | **Accepted:** relay is semi-trusted; sees Peer IDs, timing, volume | Communication graph visible to relay |
| **I**nformation disclosure | Metadata visible on WAN path | TLS 1.3 on the hop | Traffic analysis of TLS record sizes/timing |
| **I**nformation disclosure | Key exfiltration | OS keystore; `0600` fallback | Host OS compromise |
| **D**enial of service | Registration flood | Per-PeerID and per-IP token bucket; Ed25519 required before registration | Relay capacity |
| **D**enial of service | Registration slot occupation via replay | One registration per `peer_id`; relay does not replace live registration | **Accepted:** temporary DoS; no session can be established without the private key; peer recovers via reconnect |
| **D**enial of service | Relay outage | Single relay, no failover | Single point of failure by design (v1) |
| **E**levation of privilege | Unauthenticated relay abuse | Ed25519 required for `REGISTER` | Key compromise under that identity |
| **E**levation of privilege | Path traversal via filename | Same sanitization as LAN mode | — |

### Out of scope (WAN)

- Relay OS compromise (E2EE survives; availability does not).
- Physical relay access.
- Traffic analysis on the WAN path.
- Relay logging policy, billing, abuse handling.

## 4. Accepted Residual Risks (v1)

1. **Metadata visible to relay** — relay is semi-trusted; sees Peer IDs, timing, volume. Out of scope to protect (candidate for research/v2).

3. **Registration DoS** — replay of `REGISTER` after the nonce window can occupy a `peer_id` slot temporarily. No confidentiality or integrity impact.
4. **Discovery metadata in LAN** — UDP broadcast is readable by any LAN host. Unavoidable for LAN discovery.
5. **No cryptographic non-repudiation** — history is stored locally; not a proof against a third party.

## 5. Not Yet Analyzed (Future Work)

- Timing side-channels on relay (`PIPE_OPEN` timing, frame sizes).
- Amplification via `RENDEZVOUS_REQUEST` (queued requests, memory growth).
- State exhaustion in incomplete handshakes.
- Traffic analysis resistance (padding, dummy traffic) — research direction.
