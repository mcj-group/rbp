#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <tuple>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

#include "message.h"
#include "mrf.h"
#include "relaxed-rbp.h"
#include "multiqueue.h"

namespace relaxed_rbp {

static MRF* mrf;
static const Message* baseMessage;
static MultiQueue* pq;
static std::atomic<uint64_t> iterations;
static std::vector<std::mutex>* locks;
static std::vector<Message>* messages;
static bool fair = true;


static inline uint64_t id(const Message* m) {
    return std::distance(baseMessage, m);
}


static inline double priority(const Message* m) {
    return utils::distance(m->logMu, mrf->getFutureMessage(*m));
}


static void thread_task(MRF* mrf, double sensitivity);


void solve(MRF* mrf, double sensitivity, uint64_t threads,
           std::vector<std::array<double,2> >* answer) {
    std::cout << "Running the software parallel "
              << "relaxed residual BP algorithm "
              << "using a MultiQueue and "
              << threads
              << " threads"
              << std::endl;

    relaxed_rbp::messages = &mrf->getMessages();
    relaxed_rbp::mrf = mrf;
    relaxed_rbp::baseMessage = messages->data();
    relaxed_rbp::locks = new std::vector<std::mutex>(mrf->getNodes());
    // Use a 4:1 ratio of queues to threads, to reduce contention and imitate
    // the Java implementation
    relaxed_rbp::pq = new MultiQueue(messages->size(), 4 * threads);

    for (Message& message: *messages) {
        pq->insert(id(&message), priority(&message));
    }

    auto startTime = std::chrono::high_resolution_clock::now();
    std::vector<std::thread*> workers;
    iterations = 0ul;

    for (uint64_t i = 1; i < threads; i++) {
        workers.push_back(new std::thread(thread_task, mrf, sensitivity));
    }
    thread_task(mrf, sensitivity);
    for (std::thread*& worker : workers) {
        worker->join();
        delete worker;
    }
    auto endTime = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime-startTime);

    std::cout << "runtime_ms " << ms.count() << std::endl;
    std::cout << "Updates " << iterations << std::endl;

    mrf->getNodeProbabilities(answer);

    delete pq;
    delete locks;
}


static void thread_task(MRF* mrf, double sensitivity) {
    uint64_t it = 0;
    while (true) {
        if (++it % 1000 == 0) {
            if (pq->peekPriority() < sensitivity) {
                iterations += it;
                return;
            }
        }

        uint64_t top_id = pq->extractMax();
        Message& m = messages->at(top_id);
        uint64_t mi = std::min(m.i, m.j);
        uint64_t mj = std::max(m.i, m.j);

        if (fair) {
            locks->at(mi).lock();
            locks->at(mj).lock();
        }

        mrf->updateMessage(m);
        std::vector<Message*> messagesFromJ = mrf->getMessagesFrom(m.j);

        for (Message* affected: messagesFromJ) {
            if (affected->j != m.i) {
                pq->changePriority(id(affected), priority(affected));
            }
        }

        pq->insert(id(&m), 0);

        if (fair) {
            locks->at(mi).unlock();
            locks->at(mj).unlock();
        }
    }
}

}
