/**
 * @file test_main.cpp
 * @brief CacheCore test suite entry point.
 *
 * Runs all test groups in sequence and prints a per-group and overall
 * pass/fail summary.  Returns exit code 0 on all-pass, 1 on any failure.
 */

#include "test_runner.h"
#include <iostream>

// Declared in the individual test files
void register_cache_tests(TestRegistry& r);
void register_lru_tests(TestRegistry& r);
void register_ttl_tests(TestRegistry& r);

int main() {
    std::cout << "\n╔══════════════════════════════════════╗\n"
              << "║     CacheCore – Test Suite           ║\n"
              << "╚══════════════════════════════════════╝\n\n";

    int totalFailed = 0;

    {
        std::cout << "[ Basic Cache Operations ]\n";
        TestRegistry r;
        register_cache_tests(r);
        totalFailed += r.run();
    }

    {
        std::cout << "\n[ LRU Eviction Order ]\n";
        TestRegistry r;
        register_lru_tests(r);
        totalFailed += r.run();
    }

    {
        std::cout << "\n[ TTL Expiration ]\n";
        std::cout << "  (some tests sleep 1.5s — this is expected)\n";
        TestRegistry r;
        register_ttl_tests(r);
        totalFailed += r.run();
    }

    std::cout << "\n";
    if (totalFailed == 0) {
        std::cout << "  ✓  All tests passed.\n";
    } else {
        std::cout << "  ✗  " << totalFailed << " test(s) FAILED.\n";
    }
    std::cout << "\n";

    return totalFailed > 0 ? 1 : 0;
}
