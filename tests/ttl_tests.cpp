/**
 * @file ttl_tests.cpp
 * @brief TTL (Time-To-Live) expiration tests.
 *
 * Tests:
 *   TTLNotYetExpired    – key with TTL=2s is retrievable immediately
 *   TTLExpired          – key with TTL=1s is gone after 1.5s sleep
 *   TTLExpiredCount     – expired counter increments on lazy removal
 *   TTLUpdateClearsTTL  – updating a key without TTL removes old TTL
 *   TTLZeroCapacity     – TTL works correctly with unlimited capacity
 *   TTLDoesNotEvict     – expiry counter != eviction counter
 *
 * Note: Tests that involve sleeping are inherently time-sensitive.
 * A TTL of 1s with a sleep of 1.5s is chosen to be robust on most
 * systems, but extremely loaded machines could theoretically fail.
 */

#include "LRUCache.h"
#include "test_runner.h"

#include <thread>
#include <chrono>
#include <string>

void register_ttl_tests(TestRegistry& r) {

    r.add("TTLNotYetExpired", [](){
        LRUCache cache(10, 0);
        cache.set("session", "abc", std::chrono::seconds(60));
        auto val = cache.get("session");
        TEST_ASSERT(val.has_value());
        TEST_ASSERT_EQ(*val, std::string("abc"));
    });

    r.add("TTLExpired", [](){
        LRUCache cache(10, 0);
        cache.set("temp", "data", std::chrono::seconds(1));

        // Key must exist right after insertion
        TEST_ASSERT(cache.get("temp").has_value());

        // Wait for expiry
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));

        auto val = cache.get("temp");
        TEST_ASSERT_FALSE(val.has_value());
    });

    r.add("TTLExpiredCount", [](){
        LRUCache cache(10, 0);
        cache.set("x", "1", std::chrono::seconds(1));
        cache.set("y", "2", std::chrono::seconds(1));

        std::this_thread::sleep_for(std::chrono::milliseconds(1500));

        cache.get("x");  // triggers lazy expiry for x
        cache.get("y");  // triggers lazy expiry for y

        TEST_ASSERT_EQ(cache.stats().expired.load(), uint64_t(2));
        TEST_ASSERT_EQ(cache.stats().misses.load(),  uint64_t(2));
    });

    r.add("TTLUpdateClearsTTL", [](){
        // SET with TTL, then SET again without TTL → key should not expire.
        LRUCache cache(10, 0);
        cache.set("k", "v1", std::chrono::seconds(1));

        // Overwrite without TTL — clears the TTL
        cache.set("k", "v2");  // no TTL argument

        std::this_thread::sleep_for(std::chrono::milliseconds(1500));

        // Should still be alive (TTL was cleared)
        auto val = cache.get("k");
        TEST_ASSERT(val.has_value());
        TEST_ASSERT_EQ(*val, std::string("v2"));
    });

    r.add("TTLZeroCapacity", [](){
        // TTL must work even with unlimited capacity (capacity=0)
        LRUCache cache(0, 0);
        cache.set("ephemeral", "bye", std::chrono::seconds(1));
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        TEST_ASSERT_FALSE(cache.get("ephemeral").has_value());
        TEST_ASSERT_EQ(cache.stats().expired.load(), uint64_t(1));
    });

    r.add("TTLDoesNotEvict", [](){
        // Expiry via TTL is counted as 'expired', NOT as 'evictions'.
        LRUCache cache(10, 0);
        cache.set("timed", "v", std::chrono::seconds(1));
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        cache.get("timed");  // triggers lazy expiry

        TEST_ASSERT_EQ(cache.stats().expired.load(),   uint64_t(1));
        TEST_ASSERT_EQ(cache.stats().evictions.load(), uint64_t(0));
    });
}
