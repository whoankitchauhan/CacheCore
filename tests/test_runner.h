/**
 * @file test_runner.h
 * @brief Minimal self-contained test framework for CacheCore.
 *
 * Usage:
 *   1. Register tests via TestRegistry::add(name, fn).
 *   2. Call TestRegistry::run() — returns 0 on all-pass, >0 on any failure.
 *
 * Assertions throw std::runtime_error on failure; the runner catches
 * them, prints the message, and continues with the next test.
 */

#pragma once

#include <string>
#include <functional>
#include <vector>
#include <utility>
#include <iostream>
#include <sstream>
#include <stdexcept>

// ── Assertion macros ────────────────────────────────────────────────────────

#define TEST_ASSERT(cond)                                               \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::ostringstream _oss;                                    \
            _oss << "Assertion failed: (" #cond ")"                     \
                 << "  [" << __FILE__ << ":" << __LINE__ << "]";        \
            throw std::runtime_error(_oss.str());                       \
        }                                                               \
    } while (false)

#define TEST_ASSERT_EQ(lhs, rhs)                                        \
    do {                                                                \
        auto _lhs = (lhs);                                              \
        auto _rhs = (rhs);                                              \
        if (!(_lhs == _rhs)) {                                          \
            std::ostringstream _oss;                                    \
            _oss << "Expected (" #lhs ") == (" #rhs ")"                 \
                 << "  got: [" << _lhs << "] vs [" << _rhs << "]"      \
                 << "  [" << __FILE__ << ":" << __LINE__ << "]";        \
            throw std::runtime_error(_oss.str());                       \
        }                                                               \
    } while (false)

#define TEST_ASSERT_TRUE(cond)  TEST_ASSERT(cond)
#define TEST_ASSERT_FALSE(cond) TEST_ASSERT(!(cond))

// ── TestRegistry ────────────────────────────────────────────────────────────

class TestRegistry {
public:
    using TestFn = std::function<void()>;

    void add(const std::string& name, TestFn fn) {
        cases_.emplace_back(name, std::move(fn));
    }

    /**
     * Run all registered tests.
     * @return Number of failed tests (0 = all passed).
     */
    int run() const {
        int passed = 0, failed = 0;
        for (const auto& [name, fn] : cases_) {
            try {
                fn();
                std::cout << "  [PASS] " << name << "\n";
                ++passed;
            } catch (const std::exception& ex) {
                std::cout << "  [FAIL] " << name << "\n"
                          << "         " << ex.what() << "\n";
                ++failed;
            } catch (...) {
                std::cout << "  [FAIL] " << name
                          << "  (unknown exception)\n";
                ++failed;
            }
        }
        std::cout << "  -----------------------------------------------\n"
                  << "  " << passed << " passed, " << failed << " failed\n";
        return failed;
    }

private:
    std::vector<std::pair<std::string, TestFn>> cases_;
};
