#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <tuple>
#include <vector>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <barrier>
#include <chrono>

#include "mrf.h"
#include "synch_bp.h"

static std::vector<int> get_allowed_cpus() {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    if (sched_getaffinity(0, sizeof(mask), &mask) != 0) {
        perror("sched_getaffinity");
    }
    std::vector<int> cpus;
    for (int c = 0; c < CPU_SETSIZE; ++c)
        if (CPU_ISSET(c, &mask))
            cpus.push_back(c);
    return cpus;
}

namespace synch_bp
{
    static uint64_t num_threads = 0;
    static std::atomic<uint64_t> updates;
    std::atomic<bool> updated; // shared flag
    static std::atomic<bool> timeout_reached = false; // Timer flag
    static const Message::Message *baseMessage;

    static inline uint64_t id(const Message::Message *m)
    {
        return std::distance(baseMessage, m);
    }

    // Helper function to check if timeout has been reached
    static inline bool hasTimedOut(
        const std::chrono::high_resolution_clock::time_point &startTime,
        uint64_t timeout_seconds)
    {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            currentTime - startTime);
        return elapsed.count() >= static_cast<int64_t>(timeout_seconds);
    }

    //  Process Messages and Update Chk/Var Nodes Assigned to this Thread
    void processMsgAndUpdateNodes(int i, int msgChunkSize, MRF *mrf, std::vector<Message::Message> &Messages,
                                  std::vector<std::array<double, 2>> &new_mu, std::barrier<> &sharedBarrier, double sensitivity,
                                  uint64_t numNodes, uint64_t startNode, uint64_t endNode,
                                  const std::chrono::high_resolution_clock::time_point &startTime,
                                  uint64_t timeout_seconds)
    {
        bool updatedLocal;
        uint64_t local_updates = 0;
        do
        {
            sharedBarrier.arrive_and_wait();
            
            // Check for timeout
            if (hasTimedOut(startTime, timeout_seconds)) {
                timeout_reached.store(true);
                updated.store(false); // Signal exit
            }
            
            updated.store(false);
            sharedBarrier.arrive_and_wait();
            
            if (timeout_reached.load()) {
                break; // Exit the loop if timeout reached
            }
            
            updatedLocal = false;
            uint64_t start_idx = msgChunkSize * i;
            uint64_t end_idx = std::min(msgChunkSize * (i + 1), static_cast<int>(Messages.size()));

            for (uint64_t k = start_idx; k < end_idx; ++k)
            {
                Message::Message &msg = Messages[k];
                new_mu[id(&msg)] = mrf->getFutureMessage(msg);
            }

            sharedBarrier.arrive_and_wait();

            double dist;
            for (uint64_t k = start_idx; k < end_idx; ++k)
            {
                Message::Message &msg = Messages[k];
                std::array<double, 2> old_mu = msg.logMu;
                dist = utils::distance(old_mu, new_mu[id(&msg)]);
                if (dist > sensitivity)
                {
                    updatedLocal = true;
                    mrf->updateMessage(msg, new_mu[id(&msg)]);
                }
                local_updates++;
            }

            sharedBarrier.arrive_and_wait();

            // Update all nodes
            for (uint64_t nodeIdx = startNode; nodeIdx < endNode; ++nodeIdx)
            {
                mrf->updateNodeSum(nodeIdx);
            }

            if (updatedLocal)
            { // and not updated
                updated.store(true);
            }
            sharedBarrier.arrive_and_wait();
        } while (updated.load() && !timeout_reached.load());
        updates += local_updates; // update atomic counter shared between threads
    }

    void solve(MRF *mrf, double sensitivity,
               std::vector<std::array<double, 2>> *answer, uint64_t _threads,
               perf_metrics &metrics, uint64_t timeout_seconds)
    {

        synch_bp::baseMessage = mrf->getMessages().data(); // used for id function
        timeout_reached.store(false); // Reset timeout flag

        /* Messages */
        auto &Messages = mrf->getMessages();
        const uint64_t numNodes = mrf->getNodes();
        std::vector<std::array<double, 2>> new_mu(Messages.size());

        /* Threads */
        num_threads = _threads;
        std::barrier<> sharedBarrier(num_threads); // Barrier bar(num_threads);
        // Create vector of thread pointers using smart pointers
        std::vector<std::thread *> workers;

        /* Performance Tracker */
        auto startTime = std::chrono::high_resolution_clock::now();

        /* Create Threads */
        auto allowed_cpus = get_allowed_cpus();
        cpu_set_t cpuset;
        const uint64_t msgChunkSize = (Messages.size() + num_threads - 1) / num_threads;
        const uint64_t nodeChunkSize = (numNodes + num_threads - 1) / num_threads;
        uint64_t nodeStart = -1, nodeEnd = -1;
        for (uint64_t i = 1; i < num_threads; ++i) 
        {
            CPU_ZERO(&cpuset);
            int coreID = allowed_cpus[i % allowed_cpus.size()];
            CPU_SET(coreID, &cpuset);
            nodeStart = i * nodeChunkSize;
            nodeEnd = std::min(nodeStart + nodeChunkSize, numNodes);

            std::thread *curr_thread = new std::thread(
                    processMsgAndUpdateNodes, i, msgChunkSize, mrf,
                    std::ref(Messages), std::ref(new_mu),
                    std::ref(sharedBarrier), sensitivity,
                    numNodes, nodeStart, nodeEnd,
                    startTime, timeout_seconds);

            int ret = pthread_setaffinity_np(curr_thread->native_handle(),
                                                sizeof(cpu_set_t), &cpuset);
            if (ret != 0)
                std::cerr << "Error setting CPU affinity for thread "
                          << i << " rc=" << ret << std::endl;

            workers.push_back(curr_thread);
        }

        if (!allowed_cpus.empty()) {
            CPU_ZERO(&cpuset);
            CPU_SET(allowed_cpus[0], &cpuset);
            sched_setaffinity(0, sizeof(cpuset), &cpuset);
        }
        nodeStart = 0;
        nodeEnd = std::min(nodeStart + nodeChunkSize, numNodes);

        processMsgAndUpdateNodes(0, msgChunkSize, mrf,
                                std::ref(Messages), std::ref(new_mu),
                                std::ref(sharedBarrier), sensitivity,
                                numNodes, nodeStart, nodeEnd,
                                startTime, timeout_seconds);
                                
        for (std::thread *&worker : workers)
        {
            worker->join();
            delete worker;
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto runtime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        metrics.runtime_ms = runtime_ms.count();
        metrics.num_updates = updates;
        
        // Log if timeout was reached
        if (timeout_reached.load()) {
            std::cout << "Algorithm reached timeout of " << timeout_seconds << " seconds" << std::endl;
        }
        
        mrf->getNodeProbabilities(answer);
    }
} // namespace synch_bp

