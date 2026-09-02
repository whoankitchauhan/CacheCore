/**
 * @file Node.h
 * @brief Doubly-linked list node for the LRU cache.
 *
 * Each Node holds a key-value pair, optional TTL metadata, and raw
 * prev/next pointers for the doubly-linked list.
 *
 * Ownership: LRUCache is the sole owner of every Node. Nodes are
 * allocated with 'new' inside LRUCache and freed with 'delete' when
 * evicted, expired, or explicitly deleted.  Raw pointers are used
 * deliberately: smart pointers with doubly-linked lists require
 * weak_ptr back-links or shared_ptr cycles, which obscure the core
 * data structure logic without adding safety.
 */

#pragma once

#include <string>
#include <chrono>

struct Node {
    std::string key;
    std::string value;

    // TTL support ---------------------------------------------------------
    // hasTTL = false means "no expiry".
    // expiresAt is only meaningful when hasTTL == true.
    bool hasTTL{false};
    std::chrono::steady_clock::time_point expiresAt{};

    // Doubly-linked list links --------------------------------------------
    Node* prev{nullptr};
    Node* next{nullptr};

    // Construct a regular node
    Node(std::string k, std::string v,
         bool ttl = false,
         std::chrono::steady_clock::time_point exp = {})
        : key(std::move(k))
        , value(std::move(v))
        , hasTTL(ttl)
        , expiresAt(exp)
    {}

    // Sentinel nodes (head / tail) use the default constructor
    Node() = default;
};
