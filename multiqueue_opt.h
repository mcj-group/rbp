#ifndef MQ_HEAP
#define MQ_HEAP

#include <cstdlib>
#include <iostream>
#include <tuple>
#include <vector>
#include <algorithm>
#include <mutex>
#include <queue>
#include <random>
#include <atomic>
#include <boost/optional.hpp>
#include <boost/align/aligned_allocator.hpp>

#define CACHELINE 64

#define FORWARD_PREFETCH_IDX 1

//#define PERF 1

// ------------------- MultiQueue Wrapper for std priority queues ------------------
// P:           type for prefetcher lambda
// Comparator:  the comparator for deciding min or max heap
// PrioType:    type of priority
// T:           type of the task
// usePrefetch: on-off switch for enabling prefetching during pop
template<
    class P,
    typename Comparator, 
    typename PrioType, 
    typename T,
    bool usePrefetch=false
>
class MultiQueue {
    using PQElement = std::tuple<PrioType, T>;
    using PQ = std::priority_queue<
        PQElement, 
        std::vector<PQElement>,
        Comparator
    >;
    Comparator compare;
    P prefetch;

    // Wrapper for individual queues
    struct PQContainer {
        uint64_t pushes = 0;
        uint64_t pops = 0;
        PQ pq;

        // Allows atomically peeking of the top element in this queue
        // to decide during pop which of the two queues have the 
        // more prioritized top task. 
        std::atomic<PrioType> top{0}; //want top to be 0 for max MQ impl

        std::atomic<bool> queueLock{false};
        inline void lock() { while (queueLock.load(std::memory_order_acquire)); }
        inline void unlock() { queueLock.store(false, std::memory_order_release); }
        // Returns true if lock was previously acquired, false otherwise
        // A return value of false means that the lock is successfully acquired
        inline bool tryLock() { 
            // Perform non-stubborn try-locking: does not wait on an acquired lock
            // Already locked, try another queue
            bool flag = queueLock.load(std::memory_order_acquire);
            if (flag) return flag;
            // Not locked, try setting flag
            return !queueLock.compare_exchange_weak(
                flag, true, std::memory_order_acq_rel, std::memory_order_acquire);
        }

    } __attribute__((aligned (CACHELINE)));

    // The list of queues
    std::vector<
        PQContainer,
        boost::alignment::aligned_allocator<PQContainer, CACHELINE>
    > queues;
    uint64_t numQueues;

    // Termination:
    //  - numIdle records the number of threads that believe
    //    there are no more work to do.
    //  - numEmpty records number of queues that are empty
    uint64_t numThreads;
    std::atomic<uint64_t> numIdle{0};
    std::atomic<uint64_t> numEmpty;

    // Manages the push&pop buffers for each thread.
    // Each thread indexes their container with the 
    // thread_local tID.
    struct BufContainer {
        std::vector<PQElement> pushBuffer;
        std::vector<PQElement> popBuffer;
        uint32_t popBufIdx;

        BufContainer(uint32_t pushBufSize, uint32_t popBufSize) {
            pushBuffer.reserve(pushBufSize);
            popBuffer.reserve(popBufSize);
            popBufIdx = 0;
        }
        
    } __attribute__((aligned (CACHELINE)));
    std::vector<
        BufContainer,
        boost::alignment::aligned_allocator<BufContainer, CACHELINE>
    > localBufs;

    uint64_t batchPopNum;
    uint64_t batchPushNum;
    
  public:

    MultiQueue(P p, uint64_t nQ, uint64_t nT,
               uint64_t batchSizePop, uint64_t batchSizePush)
        : prefetch(p)
        , queues(nQ)
        , numQueues(nQ)
        , numThreads(nT)
        , numEmpty(nQ)
        , batchPopNum(batchSizePop)
        , batchPushNum(batchSizePush)
    {
        assert(batchSizePop >= 1);
        assert(batchSizePush >= 1);
        assert(nT >= 1);
        assert(nQ > 1);
        for (uint64_t i = 0 ; i < numThreads; i++) {
            localBufs.push_back(BufContainer(batchSizePush, batchSizePop));
        }
    };

