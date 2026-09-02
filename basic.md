# CacheCore – Quick Guide & Interview Q&A

---

## 1. What is this project?
**CacheCore** is a fast in-memory key-value store built in modern C++.
It works like a dictionary in your computer's RAM:
* You save data with `SET key value`.
* You get data back with `GET key`.
* You delete data with `DELETE key`.

---

## 2. What problem does it solve?
Fetching data from a database or over the internet takes milliseconds. Reading from RAM takes nanoseconds.
A **cache** temporarily keeps frequently-read data in memory so your application runs 100x to 1000x faster.

---

## 3. How does it work internally? (The Core Idea)
We combine two data structures:

1. **HashMap (`std::unordered_map`)**:
   * Finds any key in $O(1)$ average time.
   * Maps key $\rightarrow$ direct pointer to that node in memory.

2. **Doubly Linked List (DLL)**:
   * Keeps track of the access order.
   * **Front (Head)** = Most Recently Used (MRU).
   * **Back (Tail)** = Least Recently Used (LRU).
   * Moving or removing a node is $O(1)$ pointer operations.

### Key Features
* **LRU Eviction**: When the cache reaches capacity and a new item arrives, the oldest item at the back is deleted.
* **TTL (Time-To-Live)**: Keys can expire after $N$ seconds (e.g., `SET session 123 TTL 10`). Expired keys are discarded on access (lazy expiry) or by a periodic background thread.
* **Concurrency**: Thread-safe using `std::mutex`. Multiple threads can safely read/write concurrently without memory corruption.

---

## 4. How to Run It

```cmd
.\build.bat          # Build the project
.\build.bat test     # Run all 25 unit tests
.\build.bat bench    # Run throughput benchmark
.\build\cachecore_cli.exe   # Start interactive CLI
```

---

# 5. Interview Questions & Answers

Here are the questions interviewers typically ask about this project, from fundamental to advanced, written in simple, clear English you can directly say.

---

### Basic / Architecture Questions

#### Q1: "Tell me about your CacheCore project."
> **Your Answer:**  
> "CacheCore is a concurrent, in-memory key-value store I built in C++17. It allows you to store and retrieve string data using `SET`, `GET`, and `DELETE`. Internally, it uses a hash table combined with a custom doubly linked list to achieve $O(1)$ lookups and $O(1)$ LRU evictions. It also supports TTL-based expiration and is made thread-safe using a mutex for concurrent operations."

#### Q2: "How do you know the data is actually stored in RAM and not disk?"
> **Your Answer:**  
> "In C++, when we allocate our `Node` structs with `new Node(...)` and store them in `std::unordered_map`, the operating system allocates that memory in the heap area of our process's Virtual Memory in RAM. We never open file streams, write to disk, or invoke disk I/O syscalls. If the program terminates, all data is immediately cleared."

#### Q3: "What is the exact data structure inside a Node?"
> **Your Answer:**  
> "Each Node holds:
> 1. `std::string key` and `std::string value`.
> 2. `bool hasTTL` and `std::chrono::steady_clock::time_point expiresAt` for expiration.
> 3. Two raw pointers: `Node* prev` and `Node* next` to link into our doubly linked list."

#### Q4: "Why did you use both a HashMap AND a Doubly Linked List? Why not just one?"
> **Your Answer:**  
> * "If we used **only a HashMap**: We could find keys in $O(1)$, but a hash map has no sense of order. We wouldn't know which item is the oldest (LRU) without scanning all keys ($O(N)$).
> * If we used **only a Linked List**: We could maintain LRU order, but finding a key would require walking the list from head to tail ($O(N)$).
> * **Together**: The HashMap gives us $O(1)$ access directly to the node pointer in the list, and the Doubly Linked List allows us to unlink and move that node to the front in $O(1)$."

#### Q5: "Why a Doubly Linked List instead of a Singly Linked List or `std::vector`?"
> **Your Answer:**  
> * "In a **vector**, removing an item from the middle or front requires shifting all subsequent elements, which is $O(N)$.
> * In a **singly linked list**, removing a node requires updating the `next` pointer of the *previous* node. Because there is no backward pointer, finding that previous node takes $O(N)$ traversal.
> * In a **doubly linked list**, every node has `prev` and `next`, so unlinking a node is just:
>   `node->prev->next = node->next;`
>   `node->next->prev = node->prev;`
>   which is strictly $O(1)$."

