#pragma once

#include <boost/heap/fibonacci_heap.hpp>
#include <cstdlib>
#include <iostream>
#include <tuple>
#include <vector>
#include <algorithm>
#include <mutex>
#include <random>

// This is a Max MultiQueue. The implementation is inspired by the MultiPQ in
// src/bp/algorithms/queues/MultiPQ.java
// TODO(mcj) support a min Q if useful.

class MultiQueue {
    static constexpr uint64_t UNQUEUED = UINT64_MAX;

    struct Node;
    using PQElement = std::tuple<double, Node*>;
    using PQ = boost::heap::fibonacci_heap<
            PQElement,
            boost::heap::compare<std::less<PQElement> >
            >;
    struct Node {
        uint64_t queue;
        PQ::handle_type handle;

        double priority() const {
            assert(queue != UNQUEUED);
            PQElement e = *handle;
            return std::get<0>(e);
        }
    };

    std::vector<Node> nodes;
    std::vector<std::mutex> locks;
    std::vector<PQ> queues;


    inline uint64_t key(const Node& n) const {
        return std::distance(nodes.data(), &n);
    }


  public:

    MultiQueue(uint64_t maxSize, uint64_t relax)
      : nodes(maxSize)
      , locks(relax)
      , queues(relax)
    {
        for (auto& node : nodes) {
            node.queue = UNQUEUED;
        }
    }


    uint64_t ThreadLocalRandom(const double min, const double max) {
        static thread_local std::mt19937_64 generator;
        std::uniform_real_distribution<> distribution(min,max);
        return distribution(generator);
    }


    // Insert the key with given priority,
    // assuming the key is not currently queued
    //
    // Importantly, this does not synchronize on the keyed object,
    // so the caller should be sequential or have private access
    // to the keyed object.
    inline void insert(uint64_t key, double priority) {
        Node& node = nodes.at(key);
        assert(node.queue == UNQUEUED);
        uint64_t queue = ThreadLocalRandom(0, queues.size()); // upper bound
        node.queue = queue;
        locks.at(queue).lock();
        auto handle = queues[queue].push(std::make_tuple(priority, &node));
        node.handle = handle;
        locks[queue].unlock();
    }


    inline void changePriority(uint64_t key, double newPriority) {
        Node& node = nodes.at(key);
        while (true) {
            uint64_t queue = node.queue;
            if (queue == UNQUEUED) {
                // Another thread removed the key
                return;
            }
            locks.at(queue).lock();
            if (node.queue != queue) {
                // Between when we started the loop and now, another thread
                // already removed the key and reinserted it.
                // Try again with the correct queue lock
                locks[queue].unlock();
                continue;
            }
            // We've locked the key node's queue, so now no other thread can
            // change the key's priority, nor extract it.
            // Consequently *I think* we don't need to synchronize node.handle
            // either. No one should be able to write it, as concurrent inserts
            // are not allowed.
            queues[queue].update(node.handle,
                                 std::make_tuple(newPriority, &node));
            locks[queue].unlock();
            return;
        }
    }


    inline void changePriority(uint64_t key,
                               double newPriority,
                               double sensitivity) {
        // FIXME(mcj) this is risky as it reads key's priority without
        // synchronization
        if (std::abs(nodes.at(key).priority() - newPriority) < sensitivity)
            return;
        changePriority(key, newPriority);
    }


    inline uint64_t extractMax() {
        while (true) {
            Node* extract = nullptr;
            {
                // Pick the higher priority max of queue i and j
                uint64_t i = ThreadLocalRandom(0, queues.size());
                uint64_t j = ThreadLocalRandom(0, queues.size());
                const PQ& pqi = queues[i];
                const PQ& pqj = queues[j];
                Node* ni = nullptr;
                Node* nj = nullptr;
                double prio_i, prio_j;
                if (!pqi.empty()) {
                    if (locks[i].try_lock()) {
                        if (!pqi.empty()) {
                            std::tie(prio_i, ni) = pqi.top();
                        }
                        locks[i].unlock();
                    } else {
                        // Queue i was not empty, but contended. Try a new pair
                        continue;
                    }
                }
                if (!pqj.empty()) {
                    if (locks[j].try_lock()) {
                        if (!pqj.empty()) {
                            std::tie(prio_j, nj) = pqj.top();
                        }
                        locks[j].unlock();
                    } else {
                        continue;
                    }
                }
                extract = (!ni) ? nj
                        : (!nj) ? ni
                        : prio_i > prio_j ? ni
                        : nj;
                if (extract == nullptr) continue;
            }

            uint64_t queue = extract->queue;
            if (queue == UNQUEUED) {
                // Another thread managed to extract this node already
                continue;
            }
            if (!locks.at(queue).try_lock()) {
                // Our queue of interest is contended, try a new pair of queues
                continue;
            }

            // We now have exclusive access to queue
            if (queues[queue].empty()) {
                // Another thread managed to extract the node
                locks[queue].unlock();
                continue;
            }
            Node* cur;
            double curPrio;
            std::tie(curPrio, cur) = queues[queue].top();
            if (cur != extract) {
                locks[queue].unlock();
                continue;
            }
            extract->queue = UNQUEUED;
            queues[queue].pop();

            locks[queue].unlock();
            return key(*extract);
        }
    }


    // Get an approximate max priority, noting that while this was running, the
    // reported max priority could have been concurrently popped, or a new max
    // priority concurrently inserted.
    //
    // N.B. this can't be const due to the need for locks
    inline double peekPriority() {
        double peek_prio = -std::numeric_limits<double>::infinity();
        for (uint64_t i = 0; i < queues.size(); i++) {
            const PQ& pq = queues[i];
            if (pq.empty()) {
                continue;
            }
            Node* next = nullptr;
            double next_prio;
            locks[i].lock();
            if (!pq.empty()) {
                std::tie(next_prio, next) = pq.top();
            }
            locks[i].unlock();
            if (next) {
                peek_prio = std::max(peek_prio, next_prio);
            }
        }
        return peek_prio;
    }


    inline bool check() {
        bool good = true;
        for (uint64_t i = 0; i < queues.size(); i++) {
            locks[i].lock();
            // good &= queues->at(i).check();
            if (queues[i].empty()) {
                continue;
            }
            Node* peek;
            double peekPrio;
            std::tie(peekPrio, peek) = queues[i].top();
            std::cout<< peekPrio <<"\n";

            if (peek != nullptr) {
                good = (peek->queue == i);
            }
            locks[i].unlock();
        }
        return good;
    }

};
