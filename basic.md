# CacheCore – Quick Guide & 50 Interview Questions (A to Z)

---

## 1. What is this project?
**CacheCore** is an in-memory, thread-safe key-value store built in C++17.
It works like a fast dictionary in your computer's RAM:
* Store data with `SET key value [TTL seconds]`.
* Retrieve data with `GET key`.
* Delete data with `DELETE key`.
* Inspect metrics with `STATS`.

---

## 2. What problem does it solve?
Reading from a database or remote API takes milliseconds because of network and disk I/O.
Reading from RAM takes nanoseconds. A cache temporarily holds frequently used data in memory to eliminate latency bottlenecks.

---

## 3. How does it work internally?
We combine two data structures:

1. **`std::unordered_map<std::string, Node*>`**:
   * Gives $O(1)$ average time key lookup.
   * Maps each key directly to the pointer of that item's node in memory.

2. **Custom Doubly Linked List (DLL)**:
   * Keeps track of access history.
   * **Front (`head_->next`)** = Most Recently Used (MRU).
   * **Back (`tail_->prev`)** = Least Recently Used (LRU).
   * Moving or removing a node is strictly $O(1)$ pointer assignments.

### Core Features
* **LRU Eviction**: When `size >= capacity`, the item at `tail_->prev` is dropped to make room.
* **TTL (Time-To-Live)**: Keys can have an expiration time. Handled both lazily (on `GET`) and actively (via a background sweep thread).
* **Thread Safety**: A `std::mutex` serializes writes and list updates so multiple threads can run safely without race conditions.
* **Atomic Stats**: Lock-free counters (`std::atomic<uint64_t>`) track hits, misses, evictions, and expired keys.

---

## 4. How to Run It

```cmd
.\build.bat          # Compile library, CLI, tests, benchmark
.\build.bat test     # Run all 25 unit tests
.\build.bat bench    # Run throughput benchmark
.\build\cachecore_cli.exe   # Start interactive CLI
```

---

# 5. 50 Real Interview Questions & Answers (A to Z)

These 50 questions cover every layer of CacheCore: high-level concepts, data structures, low-level pointers, concurrency, modern C++, edge cases, and systems scaling.

---

### Category 1: Project Overview & Fundamentals (Q1 – Q7)

#### Q1: "What is CacheCore?"
> **Answer:**  
> "CacheCore is an in-memory key-value store built in C++17. It allows fast data storage and retrieval in RAM using commands like `SET`, `GET`, and `DELETE`. It features $O(1)$ LRU eviction, TTL-based expiration, thread safety, and lock-free statistics tracking."

#### Q2: "Why build an in-memory cache instead of using a database directly?"
> **Answer:**  
> "Databases persist data to disk and handle complex query logic, which takes milliseconds. In-memory caches hold frequently queried data in RAM, returning answers in nanoseconds. This reduces database load and speeds up response times."

#### Q3: "How do you know data is stored in RAM and not on disk?"
> **Answer:**  
> "When we construct nodes via `new Node(...)` and store them in `std::unordered_map`, the OS allocates this memory in our process heap space in RAM. Our code contains zero file I/O or disk system calls (`write()`, `fsync()`, `ofstream`). If the process terminates, all data ceases to exist."

#### Q4: "What does 'concurrency' mean in the context of this project?"
> **Answer:**  
> "It means multiple threads can invoke cache operations (`set`, `get`, `remove`) at the same time without causing memory corruption, data races, or invalid pointer states."

#### Q5: "Is CacheCore a Redis clone?"
> **Answer:**  
> "No. CacheCore is an educational, in-process C++ library demonstrating core caching data structures. Redis is a distributed production server with networking (TCP/RESP), persistence (RDB/AOF), clustering, replication, and diverse data structures (sets, sorted sets, streams)."

