# CacheCore – Complete Learning Guide

> **How to use this guide:**
> Read it top to bottom once. Then use it as a reference when preparing
> for interviews. Every explanation connects to the actual code in this
> project — nothing here is generic theory.

---

## 1. PROJECT IN ONE MINUTE

### What is CacheCore?

CacheCore is a program that stores key-value pairs in RAM so you can
retrieve them very quickly.

Think of it like a dictionary that lives in memory:

```
SET name Ankit    →  stores "Ankit" under the key "name"
GET name          →  retrieves "Ankit"
DELETE name       →  removes it
```

### What problem does it solve?

Imagine your program repeatedly needs to look up the same information —
maybe a user's profile, or the result of a heavy calculation. Reading
from a database every time is slow. A cache keeps a copy of the most
important data in RAM so the next lookup is instant.

### What does "in-memory" mean?

The data lives entirely in RAM (Random Access Memory), not on a hard
disk. RAM is much faster to read than a disk, but its contents
disappear when the program exits. CacheCore does not save anything to
disk. That is by design.

### What does "key-value store" mean?

Every piece of data has two parts:

- **Key:** a unique name (like a dictionary word). In CacheCore, keys
  are plain strings: `"name"`, `"session:123"`, `"user:42"`.
- **Value:** the data attached to that key. Also a plain string in
  CacheCore: `"Ankit"`, `"abc123"`, `"Delhi"`.

### What does "concurrent" mean?

Multiple threads (independent workers inside the same program) can call
SET, GET, and DELETE at the same time. CacheCore handles this safely —
no two threads corrupt each other's data.

### What is the final project capable of doing?

| Capability | Implemented? |
|---|---|
| Store string key-value pairs | ✅ Yes |
| Retrieve values by key | ✅ Yes |
| Delete keys | ✅ Yes |
| LRU eviction (drops least-used key when full) | ✅ Yes |
| TTL — keys auto-expire after N seconds | ✅ Yes |
| Thread-safe concurrent access | ✅ Yes |
| Statistics (hits, misses, evictions, expired) | ✅ Yes |
| Command-line interface | ✅ Yes |
| Benchmarking harness | ✅ Yes |
| Automated tests (25 tests) | ✅ Yes |
| Networking / remote access | ❌ Not implemented |
| Persistence (saving to disk) | ❌ Not implemented |
| Distributed caching | ❌ Not implemented |

---

## 2. COMPLETE TECHNOLOGY STACK

Everything listed here is actually used in the CacheCore source code.

### C++17

**What it is:** A version of the C++ programming language from 2017.

