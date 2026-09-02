/**
 * @file lru_tests.cpp
 * @brief LRU eviction order tests.
 *
 * Tests:
 *   LRUEvictOldest          – full cache evicts the oldest-inserted key
 *   LRUGetPreservesKey      – GET moves key to MRU, saving it from eviction
 *   LRUSetUpdateMovesToMRU  – SET on existing key moves it to MRU
 *   LRUEvictionCount        – eviction counter is correct
 *   LRUMultiEvict           – filling beyond capacity evicts in order
 *   LRUOrderAfterDeletes    – deleting non-LRU keys leaves order intact
 */

#include "LRUCache.h"
#include "test_runner.h"

#include <string>

void register_lru_tests(TestRegistry& r) {

    // ── Core LRU behaviour ───────────────────────────────────────────

    r.add("LRUEvictOldest", [](){
        // Insert in order a, b, c into capacity-3 cache.
        // Adding d must evict a (the LRU).
        LRUCache cache(3, 0);
        cache.set("a", "1");
        cache.set("b", "2");
        cache.set("c", "3");
        cache.set("d", "4");  // evicts "a"

        TEST_ASSERT_FALSE(cache.get("a").has_value());
        TEST_ASSERT(cache.get("b").has_value());
        TEST_ASSERT(cache.get("c").has_value());
        TEST_ASSERT(cache.get("d").has_value());
    });

    r.add("LRUGetPreservesKey", [](){
        // a, b, c inserted.  GET a → a becomes MRU.
        // Adding d evicts b (now LRU), NOT a.
        LRUCache cache(3, 0);
        cache.set("a", "1");  // LRU order: [a]
        cache.set("b", "2");  // LRU order: [b, a]
        cache.set("c", "3");  // LRU order: [c, b, a]
        cache.get("a");       // LRU order: [a, c, b]  ← a promoted
        cache.set("d", "4");  // evicts b (LRU)

        TEST_ASSERT_FALSE(cache.get("b").has_value());  // evicted
        TEST_ASSERT(cache.get("a").has_value());         // promoted, survived
        TEST_ASSERT(cache.get("c").has_value());
        TEST_ASSERT(cache.get("d").has_value());
    });

    r.add("LRUSetUpdateMovesToMRU", [](){
        // Updating an existing key moves it to MRU.
        LRUCache cache(3, 0);
        cache.set("a", "1");  // LRU order: [a]
        cache.set("b", "2");  // LRU order: [b, a]
        cache.set("c", "3");  // LRU order: [c, b, a]
        cache.set("a", "X");  // Update a → LRU order: [a, c, b]
        cache.set("d", "4");  // evicts b (LRU)

        TEST_ASSERT_FALSE(cache.get("b").has_value());  // evicted
        TEST_ASSERT_EQ(*cache.get("a"), std::string("X"));  // survived + updated
        TEST_ASSERT(cache.get("c").has_value());
        TEST_ASSERT(cache.get("d").has_value());
    });

    r.add("LRUEvictionCount", [](){
        LRUCache cache(2, 0);
        cache.set("a", "1");
        cache.set("b", "2");
        cache.set("c", "3");  // evicts a
        cache.set("d", "4");  // evicts b
        TEST_ASSERT_EQ(cache.stats().evictions.load(), uint64_t(2));
    });

    r.add("LRUMultiEvict", [](){
        // Fill capacity-5 cache, then insert 5 more.
        // All original keys should be evicted in FIFO order.
        LRUCache cache(5, 0);
        for (int i = 0; i < 5; ++i) cache.set("old" + std::to_string(i), "v");
        for (int i = 0; i < 5; ++i) cache.set("new" + std::to_string(i), "v");

        for (int i = 0; i < 5; ++i) {
            TEST_ASSERT_FALSE(cache.get("old" + std::to_string(i)).has_value());
        }
        for (int i = 0; i < 5; ++i) {
            TEST_ASSERT(cache.get("new" + std::to_string(i)).has_value());
        }
        TEST_ASSERT_EQ(cache.stats().evictions.load(), uint64_t(5));
    });

    r.add("LRUOrderAfterDeletes", [](){
        // Delete a key that is NOT the LRU.  After the delete, the correct
        // LRU ordering must still be maintained.
        //
        // Capacity 3: insert a, b, c → order [c(MRU), b, a(LRU)]
        // Delete b (middle node) → order [c(MRU), a(LRU)], size=2
        // Insert d → size=3, no eviction yet (still room)
        // Insert e → evicts a (the LRU)
        LRUCache cache(3, 0);
        cache.set("a", "1");   // LRU: [a]
        cache.set("b", "2");   // LRU: [b, a]
        cache.set("c", "3");   // LRU: [c, b, a]
        cache.remove("b");     // delete middle node → [c, a], size=2
        cache.set("d", "4");   // insert → [d, c, a], size=3 (full again)
        cache.set("e", "5");   // insert → evicts a (LRU) → [e, d, c]

        TEST_ASSERT_FALSE(cache.get("a").has_value());  // evicted (was LRU)
        TEST_ASSERT_FALSE(cache.get("b").has_value());  // deleted explicitly
        TEST_ASSERT(cache.get("c").has_value());
        TEST_ASSERT(cache.get("d").has_value());
        TEST_ASSERT(cache.get("e").has_value());
        TEST_ASSERT_EQ(cache.stats().evictions.load(), uint64_t(1));
    });
}
