/**
 * @file benchmark.cpp
 * @brief Multi-threaded throughput benchmark for CacheCore.
 *
 * Workload:
 *   Each worker thread performs OPS_PER_THREAD random operations.
 *   READ_RATIO controls the fraction that are GETs vs SETs.
 *   Keys are drawn from a key space twice the cache capacity to
 *   produce realistic cache pressure and eviction.
 *
 * IMPORTANT: This benchmark measures ACTUAL performance on the
 * machine it runs on.  Results are printed after measurement and are
 * NOT invented or estimated in advance.
 */

#include "LRUCache.h"

#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <string>

// ── Benchmark parameters ────────────────────────────────────────────────────

static constexpr size_t CACHE_CAPACITY   = 10'000;
static constexpr size_t OPS_PER_THREAD   = 200'000;
static constexpr double READ_RATIO       = 0.75;   // 75% GET, 25% SET
// Key space = 2× capacity → ~50% hit rate for GETs
static constexpr size_t KEY_SPACE        = CACHE_CAPACITY * 2;

// ── Result struct ────────────────────────────────────────────────────────────

struct BenchResult {
    int      numThreads;
    uint64_t totalOps;
    double   durationSec;
    double   opsPerSec;
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
};

// ── Worker ───────────────────────────────────────────────────────────────────

static void worker(LRUCache& cache, int tid, size_t opsCount) {
    // Different seed per thread → different access patterns
    std::mt19937_64 rng(static_cast<uint64_t>(tid) * 6364136223846793005ULL);
    std::uniform_real_distribution<double> opDist(0.0, 1.0);
    std::uniform_int_distribution<size_t>  keyDist(0, KEY_SPACE - 1);

    for (size_t i = 0; i < opsCount; ++i) {
        const std::string key = "k" + std::to_string(keyDist(rng));
        if (opDist(rng) < READ_RATIO) {
            cache.get(key);
        } else {
            cache.set(key, "v");
        }
    }
}

// ── Run one benchmark configuration ─────────────────────────────────────────

static BenchResult runBench(int numThreads) {
    // Disable background cleanup thread during benchmark (cleanupIntervalSec=0).
    LRUCache cache(CACHE_CAPACITY, 0);

    // Pre-populate half the cache so early GETs aren't all misses.
    for (size_t i = 0; i < CACHE_CAPACITY / 2; ++i) {
        cache.set("k" + std::to_string(i), "v");
    }
    cache.resetStats();  // Don't count pre-population in results.

    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(numThreads));

    const auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(worker, std::ref(cache), i, OPS_PER_THREAD);
    }
    for (auto& t : threads) t.join();

    const auto endTime = std::chrono::high_resolution_clock::now();
    const double elapsed =
        std::chrono::duration<double>(endTime - startTime).count();

    const uint64_t totalOps =
        static_cast<uint64_t>(numThreads) * OPS_PER_THREAD;

    const auto& s = cache.stats();
    return {
        numThreads,
        totalOps,
        elapsed,
        static_cast<double>(totalOps) / elapsed,
        s.hits.load(),
        s.misses.load(),
        s.evictions.load()
    };
}

// ── Main ────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "╔══════════════════════════════════════════════════╗\n"
              << "║          CacheCore – Benchmark Results           ║\n"
              << "╚══════════════════════════════════════════════════╝\n\n"
              << "  Cache capacity : " << CACHE_CAPACITY   << " keys\n"
              << "  Ops per thread : " << OPS_PER_THREAD   << "\n"
              << "  Read ratio     : " << (READ_RATIO * 100) << "% GET  "
              << (100.0 - READ_RATIO * 100) << "% SET\n"
              << "  Key space      : " << KEY_SPACE << " (2× capacity)\n\n";

    // Column header
    std::cout << std::left
              << std::setw(10) << "Threads"
              << std::setw(16) << "Total Ops"
              << std::setw(14) << "Time (s)"
              << std::setw(18) << "Ops/sec"
              << std::setw(12) << "Hits"
              << std::setw(12) << "Misses"
              << std::setw(12) << "Evictions"
              << "\n"
              << std::string(94, '-') << "\n";

    for (int threads : {1, 4, 8, 16, 32}) {
        const auto r = runBench(threads);
        std::cout << std::left
                  << std::setw(10) << r.numThreads
                  << std::setw(16) << r.totalOps
                  << std::setw(14) << std::fixed << std::setprecision(3) << r.durationSec
                  << std::setw(18) << std::fixed << std::setprecision(0) << r.opsPerSec
                  << std::setw(12) << r.hits
                  << std::setw(12) << r.misses
                  << std::setw(12) << r.evictions
                  << "\n";
    }

    std::cout << "\n  These numbers were measured live on this machine.\n"
              << "  Performance varies by CPU, OS scheduler, and cache pressure.\n"
              << "  The single-mutex design serialises all threads; use these\n"
              << "  results to understand the overhead of synchronisation, not\n"
              << "  to compare against lock-free implementations.\n";
    return 0;
}