**Why we use it:** We need `std::optional` (to represent "value or
nothing"), structured bindings (`auto& [k, v]`), and `std::chrono`
improvements. GCC 6.3 (2016) does not support these — you need at
least GCC 7. This project was built with **GCC 14.2.0** (via
w64devkit).

**Where in CacheCore:** Every `.cpp` and `.h` file. The CMakeLists.txt
enforces it with `set(CMAKE_CXX_STANDARD 17)`.

### STL (Standard Template Library)

**What it is:** The collection of ready-made containers, algorithms,
and utilities that come with C++. We use it instead of writing
everything from scratch.

**Why we use it:** Saves time and avoids bugs. The hash table and
memory management in `std::unordered_map` are already well-tested.

**Where in CacheCore:** `std::unordered_map`, `std::string`,
`std::vector`, `std::thread`, `std::mutex`, `std::atomic`, etc.

### `std::unordered_map<std::string, Node*>`

**What it is:** A hash table. Given a key, it finds the matching
entry in O(1) average time.

**Why we use it:** We need to find a node in the linked list given
only its key — without scanning the entire list.

**Where in CacheCore:** `LRUCache.h` line 127:
```cpp
std::unordered_map<std::string, Node*> map_;
```
The value stored is `Node*` — a raw pointer directly to the node in
the doubly-linked list.

**Why not something else:** `std::map` would be O(log n) per lookup.
`std::unordered_map` is O(1) average. We want the cache to be fast.

### `std::string`

**What it is:** C++'s built-in text/string type.

**Why we use it:** Keys and values are both text. `std::string` handles
memory automatically.

**Where in CacheCore:** The `Node` struct stores `key` and `value` as
`std::string`. The map key type is also `std::string`.

### Doubly Linked List (custom, hand-written)

**What it is:** A list where each element (Node) has a pointer to the
previous element and a pointer to the next element.

**Why we use it:** To track which key was accessed most recently and
which was accessed least recently, and to reorder nodes in O(1) time.

**Where in CacheCore:** The private methods `removeNode`,
`insertAfterHead`, `moveToFront`, and `evictLRU` in `LRUCache.cpp`
implement the list operations. Each `Node` has `prev` and `next` raw
pointers.

### `std::mutex` and `std::lock_guard`

**What it is:** A mutex is a lock. Only one thread can hold it at a
time. `std::lock_guard` acquires the lock when created and releases it
automatically when it goes out of scope (RAII).

**Why we use it:** Multiple threads access the same cache. Without a
lock, two threads could corrupt the linked list simultaneously.

**Where in CacheCore:** In `LRUCache.cpp`, every public method starts
with:
```cpp
std::lock_guard<std::mutex> lock(mtx_);
```
`mtx_` is the mutex declared in `LRUCache.h` line 134.

### `std::unique_lock`

**What it is:** A more flexible lock. Unlike `lock_guard`, it can be
released and re-acquired manually, and it works with
`condition_variable`.

**Why we use it:** The background TTL cleanup thread needs to
temporarily release the lock while sleeping, then re-acquire it when it
wakes up. `condition_variable::wait_for` requires `unique_lock`.

**Where in CacheCore:** `LRUCache.cpp` line 270 in `cleanupLoop()`:
```cpp
std::unique_lock<std::mutex> lock(mtx_);
cv_.wait_for(lock, std::chrono::seconds(cleanupIntervalSec_), ...);
```

### `std::thread`

**What it is:** Represents a single thread of execution. You can
start a function on a new thread using `std::thread`.

**Why we use it:** The background TTL cleanup runs on its own thread
so it does not block the main program. The benchmark also creates
multiple threads to test concurrent performance.

**Where in CacheCore:**
- `LRUCache.h` line 139: `std::thread cleanupThread_;`
- Constructor in `LRUCache.cpp` line 41:
  ```cpp
  cleanupThread_ = std::thread(&LRUCache::cleanupLoop, this);
  ```
- `benchmark.cpp` lines 82-85: creates 1/4/8/16/32 worker threads.

### `std::chrono`

**What it is:** C++'s time library. It lets you get the current time
and represent durations (like 60 seconds).

**Why we use it:** TTL needs to store an expiration timestamp and
compare it to the current time.

**Where in CacheCore:**
- `Node.h` line 29: `std::chrono::steady_clock::time_point expiresAt;`
- `LRUCache.cpp` line 122: `std::chrono::steady_clock::now() > node->expiresAt`
- `LRUCache.cpp` line 166: `std::chrono::steady_clock::now() + *ttl`

`steady_clock` is used (not `system_clock`) because it never goes
backward — it is not affected by the system clock being adjusted.

### `std::atomic<uint64_t>`

**What it is:** An integer that can be incremented from multiple
threads simultaneously without corruption, without needing a mutex.

**Why we use it:** Statistics counters (hits, misses, evictions,
expired) are updated very frequently. Using a mutex for every counter
update would slow things down. Atomics are hardware-level operations
that are safe without locks.

**Where in CacheCore:** `Statistics.h` lines 21-24:
```cpp
std::atomic<uint64_t> hits{0};
std::atomic<uint64_t> misses{0};
std::atomic<uint64_t> evictions{0};
std::atomic<uint64_t> expired{0};
```
Updated in `LRUCache.cpp` with:
```cpp
stats_.hits.fetch_add(1, std::memory_order_relaxed);
```

### `std::condition_variable`

**What it is:** Lets a thread sleep and wake up when notified by
another thread.

**Why we use it:** The background cleanup thread should sleep for N
seconds between sweeps, but must wake up immediately when the cache is
destroyed. A plain `sleep` would not wake on destruction.

**Where in CacheCore:** `LRUCache.h` line 140: `std::condition_variable cv_;`
The destructor calls `cv_.notify_all()` to wake the sleeping thread
immediately so it can exit.

### `std::optional<T>`

**What it is:** A wrapper that either contains a value of type T, or
contains nothing. It replaces the old pattern of returning -1 or nullptr
to signal "not found".

**Why we use it:** `get()` either returns the value string (if found)
or nothing (if not found / expired). `std::optional<std::string>` makes
this clean and safe.

**Where in CacheCore:**
- `LRUCache.h` line 89: `std::optional<std::string> get(const std::string& key);`
- `LRUCache.h` line 80: `std::optional<std::chrono::seconds> ttl = std::nullopt`
- `LRUCache.cpp` line 192: `return std::nullopt;` (on miss)
- `LRUCache.cpp` line 209: `return node->value;` (on hit)

### CMake

**What it is:** A build system generator. You describe what you want
to build in a `CMakeLists.txt` file, and CMake generates the actual
build files (Makefiles, Visual Studio projects, etc.) for your platform.

**Why we use it:** Compiling 15 source files with the right flags,
links, and include paths by hand would be tedious and error-prone.
CMake does it automatically and works on Linux, macOS, and Windows.

**Where in CacheCore:** `CMakeLists.txt`. It defines 4 build targets:
`cachecore_lib` (static library), `cachecore_cli`, `cachecore_tests`,
`cachecore_bench`.

### Git/GitHub

**What it is:** Git is a version control system. GitHub hosts Git
repositories online.

**Why we use it:** To track changes, share the project, and provide a
professional presentation of the code.

**Where in CacheCore:** `.gitignore` excludes the `build/` directory
and compiled binaries.

### Self-written test framework (`test_runner.h`)

**What it is:** A simple 78-line header that defines `TestRegistry`
(a list of test functions) and assertion macros (`TEST_ASSERT`,
`TEST_ASSERT_EQ`).

**Why we use it:** No external dependencies needed. The test framework
is easy to understand and audit. GoogleTest was intentionally not used.

**Where in CacheCore:** `tests/test_runner.h`. Used by
`cache_tests.cpp`, `lru_tests.cpp`, `ttl_tests.cpp`,
and `test_main.cpp`.

---

## 3. COMPLETE PROJECT STRUCTURE

```
CacheCore/
│
├── include/
│   ├── Node.h           ← The data structure node
│   ├── Statistics.h     ← Counter declarations
│   └── LRUCache.h       ← The main cache class interface
│
├── src/
│   ├── LRUCache.cpp     ← All cache logic (301 lines)
│   └── Statistics.cpp   ← format() and reset()
│
├── tests/
│   ├── test_runner.h    ← Mini test framework
│   ├── test_main.cpp    ← Test suite entry point
│   ├── cache_tests.cpp  ← 13 basic tests
│   ├── lru_tests.cpp    ← 6 LRU ordering tests
│   └── ttl_tests.cpp    ← 6 TTL expiry tests
│
├── benchmarks/
│   └── benchmark.cpp    ← Multi-thread throughput test
│
├── examples/
│   └── main.cpp         ← The CLI (command-line interface)
│
├── CMakeLists.txt       ← Build configuration
├── README.md            ← Public project documentation
├── build.bat            ← Windows one-click build helper
└── .gitignore           ← Tells git what to ignore
```

### `include/Node.h`

**Why it exists:** Defines the `Node` struct — the building block of
the doubly-linked list.

**What is inside:** A `struct Node` with fields: `key`, `value`,
`hasTTL`, `expiresAt`, `prev`, `next`. Two constructors: one for real
nodes (takes key/value), one default constructor for sentinel nodes.

**Who uses it:** `LRUCache.h` includes it. Every pointer in the linked
list points to a `Node`.

**What happens without it:** Nothing compiles. `LRUCache` cannot exist
without `Node`.

### `include/Statistics.h`

**Why it exists:** Groups all four counters in one place. Keeps the
`LRUCache` class clean.

**What is inside:** `struct Statistics` with four `std::atomic<uint64_t>`
fields: `hits`, `misses`, `evictions`, `expired`. Declares `reset()`
and `format()`.

**Who uses it:** `LRUCache.h` includes it. `LRUCache` has a member
`Statistics stats_`.

**What happens without it:** The cache has no way to count or report
performance metrics.

### `include/LRUCache.h`

**Why it exists:** Declares the public interface of `LRUCache`. Any
file that wants to use the cache `#include`s this.

**What is inside:** The `LRUCache` class declaration. Public methods:
`set`, `get`, `remove`, `size`, `capacity`, `stats`, `statsString`,
`resetStats`. Private members: `capacity_`, `map_`, `head_`, `tail_`,
`mtx_`, `stats_`, `cleanupThread_`, `cv_`, `shutdown_`,
`cleanupIntervalSec_`. Private helpers: `removeNode`,
`insertAfterHead`, `moveToFront`, `evictLRU`, `isExpired`,
`cleanupLoop`.

**Who uses it:** `src/LRUCache.cpp` (implements it), `examples/main.cpp`
(uses the cache in the CLI), all test files, and `benchmark.cpp`.

**What happens without it:** Nothing can use the cache.

### `src/LRUCache.cpp`

**Why it exists:** Contains the actual implementations of all
`LRUCache` methods. The `.h` file only declares what exists; this
`.cpp` file defines how it works.

**What is inside:** Constructor (allocates sentinels, starts cleanup
thread), Destructor (signals thread, joins it, deletes all nodes), DLL
helpers (`removeNode`, `insertAfterHead`, `moveToFront`, `evictLRU`),
`isExpired`, `set`, `get`, `remove`, inspection methods, `cleanupLoop`.

**Who uses it:** Compiled into `cachecore_lib.a` (the static library).
All three executables link against this library.

**What happens without it:** The library does not exist. Nothing links.

### `src/Statistics.cpp`

**Why it exists:** Implements `Statistics::reset()` and
`Statistics::format()`.

**What is inside:**
- `reset()` — stores 0 into all four atomic counters.
- `format()` — reads all four counters, calculates hit rate, builds a
  formatted string using `std::ostringstream`.

**Who uses it:** Compiled into `cachecore_lib`. Called by
`LRUCache::statsString()` and `LRUCache::resetStats()`.

**What happens without it:** `STATS` command and benchmark resets do
not work.

### `examples/main.cpp`

**Why it exists:** The command-line interface. This is how a human
interacts with CacheCore.

**What is inside:** `main()` reads an optional capacity argument,
creates an `LRUCache(capacity, 30)` (background cleanup every 30
seconds), then enters an infinite loop reading commands from stdin.
For each command it calls the right `LRUCache` method and prints the
result.

**Who uses it:** Compiled into `cachecore_cli.exe`. This is what you
run when you type `.\build\cachecore_cli.exe`.

**What happens without it:** No interactive way to use the cache.

### `tests/test_runner.h`

**Why it exists:** A minimal test framework so we do not need GoogleTest
or any external library.

**What is inside:**
- `TEST_ASSERT(condition)` — throws `std::runtime_error` if false.
- `TEST_ASSERT_EQ(a, b)` — throws if `a != b`, printing both values.
- `TEST_ASSERT_TRUE` / `TEST_ASSERT_FALSE` — wrappers.
- `class TestRegistry` — stores a list of `(name, function)` pairs.
  `add()` registers a test. `run()` executes all tests, prints PASS/FAIL.

**Who uses it:** All three test files and `test_main.cpp`.

### `tests/test_main.cpp`

**Why it exists:** Entry point for the test binary. Calls the three
registration functions, runs each group, prints the overall result.

**What is inside:** Calls `register_cache_tests(r)`,
`register_lru_tests(r)`, `register_ttl_tests(r)`, then `r.run()` for
each group.

### `tests/cache_tests.cpp`

13 tests covering: BasicSet, BasicGet, GetMiss, Delete, DeleteMiss,
UpdateExistingKey, UpdatePreservesSize, CapacityZeroUnlimited,
CapacityOne, CapacityOneSameKey, ConcurrentAccess, StatsCounting,
StatsEvictions.

### `tests/lru_tests.cpp`

6 tests covering: LRUEvictOldest, LRUGetPreservesKey,
LRUSetUpdateMovesToMRU, LRUEvictionCount, LRUMultiEvict,
LRUOrderAfterDeletes.

### `tests/ttl_tests.cpp`

6 tests covering: TTLNotYetExpired, TTLExpired, TTLExpiredCount,
TTLUpdateClearsTTL, TTLZeroCapacity, TTLDoesNotEvict.
(Some tests call `std::this_thread::sleep_for` to wait for TTL to expire.)

### `benchmarks/benchmark.cpp`

**Why it exists:** Measures actual throughput with different thread
counts. The results show how a single mutex limits scalability.

**What is inside:** Constants (capacity=10000, ops=200000, read=75%),
`worker()` function (each thread does random GETs and SETs),
`runBench(N)` (creates N threads, times them, returns stats),
`main()` (runs for 1/4/8/16/32 threads, prints a table).

### `CMakeLists.txt`

**Why it exists:** Tells CMake how to compile everything.

**What is inside:**
- Sets C++17 standard.
- Calls `find_package(Threads REQUIRED)` — handles pthread on Linux
  and Win32 threads on Windows automatically.
- Defines `cachecore_lib` (static library from `LRUCache.cpp` +
  `Statistics.cpp`).
- Defines `cachecore_cli`, `cachecore_tests`, `cachecore_bench`
  executables, all linking against `cachecore_lib`.
- Calls `enable_testing()` so `ctest` can run the tests.

---

## 4. COMPLETE APPLICATION FLOW

### SET name Ankit

```
User types:  SET name Ankit
                 │
          examples/main.cpp
                 │
  splitTokens() → ["SET", "name", "Ankit"]
  toUpper("SET") == "SET"  ✓
  key   = "name"
  value = "Ankit"
  ttl   = std::nullopt (no TTL given)
                 │
  cache.set("name", "Ankit", nullopt)
                 │
          LRUCache::set()
                 │
  std::lock_guard<std::mutex> lock(mtx_)  ← LOCK ACQUIRED
                 │
  map_.find("name")
  → not found (first time)
                 │
  capacity_ > 0 && map_.size() >= capacity_?
  → No (cache has room)
                 │
  node = new Node("name", "Ankit", false, {})
  map_.emplace("name", node)
  insertAfterHead(node)
    → node becomes MRU: HEAD ⇄ [name] ⇄ TAIL
                 │
  return true    ← LOCK RELEASED (lock_guard destructs)
                 │
  CLI prints: OK
```

### GET name

```
User types:  GET name
                 │
          examples/main.cpp
                 │
  splitTokens() → ["GET", "name"]
  cache.get("name")
                 │
          LRUCache::get()
                 │
  std::lock_guard<std::mutex> lock(mtx_)  ← LOCK ACQUIRED
                 │
  map_.find("name")  → FOUND (iterator `it`)
                 │
  node = it->second   (the Node* we stored earlier)
                 │
  isExpired(node)?
  → node->hasTTL == false → NOT EXPIRED
                 │
  moveToFront(node)
    removeNode(node):  HEAD ⇄ TAIL  (temporarily)
    insertAfterHead(node): HEAD ⇄ [name] ⇄ TAIL
                 │
  stats_.hits.fetch_add(1)
                 │
  return node->value  → "Ankit"  ← LOCK RELEASED
                 │
  CLI prints: Ankit
```

### DELETE name

```
User types:  DELETE name
                 │
          examples/main.cpp
                 │
  cache.remove("name")
                 │
          LRUCache::remove()
                 │
  std::lock_guard<std::mutex> lock(mtx_)  ← LOCK ACQUIRED
                 │
  map_.find("name") → FOUND
                 │
  node = it->second
  removeNode(node):  HEAD ⇄ TAIL  (node unlinked from list)
  map_.erase(it)     (node removed from HashMap)
  delete node         (memory freed)
                 │
  return true    ← LOCK RELEASED
                 │
  CLI prints: OK
```

### STATS

```
User types:  STATS
                 │
          examples/main.cpp
                 │
  cache.statsString()
                 │
          LRUCache::statsString()
                 │
  std::lock_guard<std::mutex> lock(mtx_)  ← LOCK ACQUIRED
                 │
  stats_.format(map_.size(), capacity_)
                 │
          Statistics::format()
          (reads atomic counters without holding lock — safe)
          h  = hits.load()
          m  = misses.load()
          ev = evictions.load()
          ex = expired.load()
          hitRate = 100.0 * h / (h + m)
          builds string with ostringstream
                 │
  returns formatted string ← LOCK RELEASED
                 │
  CLI prints the string
```

### EXIT

```
User types:  EXIT
                 │
          examples/main.cpp
                 │
  cmd == "EXIT"  → true
  cout << "Goodbye!\n"
  break  (exits the while loop)
                 │
  main() returns 0
                 │
  LRUCache destructor runs:
    sets shutdown_ = true (under lock)
    cv_.notify_all()  (wakes background thread)
    cleanupThread_.join()  (waits for thread to finish)
    deletes all Node objects one by one
    deletes head_ and tail_ sentinels
```

---

## 5. KEY-VALUE STORE

A key-value store is like a dictionary. Each entry has two parts:

| Term | What it means | CacheCore example |
|---|---|---|
| Key | The unique name to look up | `"name"`, `"city"`, `"session:123"` |
| Value | The data attached to that key | `"Ankit"`, `"Delhi"`, `"abc123"` |
| Entry | One key + one value together | The whole Node |
| Lookup | Find a value by key | `GET name` |
| Insertion | Add a new key | `SET city Delhi` |
| Update | Change value of existing key | `SET name Rahul` (was Ankit) |
| Deletion | Remove a key entirely | `DELETE name` |

### What happens when you SET an existing key?

```
SET A 10   → new entry, A=10
SET A 20   → A already exists
```

In `LRUCache::set()`:

```cpp
auto it = map_.find(key);          // "A" is found
if (it != map_.end()) {
    Node* node = it->second;
    node->value = value;           // update value: 10 → 20
    // ... TTL handling ...
    moveToFront(node);             // A becomes MRU
    return true;
}
```

The key `"A"` appears exactly once in the HashMap and exactly once in
the linked list. The value is updated **in place** on the existing
node. No new node is created. No duplicate entry is added. The test
`UpdatePreservesSize` verifies this: after two SETs on the same key,
`cache.size()` is still 1.

---

## 6. HASHMAP

### What is a HashMap?

A HashMap stores pairs of (key, value). The key goes through a hash
function that converts it into an array index. The value is stored at
that index.

```
key = "name"
hash("name") → some large number → index 42
array[42] = <pointer to Node>
```

To look up `"name"` again: compute `hash("name")` → 42 → read array[42].
No searching required. That is why it is O(1) average.

### The actual declaration in CacheCore

From `LRUCache.h` line 127:
```cpp
std::unordered_map<std::string, Node*> map_;
```

- **Key type:** `std::string` — the cache key (e.g., `"name"`)
- **Value type:** `Node*` — a raw pointer directly to the node in the
  doubly-linked list

This is the crucial design choice: the HashMap does not store the value
string. It stores a **pointer to the Node**. This pointer lets us jump
directly to the node's position in the linked list in O(1), so we can
move it to the front (MRU) without searching.

### What is a hash function?

A hash function takes a string and converts it to a number.
`std::unordered_map` uses a built-in hash function for `std::string`.

```
hash("name")    → 3847291028
hash("city")    → 9182736450
hash("session") → 1029384756
```

The number is then converted to an array index using modulo.

### What is a hash collision?

Two different keys might produce the same array index:

```
hash("name") % array_size → 42
hash("abcd") % array_size → 42   ← collision!
```

`std::unordered_map` handles collisions internally (using a linked list
at each bucket). When there are many collisions, lookup degrades toward
O(n). In practice, with a good hash function and reasonable load, this
almost never happens.

### Why is complexity "average O(1)", not "guaranteed O(1)"?

Because hash collisions can, in theory, make everything land in the same
bucket. The worst case is O(n). In interviews, always say:
> "`std::unordered_map` provides **average-case O(1)** lookup. Worst
> case is O(n) on collisions, which is rare with a good hash function."

---

## 7. DOUBLY LINKED LIST

### What is a linked list?

An array stores elements at fixed positions by index. A linked list
stores elements anywhere in memory, and each element has a pointer to
the next one.

```
Array:   [A][B][C][D]   ← fixed positions

Linked:  A → B → C → D → null
         each element points to the next
```

### What is a node?

The element stored in the list. In CacheCore, every `Node` holds the
key, value, TTL info, and two pointers: `prev` and `next`.

### What is a doubly linked list?

Each node points to **both** the next element and the previous element:

```
null ← A ⇄ B ⇄ C → null
```

### Why do we need both `prev` and `next`?

To remove a node from the middle of the list in O(1), you need to
update both its neighbors:

```
Before:  A ⇄ B ⇄ C
Remove B:
  B->prev->next = B->next   → A->next = C
  B->next->prev = B->prev   → C->prev = A
After:   A ⇄ C
```

If you only had `next` (singly linked list), you would need to find
the node before B by scanning from the start — that would be O(n).

### Why not an array or vector?

Inserting or removing from the middle of an array/vector is O(n)
because everything after the removed element must shift. The doubly
linked list does the same in O(1).

### The sentinel nodes: HEAD and TAIL

In CacheCore's list, there are two special nodes that never hold real
data:

```
HEAD ⇄ [MRU node] ⇄ ... ⇄ [LRU node] ⇄ TAIL
```

- `head_->next` is always the Most Recently Used real node.
- `tail_->prev` is always the Least Recently Used real node.
- When the cache is empty: `head_->next == tail_` and
  `tail_->prev == head_`.

**Why use sentinels?** Without them, every insert/remove function needs
to check "is this the first node?" and "is this the last node?" with
special `if` statements. Sentinels remove all these edge cases. You
always have neighbors to update — they are just the sentinels. The code
becomes simpler and less bug-prone.

From `LRUCache.cpp` (constructor):
```cpp
head_ = new Node();
tail_ = new Node();
head_->next = tail_;
tail_->prev = head_;
```

---

## 8. NODE

From `include/Node.h`:

```cpp
struct Node {
    std::string key;
    std::string value;
    bool        hasTTL{false};
    std::chrono::steady_clock::time_point expiresAt{};
    Node*       prev{nullptr};
    Node*       next{nullptr};
};
```

| Field | Type | Purpose | Who reads it | Who writes it |
|---|---|---|---|---|
| `key` | `std::string` | The lookup key (e.g., `"name"`) | `evictLRU()` (to erase from map), `cleanupLoop()` | Constructor only |
| `value` | `std::string` | The stored value (e.g., `"Ankit"`) | `get()` (to return to caller) | Constructor, `set()` update path |
| `hasTTL` | `bool` | Does this key have an expiry time? | `isExpired()`, `cleanupLoop()` | Constructor, `set()` update path |
| `expiresAt` | `time_point` | The absolute moment when it expires | `isExpired()` compares to `now()` | Constructor, `set()` update path |
| `prev` | `Node*` | Pointer to the previous node in the list | `removeNode()`, `insertAfterHead()` | `removeNode()`, `insertAfterHead()` |
| `next` | `Node*` | Pointer to the next node in the list | `removeNode()`, `insertAfterHead()` | `removeNode()`, `insertAfterHead()` |

**Why raw pointers?**  
The comment in `Node.h` explains it directly:
> "Smart pointers with doubly-linked lists require weak_ptr back-links
> or shared_ptr cycles, which obscure the core data structure logic
> without adding safety."

`LRUCache` owns all nodes. It calls `new Node(...)` to create them
and `delete node` to destroy them. This is simple, explicit, and easy
to explain.

---

## 9. WHY HASHMAP + DOUBLY LINKED LIST?

This combination is the fundamental insight behind LRU caching. It is
the most important interview concept in this project.

### Problem 1: HashMap alone

```cpp
std::unordered_map<std::string, std::string> cache;
cache["name"] = "Ankit";
```

Fast lookup — O(1). But which key was used least recently? You have
no idea. The HashMap doesn't track order.

### Problem 2: Linked list alone

```
HEAD ⇄ [name=Ankit] ⇄ [city=Delhi] ⇄ [age=22] ⇄ TAIL
```

Order is tracked. The last item is the LRU. But to find `"city"`, you
scan from HEAD: check name, check city — found. That is O(n).

### The solution: Use both together

```
HashMap:   "name" → Node*  ←────┐
           "city" → Node*  ←──┐ │
           "age"  → Node*  ←┐ │ │
                             │ │ │
Linked list: HEAD ⇄ [age] ⇄ [city] ⇄ [name] ⇄ TAIL
```

When `GET name` is called:
1. HashMap: find `"name"` → get `Node*` in O(1)
2. Using the pointer, go directly to that node in the list
3. Move it to front in O(1) (no searching)

The HashMap gives us the pointer. The pointer lets us jump into the
middle of the list instantly. The list maintains order.

**Together:** O(1) average for lookup AND O(1) for reordering.

---

## 10. LRU CACHE

### What does LRU mean?

**Least Recently Used.** When the cache is full and a new item needs to
be stored, we discard the item that was used the longest time ago.

The idea: if something has not been accessed for a long time, it is
probably not needed soon. Get rid of it to make room for something new.

### Why do caches need eviction?

RAM is limited. If you kept adding items forever, you would run out of
memory. Eviction is how the cache limits its own size.

### The vocabulary

| Term | Meaning |
|---|---|
| Capacity | Maximum number of keys the cache can hold |
| MRU | Most Recently Used — the key accessed most recently |
| LRU | Least Recently Used — the key accessed least recently |
| Eviction | Removing a key to make room for a new one |

### A simple example with capacity = 3

```
Step 1: SET A 10
  List: HEAD ⇄ [A] ⇄ TAIL
  A is MRU (and LRU, since it's the only one)

Step 2: SET B 20
  List: HEAD ⇄ [B] ⇄ [A] ⇄ TAIL
  B is MRU, A is LRU

Step 3: SET C 30
  List: HEAD ⇄ [C] ⇄ [B] ⇄ [A] ⇄ TAIL
  C is MRU, A is LRU, cache is FULL

Step 4: GET A
  A is found → move to front
  List: HEAD ⇄ [A] ⇄ [C] ⇄ [B] ⇄ TAIL
  A is now MRU, B is now LRU

Step 5: SET D 40   (cache is full, must evict)
  Evict LRU → evict B
  List: HEAD ⇄ [D] ⇄ [A] ⇄ [C] ⇄ TAIL
  GET B → MISS (B was evicted)
  GET A → "10" (A survived because we accessed it in step 4)
```

This is exactly what the `LRUGetPreservesKey` test in
`tests/lru_tests.cpp` verifies.

---

## 11. LRU INTERNAL OPERATIONS

### `removeNode(Node* node)`

**What it does:** Unlinks a node from the doubly-linked list without
deleting it. The node still exists in memory, just detached from the
list.

**Code from `LRUCache.cpp` lines 74-79:**
```cpp
void LRUCache::removeNode(Node* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->prev = nullptr;
    node->next = nullptr;
}
```

**Pointer diagram (removing B from HEAD ⇄ A ⇄ B ⇄ C ⇄ TAIL):**

```
Before:
  A->next = B
  B->prev = A
  B->next = C
  C->prev = B

After line 1:  node->prev->next = node->next
  A->next = C   (A now skips over B)

After line 2:  node->next->prev = node->prev
  C->prev = A   (C now looks back at A, not B)

After lines 3-4:  node->prev = nullptr; node->next = nullptr
  B->prev = null
  B->next = null   (B is fully detached)

Result: HEAD ⇄ A ⇄ C ⇄ TAIL
        B is floating: null ← B → null
```

**Time complexity:** O(1) — just four pointer assignments.

### `insertAfterHead(Node* node)`

**What it does:** Places a node immediately after the `head_` sentinel,
making it the MRU entry.

**Code from `LRUCache.cpp` lines 85-90:**
```cpp
void LRUCache::insertAfterHead(Node* node) {
    node->next       = head_->next;
    node->prev       = head_;
    head_->next->prev = node;
    head_->next       = node;
}
```

**Pointer diagram (inserting B at front when list is HEAD ⇄ A ⇄ TAIL):**

```
Before:
  head_->next = A
  A->prev = head_

Line 1: node->next = head_->next
  B->next = A

Line 2: node->prev = head_
  B->prev = head_

Line 3: head_->next->prev = node
  A->prev = B

Line 4: head_->next = node
  head_->next = B

Result: HEAD ⇄ B ⇄ A ⇄ TAIL
        B is now MRU
```

**Time complexity:** O(1) — four pointer assignments.

### `moveToFront(Node* node)`

**What it does:** Moves an existing node to the MRU position. Called
on every cache hit.

**Code from `LRUCache.cpp` lines 96-99:**
```cpp
void LRUCache::moveToFront(Node* node) {
    removeNode(node);
    insertAfterHead(node);
}
```

Just two calls. Remove from current position, insert at front.

**Time complexity:** O(1) — constant number of pointer operations.

### `evictLRU()`

**What it does:** Removes and deletes the Least Recently Used node
(which is always at `tail_->prev`).

**Code from `LRUCache.cpp` lines 108-116:**
```cpp
void LRUCache::evictLRU() {
    Node* lru = tail_->prev;
    if (lru == head_) return;   // empty cache, nothing to evict

    removeNode(lru);
    map_.erase(lru->key);       // remove from HashMap too
    delete lru;                  // free memory
    stats_.evictions.fetch_add(1, std::memory_order_relaxed);
}
```

**Pointer diagram (evicting A when list is HEAD ⇄ B ⇄ A ⇄ TAIL):**

```
lru = tail_->prev = A

removeNode(A):
  B->next = TAIL
  TAIL->prev = B

map_.erase("a_key")    ← remove from HashMap
delete A               ← free memory
evictions++

Result: HEAD ⇄ B ⇄ TAIL
        A is gone
```

**Time complexity:** O(1) — direct access to LRU via `tail_->prev`.

---

## 12. GET INTERNAL FLOW

**Command:** `GET B`

```
LRUCache::get("B"):

  Step 1: Acquire lock
    std::lock_guard<std::mutex> lock(mtx_)

  Step 2: Look up "B" in HashMap
    auto it = map_.find("B")

  Step 3: Key not found?
    if (it == map_.end()) {
        stats_.misses.fetch_add(1)
        return std::nullopt    → CLI prints "(nil)"
    }

  Step 4: Get the Node pointer
    Node* node = it->second

  Step 5: Check TTL (lazy expiry)
    if (isExpired(node)) {
        // node->hasTTL == true AND now() > node->expiresAt
        removeNode(node)
        map_.erase(it)
        delete node
        stats_.misses.fetch_add(1)
        stats_.expired.fetch_add(1)
        return std::nullopt    → CLI prints "(nil)"
    }

  Step 6: Move node to MRU position (cache hit)
    moveToFront(node)

  Step 7: Update hit counter
    stats_.hits.fetch_add(1)

  Step 8: Return value
    return node->value    → CLI prints "value_of_B"

  Lock released automatically (lock_guard destructor)
```

**Why does GET require a lock?**

GET is not a pure read. It calls `moveToFront(node)`, which modifies
the doubly-linked list (changes `prev` and `next` pointers). If two
threads did this simultaneously without a lock, they could corrupt the
list. A thread might update a pointer just as another thread is reading
the same pointer.

---

## 13. SET INTERNAL FLOW

### New key: `SET A 10`

```
LRUCache::set("A", "10", nullopt):

  Acquire lock
  map_.find("A") → NOT FOUND

  Eviction check:
    capacity_ > 0 && map_.size() >= capacity_?
    → If YES: evictLRU()  (removes tail_->prev)
    → If NO:  skip

  Create node:
    hasTTL = false (nullopt)
    expiresAt = {} (empty)
    node = new Node("A", "10", false, {})

  Insert into HashMap:
    map_.emplace("A", node)

  Insert into list at MRU:
    insertAfterHead(node)

  return true
  Release lock
```

### Existing key: `SET A 20`

```
LRUCache::set("A", "20", nullopt):

  Acquire lock
  map_.find("A") → FOUND (it points to existing node)

  Update path:
    Node* node = it->second
    node->value = "20"           ← update value in place
    node->hasTTL = false         ← clear any old TTL
    node->expiresAt = {}
    moveToFront(node)            ← A becomes MRU

  return true
  Release lock
```

No new node is created. No eviction happens. The existing node is
updated in place.

### With TTL: `SET session abc TTL 60`

```
ttl = std::optional<std::chrono::seconds>(60)

// In the new-key path:
hasTTL = true
expiresAt = steady_clock::now() + 60s
node = new Node("session", "abc", true, expiresAt)
```

---

## 14. DELETE INTERNAL FLOW

**Command:** `DELETE A`

```
LRUCache::remove("A"):

  Step 1: Acquire lock

  Step 2: Look up "A" in HashMap
    auto it = map_.find("A")

  Step 3: Not found?
    if (it == map_.end()) return false   → CLI prints "(nil)"

  Step 4: Get node pointer
    Node* node = it->second

  Step 5: Unlink from doubly-linked list
    removeNode(node)
    (node->prev and node->next are now null)

  Step 6: Remove from HashMap
    map_.erase(it)

  Step 7: Free memory
    delete node

  Step 8: return true   → CLI prints "OK"

  Lock released
```

After DELETE, the node is completely gone:
- Not in the HashMap (step 6)
- Not in the linked list (step 5)
- Memory is freed (step 7)

Note: DELETE does **not** increment the evictions counter.
Evictions only happen when capacity is exceeded (LRU eviction).
Manual deletes are separate events.

---

## 15. TTL

### What is TTL?

TTL stands for **Time To Live**. It is a duration in seconds after
which a key automatically expires and becomes unavailable.

**Real-world use case:** Store a login session token. You want it to
automatically become invalid after 60 seconds, even if nobody
explicitly deletes it.

### How TTL is stored

From `LRUCache.cpp` (set() function):

```cpp
bool hasTTL = ttl.has_value();
auto expiresAt = hasTTL
    ? std::chrono::steady_clock::now() + *ttl
    : std::chrono::steady_clock::time_point{};

Node* node = new Node(key, value, hasTTL, expiresAt);
```

When you write `SET session abc TTL 60`:

```
Current time (now):  T
TTL:                 60 seconds
expiresAt:           T + 60 seconds  ← stored in node
```

We store the **absolute expiry time**, not the duration. This is better
than storing the duration because: you always know the answer to
"has it expired?" by simply comparing `now()` to `expiresAt`.

### How TTL is checked

From `LRUCache.cpp` lines 120-123:

```cpp
bool LRUCache::isExpired(const Node* node) const noexcept {
    if (!node->hasTTL) return false;
    return std::chrono::steady_clock::now() > node->expiresAt;
}
```

Two conditions:
1. Does the node have a TTL? (`hasTTL == true`)
2. Has the current time passed the expiry time?

If both are true, the key has expired.

---

## 16. LAZY EXPIRATION

### What is lazy expiration?

Lazy expiration means: **don't check TTL at a fixed schedule. Check it
only when someone tries to access the key.**

### Example

```
SET A value TTL 2     → A expires in 2 seconds

Wait 3 seconds...

GET A:
  1. map_.find("A") → FOUND
  2. isExpired(node)?
     → hasTTL == true
     → now() > expiresAt?  → YES  (3s > 2s)
  3. removeNode(node) + map_.erase + delete node
  4. stats_.expired++
  5. return nullopt  → "(nil)"
```

The key is only removed at the moment it is accessed after expiration.

### What if an expired key is never accessed?

It stays in memory until the **background cleanup thread** runs.

In `LRUCache.cpp`, the `cleanupLoop()` function:

```cpp
void LRUCache::cleanupLoop() {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait_for(lock,
                     std::chrono::seconds(cleanupIntervalSec_),
                     [this]{ return shutdown_; });
        if (shutdown_) break;

        // Scan all nodes for expired ones
        std::vector<std::string> toDelete;
        const auto now = std::chrono::steady_clock::now();
        for (const auto& [k, node] : map_) {
            if (node->hasTTL && now > node->expiresAt)
                toDelete.push_back(k);
        }
        // Delete them
        for (const auto& k : toDelete) { ... }
    }
}
```

- Wakes every `cleanupIntervalSec_` seconds.
- In the CLI (`examples/main.cpp`), this is set to **30 seconds**:
  `LRUCache cache(capacity, 30)`.
- In benchmarks, it is set to 0 (disabled) to avoid interference.
- The sweep scans the entire map — O(n) — but runs infrequently.

**Summary:**
- Lazy expiry: correct, immediate, no extra thread needed.
- Background sweep: memory optimization only. Removes expired keys that
  are never accessed again.

---

## 17. CONCURRENCY

### What is a thread?

A thread is an independent sequence of instructions that runs inside
the same program. Multiple threads share the same memory but execute
simultaneously (or appear to, via time-slicing).

### Why might multiple threads access CacheCore?

In a real application, a web server might handle many requests at the
same time. Each request runs in its own thread. If they all share a
cache, they all call `get()` and `set()` concurrently.

### The problem

Both threads work on the same data:

```
Thread 1:   GET A → finds node → starts moveToFront(node)
                                  → changes node->prev->next
Thread 2:   GET B → finds node → starts moveToFront(node)
                                  → reads node->prev
```

If these happen at the same time, Thread 2 might read a pointer while
Thread 1 is in the middle of changing it. The pointer is in an
inconsistent half-updated state. This is a **data race** — undefined
behavior in C++.

---

## 18. RACE CONDITION

### Simple example

Two threads both try to increment a variable:

```
int counter = 0;

Thread 1: counter = counter + 1
Thread 2: counter = counter + 1
```

In three machine instructions per thread:
1. Read counter (0)
2. Add 1 (1)
3. Write back (1)

If both threads read (step 1) before either writes (step 3):
- Thread 1 reads 0, gets 1, writes 1
- Thread 2 reads 0, gets 1, writes 1
- Result: counter = 1   ← should be 2!

### In CacheCore, without locking:

Thread 1 does `GET A` → calls `moveToFront(A)`:
```cpp
// removeNode(A):
A->prev->next = A->next;   // Thread 1 writes this
```

Thread 2 simultaneously does `GET B` → calls `moveToFront(B)`:
```cpp
// removeNode(B):
B->prev->next = B->next;   // Thread 2 writes this
// BUT if B->prev == A:
// Thread 2 reads A->next to store in B->prev->next
// Which A->next? The one before or after Thread 1's write?
```

This leads to:
- Wrong pointers → visiting wrong nodes
- Infinite loops → cycle in the list
- Crashes → null pointer dereference
- Silent wrong results → returning wrong values

This is why every method that touches `map_` or the list must hold
`mtx_`.

---

## 19. MUTEX AND LOCKING

### `std::mutex`

A mutex (short for mutual exclusion) is a lock. At most one thread can
hold it at a time. If Thread 2 tries to lock a mutex already held by
Thread 1, Thread 2 waits (sleeps) until Thread 1 releases it.

In CacheCore: `mutable std::mutex mtx_` in `LRUCache.h` line 134.

### `std::lock_guard<std::mutex>`

Acquires the mutex when created. Releases it automatically when it goes
out of scope (end of function, or even if an exception is thrown).
This is RAII — Resource Acquisition Is Initialization.

From `LRUCache.cpp` in every public method:
```cpp
bool LRUCache::set(...) {
    std::lock_guard<std::mutex> lock(mtx_);  // ← acquires here
    // ... do work ...
    return true;
}  // ← lock released here automatically
```

### `std::unique_lock<std::mutex>`

More flexible than `lock_guard`. Can be released and re-acquired
manually. Required by `condition_variable::wait_for`.

Used **only** in `cleanupLoop()`:
```cpp
std::unique_lock<std::mutex> lock(mtx_);
cv_.wait_for(lock,
             std::chrono::seconds(cleanupIntervalSec_),
             [this]{ return shutdown_; });
```

`wait_for` automatically releases the lock while sleeping (so other
threads can access the cache during cleanup sleep) and re-acquires it
when the thread wakes up.

### The sequence for any operation:

```
Thread arrives at set() / get() / remove()
          ↓
Acquire mtx_  (if another thread holds it, wait here)
          ↓
  CRITICAL SECTION (only one thread here at a time)
  Do the actual work: HashMap + DLL operations
          ↓
Release mtx_  (lock_guard destructor)
          ↓
Other threads can proceed
```

### Why does GET need a lock?

GET modifies the doubly-linked list (`moveToFront`). It is not a
read-only operation. Without the lock, two concurrent GETs would race
to update each other's nodes' `prev`/`next` pointers simultaneously.

---

## 20. CRITICAL SECTION AND LOCK CONTENTION

### What is a critical section?

The piece of code that touches shared data. In CacheCore, every public
method body (after the `lock_guard` line) is a critical section.

**Only one thread can be inside a critical section at a time.**

### Why should it be small?

While Thread 1 is inside the critical section, every other thread
wanting to do any cache operation must wait. The bigger the critical
section, the longer the wait, the worse the throughput.

In CacheCore, the critical section is kept as small as possible:
- Only HashMap and linked list operations happen inside the lock.
- Statistics are `std::atomic` — they are updated without the lock
  (just outside the critical section, or with relaxed atomic operations
  that don't need a lock).

### What is lock contention?

When multiple threads compete for the same lock, only one wins. The
others wait. This waiting is contention.

High contention = threads spend more time waiting than working.

### Benchmark observation

From the actual benchmark results:

```
1 thread:   3,650,701 ops/sec   (zero contention)
4 threads:  1,852,568 ops/sec   (contention begins)
8 threads:  1,896,366 ops/sec   (plateau)
16 threads: 1,644,645 ops/sec   (diminishing returns)
32 threads: 1,693,912 ops/sec   (no improvement)
```

At 1 thread, there is no contention. At 4+ threads, threads wait for
the single mutex, so total throughput plateaus. This is the expected
behavior of a single global mutex.

---

## 21. STATISTICS

From `include/Statistics.h`:

```cpp
struct Statistics {
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<uint64_t> evictions{0};
    std::atomic<uint64_t> expired{0};
};
```

| Counter | When it changes |
|---|---|
| `hits` | +1 on every successful GET (key found, not expired) |
| `misses` | +1 on GET when key not found, OR key is expired |
| `evictions` | +1 when a key is removed by LRU policy (cache full) |
| `expired` | +1 when a key is removed because its TTL passed |

Note: `evictions` and `expired` are separate. If a key expires due
to TTL, it counts as `expired`, not `evictions`. This distinction
matters for understanding why your hit rate dropped.

### How hit rate is calculated

From `Statistics.cpp`:

```cpp
const uint64_t total = h + m;
const double hitRate = total > 0
    ? (100.0 * h / total)
    : 0.0;
```

Example:
```
hits = 87, misses = 13
total = 100
hitRate = 100.0 * 87 / 100 = 87.0%
```

### The STATS output

```
Capacity:     1000
Current Size: 342
Hits:         87
Misses:       13
Evictions:    5
Expired:      3
Hit Rate:     87.0%
```

`Current Size` and `Capacity` are not in `Statistics` — they come from
`map_.size()` and `capacity_` passed in under the lock in
`LRUCache::statsString()`.

---

## 22. BENCHMARKING

### Why benchmarking?

O(1) average is a theoretical claim. "O(1) average" does not tell you
whether 1 million operations take 1 millisecond or 10 seconds. The only
way to know actual performance is to measure it.

### Key concepts

| Term | Meaning |
|---|---|
| Throughput | Total operations completed per second |
| Ops/sec | Operations per second — how fast the system works overall |
| Latency | Time per individual operation — how long ONE operation takes |
| Contention | Threads fighting over the same lock |

### How the benchmark works

From `benchmarks/benchmark.cpp`:

**Parameters:**
```cpp
CACHE_CAPACITY = 10,000 keys
OPS_PER_THREAD = 200,000 operations
READ_RATIO     = 0.75   // 75% GET, 25% SET
KEY_SPACE      = 20,000  // 2× capacity
```

**Setup:**
1. Create cache with capacity 10,000 (no background cleanup thread).
2. Pre-populate 5,000 keys so early GETs are not all misses.
3. `resetStats()` — don't count the setup in results.

**Measurement:**
1. Start high-resolution timer.
2. Launch N threads, each running `worker()` — 200,000 random GET/SET.
3. Join all threads (wait for them all to finish).
4. Stop timer. Calculate elapsed seconds.
5. `opsPerSec = totalOps / elapsed`.

**Thread counts tested:** 1, 4, 8, 16, 32.

The benchmark does NOT invent numbers. It prints whatever it actually
measured on the machine where it ran.

---

## 23. CMAKE

### What is CMake?

A build system generator. You write `CMakeLists.txt` in a simple
language, and CMake converts it into actual Makefiles (or Visual Studio
project files, etc.) for your platform.

### Why do we use it?

Without CMake, you would have to type something like:
```
g++ -std=c++17 -I include -c src/LRUCache.cpp -o LRUCache.o
g++ -std=c++17 -I include -c src/Statistics.cpp -o Statistics.o
ar rcs libcachecore.a LRUCache.o Statistics.o
g++ -std=c++17 -I include examples/main.cpp libcachecore.a -o cachecore_cli -lpthread
```
...and similar for tests and benchmarks. For 15 files this is
error-prone and platform-dependent. CMake handles all of this.

### Key commands in `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.16)
```
Ensures the user's CMake is new enough.

```cmake
project(CacheCore VERSION 1.0.0 LANGUAGES CXX)
```
Names the project and declares we're using C++.

```cmake
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```
Forces C++17. If the compiler doesn't support it, CMake fails with a
clear error.

```cmake
find_package(Threads REQUIRED)
```
Finds the threading library on any platform (pthreads on Linux/macOS,
native threads on Windows). Stores it as `Threads::Threads`.

```cmake
add_library(cachecore_lib STATIC
    src/LRUCache.cpp
    src/Statistics.cpp
)
target_include_directories(cachecore_lib PUBLIC include)
target_link_libraries(cachecore_lib PUBLIC Threads::Threads)
```
Builds a static library `.a` file from two `.cpp` files. Any
executable that links against it gets the `include/` directory
on its include path automatically.

```cmake
add_executable(cachecore_cli examples/main.cpp)
target_link_libraries(cachecore_cli PRIVATE cachecore_lib)
```
Builds the CLI executable by compiling `main.cpp` and linking it
against `cachecore_lib`.

### Commands you type

```bash
# Generate build files (once, or when CMakeLists.txt changes):
cmake -S . -B build -G "MinGW Makefiles"

# Compile everything:
cmake --build build

# Run tests via CTest:
ctest --test-dir build --output-on-failure
```

---

## 24. HOW TO BUILD AND RUN

### Step 1: Prerequisites

You need:
- CMake 3.16+ (installed at `C:\Program Files\CMake\bin`)
- GCC 14+ (w64devkit at `%USERPROFILE%\Desktop\w64devkit\bin`)

### Step 2: Clone the repo

```bash
git clone https://github.com/whoankitchauhan/CacheCore.git
cd CacheCore
```

### Step 3: One-click build (Windows)

```bat
build.bat test     ← build + run all 25 tests
build.bat bench    ← build + run benchmark
```

### Step 4: Manual CMake build (PowerShell)

```powershell
$env:PATH = "$env:USERPROFILE\Desktop\w64devkit\bin;$env:PATH"
& "C:\Program Files\CMake\bin\cmake.exe" -S . -B build -G "MinGW Makefiles"
& "C:\Program Files\CMake\bin\cmake.exe" --build build
```

### Step 5: Run the CLI

```powershell
.\build\cachecore_cli.exe          # capacity defaults to 1000
.\build\cachecore_cli.exe 500      # capacity = 500
.\build\cachecore_cli.exe 0        # capacity = 0 (unlimited)
```

### Step 6: Run the tests

```powershell
.\build\cachecore_tests.exe
```

### Step 7: Run the benchmark

```powershell
.\build\cachecore_bench.exe
```

---

## 25. CLI COMMAND REFERENCE

| Command | Syntax | Expected output |
|---|---|---|
| SET | `SET key value` | `OK` |
| SET with TTL | `SET key value TTL seconds` | `OK` |
| GET (hit) | `GET key` | The stored value |
| GET (miss) | `GET key` | `(nil)` |
| DELETE (exists) | `DELETE key` | `OK` |
| DELETE (missing) | `DELETE key` | `(nil)` |
| STATS | `STATS` | Formatted statistics block |
| EXIT | `EXIT` | `Goodbye!` then program ends |

**Notes:**
- Commands are case-insensitive: `set`, `SET`, `Set` all work.
  (`toUpper()` is called in `main.cpp` line 86.)
- `DEL` also works as an alias for `DELETE`.
- `QUIT` also works as an alias for `EXIT`.
- TTL must be a positive integer. `SET x v TTL 0` gives an error.
- If fewer than the required tokens are provided, you get a usage
  error.

---

## 26. COMPLETE DRY RUN

**Setup:** `LRUCache cache(3, 0)` — capacity 3, no background thread.

---

### `SET A 10`

```
HashMap: { "A" → &NodeA }
List:    HEAD ⇄ [A=10] ⇄ TAIL
Size:    1
LRU:     A
Stats:   hits=0 misses=0 evictions=0 expired=0
```

### `SET B 20`

```
HashMap: { "A" → &NodeA, "B" → &NodeB }
List:    HEAD ⇄ [B=20] ⇄ [A=10] ⇄ TAIL
Size:    2
MRU: B   LRU: A
Stats:   hits=0 misses=0 evictions=0 expired=0
```

### `SET C 30`

```
HashMap: { "A", "B", "C" → ... }
List:    HEAD ⇄ [C=30] ⇄ [B=20] ⇄ [A=10] ⇄ TAIL
Size:    3  (FULL)
MRU: C   LRU: A
Stats:   hits=0 misses=0 evictions=0 expired=0
```

### `GET A`

A is found. Not expired. `moveToFront(A)`:

```
HashMap: { "A", "B", "C" → ... }
List:    HEAD ⇄ [A=10] ⇄ [C=30] ⇄ [B=20] ⇄ TAIL
Size:    3
MRU: A   LRU: B   (B is now the least recently used)
Stats:   hits=1 misses=0 evictions=0 expired=0
Output:  10
```

### `SET D 40`

Cache is full (3/3). Evict LRU = B.

```
evictLRU():
  lru = tail_->prev = Node B
  removeNode(B) → HEAD ⇄ [A] ⇄ [C] ⇄ TAIL
  map_.erase("B")
  delete NodeB
  evictions++

Insert D:
  map_.emplace("D", &NodeD)
  insertAfterHead(NodeD)

HashMap: { "A", "C", "D" → ... }   (B is gone)
List:    HEAD ⇄ [D=40] ⇄ [A=10] ⇄ [C=30] ⇄ TAIL
Size:    3
MRU: D   LRU: C
Stats:   hits=1 misses=0 evictions=1 expired=0
Output:  OK
```

### `GET B`

```
map_.find("B") → NOT FOUND
misses++

HashMap: { "A", "C", "D" → ... }
List:    HEAD ⇄ [D=40] ⇄ [A=10] ⇄ [C=30] ⇄ TAIL  (unchanged)
Stats:   hits=1 misses=1 evictions=1 expired=0
Output:  (nil)
```

### `DELETE A`

```
map_.find("A") → FOUND
removeNode(NodeA): HEAD ⇄ [D] ⇄ [C] ⇄ TAIL
map_.erase("A")
delete NodeA

HashMap: { "C", "D" → ... }
List:    HEAD ⇄ [D=40] ⇄ [C=30] ⇄ TAIL
Size:    2
Stats:   hits=1 misses=1 evictions=1 expired=0
Output:  OK
```

Note: DELETE does NOT change evictions. Evictions are only LRU-triggered.

### `STATS`

```
statsString() reads:
  capacity_  = 3
  map_.size() = 2
  hits       = 1
  misses     = 1
  evictions  = 1
  expired    = 0
  hitRate    = 100.0 * 1 / (1+1) = 50.0%

Output:
  Capacity:     3
  Current Size: 2
  Hits:         1
  Misses:       1
  Evictions:    1
  Expired:      0
  Hit Rate:     50.0%
```

---

## 27. EDGE CASES

| Situation | What CacheCore does |
|---|---|
| Empty cache + GET | `map_.find()` returns `map_.end()`. Miss counter +1. Returns `(nil)`. |
| Missing key | Same as above. |
| Duplicate key (SET twice) | Update path: value updated in place, node moved to MRU. `size()` stays the same. |
| Updating existing key | Same as duplicate key. No new node, no eviction. |
| Capacity = 0 | `if (capacity_ > 0 && ...)` is always false. No eviction ever. Cache grows without limit. |
| Capacity = 1 | Every new unique key evicts the previous one. Verified by `CapacityOne` test. |
| Full cache | `evictLRU()` runs before inserting new key. Exactly one key removed. |
| Eviction | `tail_->prev` is removed in O(1). Eviction counter +1. |
| Expired key (accessed) | Removed lazily in `get()`. Miss counter +1, expired counter +1. |
| Expired key (not accessed) | Remains in memory until background cleanup thread runs (every 30s in CLI). |
| Non-expired TTL | GET returns value normally, moves to MRU. Counted as hit. |
| GET changing LRU order | `moveToFront()` called on every hit. Node moves to `head_->next`. |
| DELETE missing key | `map_.find()` returns end. Returns `false`. CLI prints `(nil)`. No crash. |
| Concurrent access | All methods lock `mtx_`. Only one thread modifies map+list at a time. No data races. |

---

## 28. TIME COMPLEXITY

| Operation | Average Time | Worst Time | Why |
|---|---|---|---|
| GET | O(1) | O(n) | HashMap lookup O(1) avg + `moveToFront` O(1) (4 pointer ops) |
| SET (new key) | O(1) | O(n) | HashMap insert O(1) avg + `insertAfterHead` O(1) |
| SET (update) | O(1) | O(n) | HashMap lookup O(1) avg + value update + `moveToFront` O(1) |
| DELETE | O(1) | O(n) | HashMap erase O(1) avg + `removeNode` O(1) |
| Evict LRU | O(1) | O(1) | `tail_->prev` is direct pointer — always O(1) |
| Background sweep | O(n) | O(n) | Scans all keys to find expired ones |

**"Average O(1)" vs "Guaranteed O(1)":**
- `std::unordered_map` is O(1) average. Worst case is O(n) if all keys
  hash to the same bucket (extremely rare in practice).
- The doubly-linked list operations (`removeNode`, `insertAfterHead`)
  are ALWAYS O(1) — they only move pointers, never search.

**Space complexity:** O(n) where n = number of stored keys. Each key
uses one `Node` (on the heap) and one entry in `map_`. Plus two
sentinel nodes (constant overhead).

---

## 29. MEMORY MANAGEMENT

### How nodes are created

In `LRUCache::set()` (insert path):
```cpp
Node* node = new Node(key, value, hasTTL, expiresAt);
```
`new` allocates the node on the heap. The pointer is stored in `map_`.

### How nodes are deleted

Four places where nodes are deleted:

| Event | Code |
|---|---|
| LRU eviction | `evictLRU()` → `delete lru;` |
| Explicit DELETE | `remove()` → `delete node;` |
| Lazy TTL expiry | `get()` expiry branch → `delete node;` |
| Background sweep | `cleanupLoop()` → `delete it->second;` |
| Cache destructor | Loop through entire list → `delete curr;` |

### Destructor cleanup

```cpp
LRUCache::~LRUCache() {
    // Signal and join background thread first
    { ... } cv_.notify_all(); cleanupThread_.join();

    // Then free all remaining nodes
    Node* curr = head_;      // start from sentinel head
    while (curr) {
        Node* next = curr->next;
        delete curr;          // free this node
        curr = next;
    }
}
```

This walks the list from `head_` (sentinel) through every real node
to `tail_` (sentinel), deleting each one. After this, no memory leaks.

### Why no smart pointers?

`std::shared_ptr` with a doubly-linked list creates a problem:
- `A->next` is a `shared_ptr<Node>` pointing to B
- `B->prev` is a `shared_ptr<Node>` pointing to A
- This is a circular reference → neither A nor B gets freed (reference
  count never reaches zero → memory leak)

Fixing this requires `std::weak_ptr` for back-links, which adds
complexity and obscures the logic. Raw pointers with a clear ownership
rule (LRUCache owns all nodes, deletes them explicitly) are simpler,
clearer, and equally safe when done correctly.

### RAII

RAII = Resource Acquisition Is Initialization. In CacheCore:
- The `std::lock_guard` acquires the mutex on construction, releases on
  destruction. You never forget to unlock.
- The `LRUCache` constructor acquires all resources (allocates
  sentinels, starts thread). The destructor releases them all (joins
  thread, frees memory). No manual cleanup needed by the caller.

---

## 30. DESIGN DECISIONS

| Decision | Problem it solves | Why this choice |
|---|---|---|
| `std::unordered_map` | Need O(1) key lookup | O(1) avg vs O(log n) for `std::map` |
| Doubly linked list | Need O(1) LRU reordering | A vector would be O(n) per remove |
| Sentinel head/tail | Edge cases in DLL ops | No null checks needed at boundaries |
| Raw `Node*` pointers | DLL with smart pointers has cycles | Explicit `new`/`delete` is simpler |
| Single `std::mutex` | Thread safety | GET also writes → shared_mutex gives no benefit |
| `std::atomic` for stats | Avoid locking for counters | Atomics are hardware-supported, no lock needed |
| Lazy TTL expiry | Correctness without extra thread | Simple, no clock skew, no threading complexity |
| Background cleanup thread | Reclaim memory from never-accessed expired keys | Lazy alone leaves memory allocated forever |
| `std::condition_variable` for cleanup | Clean shutdown | Plain `sleep` would delay destructor |
| `std::optional<std::string>` for `get()` | Return "value or nothing" | Better than returning `""` or `-1` |
| Absolute `expiresAt` timestamp | TTL check is simple comparison | No need to track elapsed time |
| String keys and values | Simplicity | Typed values would need `std::variant` — out of scope |
| Capacity 0 = unlimited | Allow no limit | Natural sentinel value, checked with `capacity_ > 0` |

---

## 31. WHY WE DID NOT BUILD REDIS

### CacheCore is:

- An **educational project** built to demonstrate caching concepts.
- A single-process, in-process library + CLI.
- About 600 lines of implementation code.
- Designed to be small enough to understand completely.

### Redis is:

- A production-grade, battle-tested caching server used by companies
  worldwide.
- Supports many data types: strings, lists, sets, sorted sets, hashes,
  bitmaps, streams.
- Has a networking layer: listens on a TCP port (default 6379) and
  speaks the RESP protocol.
- Has persistence: RDB (snapshots) and AOF (append-only file).
- Supports replication, clustering, and Sentinel for high availability.
- Has Lua scripting, pub/sub, transactions, and many other features.
- Is hundreds of thousands of lines of C code.

CacheCore is **not** a Redis clone. It is an implementation of the same
fundamental idea (HashMap + linked list for LRU) in a small,
educational package. The comment in `Node.h` says exactly this:

> "CacheCore is an educational implementation of fundamental caching
> concepts and is not intended to replace Redis."

---

## 32. WHAT CACHECORE DOES NOT DO

These features are **NOT implemented** in the current version:

| Feature | Status |
|---|---|
| Persistence (save to disk) | ❌ Not implemented |
| Networking (TCP server) | ❌ Not implemented |
| Multiple data types (lists, sets, hashes) | ❌ Not implemented — values are only strings |
| Distributed caching | ❌ Not implemented |
| Replication | ❌ Not implemented |
| Clustering | ❌ Not implemented |
| Fault tolerance | ❌ Not implemented |
| Authentication | ❌ Not implemented |
| Multiple eviction policies (LFU, FIFO, random) | ❌ Not implemented — only LRU |
| Sharded locking (one mutex per shard) | ❌ Not implemented — single global mutex |
| TTL priority queue (O(log n) cleanup) | ❌ Not implemented — background sweep is O(n) |
| SCAN or key enumeration | ❌ Not implemented |

When an interviewer asks "Can CacheCore do X?", check this list and
answer honestly. It is better to say "that is not in the current
implementation" than to claim a feature that does not exist.

---

## 33. INTERVIEW QUESTIONS AND ANSWERS

### BASIC

**Q: What is CacheCore?**

A: CacheCore is a concurrent in-memory key-value store I built in C++17.
It stores string key-value pairs in RAM and supports LRU eviction, TTL
expiration, thread-safe access, and statistics. It is an educational
project demonstrating how caching systems work at the data structure level.

**Q: What is a cache?**

A: A cache is a fast storage layer that holds copies of frequently
accessed data. Instead of fetching the same data from a slow source
(like a database) every time, you store it in a cache and retrieve it
from RAM in nanoseconds.

**Q: What is an in-memory store?**

A: Data lives entirely in RAM. It is much faster to read than disk, but
the data is lost when the program exits. CacheCore does not persist
data to disk.

**Q: What is a key-value store?**

A: Every piece of data has a unique key (a name) and a value (the data).
You store with `SET key value` and retrieve with `GET key`. Think of it
like a dictionary.

**Q: Why do we need caching?**

A: To avoid repeatedly fetching the same data from a slow source. If
100 requests all need the same user profile, fetching it once and
caching the result is much faster than hitting the database 100 times.

---

### DATA STRUCTURES

**Q: Why `std::unordered_map`?**

A: We need to find a node in the linked list given only its key. The
HashMap gives us a direct pointer to that node in O(1) average time.
`std::map` would be O(log n). For a cache, O(1) matters.

**Q: What is hashing?**

A: A hash function converts a key (like `"name"`) into a large integer.
That integer is used as an array index. This lets us store and retrieve
values without searching — just compute the index and look it up directly.

**Q: What is a hash collision?**

A: Two different keys producing the same array index. `std::unordered_map`
handles this internally. In the worst case (all keys collide), lookup
becomes O(n). In practice, with a good hash function, this is very rare.

**Q: Why a doubly linked list?**

A: We need to remove a node from anywhere in the list and insert it at
the front — all in O(1). A doubly linked list lets us do this with just
four pointer assignments because each node knows its neighbors directly.
A singly linked list or array would require O(n) to find the predecessor
or shift elements.

**Q: Why not a singly linked list?**

A: To remove node B from A→B→C, you need A's pointer to update
`A->next = C`. With a singly linked list, you must scan from the start
to find A. That's O(n). With a doubly linked list, `B->prev` gives you
A directly — O(1).

**Q: Why HashMap + linked list?**

A: HashMap alone can't track access order. Linked list alone is O(n) to
find a specific key. Together: the HashMap gives us a direct pointer to
the node in O(1), and the list maintains order. We get O(1) lookup AND
O(1) LRU reordering.

---

### LRU

**Q: What is LRU?**

A: Least Recently Used. When the cache is full and a new key needs to be
stored, we evict the key that was accessed least recently. The idea:
if you haven't used something in a long time, you probably won't need it
soon.

**Q: Why do we need eviction?**

A: RAM is limited. Without eviction, a cache would grow until the program
runs out of memory.

**Q: What is MRU?**

A: Most Recently Used — the key that was accessed most recently. In our
linked list, it is always at `head_->next`.

**Q: How does GET change LRU order?**

A: Every GET that hits a key calls `moveToFront(node)`, which unlinks the
node from its current position and reinserts it at `head_->next`. Now it
is the MRU and will be the last to be evicted.

**Q: How does eviction work?**

A: `evictLRU()` looks at `tail_->prev` (the LRU node), removes it from
the list and the HashMap, and frees its memory. This is O(1) because
`tail_->prev` is a direct pointer.

---

### TTL

**Q: What is TTL?**

A: Time To Live. A duration in seconds after which a key automatically
expires. `SET session abc TTL 60` means the key is valid for 60 seconds.

**Q: Why store `expiresAt` instead of a duration?**

A: `expiresAt = now() + duration`. Storing the absolute expiry time means
the check is always `now() > expiresAt` — a single comparison. If we stored
the duration, we'd need to track when the key was inserted and compute
elapsed time every check.

**Q: What is lazy expiration?**

A: We don't scan for expired keys on a timer. We check TTL only when a
specific key is accessed. If the key is expired at that moment, we delete
it then. This is simple and correct — no extra thread needed for correctness.

**Q: What happens when a key expires?**

A: On `GET` of an expired key: the node is removed from the list and the
HashMap, memory is freed, the `misses` and `expired` counters are
incremented, and `(nil)` is returned. This is the lazy expiry path.
If the key is never accessed again, the background cleanup thread removes
it during its periodic sweep.

---

### CONCURRENCY

**Q: What is a thread?**

A: An independent sequence of instructions running inside the same
program. Multiple threads share memory and run simultaneously (or
appear to via time-slicing).

**Q: What is a race condition?**

A: When the result of a program depends on which thread runs first. If
two threads modify the same data simultaneously without coordination,
the data can be corrupted.

**Q: Why do we need a mutex?**

A: The HashMap and linked list are shared between all threads. If two
threads modify the list at the same time, they can corrupt each other's
pointer updates. The mutex ensures only one thread can modify shared
state at a time.

**Q: Why does GET need locking?**

A: GET is not read-only. It calls `moveToFront()`, which modifies the
doubly-linked list (changes `prev` and `next` pointers). Without a lock,
concurrent GETs would race to update the same pointers simultaneously,
corrupting the list.

**Q: What is a critical section?**

A: The code that accesses shared data. In CacheCore, it is everything
between acquiring and releasing `mtx_`. Only one thread can be in a
critical section at a time.

**Q: What is lock contention?**

A: When multiple threads compete for the same lock. The losers must wait.
High contention means threads spend more time waiting than working. This
is visible in the benchmark: throughput plateaus above 4 threads because
all threads share one mutex.

---

### COMPLEXITY

**Q: Why is GET O(1) average?**

A: `map_.find(key)` is O(1) average (hash table lookup). `moveToFront`
is O(1) (four pointer assignments). Together: O(1) average.

**Q: Why is SET O(1) average?**

A: For new keys: `map_.emplace` is O(1) average. `insertAfterHead` is
O(1). Eviction (`evictLRU`) is O(1) — direct access via `tail_->prev`.
For updates: `map_.find` O(1), value update O(1), `moveToFront` O(1).

**Q: What are the limitations of `std::unordered_map`?**

A: Worst case is O(n) on hash collisions. In extreme cases (all keys hash
to the same bucket), all operations degrade to O(n). In practice this
is very rare with good input data and a good hash function.

---

### C++

**Q: What is RAII?**

A: Resource Acquisition Is Initialization. Resources (locks, memory) are
acquired in a constructor and released in the destructor. `std::lock_guard`
acquires the mutex on creation and releases it when it goes out of scope —
even if an exception is thrown. In CacheCore, the destructor frees all
nodes and joins the background thread.

**Q: Why use references?**

A: `const std::string& key` passes the key without copying it. Copying
a large string for every GET would be wasteful.

**Q: Why use raw pointers?**

A: The doubly-linked list uses raw `Node*` pointers because smart
pointers with two-way links create circular references (shared_ptr
cycles that prevent deallocation). Raw pointers with clear ownership
rules (LRUCache owns all nodes) are simpler and equally safe here.

**Q: What is const correctness?**

A: Using `const` where data should not be modified. `size() const`,
`capacity() const`, `stats() const`, and `isExpired(const Node*) const`
promise not to modify the object. `mutable std::mutex mtx_` is marked
mutable so `const` methods can still lock it.

---

### SYSTEM DESIGN

**Q: How would you scale CacheCore?**

A: The main bottleneck is the single mutex. One improvement: shard the
cache into N buckets, each with its own mutex. Threads accessing
different keys can proceed simultaneously. This is called "sharded
locking."

**Q: How would distributed caching work?**

A: Not implemented in CacheCore. A distributed cache (like Redis Cluster)
distributes keys across multiple machines. Consistent hashing decides
which machine stores which key. CacheCore is single-machine.

**Q: What is consistent hashing?**

A: A technique for distributing keys across N machines such that adding
or removing a machine only moves ~1/N of the keys, not all of them.
Not implemented in CacheCore.

**Q: How would you add persistence?**

A: Two options: (1) Snapshots — periodically write all key-value pairs
to a file. On restart, load the file. (2) Write-ahead log — append
every SET/DELETE to a file. On restart, replay the log. Neither is
implemented in CacheCore.

---

## 34. "EXPLAIN THIS CODE" SECTION

### `Node` struct (`include/Node.h`)

```cpp
struct Node {
    std::string key;
    std::string value;
    bool        hasTTL{false};
    std::chrono::steady_clock::time_point expiresAt{};
    Node*       prev{nullptr};
    Node*       next{nullptr};
};
```

**What it does:** Holds one cache entry. It is the element of the
doubly-linked list and the value pointed to by the HashMap.

**Why it exists:** The cache needs to store the key-value pair, TTL
info, and the list navigation pointers all in one object that both
the HashMap and the list can reference simultaneously.

**Interactions:** Created by `LRUCache::set()`, navigated by all DLL
helper functions, deleted by `evictLRU()`, `remove()`, `get()` (expiry
path), and the destructor.

**Complexity:** O(1) to construct. Just copies/moves the key and value.

**Edge cases:** The default constructor creates sentinel nodes with
empty key, value, no TTL, and null pointers. The normal constructor
takes key and value and allows optional TTL.

---

### `removeNode` (`src/LRUCache.cpp` lines 74-79)

```cpp
void LRUCache::removeNode(Node* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->prev = nullptr;
    node->next = nullptr;
}
```

**What it does:** Removes a node from the linked list (does not free
memory).

**Why it exists:** Used by `moveToFront` (before re-inserting at front),
`evictLRU` (before deleting), `remove` (before deleting), and the
expiry path in `get`.

**Pointer changes:**
- Line 1: The node before `node` now skips over `node`.
- Line 2: The node after `node` now looks back past `node`.
- Lines 3-4: Clear the detached node's pointers (defensive programming).

**Complexity:** O(1).

**Edge cases:** Works correctly when `node` is the only real node in the
list because its neighbors are the sentinels (head_ and tail_), which
are always present. No null pointer risk.

---

### `insertAfterHead` (`src/LRUCache.cpp` lines 85-90)

```cpp
void LRUCache::insertAfterHead(Node* node) {
    node->next       = head_->next;
    node->prev       = head_;
    head_->next->prev = node;
    head_->next       = node;
}
```

**What it does:** Places `node` at the MRU position (immediately after
the head sentinel).

**Why it exists:** Called after every successful SET (new key) and after
every cache hit in GET (via `moveToFront`).

**The order of these four lines matters:**
- You must save `head_->next` into `node->next` BEFORE overwriting
  `head_->next`.
- Line 3 (`head_->next->prev = node`) uses the OLD value of `head_->next`
  (now `node->next`) to update the previously-first node's back-pointer.

**Complexity:** O(1).

---

### `LRUCache::set` (`src/LRUCache.cpp` lines 136-173)

```cpp
bool LRUCache::set(const std::string& key, const std::string& value,
                   std::optional<std::chrono::seconds> ttl) {
    std::lock_guard<std::mutex> lock(mtx_);    // acquire lock

    auto it = map_.find(key);
    if (it != map_.end()) {                    // key exists → update
        Node* node = it->second;
        node->value = value;
        if (ttl.has_value()) { ... set TTL ... }
        else                 { node->hasTTL = false; ... }
        moveToFront(node);
        return true;
    }

    if (capacity_ > 0 && map_.size() >= capacity_) {
        evictLRU();                            // make room
    }

    Node* node = new Node(key, value, hasTTL, expiresAt);
    map_.emplace(key, node);
    insertAfterHead(node);
    return true;
}                                              // lock released here
```

**What it does:** Stores a key-value pair. Updates in place if key
exists (no duplicate), creates new if not. Evicts LRU if at capacity.

**Two paths:**
1. **Update path** (key found): updates value + TTL + moveToFront. No
   allocation, no eviction.
2. **Insert path** (key not found): maybe evict, allocate, insert.

**Complexity:** O(1) average.

**Edge case:** `capacity_ == 0` → the eviction check never triggers.
Unlimited cache.

---

### `LRUCache::get` (`src/LRUCache.cpp` lines 186-210)

```cpp
std::optional<std::string> LRUCache::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = map_.find(key);
    if (it == map_.end()) {
        stats_.misses.fetch_add(1, std::memory_order_relaxed);
        return std::nullopt;
    }

    Node* node = it->second;
    if (isExpired(node)) {
        removeNode(node); map_.erase(it); delete node;
        stats_.misses.fetch_add(1, std::memory_order_relaxed);
        stats_.expired.fetch_add(1, std::memory_order_relaxed);
        return std::nullopt;
    }

    moveToFront(node);
    stats_.hits.fetch_add(1, std::memory_order_relaxed);
    return node->value;
}
```

**Three paths:**
1. Key not in map → miss, return nullopt.
2. Key in map but expired → remove, miss + expired, return nullopt.
3. Key in map, valid → moveToFront, hit, return value.

**Complexity:** O(1) average.

---

### `LRUCache::remove` (`src/LRUCache.cpp` lines 217-228)

```cpp
bool LRUCache::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = map_.find(key);
    if (it == map_.end()) return false;
    Node* node = it->second;
    removeNode(node);
    map_.erase(it);
    delete node;
    return true;
}
```

**What it does:** Removes a key completely. Returns `false` if not found.

**Three steps:** unlink from list, erase from map, free memory.

Note: does not increment `evictions`. DELETE is explicit, not an LRU
eviction.

**Complexity:** O(1) average.

---

### `cleanupLoop` (`src/LRUCache.cpp` lines 268-300)

```cpp
void LRUCache::cleanupLoop() {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait_for(lock,
                     std::chrono::seconds(cleanupIntervalSec_),
                     [this]{ return shutdown_; });
        if (shutdown_) break;

        std::vector<std::string> toDelete;
        const auto now = std::chrono::steady_clock::now();
        for (const auto& [k, node] : map_) {
            if (node->hasTTL && now > node->expiresAt)
                toDelete.push_back(k);
        }
        for (const auto& k : toDelete) { ... delete ... }
    }
}
```

**What it does:** Background sweep that removes expired keys even if they
are never accessed.

**Why `toDelete` vector?** You cannot erase from `map_` while iterating
it — that would invalidate the iterator. So we first collect expired
keys into a separate vector, then erase them in a second loop.

**Why `condition_variable`?** `wait_for` sleeps for the interval but
wakes immediately if `shutdown_` becomes true. The destructor sets
`shutdown_ = true` and calls `cv_.notify_all()`, causing this function
to exit promptly.

**Complexity:** O(n) per sweep, where n = number of keys. Runs
infrequently (every 30s in the CLI).

---

## 35. COMMON CONFUSIONS

| Concept A | vs | Concept B | Key difference |
|---|---|---|---|
| Cache | Database | Cache is fast but temporary. Database is slow but persistent. |
| HashMap (`map_`) | Linked list | HashMap: fast find by key. List: fast reorder by position. |
| MRU | LRU | MRU = used most recently = at front. LRU = used least recently = at back. |
| TTL expiry | LRU eviction | TTL: key expires by time. Eviction: key removed because cache is full. |
| Hit | Miss | Hit: key found and returned. Miss: key not found or expired. |
| Thread | Process | Process = separate program with its own memory. Thread = worker within one process, sharing memory. |
| `std::mutex` | `std::atomic` | Mutex: for protecting blocks of code with multiple operations. Atomic: for single operations (increment) on a single variable. |
| Lazy expiration | Active (background) expiration | Lazy: check only on access. Active: scan periodically. CacheCore does both. |
| Average O(1) | Guaranteed O(1) | Average O(1): fast almost always, but hash collisions can degrade to O(n). Guaranteed O(1): e.g., doubly linked list operations — always constant. |
| CMake | Compiler | CMake generates build files. The compiler (g++) actually converts C++ code to machine code. |
| Source code | Executable | `.cpp` and `.h` files are source. `cachecore_cli.exe` is the executable after compilation. |

---

## 36. 30-SECOND PROJECT EXPLANATION

> "CacheCore is a concurrent in-memory key-value store I built in C++17.
> The core data structure is a hashmap combined with a doubly-linked list.
> The hashmap gives O(1) average-case key lookup, and the linked list
> maintains access order for LRU eviction. When the cache is full, the
> least recently used key is evicted to make room for new data. Keys can
> also have an optional TTL — they expire automatically after a set number
> of seconds. The cache is thread-safe using a mutex, and I verified this
> with a concurrent test using 8 threads. I also wrote a benchmark that
> measures throughput across 1 to 32 threads, and a CLI that supports
> SET, GET, DELETE, STATS, and EXIT."

---

## 37. 2-MINUTE PROJECT EXPLANATION

> "CacheCore is an in-memory key-value store I built from scratch in C++17
> to deeply understand how caching systems work.
>
> **The problem:** Programs often need the same data repeatedly. Fetching
> it every time from a database is slow. A cache stores it in RAM for
> fast retrieval.
>
> **The architecture:** The core is a hashmap combined with a
> doubly-linked list. The hashmap — specifically `std::unordered_map<string, Node*>` —
> maps each key to a pointer directly into the linked list. This gives
> us O(1) average-case lookup. The linked list maintains access order:
> the most recently used item is at the front, the least recently used
> is at the back.
>
> **LRU eviction:** When the cache hits its capacity, the node at the
> back of the list — the least recently used — is evicted in O(1) time
> by accessing `tail->prev` directly.
>
> **TTL:** Keys can expire. When SET is called with a TTL, I store an
> absolute expiry timestamp in the node. On GET, I check if the current
> time has passed that timestamp. If yes, the key is removed lazily.
> A background thread also periodically sweeps for expired keys that
> were never accessed again.
>
> **Concurrency:** Every public method acquires a `std::mutex` at the
> start. This is important even for GET, because GET modifies the
> linked list order. Statistics counters use `std::atomic` so they can
> be updated from any thread without holding the main lock.
>
> **Results:** The project has 25 automated tests covering basic ops,
> LRU ordering, TTL expiration, and 8-thread concurrent access. The
> benchmark showed ~3.6 million operations per second with 1 thread,
> plateauing around 1.9M with higher thread counts — a clear
> demonstration of single-mutex contention."

---

## 38. FINAL KNOWLEDGE CHECKLIST

- [x] What CacheCore is
- [x] Why caching is needed
- [x] What in-memory means
- [x] What key-value storage means
- [x] How SET works (new key path + update path)
- [x] How GET works (hit / miss / TTL expiry paths)
- [x] How DELETE works
- [x] How `std::unordered_map` works
- [x] How hashing works
- [x] What a linked list is
- [x] What a doubly linked list is
- [x] Why we use a doubly linked list (O(1) removal + insertion)
- [x] What sentinel nodes are and why we use them
- [x] What LRU means
- [x] How eviction works (`tail_->prev` direct access)
- [x] What TTL means
- [x] Why we store absolute `expiresAt` instead of duration
- [x] What lazy expiration is
- [x] What the background cleanup thread does
- [x] What a thread is
- [x] What a race condition is
- [x] Why mutex is needed
- [x] Why GET requires locking (modifies DLL order)
- [x] Difference between `lock_guard` and `unique_lock`
- [x] Why `condition_variable` is used in cleanup thread
- [x] What `std::atomic` is and when to use it instead of mutex
- [x] How statistics work
- [x] What hit rate means and how it is calculated
- [x] How the benchmark works
- [x] What lock contention is
- [x] How CMake builds the project
- [x] How to run the CLI
- [x] How to run tests
- [x] How to run benchmarks
- [x] Average O(1) vs guaranteed O(1)
- [x] Space complexity: O(n)
- [x] Why raw pointers instead of smart pointers in the DLL
- [x] What RAII means
- [x] What CacheCore does NOT do (no persistence, no networking)
- [x] How to explain the project in 30 seconds and 2 minutes

---

*This guide was written against the actual CacheCore source code.
Every code snippet shown here is taken directly from the project.
Every behavior described matches what the code actually does.*
