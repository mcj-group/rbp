#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <tuple>
#include <unistd.h> //for sched_getaffinity
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <barrier>
#include <variant>

#include "message.h"
#include "mrf.h"
#include "relaxed_rbp.h"
#include "multiqueue_opt.h"
#include "StealingMultiQueue.h"

//for CSR MRF
#define VAR_DEG 3
#define CHK_DEG 6


static std::vector<int> get_allowed_cpus() {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    if (sched_getaffinity(0, sizeof(mask), &mask) != 0) {
        perror("sched_getaffinity");
    }
    std::vector<int> cpus;
    for (int c = 0; c < CPU_SETSIZE; ++c) {
        if (CPU_ISSET(c, &mask))
            cpus.push_back(c);
    }
    return cpus;
}

static void
    #ifdef PERF
            __attribute__ ((noinline))
    #else
            __attribute__ ((always_inline)) inline
    #endif

            perf_lock(std::mutex& m) {
                    m.lock();
            }

static void
    #ifdef PERF
            __attribute__ ((noinline))
    #else
            __attribute__ ((always_inline)) inline
    #endif

            perf_unlock(std::mutex& m) {
                    m.unlock();
            }


namespace relaxed_rbp{

    static std::vector<Message::Message>* messages;

    struct stat {
        uint64_t iters = 0;
        uint64_t updates = 0;
        uint64_t pushes = 0;
        uint64_t pops = 0;
        uint64_t skips = 0;
    };

    static MRF* mrf;
    static const Message::Message* baseMessage;
    static std::vector<std::mutex>* locks;
    static std::atomic<double>* priorities;

    static inline uint64_t id(const Message::Message* m) {
        return std::distance(baseMessage, m);
    }

    template <class T>
    static inline double priority(const Message::Message* m, T futureMessage) {
        return utils::distance(m->logMu, futureMessage);
    }

    static inline double priority(const Message::Message* m) {
        return priority(m, mrf->getFutureMessage(*m));
    }

    template<class PQ_TYPE>
    static void thread_task(MRF* mrf, uint64_t threads, double sensitivity, stat *stats, PQ_TYPE &pq, std::barrier<>& sharedBarrier);
    
    void solve_mq(MRF* mrf, double sensitivity,
                  std::vector<std::array<double,2> >* answer,
                  uint64_t threads, int queueNum, int batchSizePop, int batchSizePush,
                  perf_metrics &metrics) {
        std::cout << "Running Relaxed Residual Belief "
                  << "Propagation with " << threads << " Thread(s) and Heap-based Multiqueue " << std::endl;
    
        relaxed_rbp::messages = &mrf->getMessages();
        relaxed_rbp::mrf = mrf;
        relaxed_rbp::baseMessage = messages->data();
        relaxed_rbp::locks = new std::vector<std::mutex>(mrf->getNodes());
        relaxed_rbp::priorities = new std::atomic<double>[messages->size()]();
    
        using PQElement = std::tuple<double, uint64_t>;
        std::function<void(uint64_t)> prefetcher = [&] (uint64_t key) -> void { }; //empty prefetcher to satisfy MQ code
        using MQ_IO = MultiQueue<decltype(prefetcher), std::less<PQElement>, double, uint64_t>;
        MQ_IO pq = MQ_IO(prefetcher, queueNum, threads, batchSizePop, batchSizePush);
    
        std::vector<std::thread*> workers;
        stat stats[threads];
        std::barrier<> doneInserting(threads);
    
        auto startTime = std::chrono::high_resolution_clock::now();
    
    #if ENABLE_THREAD_PINNING
        // Get all CPUs Slurm granted to this job
        auto allowed_cpus = get_allowed_cpus();
        cpu_set_t cpuset;
    #endif
    
        // ---- Launch workers ----
        for (uint64_t i = 1; i < threads; i++) {
            //should be fast/cheap to pass doubles, pointers by value (copy) 
            //passing by reference only for pq and barrier because they are larger
            std::thread* newThread = new std::thread(thread_task<MQ_IO>, mrf, threads,
                                sensitivity, &stats[i], std::ref(pq), std::ref(doneInserting));
    
    #if ENABLE_THREAD_PINNING
            CPU_ZERO(&cpuset);
            int coreID = allowed_cpus[i % allowed_cpus.size()];   // sequential mapping
            CPU_SET(coreID, &cpuset);
            int rc = pthread_setaffinity_np(newThread->native_handle(),
                                            sizeof(cpu_set_t), &cpuset);
            if (rc != 0) {
                std::cerr << "Error pinning thread " << i
                          << " to CPU " << coreID << " rc=" << rc << "\n";
            }
    #endif
            workers.push_back(newThread);
        }
    
    #if ENABLE_THREAD_PINNING
        // Pin main thread to the first allowed CPU
        if (!allowed_cpus.empty()) {
            CPU_ZERO(&cpuset);
            CPU_SET(allowed_cpus[0], &cpuset);
            sched_setaffinity(0, sizeof(cpuset), &cpuset);
        }
    #endif
    
        // ---- Main thread also works ----
        thread_task<MQ_IO>(mrf, threads, sensitivity, &stats[0], std::ref(pq), std::ref(doneInserting));
    
        for (std::thread*& worker : workers) {
            worker->join();
            delete worker;
        }
    
        auto endTime = std::chrono::high_resolution_clock::now();
        auto runtime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime-startTime);
    
