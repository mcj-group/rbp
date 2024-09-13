#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <queue>
#include <tuple>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>

#include "message.h"
#include "mrf.h"

#include "MultiQueue.h"

namespace heap_rbp {

// This algorithm is based on relaxed-rbp.cpp with filtering to reduce
// queue pressure.
// This version is implemented with the heap-based MultiQueue as the scheduler.

static std::vector<Message>* messages;

struct stat {
    uint64_t iters = 0;
    uint64_t updates = 0;
    uint64_t skips = 0;
};

static MRF* mrf;
static const Message* baseMessage;
static std::vector<std::mutex>* locks;
static std::atomic<double>* priorities;
constexpr static const unsigned MAX_PREFETCH = 64;

static inline uint64_t id(const Message* m) {
    return std::distance(baseMessage, m);
}

template <class T>
static inline double priority(const Message* m, T futureMessage) {
    return utils::distance(m->logMu, futureMessage);
}

static inline double priority(const Message* m) {
    return priority(m, mrf->getFutureMessage(*m));
}

template<typename MQ_IO>
static void thread_task(MRF* mrf, double sensitivity, stat *stats, MQ_IO &pq){
    uint64_t updates = 0;
    uint64_t skips = 0;
    uint64_t it = 0;
    pq.initTID();

    while (true) {
        double pushedPrio;
        Message *m;
        auto item = pq.pop();
        if (item) std::tie(pushedPrio, m) = item.get();
        else break; 
        it++;

        uint64_t mi = std::min(m->i, m->j);
        uint64_t mj = std::max(m->i, m->j);
        locks->at(mi).lock();
        locks->at(mj).lock();

        uint64_t mID = id(m);
        double curPrio = priorities[mID].load(std::memory_order_seq_cst);
        if (curPrio < pushedPrio) {
            // Outdated, re-insert with the current priority
            if (curPrio > sensitivity) {
                pq.push(curPrio, m);
            }
            skips++;

        } else {
            // Up to date, perform updates and enqueue messages to neighbors
            auto futureMessage = mrf->getFutureMessage(*m);
            mrf->updateMessage(*m, futureMessage);
            priorities[mID] = 0.0;

            auto fromJ = mrf->getMessagesFrom(m->j);
            for (Message* affected : fromJ) {
                if (affected->j == m->i) {
                    continue;
                }

                uint64_t affID = id(affected);
                double affNewPrio = priority(affected);
                double affCurPrio = priorities[affID].load(std::memory_order_seq_cst);

                // only update the affected's priority if affNewPrio is more prioritized
                if (affCurPrio < affNewPrio) {
                    if (affNewPrio > sensitivity) {
                        while (affCurPrio < affNewPrio &&
                            !priorities[affID].compare_exchange_weak(
                                affCurPrio, affNewPrio,
                                std::memory_order_seq_cst, std::memory_order_seq_cst));
                        // if swapped, push the msg into the queue again
                        if (affCurPrio < affNewPrio) {
                            pq.push(affNewPrio, affected);
                        }
                    }
                } else {
                    if (affCurPrio > sensitivity) {
                        while (affCurPrio < affNewPrio &&
                            !priorities[affID].compare_exchange_weak(
                                affCurPrio, affNewPrio,
                                std::memory_order_seq_cst, std::memory_order_seq_cst));
                    }
                }
            }
            updates++;
        }

        locks->at(mi).unlock();
        locks->at(mj).unlock();
    }

    stats->iters=it;
    stats->updates=updates;
    stats->skips=skips;
}

template<bool usePrefetch>
void spawnTasks(double sensitivity, int threadNum, int queueNum, int batchSizePop,
                int batchSizePush, int stickiness) {

    using PQElement = std::tuple<double, Message*>;
    std::function<void(Message *)> prefetcher = [&] (Message *m) -> void {
        __builtin_prefetch(&priorities[id(m)], 1, 3);
    };

    using MQ_IO = mbq::MultiQueue<decltype(prefetcher), std::less<PQElement>, double, Message*, usePrefetch>;
    MQ_IO pq(prefetcher, queueNum, threadNum, batchSizePop, batchSizePush, stickiness);

    for (Message& message: *messages) {
        double prio = priority(&message);
        if (prio > sensitivity) {
            priorities[(id(&message))] = prio;
            pq.push(priority(&message), &message);
        }
    }

    std::vector<std::thread*> workers;
    stat stats[threadNum];

    auto startTime = std::chrono::high_resolution_clock::now();

    cpu_set_t cpuset;
    for (uint64_t i = 1; i < threadNum; i++) {
        CPU_ZERO(&cpuset);
        uint64_t coreID = i;
        CPU_SET(coreID, &cpuset);
        std::thread *newThread = new std::thread(
            thread_task<MQ_IO>, std::ref(mrf), 
            std::ref(sensitivity), &stats[i], std::ref(pq)
        );
        int rc = pthread_setaffinity_np(newThread->native_handle(),
                                        sizeof(cpu_set_t), &cpuset);
        if (rc != 0) {
            std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
        }
        workers.push_back(newThread);
    }
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    sched_setaffinity(0, sizeof(cpuset), &cpuset);
    thread_task<MQ_IO>(mrf, sensitivity, &stats[0], pq);
    for (std::thread*& worker : workers) {
        worker->join();
        delete worker;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime-startTime);

    pq.stat();
    std::cout << "runtime_ms " << ms.count() << std::endl;

    uint64_t totalIters = 0;
    uint64_t totalUpdates = 0;
    uint64_t totalSkips = 0;
    for (int i = 0; i < threadNum; i++) {
        totalIters += stats[i].iters;
        totalUpdates += stats[i].updates;
        totalSkips += stats[i].skips;
    }

    std::cout << "totalIters = " << totalIters << std::endl;
    std::cout << "totalUpdates = " << totalUpdates << std::endl;
    std::cout << "totalSkips = " << totalSkips << std::endl;
}

void solve(MRF* mrf, double sensitivity,
           std::vector<std::array<double,2> >* answer,
           int threadNum, int queueNum, int batchSizePop, int batchSizePush,
           int usePrefetch, int stickiness) {
    std::cout << "Running a relaxed version of residual belief "
              << "propagation with heap-based multiqueue"
              << std::endl;
    std::cout << "threads = " << threadNum << "\n";
    std::cout << "queues = " << queueNum << "\n";
    std::cout << "batchSizePop = " << batchSizePop << "\n";
    std::cout << "batchSizePush = " << batchSizePush << "\n";
    std::cout << "usePrefetch = " << usePrefetch << "\n";
    std::cout << "stickiness = " << stickiness << "\n";

    heap_rbp::messages = &mrf->getMessages();
    heap_rbp::mrf = mrf;
    heap_rbp::baseMessage = messages->data();
    heap_rbp::locks = new std::vector<std::mutex>(mrf->getNodes());
    heap_rbp::priorities = new std::atomic<double>[messages->size()]();

    if (usePrefetch) 
        spawnTasks<true>(sensitivity, threadNum, queueNum, batchSizePop, batchSizePush, stickiness);
    else
        spawnTasks<false>(sensitivity, threadNum, queueNum, batchSizePop, batchSizePush, stickiness);

    mrf->getNodeProbabilities(answer);

    delete locks;
}

}
