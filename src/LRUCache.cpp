/**
 * @file LRUCache.cpp
 * @brief LRUCache method implementations.
 *
 * Data structure invariant (must hold at the start and end of every
 * public method, while mtx_ is held):
 *
 *   head_ <-> [MRU node] <-> ... <-> [LRU node] <-> tail_
 *
 *   map_.size() == number of real nodes in the list
 *   (head_ and tail_ are sentinels and are NOT in map_)
 *
 * Thread safety guarantee:
 *   Every method that touches map_ or the DLL acquires mtx_ before
 *   doing so and releases it on return (RAII via std::lock_guard /
 *   std::unique_lock).  Statistics counters are std::atomic and are
 *   updated without holding mtx_.
 */

#include "LRUCache.h"

#include <cassert>
#include <stdexcept>

// ── Constructor / Destructor ────────────────────────────────────────────────

LRUCache::LRUCache(size_t capacity, size_t cleanupIntervalSec)
    : capacity_(capacity)
    , cleanupIntervalSec_(cleanupIntervalSec)
{
    // Allocate sentinel nodes.
    // head_->next is always the MRU entry (or tail_ if empty).
    // tail_->prev is always the LRU entry (or head_ if empty).
    head_ = new Node();
    tail_ = new Node();
    head_->next = tail_;
    tail_->prev = head_;

    // Start background cleanup thread only if requested.
    if (cleanupIntervalSec_ > 0) {
        cleanupThread_ = std::thread(&LRUCache::cleanupLoop, this);
    }
}

LRUCache::~LRUCache() {
    // 1. Signal background thread to exit.
    {
        std::lock_guard<std::mutex> lock(mtx_);
        shutdown_ = true;
    }
    cv_.notify_all();  // Must be outside the lock to avoid missed wakeup.

    // 2. Join background thread (if it was started).
    if (cleanupThread_.joinable()) {
        cleanupThread_.join();
    }

    // 3. Free all real nodes then the sentinels.
    //    No lock needed — thread is done, no other accessor exists.
    Node* curr = head_;
    while (curr) {
        Node* next = curr->next;
        delete curr;
        curr = next;
    }
}

// ── Private DLL helpers (called with mtx_ held) ─────────────────────────────

/**
 * Unlink 'node' from the doubly-linked list.
 * Clears node->prev and node->next to catch accidental re-use.
 */
void LRUCache::removeNode(Node* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->prev = nullptr;
    node->next = nullptr;
}

/**
 * Insert 'node' immediately after head_ (MRU position).
 * node->prev and node->next must be nullptr (freshly unlinked or new).
 */
void LRUCache::insertAfterHead(Node* node) {
    node->next       = head_->next;
    node->prev       = head_;
    head_->next->prev = node;
    head_->next       = node;
}

/**
 * Move an already-linked node to the MRU position.
 * Called on every cache hit so the access order stays correct.
 */
void LRUCache::moveToFront(Node* node) {
    removeNode(node);
    insertAfterHead(node);
}

/**
 * Evict the Least Recently Used node (tail_->prev).
 * Removes from map_, unlinks from list, and deletes the node.
 * Increments the evictions counter.
 *
 * Precondition: map_ is non-empty (i.e. tail_->prev != head_).
 */
void LRUCache::evictLRU() {
    Node* lru = tail_->prev;
    if (lru == head_) return;  // Defensive: cache is already empty.

    removeNode(lru);
    map_.erase(lru->key);
    delete lru;
    stats_.evictions.fetch_add(1, std::memory_order_relaxed);
}

// ── Private TTL helper (called with mtx_ held) ──────────────────────────────

bool LRUCache::isExpired(const Node* node) const noexcept {
    if (!node->hasTTL) return false;
    return std::chrono::steady_clock::now() > node->expiresAt;
}

// ── Public API ───────────────────────────────────────────────────────────────

/**
 * SET key value [TTL]
 *
 * If the key already exists, update its value (and TTL) and move it
 * to MRU — no duplicate entry is created.
 *
 * If the key is new and the cache is at capacity, evict the LRU entry
 * first, then insert the new node at the MRU position.
 */