#### Q6: "What are the core commands supported by the CLI?"
> **Answer:**  
> "The CLI supports:
> * `SET key value [TTL seconds]`
> * `GET key`
> * `DELETE key` (or `DEL`)
> * `STATS`
> * `EXIT` (or `QUIT`)"

#### Q7: "What happens if a user passes capacity 0 to the constructor?"
> **Answer:**  
> "Capacity `0` indicates unlimited capacity. The eviction check `capacity_ > 0 && map_.size() >= capacity_` evaluates to false, allowing the cache to grow without ever evicting keys."

---

### Category 2: Data Structures & Architecture (Q8 – Q16)

#### Q8: "What two main data structures form CacheCore?"
> **Answer:**  
> "A hash table (`std::unordered_map<std::string, Node*>`) and a custom doubly linked list with sentinel `head_` and `tail_` nodes."

#### Q9: "Why combine a HashMap and a Doubly Linked List?"
> **Answer:**  
> "A HashMap provides $O(1)$ average key lookups but has no access order. A linked list tracks order but searching for a key is $O(N)$. Combining them gives the best of both: the map yields direct $O(1)$ access to a node in the list, and the doubly linked list allows $O(1)$ reordering to front."

#### Q10: "What does the `Node` structure contain?"
> **Answer:**  
> "Each node contains:
> * `std::string key` and `std::string value`
> * `bool hasTTL` and `std::chrono::steady_clock::time_point expiresAt`
> * Two raw pointers: `Node* prev` and `Node* next`"

#### Q11: "Why use a Doubly Linked List instead of a Singly Linked List?"
> **Answer:**  
> "To remove a node in $O(1)$, we must update the `next` pointer of the node *before* it. A doubly linked list gives us `node->prev` directly. In a singly linked list, finding the preceding node requires an $O(N)$ linear traversal from the head."

#### Q12: "Why not use `std::vector` instead of a linked list?"
> **Answer:**  
> "Removing or inserting an element at the front or middle of a vector requires shifting all following elements, which is an $O(N)$ memory-copy operation. A linked list unlinks and relinks nodes in $O(1)$ time by adjusting four pointers."

#### Q13: "What are Sentinel Nodes (dummy head and tail)?"
> **Answer:**  
> "They are placeholder nodes that never store user data. The list is initialized with `head_->next = tail_` and `tail_->prev = head_`. Real items are always inserted between them."

#### Q14: "Why use Sentinel Nodes instead of `nullptr` for list boundaries?"
> **Answer:**  
> "Sentinels eliminate edge-case conditionals like `if (head == nullptr)` or `if (node == tail)`. Every valid node always has non-null `prev` and `next` neighbors, dramatically simplifying insertion and deletion code and avoiding null-pointer dereferencing bugs."

#### Q15: "What does the HashMap actually store as its value?"
> **Answer:**  
> "It stores a raw pointer `Node*` pointing to the exact node in the doubly linked list, not a duplicate copy of the value string."

#### Q16: "Why store the key inside the Node when it is already in the HashMap?"
> **Answer:**  
> "When the cache evicts the oldest item at `tail_->prev`, we only have the `Node*`. To erase it from the hash table via `map_.erase(key)`, the node must know its own key."

---

### Category 3: LRU Eviction Logic (Q17 – Q21)

#### Q17: "What does LRU stand for, and what is the intuition behind it?"
> **Answer:**  
> "Least Recently Used. It operates on temporal locality: data accessed recently will likely be accessed again soon. Therefore, the item that has gone unaccessed the longest is the best candidate to discard when memory is full."

#### Q18: "What are MRU and LRU positions in your linked list?"
> **Answer:**  
> "* **MRU (Most Recently Used):** Located right after the head sentinel (`head_->next`).
> * **LRU (Least Recently Used):** Located right before the tail sentinel (`tail_->prev`)."

#### Q19: "How does `removeNode(Node* node)` work?"
> **Answer:**  
> "It unlinks the node by connecting its neighbors to each other:
> ```cpp
> node->prev->next = node->next;
> node->next->prev = node->prev;
> node->prev = nullptr;
> node->next = nullptr;
> ```
> This takes strictly $O(1)$ constant time."

