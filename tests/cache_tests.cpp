/**
 * @file cache_tests.cpp
 * @brief Basic key-value operation tests + concurrency smoke test.
 *
 * Tests:
 *   BasicSet             – SET stores a value
 *   BasicGet             – GET retrieves it
 *   GetMiss              – GET on unknown key returns nullopt
 *   Delete               – DELETE removes the key
 *   DeleteMiss           – DELETE on unknown key returns false
 *   UpdateExistingKey    – SET on existing key updates value, not duplicate
 *   UpdatePreservesSize  – map stays at 1 after double-SET
 *   CapacityZeroUnlimited– capacity=0 means unlimited
 *   CapacityOne          – capacity=1 evicts previous key on new insert
 *   CapacityOneSameKey   – capacity=1 update doesn't evict
 *   ConcurrentAccess     – 8 threads doing mixed ops produce no crashes
 *   StatsCounting        – hit/miss counters increment correctly
 */

#include "LRUCache.h"
#include "test_runner.h"

#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <string>

void register_cache_tests(TestRegistry& r) {

    // ── Basic operations ─────────────────────────────────────────────

    r.add("BasicSet", [](){
        LRUCache cache(10, 0);
        cache.set("name", "Ankit");
        auto val = cache.get("name");
        TEST_ASSERT(val.has_value());
        TEST_ASSERT_EQ(*val, std::string("Ankit"));
    });

    r.add("BasicGet", [](){
        LRUCache cache(10, 0);
        cache.set("city", "Delhi");
        TEST_ASSERT_EQ(*cache.get("city"), std::string("Delhi"));
    });

    r.add("GetMiss", [](){
        LRUCache cache(10, 0);
        auto val = cache.get("nonexistent");
        TEST_ASSERT_FALSE(val.has_value());
    });

    r.add("Delete", [](){
        LRUCache cache(10, 0);
        cache.set("k", "v");
        TEST_ASSERT(cache.remove("k"));
        TEST_ASSERT_FALSE(cache.get("k").has_value());
    });

    r.add("DeleteMiss", [](){
        LRUCache cache(10, 0);
        TEST_ASSERT_FALSE(cache.remove("ghost"));
    });

    // ── Update semantics ─────────────────────────────────────────────

    r.add("UpdateExistingKey", [](){
        LRUCache cache(10, 0);
        cache.set("k", "first");
        cache.set("k", "second");
        TEST_ASSERT_EQ(*cache.get("k"), std::string("second"));
    });

    r.add("UpdatePreservesSize", [](){
        LRUCache cache(10, 0);
        cache.set("k", "v1");
        cache.set("k", "v2");
        // No duplicate entries — map must still have exactly 1 key
        TEST_ASSERT_EQ(cache.size(), size_t(1));
    });

    // ── Capacity edge cases ──────────────────────────────────────────

    r.add("CapacityZeroUnlimited", [](){
        LRUCache cache(0, 0);  // 0 = unlimited
        for (int i = 0; i < 5000; ++i) {
            cache.set("k" + std::to_string(i), "v");
        }
        TEST_ASSERT_EQ(cache.size(), size_t(5000));
    });

    r.add("CapacityOne", [](){
        LRUCache cache(1, 0);
        cache.set("a", "1");
        cache.set("b", "2");  // evicts "a"
        TEST_ASSERT_FALSE(cache.get("a").has_value());
        TEST_ASSERT(cache.get("b").has_value());
        TEST_ASSERT_EQ(cache.size(), size_t(1));
    });

    r.add("CapacityOneSameKey", [](){
        LRUCache cache(1, 0);
        cache.set("a", "1");
        cache.set("a", "2");  // update — should NOT evict
        TEST_ASSERT_EQ(cache.size(), size_t(1));
        TEST_ASSERT_EQ(*cache.get("a"), std::string("2"));
    });

    // ── Concurrency smoke test ───────────────────────────────────────

    r.add("ConcurrentAccess", [](){
        // 8 threads do mixed GET/SET/DELETE on a shared cache.
        // Success criterion: no crash, no data race (run under TSAN
        // for definitive validation), ops counter reaches expected total.
        LRUCache cache(500, 0);

        const int NUM_THREADS = 8;
        const int OPS         = 10'000;
        std::atomic<int> completedOps{0};

        std::vector<std::thread> threads;
        threads.reserve(NUM_THREADS);
        for (int t = 0; t < NUM_THREADS; ++t) {
            threads.emplace_back([&, t](){
                std::mt19937 rng(static_cast<unsigned>(t));
                std::uniform_int_distribution<int> keyDist(0, 99);
                std::uniform_int_distribution<int> opDist(0, 2);

                for (int i = 0; i < OPS; ++i) {
                    const std::string key = "k" + std::to_string(keyDist(rng));
                    switch (opDist(rng)) {
                        case 0: cache.set(key, "val"); break;
                        case 1: cache.get(key);        break;
                        case 2: cache.remove(key);     break;
                    }
                    completedOps.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        for (auto& th : threads) th.join();
        TEST_ASSERT_EQ(completedOps.load(), NUM_THREADS * OPS);
    });

    // ── Statistics ───────────────────────────────────────────────────

    r.add("StatsCounting", [](){
        LRUCache cache(10, 0);
        cache.set("a", "1");
        cache.get("a");   // hit
        cache.get("a");   // hit
        cache.get("b");   // miss
        TEST_ASSERT_EQ(cache.stats().hits.load(),   uint64_t(2));
        TEST_ASSERT_EQ(cache.stats().misses.load(), uint64_t(1));
    });

    r.add("StatsEvictions", [](){
        LRUCache cache(2, 0);
        cache.set("a", "1");
        cache.set("b", "2");
        cache.set("c", "3");  // evicts "a"
        TEST_ASSERT_EQ(cache.stats().evictions.load(), uint64_t(1));
    });
}