bool LRUCache::set(const std::string& key,
                   const std::string& value,
                   std::optional<std::chrono::seconds> ttl)
{
    std::lock_guard<std::mutex> lock(mtx_);

    // ── Update path ────────────────────────────────────────────────────
    auto it = map_.find(key);
    if (it != map_.end()) {
        Node* node = it->second;
        node->value = value;
        if (ttl.has_value()) {
            node->hasTTL  = true;
            node->expiresAt = std::chrono::steady_clock::now() + *ttl;
        } else {
            node->hasTTL = false;
            node->expiresAt = {};
        }
        moveToFront(node);
        return true;
    }

    // ── Insert path ────────────────────────────────────────────────────
    // Evict LRU if at capacity (capacity 0 = unlimited).
    if (capacity_ > 0 && map_.size() >= capacity_) {
        evictLRU();
    }

    bool hasTTL = ttl.has_value();
    auto expiresAt = hasTTL
        ? std::chrono::steady_clock::now() + *ttl
        : std::chrono::steady_clock::time_point{};

    Node* node = new Node(key, value, hasTTL, expiresAt);
    map_.emplace(key, node);
    insertAfterHead(node);
    return true;
}

/**
 * GET key
 *
 * Returns the value or std::nullopt on miss.
 * On a hit:  moves the node to MRU, increments hits.
 * On a miss: increments misses.
 * On expiry: removes the node, increments misses + expired.
 *
 * Expiry is checked lazily here (no background thread needed for
 * correctness; the background thread only reclaims memory sooner).
 */
std::optional<std::string> LRUCache::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mtx_);

    auto it = map_.find(key);
    if (it == map_.end()) {
        stats_.misses.fetch_add(1, std::memory_order_relaxed);
        return std::nullopt;
    }

    Node* node = it->second;

    // Lazy TTL check.
    if (isExpired(node)) {
        removeNode(node);
        map_.erase(it);
        delete node;
        stats_.misses.fetch_add(1, std::memory_order_relaxed);
        stats_.expired.fetch_add(1, std::memory_order_relaxed);
        return std::nullopt;
    }

    moveToFront(node);
    stats_.hits.fetch_add(1, std::memory_order_relaxed);
    return node->value;
}

/**
 * DELETE key
 *
 * @return true if the key existed, false on miss.
 */
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

// ── Inspection ───────────────────────────────────────────────────────────────

size_t LRUCache::size() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return map_.size();
}

size_t LRUCache::capacity() const {
    return capacity_;  // immutable after construction, no lock needed
}

const Statistics& LRUCache::stats() const {
    return stats_;  // atomics — safe without lock
}

std::string LRUCache::statsString() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return stats_.format(map_.size(), capacity_);
}

void LRUCache::resetStats() noexcept {
    stats_.reset();
}

// ── Background cleanup thread ────────────────────────────────────────────────

/**
 * Runs on a dedicated thread.  Wakes every cleanupIntervalSec_ seconds
 * (or immediately when shutdown_ is set) to sweep the map for expired
 * keys and remove them.
 *
 * This is an optimisation only: correctness is maintained by lazy
 * expiry in get().  The background thread reclaims memory from keys
 * that are never accessed again after their TTL.
 *
 * The thread holds mtx_ during the sweep.  This is acceptable because
 * the sweep is infrequent and bounded by map_.size().
 */
void LRUCache::cleanupLoop() {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx_);

        // Sleep for the interval, or until notified (shutdown).
        cv_.wait_for(lock,
                     std::chrono::seconds(cleanupIntervalSec_),
                     [this]{ return shutdown_; });

        if (shutdown_) break;

        // Collect expired keys into a local vector to avoid iterator
        // invalidation while erasing from map_.
        std::vector<std::string> toDelete;
        const auto now = std::chrono::steady_clock::now();
        for (const auto& [k, node] : map_) {
            if (node->hasTTL && now > node->expiresAt) {
                toDelete.push_back(k);
            }
        }

        for (const auto& k : toDelete) {
            auto it = map_.find(k);
            if (it != map_.end()) {
                removeNode(it->second);
                delete it->second;
                map_.erase(it);
                stats_.expired.fetch_add(1, std::memory_order_relaxed);
            }
        }
        // lock releases here (unique_lock dtor)
    }
}