        uint64_t total_updates = 0;
        for (uint64_t i = 0; i < threads; i++) {
            total_updates += stats[i].updates;
        }
    
        mrf->getNodeProbabilities(answer);
        //Update metrics
        metrics.runtime_ms = runtime_ms.count();
        metrics.num_updates = total_updates;
    
        delete locks;
    }
    
    void solve_smq(MRF* mrf, double sensitivity,
                   std::vector<std::array<double,2>>* answer,
                   uint64_t threads,
                   perf_metrics &metrics) {
        std::cout << "Running Relaxed Residual Belief "
                  << "Propagation with " << threads << " Thread(s) and Stealing Multi-Queue " << std::endl;
    
        relaxed_rbp::messages = &mrf->getMessages();
        relaxed_rbp::mrf = mrf;
        relaxed_rbp::baseMessage = messages->data();
        relaxed_rbp::locks = new std::vector<std::mutex>(mrf->getNodes());
        relaxed_rbp::priorities = new std::atomic<double>[messages->size()]();
    
        //SMQ
        int smq_size = messages->size();
        using SMQ_IO = StealingMultiQueue;
        SMQ_IO pq = SMQ_IO(smq_size, threads);
    
        std::vector<std::thread*> workers;
        stat stats[threads];
        std::barrier<> doneInserting(threads);
    
        auto startTime = std::chrono::high_resolution_clock::now();
    
    #if ENABLE_THREAD_PINNING
        auto allowed_cpus = get_allowed_cpus();
        cpu_set_t cpuset;
    #endif
    
        for (uint64_t i = 1; i < threads; i++)
        {
            std::thread *newThread = new std::thread(thread_task<SMQ_IO>, mrf, threads,
                                sensitivity, &stats[i], std::ref(pq), std::ref(doneInserting));
    
    #if ENABLE_THREAD_PINNING
            CPU_ZERO(&cpuset);
            int coreID = allowed_cpus[i % allowed_cpus.size()];
            CPU_SET(coreID, &cpuset);
            int rc = pthread_setaffinity_np(newThread->native_handle(),
                                            sizeof(cpu_set_t), &cpuset);
            if (rc != 0) {
                std::cerr << "Error pinning thread " << i
                          << " to CPU " << coreID << " rc=" << rc << "\n";
            }
    #endif
            workers.push_back(newThread);
        }
    
    #if ENABLE_THREAD_PINNING
        if (!allowed_cpus.empty()) {
            CPU_ZERO(&cpuset);
            CPU_SET(allowed_cpus[0], &cpuset);
            sched_setaffinity(0, sizeof(cpuset), &cpuset);
        }
    #endif
    
        thread_task<SMQ_IO>(mrf, threads, sensitivity, &stats[0], std::ref(pq), std::ref(doneInserting));
    
        for (std::thread*& worker : workers) {
        worker->join();
        delete worker;
    }
    
        auto endTime = std::chrono::high_resolution_clock::now();
        auto runtime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime-startTime);
    
        uint64_t total_updates = 0;
        for (uint64_t i = 0; i < threads; i++) {
            total_updates += stats[i].updates;
        }
    
        mrf->getNodeProbabilities(answer);
        //Update metrics
        metrics.runtime_ms = runtime_ms.count();
        metrics.num_updates = total_updates;
    
        delete locks;
    }

    template<class PQ_TYPE>
    static void thread_task(MRF* mrf, uint64_t threads, double sensitivity, stat *stats, PQ_TYPE &pq, std::barrier<>& sharedBarrier){
        uint64_t updates = 0;
        uint64_t pushes = 0;
        uint64_t pops = 0;
        uint64_t skips = 0;
        uint64_t it = 0;
        pq.initTID();

        //split messages and insert into pq
        int split_msgs = std::ceil(double(messages->size()) / threads);
        int base_range = pq.tID * split_msgs; // partitioning the Messages
        //std::cout << "thread id is " << pq.tID << std::endl;
        for (int i = base_range; i < std::min(base_range + split_msgs, int(messages->size())); i++) { //changed to handle remainder
            Message::Message& message = messages->at(i);
            double prio = priority(&message);
            if (prio > sensitivity) {
                priorities[(id(&message))] = prio;
                pq.push(priority(&message), id(&message));
            }
        }
        sharedBarrier.arrive_and_wait();

        while (true) {
            double pushedPrio;
            uint64_t mID;
            auto item = pq.pop();
            if (item) std::tie(pushedPrio, mID) = item.get();
            else break;
            pops++;

            Message::Message* m = &(messages->at(mID));
            uint64_t mi = std::min(m->i, m->j);
            uint64_t mj = std::max(m->i, m->j);
            perf_lock(locks->at(mi)); //.lock();
            perf_lock(locks->at(mj)); //.lock();

            // uint64_t mID = id(m); //no longer need because pq stored id as key
            double curPrio = priorities[mID].load(std::memory_order_relaxed);
            if (curPrio < pushedPrio) {
                if (curPrio > sensitivity) {
                    pq.push(curPrio, mID);
                }
                skips++;

            } else {
                mrf->updateMessage(*m);
                updates++;
                priorities[mID] = 0.0;

                auto fromJ = mrf->getMessagesFrom(m->j);
                for (Message::Message* affected : fromJ) {
                    if (affected->j == m->i) {
                        continue;
                    } //residual_bp_filtered_inserts_only skips this check because this is backward edge and so prio of 0 anyways

                    uint64_t affID = id(affected);
                    double affNewPrio = priority(affected);
                    double affCurPrio = priorities[affID].load(std::memory_order_relaxed);

                    //priority update
                    while (affCurPrio != affNewPrio) { //can skip if priority is unchanged
                        if (affCurPrio < affNewPrio) { //new priority is higher than priority stored in priorities array
                            if (affNewPrio > sensitivity) {
                                bool swapped = priorities[affID].compare_exchange_weak(
                                    affCurPrio, affNewPrio);

                                if (swapped) {
                                    pq.push(affNewPrio, id(affected)); //reinsert into priority queue to consider for message update in future iterations
                                    break;
                                }
                            }
                            else { //new priority is less than sensitivity, so don't reinsert into PQ, no longer contender for future message updates
                                break;
                            }
                        }
                        else if (affCurPrio > affNewPrio) {
                            if (affCurPrio > sensitivity) {
                                bool swapped = priorities[affID].compare_exchange_weak(
                                    affCurPrio, affNewPrio);

                                if (swapped) { //no need to push -- post-pop will reinsert
                                    break;
                                }
                            }
                            else {
                                break;
                            }
                        }
                    }
                }
            }

            perf_unlock(locks->at(mi)); //.unlock();
            perf_unlock(locks->at(mj)); //.unlock();
            it++;
        }

        stats->iters=it;
        stats->updates=updates;
        stats->pops=pops;
        stats->pushes=pushes;
        stats->skips=skips;
    }

} //namespace relaxed_rbp

