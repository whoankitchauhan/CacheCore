<div align="center">

# ⚡ CacheCore

### Concurrent In-Memory Key-Value Store in C++17

[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.16+-green.svg)](https://cmake.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

</div>

---

> **CacheCore is an educational implementation of fundamental caching concepts.**
> It is not a Redis replacement, not production-ready, and makes no claims of
> distributed caching, persistence, or million-requests-per-second throughput.
> Its purpose is to demonstrate how to build a correct, concurrent, expiring
> LRU cache from scratch in C++ — suitable for study, interview preparation,
> and understanding systems fundamentals.

---

## Why CacheCore?

Modern caching systems like Redis are massive production codebases. Understanding *why* they work the way they do — HashMap + linked list, O(1) operations, LRU ordering — requires building a small version from scratch.

CacheCore implements exactly the data structures and algorithms that underpin real caching systems, in approximately 500 lines of well-commented C++.

---

## Architecture

```
┌────────────────────────────────────────────────────────────┐
│                        LRUCache                            │
│                                                            │
│  std::unordered_map<string, Node*>                         │
│    "name" ──────────────────────────┐                      │
│    "city" ──────────────┐           │                      │
│    "key3" ──────┐       │           │                      │
│                 ▼       ▼           ▼                      │
│  HEAD ⇄ [key3] ⇄ [city] ⇄ [name] ⇄ TAIL                   │
│  (MRU)                              (LRU)                  │
│                                                            │
│  Sentinel HEAD and TAIL simplify boundary conditions:      │
│  no nullptr checks during insert/remove.                   │
└────────────────────────────────────────────────────────────┘
```

The cache has two data structures in one object:

| Structure | Role |
|---|---|
| `std::unordered_map<string, Node*>` | O(1) average-case key lookup |
| Doubly-linked list (raw pointer DLL) | O(1) LRU order maintenance |

### Why both?

- The **HashMap alone** can look up a key quickly, but cannot efficiently track *which* key was accessed *least* recently.
- The **DLL alone** can maintain order, but lookups are O(n).
- Together, the HashMap stores a pointer directly to the node in the list. The cache can find a node in O(1) *and* reorder it in O(1).

---

## Data Structures

### Node (`include/Node.h`)

```cpp
struct Node {
    std::string key;
    std::string value;
    bool        hasTTL;
    std::chrono::steady_clock::time_point expiresAt;
    Node*       prev;
    Node*       next;
};
```

Raw `prev`/`next` pointers are used deliberately. Smart pointers with doubly-linked lists introduce circular references (`shared_ptr` cycles) or back-link overhead (`weak_ptr`) that obscure the data structure logic. `LRUCache` owns all nodes via `new`/`delete`.

### LRUCache (`include/LRUCache.h`)

```cpp
class LRUCache {
    size_t capacity_;
    std::unordered_map<std::string, Node*> map_;
    Node* head_;   // sentinel — MRU side
    Node* tail_;   // sentinel — LRU side
    mutable std::mutex mtx_;
    Statistics stats_;
    // ...background thread members...
};
```

### Statistics (`include/Statistics.h`)

```cpp
struct Statistics {
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<uint64_t> evictions{0};
    std::atomic<uint64_t> expired{0};
};
```

`std::atomic` allows incrementing counters from any thread without holding the main cache mutex.

---

## LRU Mechanism

**Invariant:** `head_->next` is always the Most Recently Used (MRU) entry. `tail_->prev` is always the Least Recently Used (LRU) entry.

**On GET (hit):**
1. Find `Node*` in HashMap → O(1) average
2. `moveToFront(node)` → unlink from current position, reinsert after `head_` → O(1)
3. Return value

**On SET (new key):**
1. If at capacity, `evictLRU()` → access `tail_->prev`, erase from HashMap and list, delete node → O(1)
2. Create new node, insert into HashMap and after `head_` → O(1) average

**On SET (existing key):**
1. Find in HashMap → O(1) average
2. Update value in place
3. `moveToFront(node)` → O(1)
4. No allocation, no eviction

**DLL core operations (all O(1)):**

```
removeNode(node):
    node->prev->next = node->next
    node->next->prev = node->prev

insertAfterHead(node):
    node->next = head_->next
    node->prev = head_
    head_->next->prev = node
    head_->next = node

moveToFront(node):
    removeNode(node)
    insertAfterHead(node)
```

---

## TTL Mechanism

```
SET session:abc Ankit TTL 60
```

The absolute expiry timestamp is stored in the node:
```
expiresAt = steady_clock::now() + 60s
```

**Lazy expiry** (on GET):
```
if (hasTTL && now() > expiresAt):
    remove from HashMap
    remove from DLL
    delete node
    increment expired counter
    return MISS
```

**Background sweep** (optional, every N seconds):
- A `std::thread` wakes via `std::condition_variable::wait_for`
- Scans the map for expired nodes and removes them
- Reclaims memory for keys that are never accessed again after TTL
- Shuts down cleanly on cache destruction (no `sleep` loops)

Lazy expiry alone is sufficient for correctness. The background thread is purely a memory optimisation.

---

## Concurrency Model

```
Thread 1       Thread 2       Thread 3
   │               │               │
   ├── GET("k") ──►│               │
   │  lock(mtx_)   │               │
   │  ...          │               │
   │  unlock       │               │
   │               ├── SET("k") ──►│
   │               │  lock(mtx_)   │
   │               │  ...          │
   │               │  unlock       │
```

**Single `std::mutex` (mtx_)** protects all accesses to `map_` and the DLL.

A `std::shared_mutex` would allow concurrent reads — but since `GET` modifies the DLL order, there are no true read-only operations. A shared mutex would give no benefit and would add complexity.

**`std::lock_guard`** is used for all operations (acquired at method entry, released on return via RAII).

**`std::atomic`** counters for statistics allow reads from any thread without the main lock.

**Background thread** uses `std::unique_lock` + `std::condition_variable` so it can be interrupted immediately on shutdown instead of sleeping a full scan interval.

Critical section size is kept minimal: work done outside the lock (e.g., building the deletion list) is noted in comments.

---

## Statistics

```
STATS output:

Capacity:     1000
Current Size: 342
Hits:         8720
Misses:       1280
Evictions:    412
Expired:      88
Hit Rate:     87.2%
```

| Counter | Meaning |
|---|---|
| Hits | Successful GETs (key found, not expired) |
| Misses | GETs on missing or expired keys |
| Evictions | Keys removed by LRU policy (capacity pressure) |
| Expired | Keys removed by TTL (lazy or background sweep) |
| Hit Rate | `hits / (hits + misses)` |

Note: evictions and expiry are counted separately. A key evicted by LRU is *not* counted as expired.

---

## Complexity Analysis

| Operation | Time | Space | Notes |
|---|---|---|---|
| GET | **O(1) avg** | O(1) | HashMap lookup + DLL moveToFront |
| SET (new) | **O(1) avg** | O(1) | HashMap insert + DLL insertAfterHead |
| SET (update) | **O(1) avg** | O(1) | HashMap lookup + DLL moveToFront, no alloc |
| DELETE | **O(1) avg** | O(1) | HashMap erase + DLL removeNode |
| Evict LRU | **O(1)** | O(1) | Direct DLL tail access |
| Background sweep | O(n) | O(k) | n = map size, k = expired count |

> **Important:** `std::unordered_map` provides **average-case O(1)** for lookup, insert, and erase. Worst case is O(n) due to hash collisions. In practice, with a good hash function and reasonable load factor, collisions are rare. This is the same trade-off Redis and every practical hash table makes.

---

## Project Structure

```
CacheCore/
│
├── include/
│   ├── Node.h           # DLL node: key, value, TTL, prev/next
│   ├── LRUCache.h       # Cache interface: capacity, set/get/remove/stats
│   └── Statistics.h     # Atomic hit/miss/eviction/expired counters
│
├── src/
│   ├── LRUCache.cpp     # Core implementation (all stages integrated)
│   └── Statistics.cpp   # format() and reset()
│
├── tests/
│   ├── test_runner.h    # Self-written test framework (no dependencies)
│   ├── test_main.cpp    # Test runner entry point
│   ├── cache_tests.cpp  # Basic ops, updates, capacity, concurrency, stats
│   ├── lru_tests.cpp    # LRU eviction order tests
│   └── ttl_tests.cpp    # TTL expiration tests
│
├── benchmarks/
│   └── benchmark.cpp    # Multi-threaded throughput benchmark
│
├── examples/
│   └── main.cpp         # CLI: SET / GET / DELETE / STATS / EXIT
│
├── CMakeLists.txt
├── README.md
└── .gitignore
```

---

## Build Instructions

### Prerequisites

| Tool | Version | Purpose |
|---|---|---|
| CMake | ≥ 3.16 | Build system |
| GCC | ≥ 7.0 | C++ compiler with C++17 support |
| make / Ninja | any | Build driver |

On Windows, use [WinLibs](https://winlibs.com/) (GCC 16+) or install via:
```powershell
winget install Kitware.CMake
winget install BrechtSanders.WinLibs.POSIX.UCRT
```

### Configure & Build

```bash
# Clone
git clone https://github.com/whoankitchauhan/CacheCore.git
cd CacheCore

# Configure (Release build)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build all targets
cmake --build build --config Release
```

On Windows with MinGW Makefiles:
```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

---

## Run Instructions

### CLI

```bash
./build/cachecore_cli [capacity]
# or on Windows:
.\build\cachecore_cli.exe 1000
```

### Tests

```bash
./build/cachecore_tests
# or:
ctest --test-dir build --output-on-failure
```

### Benchmark

```bash
./build/cachecore_bench
```

---

## CLI Examples

```
╔══════════════════════════════════════════╗
║   CacheCore – In-Memory Key-Value Store  ║
╚══════════════════════════════════════════╝
  Capacity : 1000
  Commands : SET key value [TTL seconds]  GET key  DELETE key  STATS  EXIT

> SET name Ankit
OK

> GET name
Ankit

> SET city Delhi
OK

> GET city
Delhi

> SET session:abc token123 TTL 60
OK

> GET session:abc
token123

> DELETE city
OK

> GET city
(nil)

> STATS
Capacity:     1000
Current Size: 2
Hits:         3
Misses:       1
Evictions:    0
Expired:      0
Hit Rate:     75.0%

> EXIT
Goodbye!
```

---

## Testing

The test suite uses a self-written test framework (`tests/test_runner.h`) with no external dependencies. Tests are grouped into three categories:

### Basic Cache Operations (`cache_tests.cpp`)
- Basic SET/GET
- GET on missing key
- DELETE
- Updating existing key (no duplicate)
- `size()` stays correct after update
- Capacity 0 (unlimited)
- Capacity 1 (single-slot eviction)
- Capacity 1 update (no false eviction)
- 8-thread concurrent access smoke test
- Statistics hit/miss counting
- Eviction counter

### LRU Eviction Order (`lru_tests.cpp`)
- Basic LRU eviction (oldest key removed)
- GET promotes key to MRU (saves it from eviction)
- SET update promotes existing key to MRU
- Multiple consecutive evictions
- Delete non-LRU key, verify remaining order

### TTL Expiration (`ttl_tests.cpp`)
- Non-expired key is accessible
- Expired key returns `(nil)` (lazy removal)
- Expired counter increments correctly
- SET without TTL clears previous TTL
- TTL works with unlimited capacity
- Expiry is distinct from eviction (separate counters)

Run all tests:
```bash
./build/cachecore_tests
```

Expected output:
```
[ Basic Cache Operations ]
  [PASS] BasicSet
  [PASS] BasicGet
  ...

[ LRU Eviction Order ]
  [PASS] LRUEvictOldest
  ...

[ TTL Expiration ]
  (some tests sleep 1.5s — this is expected)
  [PASS] TTLNotYetExpired
  ...

  ✓  All tests passed.
```

---

## Benchmarking

The benchmark (`benchmarks/benchmark.cpp`) runs the following workload:

- **Cache capacity:** 10,000 keys
- **Ops per thread:** 200,000
- **Read ratio:** 75% GET, 25% SET
- **Key space:** 20,000 keys (2× capacity → realistic cache pressure)
- **Thread counts:** 1, 4, 8, 16, 32

Each configuration pre-populates 5,000 keys, resets stats, then measures throughput.

### Actual Benchmark Results

> Results measured on the development machine (Windows, GCC 14.2.0, Release build).
> Numbers will vary with CPU, OS scheduler, and memory speed.

```
Cache capacity : 10000 keys
Ops per thread : 200000
Read ratio     : 75% GET  25% SET
Key space      : 20000 (2× capacity)

Threads   Total Ops    Time (s)   Ops/sec        Hits       Misses     Evictions
----------------------------------------------------------------------------------
1         200000       0.055      3,650,701      71,942     78,127     20,794
4         800000       0.432      1,852,568     296,603    303,462     96,126
8         1600000      0.844      1,896,366     596,178    603,700    195,463
16        3200000      1.946      1,644,645   1,196,232  1,203,500    395,893
32        6400000      3.778      1,693,912   2,396,419  2,403,582    796,274
```

**Key observations:**
- **1 thread:** ~3.6M ops/sec — baseline, zero lock contention
- **4→8 threads:** throughput holds near 1.85–1.9M ops/sec total; the single mutex begins serializing threads
- **16→32 threads:** throughput plateau; mutex contention dominates; adding more threads does not increase total ops/sec

This illustrates exactly why production systems (Redis single-threaded, memcached with per-slab locks, etc.) use different concurrency strategies than a single global mutex.

---

## Limitations

1. **Single mutex** — all threads serialize through one lock. Throughput will plateau under high concurrency. Sharding the cache into N sub-caches with N locks would improve scalability.

2. **No persistence** — data lives only in RAM. Process exit loses all data. Persistence would require a WAL (write-ahead log) or snapshot mechanism.

3. **No networking** — CacheCore is a library/CLI, not a server. Adding a network protocol (TCP + RESP) would allow remote access, similar to how Redis works.

4. **String-only values** — values are always `std::string`. Supporting typed values (int, binary, list, set) would require a type-tagged union or `std::variant`.

5. **O(n) background sweep** — the TTL cleanup thread scans the entire map. A priority queue of `(expiresAt, key)` pairs would allow O(log n) targeted cleanup.

6. **Average-case O(1)** — `std::unordered_map` worst case is O(n) on hash collisions. A hash table with Robin Hood hashing or consistent seed salting would mitigate this.

---

## Future Improvements

- [ ] **Sharded locking** — N sub-caches, each with its own mutex, for better multi-core scaling
- [ ] **TTL priority queue** — O(log n) expiry cleanup instead of full scan
- [ ] **Typed values** — `std::variant<int64_t, double, std::string, std::vector<uint8_t>>`
- [ ] **Persistence** — RDB-style snapshots or AOF-style write-ahead log
- [ ] **Network layer** — TCP server with a simple text protocol for remote access
- [ ] **Reader-writer lock** — if a pure read-only `peek()` (no LRU update) is added
- [ ] **Metrics export** — Prometheus-compatible `/metrics` endpoint
- [ ] **Size-based eviction** — evict by value byte-size instead of count

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

*CacheCore was built as a complete, from-scratch systems programming exercise. Every data structure, every pointer, every synchronisation primitive is implemented explicitly — nothing is hidden behind a library.*
