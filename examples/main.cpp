/**
 * @file main.cpp
 * @brief CacheCore command-line interface.
 *
 * Usage:
 *   cachecore_cli [capacity]
 *
 *   capacity  Maximum number of keys (default: 1000, 0 = unlimited).
 *
 * Supported commands:
 *   SET  key value [TTL <seconds>]
 *   GET  key
 *   DELETE key
 *   STATS
 *   EXIT
 */

#include "LRUCache.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

// ── Helpers ─────────────────────────────────────────────────────────────────

static std::vector<std::string> splitTokens(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);
    return tokens;
}

static std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
    return s;
}

// ── Main ────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    size_t capacity = 1000;
    if (argc >= 2) {
        try {
            long raw = std::stol(argv[1]);
            if (raw >= 0) capacity = static_cast<size_t>(raw);
        } catch (...) {
            std::cerr << "Warning: invalid capacity '" << argv[1]
                      << "', using default 1000.\n";
        }
    }

    // Background TTL sweep every 30 seconds.
    LRUCache cache(capacity, 30);

    std::cout << "╔══════════════════════════════════════════╗\n"
              << "║   CacheCore – In-Memory Key-Value Store  ║\n"
              << "╚══════════════════════════════════════════╝\n"
              << "  Capacity : " << (capacity == 0 ? "unlimited" : std::to_string(capacity)) << "\n"
              << "  Commands : SET key value [TTL seconds]\n"
              << "             GET key\n"
              << "             DELETE key\n"
              << "             STATS\n"
              << "             EXIT\n\n";

    std::string line;
    while (true) {
        std::cout << "> ";
        std::cout.flush();
        if (!std::getline(std::cin, line)) break;

        // Trim leading/trailing whitespace
        const auto first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        const auto last = line.find_last_not_of(" \t\r\n");
        line = line.substr(first, last - first + 1);
        if (line.empty()) continue;

        auto tokens = splitTokens(line);
        if (tokens.empty()) continue;

        const std::string cmd = toUpper(tokens[0]);

        // ── EXIT ──────────────────────────────────────────────────────
        if (cmd == "EXIT" || cmd == "QUIT") {
            std::cout << "Goodbye!\n";
            break;
        }

        // ── SET key value [TTL seconds] ───────────────────────────────
        if (cmd == "SET") {
            if (tokens.size() < 3) {
                std::cout << "(error) Usage: SET key value [TTL seconds]\n";
                continue;
            }
            const std::string& key   = tokens[1];
            const std::string& value = tokens[2];

            std::optional<std::chrono::seconds> ttl;
            if (tokens.size() >= 5 && toUpper(tokens[3]) == "TTL") {
                try {
                    long secs = std::stol(tokens[4]);
                    if (secs <= 0) {
                        std::cout << "(error) TTL must be a positive integer\n";
                        continue;
                    }
                    ttl = std::chrono::seconds(secs);
                } catch (...) {
                    std::cout << "(error) Invalid TTL value: " << tokens[4] << "\n";
                    continue;
                }
            }
            cache.set(key, value, ttl);
            std::cout << "OK\n";
            continue;
        }

        // ── GET key ───────────────────────────────────────────────────
        if (cmd == "GET") {
            if (tokens.size() < 2) {
                std::cout << "(error) Usage: GET key\n";
                continue;
            }
            auto val = cache.get(tokens[1]);
            std::cout << (val ? *val : "(nil)") << "\n";
            continue;
        }

        // ── DELETE key ────────────────────────────────────────────────
        if (cmd == "DELETE" || cmd == "DEL") {
            if (tokens.size() < 2) {
                std::cout << "(error) Usage: DELETE key\n";
                continue;
            }
            if (cache.remove(tokens[1])) std::cout << "OK\n";
            else                          std::cout << "(nil)\n";
            continue;
        }

        // ── STATS ─────────────────────────────────────────────────────
        if (cmd == "STATS") {
            std::cout << cache.statsString();
            continue;
        }

        std::cout << "(error) Unknown command: " << tokens[0]
                  << ".  Type EXIT to quit.\n";
    }

    return 0;
}
