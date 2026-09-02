/**
 * @file Statistics.cpp
 * @brief Statistics method implementations.
 */

#include "Statistics.h"

#include <sstream>
#include <iomanip>

void Statistics::reset() noexcept {
    hits.store(0, std::memory_order_relaxed);
    misses.store(0, std::memory_order_relaxed);
    evictions.store(0, std::memory_order_relaxed);
    expired.store(0, std::memory_order_relaxed);
}

std::string Statistics::format(size_t currentSize, size_t capacity) const {
    const uint64_t h = hits.load(std::memory_order_relaxed);
    const uint64_t m = misses.load(std::memory_order_relaxed);
    const uint64_t ev = evictions.load(std::memory_order_relaxed);
    const uint64_t ex = expired.load(std::memory_order_relaxed);
    const uint64_t total = h + m;
    const double hitRate = total > 0
        ? (100.0 * static_cast<double>(h) / static_cast<double>(total))
        : 0.0;

    std::ostringstream oss;
    oss << "Capacity:     " << capacity    << "\n"
        << "Current Size: " << currentSize << "\n"
        << "Hits:         " << h           << "\n"
        << "Misses:       " << m           << "\n"
        << "Evictions:    " << ev          << "\n"
        << "Expired:      " << ex          << "\n"
        << std::fixed << std::setprecision(1)
        << "Hit Rate:     " << hitRate     << "%\n";
    return oss.str();
}
