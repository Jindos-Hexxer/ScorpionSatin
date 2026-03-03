# Networking (MsQuic)

MsQuic provides the transport layer for all client-server communication. It implements the QUIC protocol, giving encrypted UDP transport with both reliable and unreliable delivery modes -- ideal for MMO traffic patterns.

## Why QUIC / MsQuic

- **Encrypted by default:** TLS 1.3 is built into the protocol. No separate encryption layer needed.
- **Multiplexed streams:** Multiple independent streams over a single connection. No head-of-line blocking between streams.
- **Unreliable datagrams:** QUIC datagrams (RFC 9221) allow fire-and-forget data for high-frequency updates.
- **Cross-platform:** MsQuic runs on Windows and Linux, matching the engine's [platform targets](../engine-architecture.md).

## Connection Model

```text
Client (Game.dll)                     Server (GameServer.so)
┌──────────────┐                      ┌──────────────┐
│  MsQuic      │◄────── QUIC ────────►│  MsQuic      │
│  Connection  │   (single connection) │  Listener    │
│              │                       │              │
│  Stream 0    │  Reliable: Chat       │  Stream 0    │
│  Stream 1    │  Reliable: Loot       │  Stream 1    │
│  Stream 2    │  Reliable: Trading    │  Stream 2    │
│  Datagrams   │  Unreliable: Position │  Datagrams   │
└──────────────┘                      └──────────────┘
```

Each client holds a single QUIC connection to the server. Within that connection:

### Reliable Streams

Used for data that must arrive in order and without loss:
- **Chat:** Player messages, system announcements.
- **Loot:** Item drops, inventory changes.
- **Trading:** Trade window state, confirmations.
- **Spells/Abilities:** Cast requests and confirmations.
- **Authentication:** Login handshake, session tokens.

### Unreliable Datagrams

Used for high-frequency, loss-tolerant data:
- **Position updates:** Sent every tick (~20-60 Hz). Losing one is acceptable because the next update supersedes it.
- **Animation state:** Current animation ID and normalized time.
- **Projectile positions:** Interpolated on the client anyway.

## Concurrency Model

MsQuic is event-driven and manages its own thread pool internally:
- Connection callbacks fire on MsQuic worker threads.
- Incoming data is placed into a lock-free queue.
- The [ECS Network Receive System](ecs.md) drains this queue on the main thread during the `PreUpdate` phase.
- Outgoing data is enqueued by the [ECS Network Send System](ecs.md) during `PostUpdate` and flushed to MsQuic.

This keeps all Flecs world access single-threaded while MsQuic handles I/O concurrently.

<!-- TODO: Document the message serialization format (packet headers, entity ID encoding) -->

## Server Listener

On the [headless server](server-headless.md), MsQuic runs as a listener accepting thousands of concurrent connections:
- Each connection maps to one authenticated player.
- The server processes all connections' incoming data each tick before running game logic.
- Zone-based architecture: multiple server instances can run different zones, with inter-server communication also over MsQuic.

## Client-Side Prediction

To mask latency, the client applies movement input locally before the server confirms:
1. Client sends movement input to the server (reliable stream).
2. Client predicts the result locally using [NavMesh movement](physics.md) (`moveAlongSurface`).
3. Server processes the input authoritatively and sends back the corrected state (unreliable datagram).
4. Client reconciles: if the server state differs, it snaps or interpolates to correct.

<!-- TODO: Detail the reconciliation/rollback strategy -->

## Bandwidth Considerations

- Position updates are delta-compressed (only send changes).
- **Interest management:** The server uses a [3-circle visibility model](scaling.md) with bitmask culling so players beyond the relevant circle never receive the packet. Events are batched per grid cell to reduce syscalls by ~90%.
- Unreliable datagrams have no retransmission overhead, keeping bandwidth predictable.

## Related Documents

- [ECS](ecs.md) -- Network Receive/Send systems integrate MsQuic with the Flecs world.
- [Server / Headless](server-headless.md) -- Server-side listener and zone architecture.
- [Collision & Spatial Logic](physics.md) -- Client-side prediction uses local NavMesh movement.
- [Scaling & Interest Management](scaling.md) -- 3-circle visibility, packet batching, broadcast tier selection.