namespace relaxed_rbp_CSR {

    static MRF_CSR* mrf;
    static const Message_CSR::Message* baseMessage;
    static std::atomic<uint64_t> total_updates;
    static std::vector<std::mutex>* locks;
    static std::vector<Message_CSR::Message>* messages;
    static std::atomic<double>* priorities;
    static bool fair = true;

    static inline uint64_t id(const Message_CSR::Message* m) {
        return std::distance(baseMessage, m);
    }

    static inline double priority(const Message_CSR::Message* m) {
        return utils_CSR::logDifference(mrf->getLogMu(*m), mrf->getFutureMessage(*m));
    }

    template<class PQ_TYPE>
    static void thread_task(MRF_CSR* mrf, uint64_t threads, double sensitivity, PQ_TYPE &pq, std::barrier<>& sharedBarrier);
    
    void solve_mq(MRF_CSR* mrf, double sensitivity,
                  std::vector<std::array<double,2>>* answer,
                  uint64_t threads, int queueNum, int batchSizePop, int batchSizePush,
                  perf_metrics &metrics) {
        std::cout << "Running Relaxed Residual Belief "
                  << "Propagation with " << threads << " Thread(s) and a Multi-Queue " << std::endl;
        relaxed_rbp_CSR::messages = &mrf->getMessages();
        relaxed_rbp_CSR::mrf = mrf;
        relaxed_rbp_CSR::baseMessage = messages->data();
        relaxed_rbp_CSR::locks = new std::vector<std::mutex>(mrf->getTotalNumNodes());
        //Use a 4:1 ratio of queues to threads, to reduce contention
        relaxed_rbp_CSR::priorities = new std::atomic<double>[messages->size()]();
    
        using PQElement = std::tuple<double, uint64_t>; //earlier version stored entire Message instead of just ID
        std::function<void(uint64_t)> prefetcher = [&] (uint64_t key) -> void { }; //empty prefetcher to satisfy MQ code
        using MQ_IO = MultiQueue<decltype(prefetcher), std::less<PQElement>, double, uint64_t>;
        MQ_IO pq(prefetcher, queueNum, threads, batchSizePop, batchSizePush);
    
        std::vector<std::thread*> workers;
        total_updates = 0ul;

        std::barrier<> doneInserting(threads);
    
        auto startTime = std::chrono::high_resolution_clock::now();
    
    #if ENABLE_THREAD_PINNING
        auto allowed_cpus = get_allowed_cpus();
        cpu_set_t cpuset;
    #endif
    
        for (uint64_t i = 1; i < threads; i++) {
            std::thread *newThread = new std::thread(thread_task<MQ_IO>, mrf, threads,
                                sensitivity, std::ref(pq), std::ref(doneInserting));
    
    #if ENABLE_THREAD_PINNING
            CPU_ZERO(&cpuset);
            int coreID = allowed_cpus[i % allowed_cpus.size()];
            CPU_SET(coreID, &cpuset);
            int rc = pthread_setaffinity_np(newThread->native_handle(),
                                            sizeof(cpu_set_t), &cpuset);
            if (rc != 0) {
                std::cerr << "Error pinning thread " << i
                          << " to CPU " << coreID << " rc=" << rc << "\n";
            }
    #endif
            workers.push_back(newThread);
        }
    
    #if ENABLE_THREAD_PINNING
        if (!allowed_cpus.empty()) {
            CPU_ZERO(&cpuset);
            CPU_SET(allowed_cpus[0], &cpuset);
            sched_setaffinity(0, sizeof(cpuset), &cpuset);
        }
    #endif
    
        thread_task<MQ_IO>(mrf, threads, sensitivity, std::ref(pq), std::ref(doneInserting));
    
        for (std::thread*& worker : workers) {
            worker->join();
            delete worker;
        }
    
        auto endTime = std::chrono::high_resolution_clock::now();
        auto runtime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime-startTime);
        //Update metrics
        metrics.runtime_ms = runtime_ms.count();
        metrics.num_updates = total_updates;
    