#### Q6: "What are Sentinel Nodes (dummy Head & Tail) and why do you use them?"
> **Your Answer:**  
> "We initialize the list with two dummy nodes: `head_` and `tail_`. A new/accessed node is always placed between `head_` and whatever follows it; the LRU node is always `tail_->prev`. Sentinels eliminate edge-case `if (head == nullptr)` checks. We never have to worry about updating the list pointers on an empty or single-item cache."

---

### LRU & TTL Questions

#### Q7: "Walk me through what happens internally during `GET key`."
> **Your Answer:**  
> 1. "We acquire the lock (`std::lock_guard<std::mutex>`).
> 2. We search `map_.find(key)`. If not found, we increment `misses` and return `std::nullopt`.
> 3. If found, we check if it is expired (`isExpired(node)`). If expired, we delete it, erase it from the map, increment `expired` + `misses`, and return `std::nullopt`.
> 4. If valid, we call `moveToFront(node)` (unlinks it and places it right after `head_`).
> 5. We increment `hits` and return the value."

#### Q8: "Walk me through what happens internally during `SET key value`."
> **Your Answer:**  
> 1. "We acquire the lock.
> 2. We check if the key already exists:
>    * If **yes (Update)**: We update the value and expiration in place, call `moveToFront(node)`, and return.
>    * If **no (Insert)**: We check if `size >= capacity`. If at capacity, we call `evictLRU()` which removes `tail_->prev`, erases it from the map, and frees its memory.
> 3. We allocate a `new Node`, insert it into `map_`, and call `insertAfterHead(node)` to make it MRU."

#### Q9: "How does TTL expiration work?"
> **Your Answer:**  
> "We use **Lazy Expiration** backed by an optional **Background Thread**:
> 1. **Lazy Expiration**: During `get()`, we compare `std::chrono::steady_clock::now() > node->expiresAt`. If expired, we delete it on the spot.
> 2. **Background Sweep**: A dedicated thread wakes up periodically (e.g., every 30s) using `cv_.wait_for` to purge expired keys that were never queried again, preventing memory leaks."

---

### Concurrency & Performance Questions

#### Q10: "Why does `GET` require a mutex lock if it is just reading data?"
> **Your Answer:**  
> "Because in an LRU cache, a `GET` is **not a read-only operation**. A cache hit must move the accessed node to the MRU position (`head_->next`). This modifies the `prev` and `next` pointers in the linked list. Without a lock, two simultaneous `GET` calls would race on updating those pointers, causing pointer corruption or crashes."

#### Q11: "Why did you use `std::atomic` for statistics instead of guarding them with the mutex?"
> **Your Answer:**  
> "Counters like `hits`, `misses`, and `evictions` are simple scalar values. Using `std::atomic<uint64_t>` allows them to be incremented with low-overhead atomic CPU instructions (`fetch_add`) without extending the lock's critical section."

#### Q12: "Why raw pointers (`Node*`) instead of `std::shared_ptr` or `std::unique_ptr`?"
> **Your Answer:**  
> "Doubly linked lists have bidirectional references: A points to B, and B points to A. With `std::shared_ptr`, this creates a circular reference cycle where reference counts never drop to zero, causing a permanent memory leak. While `std::weak_ptr` can break cycles, it adds overhead and complexity. In our design, `LRUCache` has single, clear ownership of all nodes, so raw pointers managed with RAII in the destructor are faster, cleaner, and leak-free."

#### Q13: "What is the bottleneck in your design, and how would you scale it?"
> **Your Answer:**  
> * **The Bottleneck**: 'We use a single global mutex (`mtx_`). Under high concurrency (e.g. 16–32 threads), threads spend time waiting in line for the lock (lock contention), which caps throughput.'
> * **How to scale**:
>   1. **Lock Striping / Sharding**: Hash keys across $N$ smaller caches (shards), each with its own independent mutex. Threads accessing different keys operate in parallel without blocking each other.
>   2. **Read-mostly optimization**: If we support a `PEEK` command that reads without updating LRU order, we could use a shared/reader-writer lock (`std::shared_mutex`)."

---

### Time & Space Complexity Summary Table

| Operation | Time Complexity | Why |
|---|:---:|---|
| `GET` | **$O(1)$ average** | Hash map lookup is $O(1)$; pointer updates in list are $O(1)$. |
| `SET` (Insert) | **$O(1)$ average** | Hash map insert is $O(1)$; eviction is $O(1)$ at `tail_->prev`. |
| `SET` (Update) | **$O(1)$ average** | Hash map lookup is $O(1)$; `moveToFront` is $O(1)$. |
| `DELETE` | **$O(1)$ average** | Hash map erase is $O(1)$; unlinking node is $O(1)$. |
| Space | **$O(N)$** | Stores $N$ keys and values in memory plus pointer overhead. |