#### Q20: "How does `insertAfterHead(Node* node)` work?"
> **Answer:**  
> "It splices the node immediately after the `head_` sentinel:
> ```cpp
> node->next = head_->next;
> node->prev = head_;
> head_->next->prev = node;
> head_->next = node;
> ```
> This makes the node the new MRU item in $O(1)$ time."

#### Q21: "Walk through what `evictLRU()` does."
> **Answer:**  
> "1. It identifies `Node* lru = tail_->prev`.
> 2. If `lru == head_`, the cache is empty, so it returns.
> 3. It calls `removeNode(lru)` to detach it from the list.
> 4. It calls `map_.erase(lru->key)` to remove it from the map.
> 5. It invokes `delete lru` to free heap memory.
> 6. It increments `stats_.evictions`."

---

### Category 4: Internal Operation Flows (Q22 – Q26)

#### Q22: "Trace the step-by-step execution of `GET key`."
> **Answer:**  
> "1. Lock the mutex (`std::lock_guard<std::mutex>`).
> 2. Look up key in `map_`. If missing, increment `misses` and return `std::nullopt`.
> 3. If present, check `isExpired(node)`. If expired, unlink, erase from map, delete node, increment `misses` + `expired`, and return `std::nullopt`.
> 4. If valid, call `moveToFront(node)`.
> 5. Increment `hits` and return `node->value`."

#### Q23: "Trace the step-by-step execution of `SET key value` for an existing key."
> **Answer:**  
> "1. Lock the mutex.
> 2. Search `map_.find(key)`. Since it exists, grab `it->second`.
> 3. Overwrite `node->value = value`.
> 4. Update or clear TTL metadata.
> 5. Call `moveToFront(node)` so it becomes the MRU.
> 6. Return `true` without allocating a new node or evicting."

#### Q24: "Trace the step-by-step execution of `SET key value` for a brand-new key."
> **Answer:**  
> "1. Lock the mutex.
> 2. Check if `capacity_ > 0 && map_.size() >= capacity_`. If so, invoke `evictLRU()`.
> 3. Construct `Node* node = new Node(key, value, hasTTL, expiresAt)`.
> 4. Insert into hash table: `map_.emplace(key, node)`.
> 5. Insert into list: `insertAfterHead(node)`.
> 6. Return `true`."

#### Q25: "Trace the execution of `DELETE key`."
> **Answer:**  
> "1. Lock the mutex.
> 2. Search `map_.find(key)`. If missing, return `false`.
> 3. Unlink from list: `removeNode(node)`.
> 4. Erase from hash table: `map_.erase(it)`.
> 5. Free memory: `delete node`.
> 6. Return `true`."

#### Q26: "Does `DELETE` increment the eviction counter?"
> **Answer:**  
> "No. Evictions refer strictly to capacity-forced drops managed by the LRU policy. Manual deletions are deliberate removals and are not counted as cache evictions."

---

### Category 5: TTL & Expiration Mechanics (Q27 – Q31)

#### Q27: "What is TTL?"
> **Answer:**  
> "Time-To-Live. A duration in seconds indicating how long a key remains valid before automatically expiring and becoming invisible to users."

#### Q28: "Why store an absolute timepoint (`expiresAt`) instead of a relative duration?"
> **Answer:**  
> "An absolute timestamp (`steady_clock::now() + ttl`) allows expiration checks to be a single comparison: `now() > expiresAt`. Storing durations would require tracking insertion time and recalculating elapsed time on every query."

#### Q29: "Why use `std::chrono::steady_clock` instead of `system_clock`?"
> **Answer:**  
> "`system_clock` reflects wall-clock time and can jump backwards if the user changes the system time or during NTP adjustments, which could prematurely expire keys or keep them alive indefinitely. `steady_clock` is monotonic and guaranteed to never move backwards."