namespace synch_bp_CSR
{
    // Struct for Statistics
    struct Stat
    {
        uint64_t updates = 0;
    };
    static uint64_t num_threads = 0;
    static std::atomic<uint64_t> updates = 0;
    static std::atomic<bool> updated;
    static std::atomic<bool> timeout_reached = false; // Timer flag
    static const Message_CSR::Message *baseMessage;

    static inline uint64_t id(const Message_CSR::Message *m)
    {
        return std::distance(baseMessage, m);
    }

    // Helper function to check if timeout has been reached
    static inline bool hasTimedOut(
        const std::chrono::high_resolution_clock::time_point &startTime,
        uint64_t timeout_seconds)
    {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            currentTime - startTime);
        return elapsed.count() >= static_cast<int64_t>(timeout_seconds);
    }

    void thread_task(
        int thread_id,
        int msgChunkSize,
        int varNodeChunkSize,
        int chkNodeChunkSize,
        int numMsgs,
        int numVarNodes,
        int numChkNodes,
        MRF_CSR *mrf,
        std::vector<Message_CSR::Message> &messages,
        std::vector<std::vector<double>> &new_mu,
        std::barrier<> &sharedBarrier,
        double sensitivity,
        Stat *stats,
        const std::chrono::high_resolution_clock::time_point &startTime,
        uint64_t timeout_seconds)
    {
        uint64_t localUpdates = 0;
        bool localUpdated = false;
        double dist = 0;
        int iteration = 0; // Iteration counter for each thread

        uint64_t startMsgIdx = msgChunkSize * thread_id;
        uint64_t endMsgIdx = std::min(msgChunkSize * (thread_id + 1), static_cast<int>(numMsgs));
        uint64_t varStart = varNodeChunkSize * thread_id;
        uint64_t varEnd = std::min(varNodeChunkSize * (thread_id + 1), numVarNodes);
        uint64_t chkStart = chkNodeChunkSize * thread_id;
        uint64_t chkEnd = std::min(chkNodeChunkSize * (thread_id + 1), numChkNodes);

        do
        {
            sharedBarrier.arrive_and_wait();
            if (thread_id == 0)
            {
                updated.store(false);
                
                // Check for timeout
                if (hasTimedOut(startTime, timeout_seconds)) {
                    timeout_reached.store(true);
                }
            }

            sharedBarrier.arrive_and_wait();
            
            if (timeout_reached.load()) {
                break; // Exit the loop if timeout reached
            }
            iteration++;

            /** Read in new messages **/
            for (uint64_t i = startMsgIdx; i < endMsgIdx; ++i)
            {
                Message_CSR::Message &msg = messages[i];
                new_mu[id(&msg)] = mrf->getFutureMessage(msg);
            }

            sharedBarrier.arrive_and_wait();
            /** Pass on (read) the new messages **/
            for (uint64_t i = startMsgIdx; i < endMsgIdx; ++i)
            {
                Message_CSR::Message &msg = messages[i];
                std::vector<double> old_mu = mrf->getLogMu(msg);
                dist = utils_CSR::logDifference(old_mu, new_mu[msg.id]); // logDifference in ldpc utils.h
                if (i == startMsgIdx)
                {
                    localUpdated = false;
                }
                if (dist > sensitivity)
                {
                    localUpdated = true;
                    localUpdates++;
                }
                mrf->copyMessage(msg, new_mu[id(&msg)]);
            }
            sharedBarrier.arrive_and_wait();

            /** Write down the new messages **/
            // Update var and chk nodes
            for (uint64_t nodeIdx = varStart; nodeIdx < varEnd; ++nodeIdx)
            {
                mrf->updateVarNodeSum(nodeIdx);
            }
            sharedBarrier.arrive_and_wait();
            for (uint64_t nodeIdx = chkStart; nodeIdx < chkEnd; ++nodeIdx)
            {
                mrf->updateChkNodeSum(nodeIdx);
            }
            sharedBarrier.arrive_and_wait();

            if (localUpdated)
            { // and not updated
                updated.store(true);
            }
            sharedBarrier.arrive_and_wait();

            // Increment the iteration counter
            iteration++;
        } while (updated.load() && !timeout_reached.load());
        stats->updates = localUpdates;
    }

    void printAnswer(const std::vector<std::vector<double>> *answer)
    {
        if (!answer)
        {
            std::cerr << "Error: answer is a null pointer." << std::endl;
            return;
        }
        std::cout << "Contents of answer:" << std::endl;
        for (size_t i = 0; i < answer->size(); ++i)
        {
            const std::vector<double> &arr = (*answer)[i];
            std::cout << "Element " << i << ": (" << arr[0] << ", " << arr[1] << ")" << std::endl;
        }
    }

    void solve(MRF_CSR *mrf, double sensitivity, uint64_t _threads,
               std::vector<std::array<double, 2>> *answer, perf_metrics &metrics,
                uint64_t timeout_seconds)
    {

        baseMessage = mrf->getMessages().data();
        timeout_reached.store(false); // Reset timeout flag
        
        /* Messages */
        auto &messages = mrf->getMessages();
        const uint64_t numChkNodes = mrf->getNumChkNodes();
        const uint64_t numVarNodes = mrf->getNumVarNodes();
        const uint64_t numMsgs = messages.size();
        std::vector<std::vector<double>> new_mu(numMsgs);

        /* Multi-threading */
        num_threads = _threads;
        auto sharedBarrier = std::barrier<>(num_threads); // Synchronization Mechanism
        // Create vector of thread pointers using smart pointers
        std::vector<std::thread *> workers;
        Stat stats[num_threads];
        uint64_t msgChunkSize = (numMsgs + num_threads - 1) / num_threads;
        uint64_t varNodeChunkSize = (numVarNodes + num_threads - 1) / num_threads;
        uint64_t chkNodeChunkSize = (numChkNodes + num_threads - 1) / num_threads;
        uint64_t varStart = -1;
        uint64_t varEnd = -1;
        uint64_t chkStart = -1;
        uint64_t chkEnd = -1;

        /* Performance Tracker */
        auto startTime = std::chrono::high_resolution_clock::now();
        auto allowed_cpus = get_allowed_cpus();
        cpu_set_t cpuset;
        for (uint64_t i = 1; i < num_threads; ++i)
        {
            CPU_ZERO(&cpuset);
            int coreID = allowed_cpus[i % allowed_cpus.size()];
            CPU_SET(coreID, &cpuset);

            std::thread *curr_thread = new std::thread(
                thread_task, i,
                msgChunkSize, varNodeChunkSize, chkNodeChunkSize,
                numMsgs, numVarNodes, numChkNodes,
                mrf, std::ref(messages), std::ref(new_mu),
                std::ref(sharedBarrier), sensitivity, &stats[i],
                startTime, timeout_seconds);
            int ret = pthread_setaffinity_np(curr_thread->native_handle(),
                                             sizeof(cpu_set_t), &cpuset);
            if (ret != 0)
                std::cerr << "Error setting CPU affinity for thread "
                          << i << " rc=" << ret << std::endl;
            workers.push_back(curr_thread);
        }

        if (!allowed_cpus.empty()) {
            CPU_ZERO(&cpuset);
            CPU_SET(allowed_cpus[0], &cpuset);
            sched_setaffinity(0, sizeof(cpuset), &cpuset);  // pin main thread
        }

        thread_task(0,
                    msgChunkSize, varNodeChunkSize, chkNodeChunkSize,
                    numMsgs, numVarNodes, numChkNodes,
                    mrf, std::ref(messages), std::ref(new_mu),
                    std::ref(sharedBarrier), sensitivity, &stats[0],
                    startTime, timeout_seconds);

        for (std::thread *&worker : workers)
        {
            worker->join();
            delete worker;
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto runtime_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

        uint64_t totalUpdates = 0;
        for (uint64_t i = 0; i < num_threads; i++)
            totalUpdates += stats[i].updates;

        metrics.runtime_ms = runtime_ms.count();
        metrics.num_updates = totalUpdates;
        
        // Log if timeout was reached
        if (timeout_reached.load()) {
            std::cout << "Algorithm reached timeout of " << timeout_seconds << " seconds" << std::endl;
        }

        mrf->getVarNodeProbabilities(answer);
    }
} // namespace synch_bp_CSR
