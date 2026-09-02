/**
 * @file Statistics.h
 * @brief Cache statistics counters.
 *
 * All counters use std::atomic so they can be incremented from any
 * thread without holding the main cache mutex.  This means the stats
 * themselves are always slightly stale (eventual consistency), but
 * they are never corrupt and reading them does not block cache ops.
 *
 * currentSize and capacity are read from LRUCache under its mutex;
 * they are passed into Statistics::format() at call time.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <string>

struct Statistics {
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<uint64_t> evictions{0};  // LRU evictions
    std::atomic<uint64_t> expired{0};    // TTL-based removals

    // Reset all counters to zero.
    void reset() noexcept;

    // Build a human-readable summary string.
    // currentSize and capacity are supplied by the caller (under lock).
    std::string format(size_t currentSize, size_t capacity) const;
};