#### Q30: "What is Lazy Expiration?"
> **Answer:**  
> "Lazy expiration means we only check if a key has expired when a client queries it with `GET`. If expired, it is deleted right then. This avoids running active timers for every key."

#### Q31: "If you have Lazy Expiration, why did you implement a Background Cleanup Thread?"
> **Answer:**  
> "If an expired key is never queried again, lazy expiration will never encounter it, leaving its memory allocated forever. The background thread wakes up periodically (e.g. every 30s) to scan and reclaim memory from abandoned keys."

---

### Category 6: Concurrency & Synchronization (Q32 – Q37)

#### Q32: "Why does `GET` require a mutex lock if it is just a 'read'?"
> **Answer:**  
> "In an LRU cache, a successful `GET` updates the node's position to MRU (`moveToFront`). This writes to `node->prev` and `node->next`. If two threads call `GET` simultaneously without a lock, they would concurrently mutate list pointers, causing memory corruption, cyclic loops, or segmentation faults."

#### Q33: "What is a Race Condition? Give a concrete example in CacheCore."
> **Answer:**  
> "A race condition occurs when concurrent threads access and manipulate shared state without synchronization. For instance, if Thread 1 unlinks Node A while Thread 2 unlinks Node B adjacent to Node A, both threads will read stale `prev`/`next` pointers and overwrite each other's updates, leaving the doubly linked list corrupted."

#### Q34: "What is the difference between `std::lock_guard` and `std::unique_lock`?"
> **Answer:**  
> "`std::lock_guard` is a lightweight RAII wrapper that strictly locks on creation and unlocks upon destruction. `std::unique_lock` provides greater flexibility: it can be explicitly unlocked, re-locked, and transferred, and it is required by `std::condition_variable::wait_for`."

#### Q35: "How does the background thread shut down gracefully when `~LRUCache()` runs?"
> **Answer:**  
> "1. The destructor locks `mtx_` and sets `shutdown_ = true`.
> 2. It signals `cv_.notify_all()`.
> 3. The background thread, which is waiting in `cv_.wait_for(...)`, wakes up, detects `shutdown_ == true`, and exits its loop.
> 4. The destructor calls `cleanupThread_.join()` to ensure the thread has completely finished before freeing memory."

#### Q36: "Why are statistics counters `std::atomic<uint64_t>` instead of standard integers?"
> **Answer:**  
> "Using atomics allows counters like `hits` and `misses` to be updated using lock-free hardware CPU instructions (`fetch_add`) with `memory_order_relaxed`. This keeps counter updates fast and allows reading stats without holding the primary cache mutex."

#### Q37: "Why did you use `std::memory_order_relaxed` for the atomic statistics?"
> **Answer:**  
> "Relaxed ordering guarantees atomic modification without enforcing memory synchronisation or ordering constraints on surrounding variables. Because cache statistics are diagnostic metrics where minor staleness is acceptable, relaxed ordering provides the highest possible performance."

---

### Category 7: Memory Management & Modern C++ (Q38 – Q42)

#### Q38: "Why use raw pointers (`Node*`) instead of `std::shared_ptr` for the list?"
> **Answer:**  
> "Doubly linked lists have circular dependencies: Node A points forward to Node B, and Node B points backward to Node A. Using `std::shared_ptr` creates reference cycles, meaning reference counts never hit zero, causing a permanent memory leak. While `std::weak_ptr` can break cycles, raw pointers with centralized ownership in `LRUCache` are faster and zero-overhead."

#### Q39: "How does CacheCore prevent memory leaks?"
> **Answer:**  
> "Every `new Node` is paired with an explicit `delete`:
> * Evictions call `delete lru`.
> * `DELETE` calls `delete node`.
> * Expired lookups call `delete node`.
> * The destructor `~LRUCache()` joins the background thread and traverses the entire remaining list from `head_` to `tail_`, deleting every node."

