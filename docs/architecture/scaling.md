# Scaling & Interest Management

To support 5,000+ players per zone, the server uses a tiered visibility system that minimizes both CPU work and outbound bandwidth. Instead of broadcasting every event to every player, the server categorizes recipients into three distance-based "Interaction Circles" and uses bitmask culling to skip irrelevant players entirely. Events are batched per grid cell to reduce kernel syscalls by ~90%.

## The 3-Circle Visibility Model

The server categorizes all players relative to an event source into three tiers:

| Circle | Distance | Network Frequency | Data Sent |
| :--- | :--- | :--- | :--- |
| **Immediate** | 0 -- 30m | 60 Hz (real-time) | Precise position, gear, buffs, animations. |
| **View Distance** | 30 -- 150m | 10 Hz (interpolated) | Low-frequency position, spell visuals only. |
| **Horizon** | 150 -- 500m | 1 Hz (ghosting) | Static nameplates, basic idle state. |

### Example: Spell Cast with 5,000 Nearby Players

If Player A casts Blizzard in the middle of 5,000 people:

- **20 Immediate players:** The server sends a high-priority [MsQuic Reliable Stream](networking.md) for damage/HP changes and an Unreliable Datagram for the instant animation.
- **1,000 View Distance players:** The server sends a shared multicast-style Datagram. These players don't need exact HP of the victims -- only a "Visual Start" packet.
- **3,980 Horizon/out-of-range players:** The bitmask check (`if (mask & spell_bit)`) returns false. The server does **zero work** for these players. Packets are never even created.

## VisibilityHierarchy Component

Each player entity in the [Flecs world](ecs.md) holds a `VisibilityHierarchy` component with three bitmasks, one per circle:

```cpp
struct VisibilityHierarchy {
    uint64_t immediate_mask; // 3x3 grid cells (local 30m)
    uint64_t view_mask;      // 7x7 grid cells (mid 150m)
    uint64_t horizon_mask;   // 15x15 grid cells (far 500m)

    Position last_pos;               // Tracking position for dirty-flag
    int last_update_x, last_update_z;
};
```

Each mask encodes which [Spatial Grid](physics.md) cells fall within that circle. A bit set to 1 means "this player is interested in events from that cell."

## Dirty-Flag Update System

Recalculating three bitmasks for 5,000 players every frame is wasteful. Instead, the server uses distance-based throttling -- the small circle updates often, the large circles update only when "dirty" (the player has moved far enough).

- **Tier 1 (Immediate):** Checked every frame. If the player crosses a cell boundary, `immediate_mask` is recalculated.
- **Tier 2 (View):** Recalculated only when the player moves more than 30m from their last View update position.
- **Tier 3 (Horizon):** Recalculated only when the player moves more than 100m from their last Horizon update position. This also resets the tracking position.

```cpp
void UpdatePlayerVisibility(flecs::iter& it, Position* p, VisibilityHierarchy* v) {
    for (auto i : it) {
        float dist_sq = Spn::Math::DistSq(p[i], v[i].last_pos);

        // Tier 1: Immediate (always update on cell crossing)
        if (HasCrossedCellBoundary(p[i])) {
            v[i].immediate_mask = Spn::Math::ComputeMask(p[i], 3);  // 3x3
        }

        // Tier 2: View (throttled -- only if moved 30m+)
        if (dist_sq > 900.0f) {
            v[i].view_mask = Spn::Math::ComputeMask(p[i], 7);       // 7x7
        }

        // Tier 3: Horizon (heavily throttled -- only if moved 100m+)
        if (dist_sq > 10000.0f) {
            v[i].horizon_mask = Spn::Math::ComputeMask(p[i], 15);   // 15x15
            v[i].last_pos = p[i];
        }
    }
}
```

In practice, ~95% of the time the server only performs a single float comparison (`dist_sq > 900`). The expensive bitmask generation only runs when a player actually travels significant distance.

## Packet Batching

Sending 1,000 individual packets for a single spell cast is what crashes most engines. ScorpionSatin batches events per grid cell per tick:

1. The server gathers all gameplay events (spells, damage, animations) for the current tick.
2. Events are grouped by the grid cell they originate from.
3. One buffer is created per cell containing all events for that cell (e.g., 10 different spells).
4. One single [MsQuic](networking.md) Datagram is sent to each player in that cell, containing all grouped events.

This reduces the number of system calls to the Linux kernel by ~90%, turning thousands of `sendmsg()` calls into a handful of batched writes.

## Cell-Occupancy Interest Lists

Instead of iterating over 5,000 players every time an event occurs, the server maintains a **Cell-Occupancy List** powered by the same [Spatial Grid](physics.md) used for entity collision:

- Each grid cell holds a list of `PlayerID`s currently inside it.
- When a spell hits Cell #50, the server only looks at the lists for Cell #50 and its neighbors.
- Finding targets is O(1) per cell lookup, regardless of whether there are 5,000 or 50,000 players on the server.

The Spatial Grid serves double duty: [entity collision checks](physics.md) and interest-list lookups share the same data structure with zero additional memory overhead.

## Broadcast Tier Selection

When the server sends event data via [MsQuic](networking.md), it selects the transport tier based on the event type and the recipient's bitmask:

| Event Type | Bitmask Check | Transport | Rationale |
| :--- | :--- | :--- | :--- |
| Combat Damage | `target.immediate_mask & cell_bit` | Reliable Stream | Must arrive; HP changes are gameplay-critical. |
| VFX / Animation | `target.view_mask & cell_bit` | Unreliable Datagram | Visual-only; loss is acceptable. |
| World Event | `target.horizon_mask & cell_bit` | Low-Frequency Update | Ambient awareness; 1 Hz is sufficient. |

If none of the masks match, the packet is **never created** -- the server skips that player entirely.

## Scalability Analysis

| Factor | How It Helps |
| :--- | :--- |
| **CPU Cache** | Flecs packs all `VisibilityHierarchy` data contiguously in memory. The CPU can process thousands of bitmask checks in a single cache-line prefetch cycle. |
| **Minimal Math** | ~95% of per-player work is a single `float > float` comparison. Bitmask generation is rare. |
| **MsQuic Efficiency** | Players beyond the relevant circle never receive the packet. A player 400m away never gets the "Combat Damage" packet, saving massive outbound bandwidth. |
| **Batching** | Grouping events by cell reduces kernel syscalls by ~90%, preventing the `sendmsg()` bottleneck that kills naive implementations. |
| **Interest Lists** | Cell-occupancy lookup is O(1). No linear scan over the full player population. |

## Related Documents

- [Networking](networking.md) -- MsQuic transport, reliable streams vs unreliable datagrams, batching delivery.
- [Server / Headless](server-headless.md) -- Zone-based scaling and server tick loop.
- [ECS](ecs.md) -- `VisibilityHierarchy` component and visibility update system in Flecs.
- [Collision & Spatial Logic](physics.md) -- Spatial Grid doubles as the cell-occupancy interest list.
