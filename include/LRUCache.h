/**
 * @file LRUCache.h
 * @brief Concurrent LRU cache with TTL support.
 *
 * Architecture:
 *   HashMap (std::unordered_map<string, Node*>)
 *     Provides O(1) average-case key lookup.
 *     Worst case is O(n) on hash collisions, which is typical of any
 *     open-addressing or chained hash map.
 *
 *   Doubly-Linked List (raw pointer DLL with sentinel head/tail)
 *     Maintains access order: head_->next is Most Recently Used (MRU),
 *     tail_->prev is Least Recently Used (LRU).
 *     O(1) removal and reinsertion of any known node (via pointer).
 *
 * Thread Safety:
 *   A single std::mutex (mtx_) protects all shared state.
 *   GET modifies LRU order, so it cannot be a shared-read operation —
 *   a std::shared_mutex would give no benefit here.
 *   Statistics counters are std::atomic and updated without mtx_.
 *
 * TTL:
 *   Expiry is checked lazily on GET.  An optional background thread
 *   performs periodic sweeps to reclaim memory from keys that are
 *   never accessed again.
 *
 * Ownership:
 *   LRUCache owns all Node objects.  Nodes are freed on eviction,
 *   explicit deletion, expiry, or cache destruction.
 */

#pragma once

#include "Node.h"
#include "Statistics.h"

#include <unordered_map>
#include <string>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <optional>
#include <chrono>
#include <vector>

class LRUCache {
public:
    /**
     * @param capacity          Maximum number of keys.  0 = unlimited.
     * @param cleanupIntervalSec  How often (seconds) the background TTL
     *                            sweep runs.  0 = no background thread.
     */
    explicit LRUCache(size_t capacity, size_t cleanupIntervalSec = 5);

    /**
     * Signals background thread to stop, joins it, then frees all nodes.
     */
    ~LRUCache();

    // Non-copyable, non-movable — owns raw pointers.
    LRUCache(const LRUCache&)            = delete;
    LRUCache& operator=(const LRUCache&) = delete;
    LRUCache(LRUCache&&)                 = delete;
    LRUCache& operator=(LRUCache&&)      = delete;

    // ── Public API ─────────────────────────────────────────────────────────

    /**
     * Insert or update key.  If the cache is at capacity, the LRU key
     * is evicted first.  An updated key moves to the MRU position.
     *
     * @param ttl  Optional TTL.  If present, the key expires after this
     *             duration.  If absent, any previous TTL is cleared.
     * @return Always true (provided for future error-return extensions).
     *
     * Complexity: O(1) average.
     */
    bool set(const std::string& key,
             const std::string& value,
             std::optional<std::chrono::seconds> ttl = std::nullopt);

    /**
     * Retrieve value for key.  Moves the key to the MRU position.
     * Returns std::nullopt on cache miss or if the key has expired.
     * Expired keys are removed immediately (lazy expiry).
     *
     * Complexity: O(1) average.
     */
    std::optional<std::string> get(const std::string& key);

    /**
     * Remove key from the cache.
     * @return true if the key existed, false on miss.
     *
     * Complexity: O(1) average.
     */
    bool remove(const std::string& key);

    // ── Inspection ─────────────────────────────────────────────────────────

    size_t size() const;      // Current number of live keys (under lock)
    size_t capacity() const;  // Configured capacity (immutable after ctor)

    const Statistics& stats() const;          // Raw atomic counters
    std::string       statsString() const;    // Formatted summary string
    void              resetStats() noexcept;  // Reset all counters to zero

private:
    // ── DLL helpers (must be called with mtx_ held) ─────────────────────

    void removeNode(Node* node);       // Unlink node from list
    void insertAfterHead(Node* node);  // Place node at MRU position
    void moveToFront(Node* node);      // removeNode + insertAfterHead
    void evictLRU();                   // Remove & delete tail_->prev

    // ── TTL helper (must be called with mtx_ held) ──────────────────────

    bool isExpired(const Node* node) const noexcept;

    // ── Background cleanup thread ────────────────────────────────────────

    void cleanupLoop();  // Runs on cleanupThread_

    // ── Data members ─────────────────────────────────────────────────────

    const size_t capacity_;  // 0 = unlimited
    std::unordered_map<std::string, Node*> map_;

    // Sentinel nodes — never hold real data, never deleted mid-life.
    // head_->next = MRU  |  tail_->prev = LRU
    Node* head_;
    Node* tail_;

    mutable std::mutex    mtx_;   // Guards map_ + DLL
    Statistics            stats_;

    // Background TTL sweep
    const size_t          cleanupIntervalSec_;
    std::thread           cleanupThread_;
    std::condition_variable cv_;
    bool                  shutdown_{false};
};
