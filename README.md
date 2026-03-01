# VegoshDB

> **Vega + Kosh** — Sanskrit for *speed* and *repository*.

Inspired by [TigerStyle](https://github.com/tigerbeetle/tigerbeetle/blob/main/docs/TIGER_STYLE.md) and [TigerBeetle](https://github.com/tigerbeetle/tigerbeetle). The core principle is simple: accept hard constraints in exchange for guaranteed, predictable, blazing fast performance. No malloc. Static allocation only. Every decision has a reason.

---

## Entry Layout

One cache line. 64 bytes exactly. No entry ever straddles two cache lines.

| Offset    | Field       | Size     | Description                                      |
|-----------|-------------|----------|--------------------------------------------------|
| [0..15]   | `key`       | 16 bytes | Raw 16-byte key                                  |
| [16..47]  | `value`     | 32 bytes | Raw 32-byte value                                |
| [48..51]  | `hash`      | 4 bytes  | Cached lower 32 bits of the XXH3 hash            |
| [52]      | `value_len` | 1 byte   | Length of the value in bytes                     |
| [53]      | `status`    | 1 byte   | `EMPTY` (0x00) or `OCCUPIED` (0x01)              |
| [54..57]  | `CRC32`     | 4 bytes  | CRC32 checksum of the value                      |
| [58..63]  | `reserved`  | 6 bytes  | Padding to reach 64 bytes                        |
| **Total** |             | **64 bytes** |                                              |

1M entries × 64 bytes = **64MB**. The entire table fits in L3 cache. Hot entries bubble into L1 and L2 naturally. The CPU does this for you.

The `hash` field caches the lower 32 bits of the XXH3 hash directly in the slot. This avoids recomputing the hash during Robin Hood displacement comparisons on every probe — the stored hash is compared first, and the key is only memcmp'd on a match.

---

## Hash Table

- **Hard limit:** 1,000,000 keys
- **Table size:** 2,097,152 slots (2²¹, ~47% load factor)
- **Index calculation:** Bitmask (`& MASK`) instead of modulo
- **Hash function:** xxHash (XXH3) — 16-byte SIMD-accelerated, essentially free on UUID-like keys
- **Collision resolution:** Robin Hood hashing with sequential probing

The table is power-of-two sized rather than prime. Prime sizing reduces modulo clustering, but at ~47% load Robin Hood hashing eliminates clustering anyway by redistributing probe length variance fairly across all entries — so the prime constraint buys nothing here. Power-of-two sizing replaces modulo with a bitmask, which is meaningfully faster when index calculation happens on every probe in a hot loop.

During insertion, if the incoming entry has traveled further from its ideal slot than the current occupant, the occupant yields. No entry accumulates a disproportionately long probe chain.

At ~47% load with high-entropy keys, **average probe length is well below 1.5**. Most lookups are one probe. On a miss, if the examined entry has a shorter probe distance than the target, the target does not exist — early termination. Double hashing has no equivalent.

There are no tombstones. Only `EMPTY` and `OCCUPIED` states exist.

---

## Operations

Two implemented so far. Five planned. Every operation that isn't here was considered and rejected.

| Operation   | Signature                                                        | Description               |
|-------------|------------------------------------------------------------------|---------------------------|
| `initializevegosh` | `int initializevegosh(void)`                            | Allocate and zero-init the global hash table |
| `insert`    | `int insert(const uint8_t *key, const uint8_t *value, const uint8_t *value_len)` | Insert or overwrite a key-value pair |
| `get`       | `int get(const uint8_t *key, uint8_t *out_value, uint8_t *value_len)` | Lookup a key and copy value into buffer |
| `DELETE`    | *(planned)*                                                      | Remove a key              |
| `SIZE`      | *(planned)*                                                      | Return current entry count |
| `FLUSHALL`  | *(planned)*                                                      | Clear the entire table    |

`insert` overwrites silently if the key already exists, consuming no additional slot. New insertions are rejected once `MAX_KEYS` (1,000,000) is reached. Returns `0` on success, `-1` on failure.

`get` copies the value into the caller-supplied buffer and writes the length into `value_len`. Returns `0` if found, `-1` if not.

---

## Concurrency Model

**Single-threaded.** No locks, no mutexes, no thread synchronization overhead.

- **Fork** — rejected. Spawning a process per O(1) GET is absurd overhead.
- **Threads** — rejected. Memory synchronization complexity contradicts the entire design philosophy.

This is the Redis model: simple, predictable, cache-friendly, zero false sharing.

---

## Networking

The KV store is identical across all stages. Only the networking layer changes. Each stage makes you feel what the previous layer costs.

| Stage | Technology | Target | Purpose |
|-------|------------|--------|---------|
| 1 | `poll` | 100K concurrent connections | Feel O(n) fd scanning degrade — understand what epoll fixes |
| 2 | `epoll` | 100K concurrent connections | O(1) active connection handling, event-driven |
| 3 | `io_uring` | 100K+ concurrent connections | Shared ring buffers, no syscall per operation |
| 4 | AF_XDP | Tens of millions of ops/sec | Partial kernel bypass via UMEM shared memory |
| 5 | DPDK | 100M+ ops/sec | Full kernel bypass — you own the NIC |

> **Note on metrics:** Poll through io_uring is measured in concurrent connections. AF_XDP and DPDK are measured in packets and ops/sec with nanosecond latency. The shift is not arbitrary — it reflects a genuine change in where the bottleneck lives. At 100GbE with 64-byte packets, the theoretical ceiling is ~148M packets/sec. This is how HFT and telco infrastructure actually works.

---

## Use Cases

These aren't a coincidence. They follow directly from the constraints.

**Routing tables** — 16-byte keys handle IPv6 prefixes natively. 32-byte values hold next hop, interface, and metadata. Overwhelmingly read-heavy, largely static. At DPDK layer, Redis cannot run here. VegoshDB can.

**Embedded systems** — Static allocation isn't a performance preference here, it's a hard requirement. No heap. Known memory footprint at compile time. Predictable behavior under all conditions. VegoshDB's constraints aren't inspired by this world — they *are* this world.

**Session token validation** — 16-byte UUID keys map to user ID, expiry, and role flags. Auth lives on the hot path. Every request hits this.

**Rate limiting** — IP or API key maps to counter and timestamp. Every request hits this too.

**Feature flags** — Feature and user segment map to flag state. Read on every request.

The common thread: fixed small keys and values, extremely hot read paths, latency as the only metric that matters.

---

## Dependencies

| Library  | Purpose                        |
|----------|--------------------------------|
| xxhash   | XXH3 hash function             |
| zlib     | CRC32 checksum of values       |

---

## Language

**C.** No runtime, no garbage collector, no abstraction tax. Full control over memory layout and system calls. The only reasonable choice given the destination.

---

## Philosophy

> Redis is a general purpose tool that happens to be fast.  
> VegoshDB is a special purpose tool that is fast *because of* its constraints.  
>
> Different tool. Different job.