#### Q40: "Why are the copy constructor and copy assignment operator deleted?"
> **Answer:**  
> "Because `LRUCache` manages raw heap pointers and dedicated background threads. A shallow default copy would duplicate pointer addresses, causing double-free crashes upon destruction. We explicitly mark them `= delete`."

#### Q41: "What does RAII mean and where is it used in CacheCore?"
> **Answer:**  
> "Resource Acquisition Is Initialization. Resources are bound to object lifetimes. Examples:
> * `std::lock_guard` acquires the mutex in its constructor and releases it in its destructor.
> * `LRUCache` initializes sentinels/threads in its constructor and cleans them up in `~LRUCache()`."

#### Q42: "Why does `get()` return `std::optional<std::string>` instead of an empty string `""` on miss?"
> **Answer:**  
> "An empty string `""` could be a valid stored value. `std::optional` explicitly models the presence or absence of a value, making cache misses distinct from empty stored strings without ambiguous sentinels."

---

### Category 8: Complexity & Edge Cases (Q43 – Q46)

#### Q43: "What is the time complexity of each CacheCore operation?"
> **Answer:**  
> * `GET`: $O(1)$ average (map lookup + list move)
> * `SET`: $O(1)$ average (map insert/lookup + list splice + optional eviction)
> * `DELETE`: $O(1)$ average (map lookup/erase + list unlink)
> * `evictLRU`: $O(1)$ strict (direct access via `tail_->prev`)"

#### Q44: "When could `std::unordered_map` operations degrade to $O(N)$?"
> **Answer:**  
> "If many keys hash to the exact same bucket (hash collision attack or a poor hash function), the bucket degrades into a linked list, making lookups $O(N)$. With standard string hashing and a balanced load factor, it stays $O(1)$ average."

#### Q45: "How does CacheCore handle capacity = 1?"
> **Answer:**  
> "If capacity is 1, inserting key A works. Inserting key B triggers `size >= capacity`, which immediately evicts key A and places key B as the sole item. Updating key B updates its value in place without triggering an eviction."

#### Q46: "What happens if a user sets an existing key with a new value and NO ttl?"
> **Answer:**  
> "The code checks `ttl.has_value()`. Since it has no value, it sets `node->hasTTL = false`, clearing any previously set expiration timestamp and converting it into a persistent key."

---

### Category 9: Benchmarking & Scalability (Q47 – Q50)

#### Q47: "What did your benchmark test, and what were the results?"
> **Answer:**  
> "The benchmark tested 10,000 capacity, 200,000 operations per thread with a 75% GET / 25% SET ratio across 1, 4, 8, 16, and 32 threads. Single-threaded throughput reached ~3.65 million ops/sec. Under multiple threads, throughput leveled off around 1.7M–1.9M ops/sec."

#### Q48: "Why did throughput plateau when thread counts increased?"
> **Answer:**  
> "Because of **lock contention** on the single global `std::mutex`. As more threads are added, they spend more time waiting for the lock rather than executing work. The critical section serializes execution."

#### Q49: "How would you redesign CacheCore to scale to 10M+ ops/sec across 32 cores?"
> **Answer:**  
> "Use **Lock Striping / Sharding**:
> Instead of one cache with one mutex, divide the key space into $N$ shards (e.g. 32 or 64). A hash function routes `key` to `shard = hash(key) % N`. Each shard has its own independent hash map, linked list, and mutex. Threads accessing different keys operate in parallel without blocking each other."

#### Q50: "What major features are missing if someone wanted to use CacheCore in production?"
> **Answer:**  
> "1. **Network Interface**: Adding an event-driven server (e.g. epoll/io_uring) supporting the Redis RESP protocol.
> 2. **Persistence**: Write-Ahead Logging (WAL) or background snapshotting (RDB style).
> 3. **Memory Limits**: Eviction based on total byte size in addition to key count.
> 4. **Rich Data Types**: Supporting lists, hashes, and sets via `std::variant`."