        mrf->getVarNodeProbabilities(answer);

        delete locks;
    }
    
    void solve_smq(MRF_CSR* mrf, double sensitivity,
                   std::vector<std::array<double,2>>* answer,
                   uint64_t threads,
                   perf_metrics &metrics) {
        std::cout << "Running Relaxed Residual Belief "
                  << "Propagation with " << threads << " Thread(s) and Stealing Multi-Queue " << std::endl;
    
        relaxed_rbp_CSR::messages = &mrf->getMessages();
        relaxed_rbp_CSR::mrf = mrf;
        relaxed_rbp_CSR::baseMessage = messages->data();
        relaxed_rbp_CSR::locks = new std::vector<std::mutex>(mrf->getTotalNumNodes());
        relaxed_rbp_CSR::priorities = new std::atomic<double>[messages->size()]();
    
        //SMQ
        int smq_size = messages->size();
        using SMQ_IO = StealingMultiQueue;
        SMQ_IO pq = SMQ_IO(smq_size, threads);
    
        std::vector<std::thread*> workers;
        total_updates = 0ul;
        std::barrier<> doneInserting(threads);
    
        auto startTime = std::chrono::high_resolution_clock::now();
    
    #if ENABLE_THREAD_PINNING
        auto allowed_cpus = get_allowed_cpus();
        cpu_set_t cpuset;
    #endif
    
        for (uint64_t i = 1; i < threads; i++) {
            std::thread *newThread = new std::thread(thread_task<SMQ_IO>, mrf, threads,
                                sensitivity, std::ref(pq), std::ref(doneInserting));
    
    #if ENABLE_THREAD_PINNING
            CPU_ZERO(&cpuset);
            int coreID = allowed_cpus[i % allowed_cpus.size()];
            CPU_SET(coreID, &cpuset);
            int rc = pthread_setaffinity_np(newThread->native_handle(),
                                            sizeof(cpu_set_t), &cpuset);
            if (rc != 0) {
                std::cerr << "Error pinning thread " << i
                          << " to CPU " << coreID << " rc=" << rc << "\n";
            }
    #endif
            workers.push_back(newThread);
        }
    
    #if ENABLE_THREAD_PINNING
        if (!allowed_cpus.empty()) {
            CPU_ZERO(&cpuset);
            CPU_SET(allowed_cpus[0], &cpuset);
            sched_setaffinity(0, sizeof(cpuset), &cpuset);
        }
    #endif
    
        thread_task<SMQ_IO>(mrf, threads, sensitivity, std::ref(pq), std::ref(doneInserting));
    
        for (std::thread*& worker : workers) {
            worker->join();
            delete worker;
        }
    
        auto endTime = std::chrono::high_resolution_clock::now();
        auto runtime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime-startTime);
        //Update metrics
        metrics.runtime_ms = runtime_ms.count();
        metrics.num_updates = total_updates;
    
        mrf->getVarNodeProbabilities(answer);
        delete locks;
    }

    template<class PQ_TYPE>
    static void thread_task(MRF_CSR *mrf, uint64_t threads, double sensitivity, PQ_TYPE &pq, std::barrier<>& sharedBarrier) {
        // std::cout<< "=== Thread Task Start ===" << std::endl;
        uint64_t updates = 0;
        uint64_t pushes = 0;
        uint64_t pops = 0;
        uint64_t skips = 0;
        uint64_t it = 0;
        std::array<Message_CSR::Message*,VAR_DEG>* msgFromVar = nullptr;
        std::array<Message_CSR::Message*,CHK_DEG>* msgFromChk = nullptr;
        pq.initTID();

        //split messages and insert into pq
        int split_msgs = std::ceil(double(messages->size()) / threads);
        int base_range = pq.tID * split_msgs; // partitioning the Messages
        //std::cout << "thread id is " << pq.tID << std::endl;
        for (int i = base_range; i < std::min(base_range + split_msgs, int(messages->size())); i++) { //changed to handle remainder
            Message_CSR::Message& message = messages->at(i);
            double prio = priority(&message);
            if (prio > sensitivity) {
                priorities[(id(&message))] = prio;
                pq.push(priority(&message), id(&message));
            }
        }
        sharedBarrier.arrive_and_wait();

        while (true) {

            double pushedPrio;
            uint64_t mID;
            auto item = pq.pop();
            if (item) std::tie(pushedPrio, mID) = item.get();
            else break;
            pops++;

            Message_CSR::Message* m = &(messages->at(mID));
            uint64_t mi = std::min(m->i,m->j);
            uint64_t mj = std::max(m->i,m->j);

            if (fair) {
                perf_lock(locks->at(mi)); //).lock();
                perf_lock(locks->at(mj)); //.lock();
            }

            // uint64_t mID = id(m); //no longer need because pq stored id as key
            double curPrio = priorities[mID].load(std::memory_order_relaxed);
            if (curPrio < pushedPrio) {
                if (curPrio > sensitivity) {
                    pq.push(curPrio, mID);
                }
                skips++;
            } else {
                mrf->getFutureMessageAndUpdate(*m);
                updates++;
                priorities[mID] = 0.0;

                if (m->chk2var) { //different sized array and functions if node is variable node versus check node
                    msgFromVar = mrf->getMessageFromVar(m->j);
                    for (Message_CSR::Message* affected: *msgFromVar) {
                        if (affected->j == m->i) {
                            continue;
                        }

                        uint64_t affID = id(affected);
                        double affNewPrio = priority(affected);
                        double affCurPrio = priorities[affID].load(std::memory_order_relaxed);

                        //priority update
                        while (affCurPrio != affNewPrio) { //can skip if priority is unchanged
                            if (affCurPrio < affNewPrio) { //new priority is higher than priority stored in priorities array
                                if (affNewPrio > sensitivity) {
                                    bool swapped = priorities[affID].compare_exchange_weak(
                                        affCurPrio, affNewPrio);

                                    if (swapped) {
                                        pq.push(affNewPrio, id(affected)); //reinsert into priority queue to consider for message update in future iterations
                                        break;
                                    }
                                }
                                else { //new priority is less than sensitivity, so don't reinsert into PQ, no longer contender for future message updates
                                    break;
                                }
                            }
                            else if (affCurPrio > affNewPrio) {
                                if (affCurPrio > sensitivity) {
                                    bool swapped = priorities[affID].compare_exchange_weak(
                                        affCurPrio, affNewPrio);

                                    if (swapped) { //no need to push -- post-pop will reinsert
                                        break;
                                    }
                                }
                                else {
                                    break;
                                }
                            }
                        }
                    }
                } else {
                    msgFromChk = mrf->getMessageFromChk(m->j);
                    for (Message_CSR::Message* affected: *msgFromChk) {
                        if (affected->j == m->i) {
                            continue;
                        }

                        uint64_t affID = id(affected);
                        double affNewPrio = priority(affected);
                        double affCurPrio = priorities[affID].load(std::memory_order_relaxed);

                        //priority update
                        while (affCurPrio != affNewPrio) { //can skip if priority is unchanged
                            if (affCurPrio < affNewPrio) { //new priority is higher than priority stored in priorities array
                                if (affNewPrio > sensitivity) {
                                    bool swapped = priorities[affID].compare_exchange_weak(
                                        affCurPrio, affNewPrio);

                                    if (swapped) {
                                        pq.push(affNewPrio, id(affected)); //reinsert into priority queue to consider for message update in future iterations
                                        break;
                                    }
                                }
                                else { //new priority is less than sensitivity, so don't reinsert into PQ, no longer contender for future message updates
                                    break;
                                }
                            }
                            else if (affCurPrio > affNewPrio) {
                                if (affCurPrio > sensitivity) {
                                    bool swapped = priorities[affID].compare_exchange_weak(
                                        affCurPrio, affNewPrio);

                                    if (swapped) { //no need to push -- post-pop will reinsert
                                        break;
                                    }
                                }
                                else {
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            //pq.push(0.0, m);

            if (fair) {
                perf_unlock(locks->at(mi)); //.unlock();
                perf_unlock(locks->at(mj)); //).unlock();
            }
            it++;
        }
        total_updates += updates;
    }

} //namespace relaxed_rbp_CSR