    // Each thread initializes its own tID by calling initTID()
    // before starting the actual processing.
    // this is a simple hack to get 0 to (threadNum-1)
    // tIDs as c++ pthreads allocate arbitrary thread IDs
    inline static thread_local uint32_t tID;
    std::atomic<uint32_t> id = 0;
    void initTID() { tID = id.fetch_add(1); }

    // Selects a random queue amongst the list of queues
#ifdef PERF
    uint64_t __attribute__ ((noinline)) threadLocalRandom() {
#else
    inline uint64_t threadLocalRandom() {
#endif
        static thread_local uint64_t x = pthread_self();
        uint64_t z = (x += UINT64_C(0x9E3779B97F4A7C15));
        z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
        z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
        return (z ^ (z >> 31)) % numQueues;
    }

    // Performs actual insertion of a task into the heap
    // Mainly for purpose of runtime breakdown
#ifdef PERF
    void __attribute__ ((noinline)) pushHeap(uint64_t queue, PQElement item) {
        queues[queue].pq.push(item);
    }
#endif

    // Performs actual insertion of tasks in the push buffer
#ifdef PERF
    void __attribute__ ((noinline)) pushInternal() {
#else
    inline void pushInternal() {
#endif
        auto& pushBuffer = localBufs[tID].pushBuffer;
        uint64_t pushSize = pushBuffer.size();
        if (pushSize == 0) return;

        // Select and lock a random queue
        uint64_t queue;
        while (true) {
            queue = threadLocalRandom();
            if (!queues[queue].tryLock()) break;
        }
        auto& q = queues[queue];

        // Update numEmpty status
        if (q.pq.empty())
            numEmpty.fetch_sub(1, std::memory_order_acq_rel);

        for (uint32_t i = 0; i < pushSize; i++) {
#ifdef PERF
            pushHeap(queue, pushBuffer[i]);
#else
            q.pq.push(pushBuffer[i]);
#endif
        }

        // Update the atomic top element if needed
        q.pushes += pushSize;
        q.top.store(
            q.pq.size() > 0 
                ? std::get<0>(q.pq.top()) 
                : 0, //want to get 0 as top element bc using max-priority for relaxed-rbp
            std::memory_order_release);
        
        q.unlock();
        pushBuffer.clear();
    }

    // Push the task into local pushBuffer until it fills up
#ifdef PERF
    void __attribute__ ((noinline)) push(PrioType prio, T key) {
#else
    inline void push(PrioType prio, T key) {
#endif
        // Each thread has a local buffer to push to.
        // pushBufSize can be dynamically adjusted for smaller push batch sizes,
        // but the max size of the buffer is always batchPushNum.
        auto& pushBuffer = localBufs[tID].pushBuffer;
        pushBuffer.push_back({prio, key});
        if (pushBuffer.size() < batchPushNum - 1) return;

        pushInternal();
    }

    // Repeatedly try popping and stop when numIdle >= threadNum,
    // That is, stop when all threads agree that there are no more work
#ifdef PERF
    boost::optional<PQElement> __attribute__ ((noinline)) pop() {
#else
    inline boost::optional<PQElement> pop() {
#endif
        auto item = popInternal();
        if (item) return item;

        // increment count and keep on trying to pop
        uint64_t num = numIdle.fetch_add(1, std::memory_order_acq_rel) + 1;
        do {
            item = popInternal();
            if (item) break;
            if (num >= numThreads) return boost::none;
            
            num = numIdle.load(std::memory_order_relaxed);

        } while (true);

        numIdle.fetch_sub(1, std::memory_order_acq_rel);
        return item;
    }

    // Performs actual dequeue of a task from the heap
    // Mainly for purpose of runtime breakdown
#ifdef PERF
    PQElement __attribute__ ((noinline)) popHeap(uint64_t queue) {
        auto& q = queues[queue];
        PQElement ret = q.pq.top();
        q.pq.pop();
        return ret;
    }
#endif

    // Performs actual popping of tasks
#ifdef PERF
    boost::optional<PQElement> __attribute__ ((noinline)) popInternal() {
#else
    inline boost::optional<PQElement> popInternal() {
#endif
        // Once all the tasks in the pop buffer is consumed,
        // perform another batch pop from the bucket queue
        auto& buf = localBufs[tID];
        auto& popBuffer = buf.popBuffer;
        auto& popBufIdx = buf.popBufIdx;
        if (popBufIdx < popBuffer.size()) {
            if (usePrefetch) {
                // Prefetch the task at FORWARD_PREFETCH_IDX ahead of 
                // the task being popped, such that its priority
                // becomes available in cache when popped.
                uint32_t prefetchIdx = popBufIdx + FORWARD_PREFETCH_IDX;
                if (prefetchIdx < popBuffer.size())
                    prefetch(std::get<1>(popBuffer[prefetchIdx]));
            }
            auto& task = popBuffer[popBufIdx++];
            return task;
        }

        // Empty the push buffer before trying to pop.
        // Note: instead of directly popping tasks from the 
        // push buffer as in previous work, we follow
        // the global priority ordering more closely by 
        // updating the MultiQueue more frequently.
        // This way, the tasks in a thread's local push buffer
        // becomes visible to other threads earlier.
        // This reduces potential priority drift amonst threads
        // at the cost of higher communication costs.
        pushInternal();

        popBufIdx = 0;
        popBuffer.clear();
        uint64_t poppingQueue = numQueues;
        PrioType EMPTY_PRIO = std::numeric_limits<PrioType>::max();
        while (true) {
            
            // Pick the higher priority max of queue i and j

            uint64_t i = threadLocalRandom();
            uint64_t j = threadLocalRandom();
            while (j == i) {
                j = threadLocalRandom();
            }

            PrioType topI = queues[i].top.load(std::memory_order_acquire);
            PrioType topJ = queues[j].top.load(std::memory_order_acquire);

            // check if there are no work tasks available
            if (topI == EMPTY_PRIO && topJ == EMPTY_PRIO) {
                uint64_t emptyQueues = numEmpty.load(std::memory_order_acquire);
                if (emptyQueues >= queues.size()) break;
                else continue;
            }

            // Pick the queue with the more prioritized top task
            // and try to lock it
            if (topI != EMPTY_PRIO && topJ != EMPTY_PRIO) {
                poppingQueue = compare({topJ, 0}, {topI, 0}) ? i : j;
            } else if (topJ == EMPTY_PRIO) {
                poppingQueue = i;
            } else {
                poppingQueue = j;
            }
            if (queues[poppingQueue].tryLock()) continue;
            
            auto& q = queues[poppingQueue];
            if (q.pq.empty()) {
                q.unlock();
                continue;
            }

            // lock acquired, perform popping

            uint64_t num;
            for (num = 0; num < batchPopNum; num++) {
                if (q.pq.empty()) break;
#ifdef PERF
                popBuffer.push_back(popHeap(poppingQueue));
#else
                popBuffer.push_back(q.pq.top());
                q.pq.pop();
#endif

            }
            q.pops += num;
            if (q.pq.empty())
                numEmpty.fetch_add(1, std::memory_order_acq_rel);

            // Update this queue's top element
            q.top.store(
                q.pq.size() > 0 
                    ? std::get<0>(q.pq.top()) 
                    : EMPTY_PRIO,
                std::memory_order_release
            );
            q.unlock();

            if (num == 0) continue;
            
            // return the first task in the pop buffer
            auto& task = popBuffer[popBufIdx++];
            return task;
        }

        return boost::none;
    }

    // Note: this is only called at the end of algorithm,
    // so it is not lock protected
    inline void stat() const {
        int totalPushes = 0, totalPops = 0;
        for (uint64_t i = 0; i < numQueues; i++) {
            totalPushes += queues[i].pushes;
            totalPops += queues[i].pops;
        }
        std::cout << "total pushes "<< totalPushes << "\n";
        std::cout << "total pops "<< totalPops << "\n";
    }

    // Note: this is only called at the end of algorithm as a
    // sanity check, therefore it is not lock protected.
    inline bool empty() const {
        for (uint i = 0; i < numQueues;i++) {
            if (!queues[i].pq.empty()) {
                return false;
            }
        }
        return true;
    }

    inline PrioType peekPriority() {
        PrioType peek_prio = queues[0].top.load(std::memory_order_acquire);
        for (uint64_t i = 1; i < queues.size(); i++) {
            PrioType curr_prio = queues[i].top.load(std::memory_order_acquire);
            if (curr_prio > peek_prio) {
                peek_prio = curr_prio;
            }
        }
        return peek_prio;
    }

};

#endif //MQ_HEAP
