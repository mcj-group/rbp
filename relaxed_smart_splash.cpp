#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <tuple>
#include <vector>
#include <unistd.h> // for sched_getaffinity
#include <thread>
#include <mutex>
#include <atomic>
#include <barrier>

#include "message.h"
#include "mrf.h"
#include "relaxed_smart_splash.h"
#include "multiqueue_opt.h"
#include "StealingMultiQueue.h"

// for CSR MRF
#define VAR_DEG 3
#define CHK_DEG 6
#define FIFO_SIZE 1000

static std::vector<int> get_allowed_cpus() {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    if (sched_getaffinity(0, sizeof(mask), &mask) != 0) {
        perror("sched_affinity");
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


namespace relaxed_smart_splash
{

    static std::vector<Message::Message> *messages;

    struct stat
    {
        uint64_t iters = 0;
        uint64_t updates = 0;
        uint64_t pushes = 0;
        uint64_t pops = 0;
        uint64_t skips = 0;
    };

    static MRF *mrf;
    static const Message::Message *baseMessage;
    static std::vector<std::mutex> *locks;
    static std::atomic<double> *priorities;

    static inline uint64_t id(const Message::Message *m)
    {
        return std::distance(baseMessage, m);
    }

    template <class T>
    static inline double priority(const Message::Message *m, T futureMessage)
    {
        return utils::distance(m->logMu, futureMessage);
    }

    // takes in vid directly and rather than vertex object from Aksenov et al
    static inline double priority(uint64_t vid)
    {
        double prio = 0;
        for (Message::Message *m : mrf->getMessagesTo(vid))
        {
            prio = std::max(prio, priority(m, mrf->getFutureMessage(*m)));
        }
        return prio;
    }

    void updateMessage(MRF *mrf, Message::Message *m)
    {

        // obtain locks
        uint64_t mi = std::min(m->i, m->j);
        uint64_t mj = std::max(m->i, m->j);
        perf_lock(relaxed_smart_splash::locks->at(mi)); //.lock();
        perf_lock(relaxed_smart_splash::locks->at(mj)); //).lock();

        // update message
        auto futureMessage = mrf->getFutureMessage(*m);
        mrf->updateMessage(*m, futureMessage);

        // release locks
        perf_unlock(relaxed_smart_splash::locks->at(mi)); //.unlock();
        perf_unlock(relaxed_smart_splash::locks->at(mj)); //.unlock();
    }

    template <typename MQ_IO>
    static void thread_task(MRF *mrf, uint64_t threads, double sensitivity, stat *stats, MQ_IO &pq, std::barrier<> &sharedBarrier, int splashH, int num_nodes);

    // multithreaded smart splash
    void solve_mq(MRF *mrf, double sensitivity,
                  std::vector<std::array<double, 2>> *answer,
                  uint64_t threads, int queueNum, int batchSizePop, int batchSizePush, int splashH,
                  perf_metrics &metrics)
    {
        std::cout << "Running SmartSplash Relaxed Residual Belief "
                  << "Propagation with " << threads << " Thread(s) and Heap-based Multiqueue " << std::endl;

        relaxed_smart_splash::messages = &mrf->getMessages();
        relaxed_smart_splash::mrf = mrf;
        relaxed_smart_splash::baseMessage = messages->data();
        relaxed_smart_splash::locks = new std::vector<std::mutex>(mrf->getNodes());
        relaxed_smart_splash::priorities = new std::atomic<double>[mrf->getNodes()](); // priority per-node for RSS instead of per-msg from RRBP

        using PQElement = std::tuple<double, uint64_t>;
        auto prefetcher = [](uint64_t) -> void {}; // returns void, empty body, nothing captured
        // std::function<void(Message::Message *)> prefetcher = [&] (Vertex* v) -> void {
        //     __builtin_prefetch(&priorities[id(v)], 1, 3); //prefetching prios array
        // };
        // can set to void to skip using prefetcher
        using MQ_IO = MultiQueue<decltype(prefetcher), std::less<PQElement>, double, uint64_t>;
        MQ_IO pq(prefetcher, queueNum, threads, batchSizePop, batchSizePush);

        std::vector<std::thread *> workers;
        stat stats[threads];

        std::barrier<> doneInserting(threads);

        auto startTime = std::chrono::high_resolution_clock::now();
#if ENABLE_THREAD_PINNING
        auto allowed_cpus = get_allowed_cpus();
        cpu_set_t cpuset;
#endif
        for (uint64_t i = 1; i < threads; i++) {
            std::thread *newThread = new std::thread(
                    thread_task<MQ_IO>, mrf, threads,
                    std::ref(sensitivity), &stats[i], std::ref(pq),
                    std::ref(doneInserting), splashH, mrf->getNodes()
                    );
#if ENABLE_THREAD_PINNING
            CPU_ZERO(&cpuset);
            int coreID = allowed_cpus[i % allowed_cpus.size()];
            CPU_SET(coreID, &cpuset);
            int rc = pthread_setaffinity_np(newThread->native_handle(),
                                            sizeof(cpu_set_t), &cpuset);
            if (rc != 0)
                std::cerr << "Error pinning thread " << i
                          << " to CPU " << coreID << " rc=" << rc << "\n";
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

        thread_task<MQ_IO>(mrf, threads, sensitivity, &stats[0], pq, std::ref(doneInserting), splashH, mrf->getNodes());
        for (std::thread *&worker : workers)
        {
            worker->join();
            delete worker;
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto runtime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        // runtime in ms
        //  std::cout << ms.count() << std::endl;

        // pq.stat(); //print total pushes and total pops

        uint64_t total_updates = 0;
        for (uint64_t i = 0; i < threads; i++)
        {
            total_updates += stats[i].updates;
        }

        mrf->getNodeProbabilities(answer);
        // Update metrics
        metrics.runtime_ms = runtime_ms.count();
        metrics.num_updates = total_updates;

        delete locks;
    }

    void solve_smq(MRF *mrf, double sensitivity,
                   std::vector<std::array<double, 2>> *answer,
                   uint64_t threads,
                   int splashH, perf_metrics &metrics)
    {
        std::cout << "Running SmartSplash Relaxed Residual Belief "
                  << "Propagation with " << threads << " Thread(s) and Stealing Multiqueue " << std::endl;

        relaxed_smart_splash::messages = &mrf->getMessages();
        relaxed_smart_splash::mrf = mrf;
        relaxed_smart_splash::baseMessage = messages->data();
        relaxed_smart_splash::locks = new std::vector<std::mutex>(mrf->getNodes());
        relaxed_smart_splash::priorities = new std::atomic<double>[mrf->getNodes()](); // priority per-node for RSS instead of per-msg from RRBP

        // SMQ
        int smq_size = messages->size();
        using SMQ_IO = StealingMultiQueue;

        SMQ_IO pq = SMQ_IO(smq_size, threads);

        std::vector<std::thread *> workers;
        stat stats[threads];

        std::barrier<> doneInserting(threads);

        auto startTime = std::chrono::high_resolution_clock::now();
#if ENABLE_THREAD_PINNING
        auto allowed_cpus = get_allowed_cpus();
        cpu_set_t cpuset;
#endif
        for (uint64_t i = 1; i < threads; i++)
        {
            std::thread *newThread = new std::thread(
                    thread_task<SMQ_IO>, mrf, threads,
                    std::ref(sensitivity), &stats[i], std::ref(pq),
                    std::ref(doneInserting), splashH, mrf->getNodes());
#if ENABLE_THREAD_PINNING
            CPU_ZERO(&cpuset);
            int coreID = allowed_cpus[i % allowed_cpus.size()];
            CPU_SET(coreID, &cpuset);
            int rc = pthread_setaffinity_np(newThread->native_handle(),
                                            sizeof(cpu_set_t), &cpuset);
            if (rc != 0)
                std::cerr << "Error pinning thread " << i
                          << " to CPU " << coreID << " rc=" << rc << "\n";
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
        thread_task<SMQ_IO>(mrf, threads, sensitivity, &stats[0], pq,
                            std::ref(doneInserting), splashH, mrf->getNodes());

        for (std::thread *&worker : workers)
        {
            worker->join();
            delete worker;
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto runtime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        uint64_t total_updates = 0;
        for (uint64_t i = 0; i < threads; i++)
        {
            total_updates += stats[i].updates;
        }
        mrf->getNodeProbabilities(answer);
        // Update metrics
        metrics.runtime_ms = runtime_ms.count();
        metrics.num_updates = total_updates;

        delete locks;
    }

    template <typename PQ_TYPE>
    static void thread_task(MRF *mrf, uint64_t threads, double sensitivity, stat *stats, PQ_TYPE &pq, std::barrier<> &sharedBarrier, int splashH, int num_nodes)
    {
        uint64_t updates = 0;
        uint64_t pushes = 0;
        uint64_t pops = 0;
        uint64_t skips = 0;
        uint64_t it = 0;
        pq.initTID();

        // split vertices and insert into pq
        int split_vertices = std::ceil(double(num_nodes) / threads);
        int base_range = pq.tID * split_vertices; // partitioning the vertices
        // std::cout << "thread id is " << pq.tID << std::endl;
        for (uint64_t vid = base_range; vid < std::min(base_range + split_vertices, num_nodes); vid++)
        { // changed to handle remainder
            double prio = priority(vid);
            if (prio > sensitivity)
            { // pre-push filter before beginning algorithm
                priorities[vid] = prio;
                pq.push(prio, vid);
            }
        }
        sharedBarrier.arrive_and_wait();

        // Mark the node explored in BFS by the iteration count
        std::vector<int> visited(num_nodes);
        // Track the depth of the current node
        std::vector<int> distance(num_nodes);

        // BFS splash tree [fifo]
        std::vector<Message::Message *> msg_BFSTree(FIFO_SIZE);
        // BFS splash tree nodes (to be updated first) [fifo]
        std::vector<int> node_affected(FIFO_SIZE);
        // FIFO read/write pointers
        int head_node;
        int capacity_node = FIFO_SIZE;

        while (true)
        {
            double pushedPrio;
            uint64_t root;
            auto item = pq.pop();
            if (item)
                std::tie(pushedPrio, root) = item.get();
            else
                break;
            pops++;

            double curRootPrio = priorities[root].load(std::memory_order_relaxed);
            if (curRootPrio < pushedPrio)
            {
                // if (curRootPrio > sensitivity)
                // {
                //     pq.push(curRootPrio, root);
                // }
                skips++;
            }
            else
            {
                head_node = 0;
                msg_BFSTree.clear();
                node_affected.clear();

                // Book-keeping
                visited[root] = it;
                distance[root] = 0;
                // Now push node/msg into the queue
                node_affected.push_back(root); // node_affected[tail_node] = root; // replacing nodes_dq.push_back(root);

                while (head_node < node_affected.size())
                {
                    int u = node_affected[head_node]; // replacing nodes_dq.front(); nodes_dq.pop_front(); // no return value
                    head_node = (head_node + 1) % capacity_node;

                    if ((visited[u] == it) && (distance[u] >= splashH + 1))
                    {
                        break;
                    }

                    for (Message::Message *m : mrf->getMessagesFrom(u))
                    {
                        if (visited[m->j] == it)
                        {
                            continue;
                        }
                        visited[m->j] = it;
                        distance[m->j] = distance[u] + 1;

                        node_affected.push_back(m->j); // node_affected[tail_node] = m->j; // nodes_dq.push_back(m->j);

                        if (distance[m->j] <= splashH)
                        {
                            msg_BFSTree.push_back(m); // replacing order.push_back(m);
                        }
                    }
                }

                for (int j = 0; j < msg_BFSTree.size(); j++)
                {
                    Message::Message *m = msg_BFSTree[msg_BFSTree.size() - j - 1]->reverse;
                    updateMessage(mrf, m);
                    updates++;
                }

                for (int i = 0; i < msg_BFSTree.size(); i++)
                {
                    Message::Message *m = msg_BFSTree[i];
                    updateMessage(mrf, m);
                    updates++;
                }

                for (int k = 0; k < node_affected.size(); k++)
                {
                    int u = node_affected[k];
                    if (u == root)
                    {
                        continue;
                    }

                    perf_lock(locks->at(u)); //.lock();

                    double affNewPrio = priority(u);
                    double affCurPrio = priorities[u].load(std::memory_order_relaxed);

                    int loop_count = 0;
                    while (affCurPrio != affNewPrio)
                    {
                        if (affCurPrio < affNewPrio)
                        {
                            if (affNewPrio > sensitivity)
                            {
                                bool swapped = priorities[u].compare_exchange_weak(
                                    affCurPrio, affNewPrio);

                                if (swapped)
                                {
                                    pq.push(affNewPrio, u);
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                        else if (affCurPrio > affNewPrio)
                        {
                            if (affCurPrio > sensitivity)
                            {
                                bool swapped = priorities[u].compare_exchange_weak(
                                    affCurPrio, affNewPrio);

                                if (swapped)
                                {
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }

                    perf_unlock(locks->at(u)); //.unlock();
                }

                perf_lock(locks->at(root)); //.lock();
                double newRootPrio = priority(root);
                curRootPrio = priorities[root].load(std::memory_order_relaxed);
                while (curRootPrio != newRootPrio)
                {
                    if (curRootPrio < newRootPrio)
                    {
                        if (newRootPrio > sensitivity)
                        {
                            bool swapped = priorities[root].compare_exchange_weak(
                                curRootPrio, newRootPrio);

                            if (swapped)
                            {
                                pq.push(newRootPrio, root);
                                break;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                    else if (curRootPrio > newRootPrio)
                    {
                        if (curRootPrio > sensitivity)
                        {
                            bool swapped = priorities[root].compare_exchange_weak(
                                curRootPrio, newRootPrio);

                            if (swapped)
                            {
                                break;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                
                perf_unlock(locks->at(root)); //.unlock();
                
            }
            it++;
        }

        // update thread's stats for main thread to combine
        stats->iters = it;
        stats->updates = updates;
        stats->pops = pops;
        stats->pushes = pushes;
        stats->skips = skips;
    }
}

/////////////////////////// LDPC ////////////////////////////////////
namespace relaxed_smart_splash_CSR
{

    static std::vector<Message_CSR::Message> *messages;

    struct stat
    {
        uint64_t iters = 0;
        uint64_t updates = 0;
        uint64_t pushes = 0;
        uint64_t pops = 0;
        uint64_t skips = 0;
    };

    static MRF_CSR *mrf;
    static const Message_CSR::Message *baseMessage;
    static std::vector<std::mutex> *locks;
    static std::atomic<double> *priorities;
    static uint64_t NUM_VAR_NODES;
    static uint64_t NUM_CHK_NODES;
    static uint64_t NUM_NODES;

    static inline uint64_t id(const Message_CSR::Message *m)
    {
        return std::distance(baseMessage, m);
    }

    template <class T>
    static inline double priority(const Message_CSR::Message *m, T futureMessage)
    {
        return utils_CSR::logDifference(mrf->getLogMu(*m), mrf->getFutureMessage(*m));
    }

    static inline double priority(uint64_t vid)
    {
        double prio = 0;
        std::array<Message_CSR::Message *, VAR_DEG> *msgToVar = nullptr;
        std::array<Message_CSR::Message *, CHK_DEG> *msgToChk = nullptr;
        if (vid < NUM_VAR_NODES)
        { // MessagesTo VarNodes
            msgToVar = mrf->getMessageToVar(vid);
            for (Message_CSR::Message *m : *msgToVar)
            {
                prio = std::max(prio, priority(m, mrf->getFutureMessage(*m)));
            }
        }
        else
        { // MessagesTo ChkNodes
            msgToChk = mrf->getMessageToChk(vid);
            for (Message_CSR::Message *m : *msgToChk)
            {
                prio = std::max(prio, priority(m, mrf->getFutureMessage(*m)));
            }
        }

        return prio;
    }

    void updateMessage(MRF_CSR *mrf, Message_CSR::Message *m)
    {

        // obtain locks
        uint64_t mi = std::min(m->i, m->j);
        uint64_t mj = std::max(m->i, m->j);
        perf_lock(relaxed_smart_splash_CSR::locks->at(mi)); //.lock();
        perf_lock(relaxed_smart_splash_CSR::locks->at(mj)); //.lock();

        // update message
        mrf->getFutureMessageAndUpdate(*m);

        // release locks
        perf_unlock(relaxed_smart_splash_CSR::locks->at(mi)); //.unlock();
        perf_unlock(relaxed_smart_splash_CSR::locks->at(mj)); //.unlock();
    }

    template <typename MQ_IO>
    static void thread_task(MRF_CSR *mrf, uint64_t threads, double sensitivity,
                            stat *stats, MQ_IO &pq, std::barrier<> &sharedBarrier, int splashH);

    // multithreaded smart splash
    void solve_mq(MRF_CSR *mrf, double sensitivity,
              std::vector<std::array<double, 2>> *answer,
              uint64_t threads, int queueNum,
              int batchSizePop, int batchSizePush,
              int splashH,
              perf_metrics &metrics)
    {
        std::cout << "Running Relaxed Smart Splash (CSR) with "
                  << threads << " Thread(s) – Multi-Queue version\n";
    
        relaxed_smart_splash_CSR::messages = &mrf->getMessages();
        relaxed_smart_splash_CSR::mrf = mrf;
        relaxed_smart_splash_CSR::baseMessage = messages->data();
        relaxed_smart_splash_CSR::NUM_VAR_NODES = mrf->getNumVarNodes();
        relaxed_smart_splash_CSR::NUM_CHK_NODES = mrf->getNumChkNodes();
        relaxed_smart_splash_CSR::NUM_NODES = mrf->getTotalNumNodes();
        relaxed_smart_splash_CSR::locks = new std::vector<std::mutex>(mrf->getTotalNumNodes());
        relaxed_smart_splash_CSR::priorities = new std::atomic<double>[messages->size()]();
    
        using PQElement = std::tuple<double,uint64_t>;
        auto prefetcher = [](uint64_t) -> void {}; // returns void, empty body, nothing captured
        using MQ_IO = MultiQueue<decltype(prefetcher), std::less<PQElement>, double, uint64_t>;
        MQ_IO pq(prefetcher, queueNum, threads, batchSizePop, batchSizePush);
    
        std::vector<std::thread*> workers;
        stat stats[threads];
        std::barrier<> doneInserting(threads);
    
        auto startTime = std::chrono::high_resolution_clock::now();
    
    #if ENABLE_THREAD_PINNING
        auto allowed_cpus = get_allowed_cpus();
        cpu_set_t cpuset;
    #endif
    
        for (uint64_t i = 1; i < threads; ++i)
        {
            std::thread *newThread = new std::thread(
                thread_task<MQ_IO>, mrf, threads,
                std::ref(sensitivity), &stats[i], std::ref(pq),
                std::ref(doneInserting), splashH);
    
    #if ENABLE_THREAD_PINNING
            CPU_ZERO(&cpuset);
            int coreID = allowed_cpus[i % allowed_cpus.size()];
            CPU_SET(coreID, &cpuset);
            int rc = pthread_setaffinity_np(newThread->native_handle(),
                                            sizeof(cpu_set_t), &cpuset);
            if (rc != 0)
                std::cerr << "Error pinning thread " << i
                          << " to CPU " << coreID << " rc=" << rc << "\n";
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
    
        // main thread
        thread_task<MQ_IO>(mrf, threads, sensitivity,
                           &stats[0], std::ref(pq), 
                           std::ref(doneInserting), splashH);
    
        for (std::thread *&t : workers) { 
            t->join(); 
            delete t; 
            t = nullptr;
        }
    
        auto endTime = std::chrono::high_resolution_clock::now();
        auto runtime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        uint64_t total_updates = 0;
        for (uint64_t i = 0; i < threads; i++)
        {
            total_updates += stats[i].updates;
        }

        metrics.runtime_ms = runtime_ms.count();
        metrics.num_updates = total_updates;
    
        mrf->getVarNodeProbabilities(answer);
        delete locks;
    }
    
    void solve_smq(MRF_CSR *mrf, double sensitivity,
                   std::vector<std::array<double, 2>> *answer,
                   uint64_t threads,
                   int splashH,
                   perf_metrics &metrics)
    {
        std::cout << "Running Relaxed Smart Splash (CSR) with "
                  << threads << " Thread(s) – Stealing Multi-Queue version\n";
    
        relaxed_smart_splash_CSR::messages = &mrf->getMessages();
        relaxed_smart_splash_CSR::mrf = mrf;
        relaxed_smart_splash_CSR::baseMessage = messages->data();
        relaxed_smart_splash_CSR::NUM_VAR_NODES = mrf->getNumVarNodes();
        relaxed_smart_splash_CSR::NUM_CHK_NODES = mrf->getNumChkNodes();
        relaxed_smart_splash_CSR::NUM_NODES = mrf->getTotalNumNodes();
        relaxed_smart_splash_CSR::locks = new std::vector<std::mutex>(mrf->getTotalNumNodes());
        relaxed_smart_splash_CSR::priorities = new std::atomic<double>[messages->size()]();
    
        int smq_size = messages->size();
        using SMQ_IO = StealingMultiQueue;
        SMQ_IO pq(smq_size, threads);
        
        stat stats[threads];

        std::vector<std::thread*> workers;
        std::barrier<> doneInserting(threads);
    
        auto startTime = std::chrono::high_resolution_clock::now();
    
    #if ENABLE_THREAD_PINNING
        auto allowed_cpus = get_allowed_cpus();
        cpu_set_t cpuset;
    #endif
    
        for (uint64_t i = 1; i < threads; ++i)
        {
            std::thread *newThread = new std::thread(
                thread_task<SMQ_IO>, mrf, threads,
                std::ref(sensitivity), &stats[i],
                std::ref(pq), std::ref(doneInserting), splashH);
    
    #if ENABLE_THREAD_PINNING
            CPU_ZERO(&cpuset);
            int coreID = allowed_cpus[i % allowed_cpus.size()];
            CPU_SET(coreID, &cpuset);
            int rc = pthread_setaffinity_np(newThread->native_handle(),
                                            sizeof(cpu_set_t), &cpuset);
            if (rc != 0)
                std::cerr << "Error pinning thread " << i
                          << " to CPU " << coreID << " rc=" << rc << "\n";
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
    
        // main thread
        thread_task<SMQ_IO>(mrf, threads, sensitivity,
                            &stats[0], std::ref(pq),
                            std::ref(doneInserting), splashH);
    
        for (std::thread *&t : workers) { 
            t->join(); 
            delete t; 
            t = nullptr;
        }
    
        auto endTime = std::chrono::high_resolution_clock::now();
        auto runtime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        uint64_t total_updates = 0;
        for (uint64_t i = 0; i < threads; i++)
        {
            total_updates += stats[i].updates;
        }

        metrics.runtime_ms = runtime_ms.count();
        metrics.num_updates = total_updates;
    
        mrf->getVarNodeProbabilities(answer);
        delete locks;
    }

    template <typename PQ_TYPE>
    static void thread_task(MRF_CSR *mrf, uint64_t threads, double sensitivity,
                            stat *stats, PQ_TYPE &pq, std::barrier<> &sharedBarrier, int splashH)
    {
        uint64_t updates = 0;
        uint64_t pushes = 0;
        uint64_t pops = 0;
        uint64_t skips = 0;
        uint64_t it = 0;
        pq.initTID();

        // Assertion to ensure constraint
        assert(NUM_CHK_NODES < NUM_VAR_NODES && "Number of check nodes must be less than number of variable nodes");
        // Split vertices and insert into pq
        uint64_t split_var_vertices = std::ceil(double(NUM_VAR_NODES) / threads);
        uint64_t split_chk_vertices = std::ceil(double(NUM_CHK_NODES) / threads);
        uint64_t split_vertices = std::ceil(double(NUM_NODES) / threads);

        int base_var_range = pq.tID * split_var_vertices;
        int base_chk_range = pq.tID * split_chk_vertices + NUM_VAR_NODES; // chk nodes idx offsetted by num_var_nodes
        for (uint64_t vid = base_var_range; vid < std::min(base_var_range + split_var_vertices, NUM_VAR_NODES); vid++)
        { // changed to handle remainder
            double prio = priority(vid);
            if (prio > sensitivity)
            { // pre-push filter before beginning algorithm
                priorities[vid] = prio;
                pq.push(prio, vid);
            }
        }
        for (uint64_t vid = base_chk_range; vid < std::min(base_chk_range + split_chk_vertices, NUM_NODES); vid++)
        { // changed to handle remainder
            double prio = priority(vid);
            if (prio > sensitivity)
            { // pre-push filter before beginning algorithm
                priorities[vid] = prio;
                pq.push(prio, vid);
            }
        }
        std::array<Message_CSR::Message *, VAR_DEG> *msgFromVar = nullptr;
        std::array<Message_CSR::Message *, CHK_DEG> *msgFromChk = nullptr;

        sharedBarrier.arrive_and_wait();

        // Mark the node explored in BFS by the iteration count
        std::vector<int> visited(NUM_NODES);
        // Track the depth of the current node
        std::vector<int> distance(NUM_NODES);

        // BFS splash tree [fifo]
        std::vector<Message_CSR::Message *> msg_BFSTree(FIFO_SIZE);
        // BFS splash tree nodes (to be updated first) [fifo]
        std::vector<int> node_affected(FIFO_SIZE);
        // FIFO read/write pointers
        int head_node;
        int capacity_node = FIFO_SIZE;

        while (true)
        {
            double pushedPrio;
            uint64_t root;
            auto item = pq.pop();
            if (item)
                std::tie(pushedPrio, root) = item.get();
            else
                break;
            pops++;

            double curRootPrio = priorities[root].load(std::memory_order_relaxed);
            if (curRootPrio < pushedPrio)
            {
                skips++;
            }
            else
            {
                head_node = 0;
                msg_BFSTree.clear();
                node_affected.clear();
                msgFromVar = nullptr;
                msgFromChk = nullptr;

                // Book-keeping
                visited[root] = it;
                distance[root] = 0;

                node_affected.push_back(root); // replacing nodes_dq.push_back(root);

                while (head_node < node_affected.size())
                {
                    int u = node_affected[head_node]; // replacing nodes_dq.front(); nodes_dq.pop_front(); // no return value
                    head_node = (head_node + 1) % capacity_node;

                    if ((visited[u] == it) && (distance[u] >= splashH + 1))
                    {
                        break;
                    }
                    if (u < NUM_VAR_NODES)
                    {
                        msgFromVar = mrf->getMessageFromVar(u);
                        for (Message_CSR::Message *m : *msgFromVar)
                        {
                            if (visited[m->j] == it)
                            {
                                continue;
                            }
                            visited[m->j] = it;
                            distance[m->j] = distance[u] + 1;

                            node_affected.push_back(m->j); // nodes_dq.push_back(m->j);

                            if (distance[m->j] <= splashH)
                            {
                                msg_BFSTree.push_back(m); // replacing order.push_back(m);
                            }
                        }
                    }
                    else
                    {
                        msgFromChk = mrf->getMessageFromChk(u);
                        for (Message_CSR::Message *m : *msgFromChk)
                        {
                            if (visited[m->j] == it)
                            {
                                continue;
                            }
                            visited[m->j] = it;
                            distance[m->j] = distance[u] + 1;

                            node_affected.push_back(m->j); // nodes_dq.push_back(m->j);

                            if (distance[m->j] <= splashH)
                            {
                                msg_BFSTree.push_back(m); // replacing order.push_back(m);
                            }
                        }
                    }
                }

                for (int j = 0; j < msg_BFSTree.size(); j++)
                {
                    Message_CSR::Message *m = mrf->getReverseMessage(*msg_BFSTree[msg_BFSTree.size() - j - 1]);
                    mrf->getFutureMessageAndUpdate(*m);
                    updates++;
                }

                for (int i = 0; i < msg_BFSTree.size(); i++)
                {
                    Message_CSR::Message *m = msg_BFSTree[i];
                    mrf->getFutureMessageAndUpdate(*m);
                    updates++;
                }

                for (int k = 0; k < node_affected.size(); k++)
                {
                    int u = node_affected[k];
                    if (u == root)
                    {
                        continue;
                    }

                    perf_lock(locks->at(u)); //.lock();

                    double affNewPrio = priority(u);
                    double affCurPrio = priorities[u].load(std::memory_order_relaxed);

                    int loop_count = 0;
                    while (affCurPrio != affNewPrio)
                    {
                        if (affCurPrio < affNewPrio)
                        {
                            if (affNewPrio > sensitivity)
                            {
                                bool swapped = priorities[u].compare_exchange_weak(
                                    affCurPrio, affNewPrio);

                                if (swapped)
                                {
                                    pq.push(affNewPrio, u);
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                        else if (affCurPrio > affNewPrio)
                        {
                            if (affCurPrio > sensitivity)
                            {
                                bool swapped = priorities[u].compare_exchange_weak(
                                    affCurPrio, affNewPrio);

                                if (swapped)
                                {
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }

                    perf_unlock(locks->at(u)); //.unlock();
                }

                perf_lock(locks->at(root)); //.lock();
                double newRootPrio = priority(root);
                curRootPrio = priorities[root].load(std::memory_order_relaxed);
                while (curRootPrio != newRootPrio)
                {
                    if (curRootPrio < newRootPrio)
                    {
                        if (newRootPrio > sensitivity)
                        {
                            bool swapped = priorities[root].compare_exchange_weak(
                                curRootPrio, newRootPrio);

                            if (swapped)
                            {
                                pq.push(newRootPrio, root);
                                break;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                    else if (curRootPrio > newRootPrio)
                    {
                        if (curRootPrio > sensitivity)
                        {
                            bool swapped = priorities[root].compare_exchange_weak(
                                curRootPrio, newRootPrio);

                            if (swapped)
                            {
                                break;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                perf_unlock(locks->at(root)); //.unlock();
                
            }
            it++;
        }

        // update thread's stats for main thread to combine
        stats->iters = it;
        stats->updates = updates;
        stats->pops = pops;
        stats->pushes = pushes;
        stats->skips = skips;
    }
}
