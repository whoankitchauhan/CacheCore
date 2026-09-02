# CacheCore – Quick & Simple Guide

## 1. What is this project?
**CacheCore** is a fast in-memory key-value store built in C++.
It works like a dictionary in your computer's RAM:
* You save data with `SET key value`.
* You get data back with `GET key`.
* You delete data with `DELETE key`.

---

## 2. What problem does it solve?
Fetching data from a database or a slow API takes time.
A **cache** keeps the most important data in fast memory (RAM) so your program can read it in nanoseconds instead of milliseconds.

---

## 3. How does it work internally? (The Core Idea)
We combine two simple data structures:

1. **HashMap (`std::unordered_map`)**:
   * Finds any key instantly in $O(1)$ time.
   * Maps each key directly to a node pointer.

2. **Doubly Linked List (DLL)**:
   * Keeps track of which items were used recently.
   * **Front (Head)** = Most Recently Used (MRU).
   * **Back (Tail)** = Least Recently Used (LRU).
   * Allows adding or moving items around in $O(1)$ time.

### What is LRU Eviction?
RAM is limited. If the cache reaches its maximum capacity and you add a new item, CacheCore removes the item at the very back of the list (**Least Recently Used**) to make room.

### What is TTL (Time To Live)?
You can tell a key to expire after a certain number of seconds (e.g., `SET token 12345 TTL 10`).
After 10 seconds, CacheCore treats it as expired and deletes it.

### Thread Safety (Concurrency)
Multiple threads can use CacheCore at the same time. A `std::mutex` (lock) makes sure only one thread touches the list or map at a given moment so data never gets corrupted.

---

## 4. How to Run It

### Step 1: Build the project
```cmd
.\build.bat
```

### Step 2: Open the interactive CLI
```cmd
.\build\cachecore_cli.exe
```

### Step 3: Commands to try
```text
SET name Ankit
GET name
SET session 999 TTL 10
GET session
STATS
DELETE name
EXIT
```

### Step 4: Run automated tests
```cmd
.\build.bat test
```
*(Runs all 25 unit tests to check correctness).*

### Step 5: Run throughput benchmark
```cmd
.\build.bat bench
```
*(Tests how many operations per second CacheCore can handle across multiple threads).*
