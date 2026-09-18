#pragma once
#include "HeapWithStealingBufferQueue.h"
#include <deque>
#include <boost/optional.hpp>

#define PROB_STEAL 8

class StealingMultiQueue {
    private:
        bool debug = false;

    public:
        static constexpr uint64_t UNQUEUED = UINT64_MAX;

        std::vector<HeapWithStealingBufferQueue> queues;
        inline thread_local static std::vector<HeapWithStealingBufferQueue::PQElement> stolenTasks; //must be static to be thread_local
        inline thread_local static uint32_t tID; //assign this with a new argument for thread_task function in relaxed-rbp.cpp
        std::atomic<uint64_t> emptyCounter;
        inline thread_local static bool decrementedFlag;
        std::atomic<uint32_t> atomic_id = 0;
        int num_threads;

        //constructor
        StealingMultiQueue(uint64_t maxSize, int _num_threads) : queues(_num_threads) { 
            emptyCounter = _num_threads;
            num_threads = _num_threads;
            decrementedFlag = false;
        }
        //destructor
        ~StealingMultiQueue() { }


        //inserting aka actually pushing into queue
    #ifdef PERF
        void __attribute__ ((noinline)) push(double priority, uint64_t key) {
    #else
        inline void push(double priority, uint64_t key) {
    #endif
            queues[tID].addLocal(std::make_tuple(priority, key));
        }

        //helper function for thread local random 
    #ifdef PERF
        uint32_t __attribute__ ((noinline)) random( ) {
    #else
        inline uint32_t random( ) {
    #endif
            static thread_local uint32_t x = std::chrono::system_clock::now().time_since_epoch().count() % 16386 + 1;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            return x;
        }

        
    #ifdef PERF
        boost::optional<HeapWithStealingBufferQueue::PQElement> __attribute__ ((noinline)) deleteTask() {
    #else
        inline boost::optional<HeapWithStealingBufferQueue::PQElement> deleteTask() {
    #endif      
            boost::optional<HeapWithStealingBufferQueue::PQElement> task;

            /*  TRY TO SELECT AN TASK FROM OUR THREAD'S STOLENTASKS BUFFER */
            while (!stolenTasks.empty()) { //repeat so long as we have tasks available. must keep as while loop bc popping here
                //obtain first task from thread's stolen tasks
                task = stolenTasks.front();
                stolenTasks.erase(stolenTasks.begin()); //delete task from front of stolenTasks buffer
                
                //do not return task if is a filler task
                if (std::get<1>(task.get()) != HeapWithStealingBufferQueue::UNQUEUED) {
                    return task;
                }
                
            }


            /* POSSIBLY TRY TO STEAL */ 
            if (num_threads > 1 && (random() % PROB_STEAL) == 0) {
                //obtain a stolen task, trySteal returns UNQUEUED if invalid task
                task = trySteal();
                if (debug) std::cout << "thread " << tID << " called trySteal() in deleteTask()" << std::endl;

                if (task) {
                    return task;
                }

            }
            
            /* TRY TO ACCESS ITS OWN QUEUE */
            task = queues[tID].extractTopLocal(); //pop from queue
            
            if (task) { //if (key != UNQUEUED) {
                return task;
            }
            //else, try to steal from itself
            stolenTasks = queues[tID].steal();
            if (stolenTasks.size() > 0) { //}.front())
                HeapWithStealingBufferQueue::PQElement frontTask = stolenTasks.front();
                stolenTasks.erase(stolenTasks.begin()); //delete task from front of stolenTasks buffer
                if (std::get<1>(frontTask) != HeapWithStealingBufferQueue::UNQUEUED) {
                    return frontTask;
                }
            }
            

            /* ALL ELSE FAILED SO TRY TO STEAL AGAIN */
            if (num_threads > 1) {
              task = trySteal(); //returns UNQUEUED id if nothing valid
              if (debug) std::cout << "thread " << tID << " called trySteal() in deleteTask()" << std::endl;
            }
            else { //cannot steal because only one thread created, so return nothing and terminate
              task = boost::none;
            }
            return task;
            
        }

    #ifdef PERF
        boost::optional<HeapWithStealingBufferQueue::PQElement> __attribute__ ((noinline)) trySteal() {
    #else
        inline boost::optional<HeapWithStealingBufferQueue::PQElement> trySteal() {
    #endif
            while (num_threads > 0) { //can only steal from another thread if we other threads exist
                //randomly select queue to steal
                uint32_t qID = random() % queues.size();

                 //if no task at head of qID's stealingBuffer, return nothing
                if (qID == tID) {
                    continue; //try again
                }
                
                //if no task at head of qID's stealingBuffer, return nothing
                if (!queues[qID].top()) {
                    break; //return boost::none;
                }
                //if no task at top of tID's stealingBuffer, or is lower priority than priority of qID's top of stealingBuffer 
                //this compares top priorities of the two queues (random thread vs current thread)
                else if (!queues[tID].top() || (queues[qID].top() > queues[tID].top())) { //sign is flipped relative to paper bc we use larger value for higher priority
                    // if (debug)  std::cout << "thread " << tID << " is trying to steal from thread " << qID << std::endl;
                    
                    stolenTasks = queues[qID].steal();
                    

                    //else, return the first task we stole
                    // if (debug) std::cout << "thread " << tID << " trySteal got stolen.front: " << std::get<0>(stolen.front())<< std::endl;
                    if (stolenTasks.size() > 0) { //}.front())
                        HeapWithStealingBufferQueue::PQElement frontTask = stolenTasks.front();
                        stolenTasks.erase(stolenTasks.begin()); //delete task from front of stolenTasks buffer
                        if (std::get<1>(frontTask) != HeapWithStealingBufferQueue::UNQUEUED) {
                            return frontTask;
                        }
                    }
                    continue; //try again, there was a valid task in stealingBuffer when we entered this else-if, but the steal() failed bc of race condition
                }
                else {
                    break; //leave while loop
                }
            }

            //we did not meet conditions to try to steal so do not return anything
            return boost::none;
       }

    /*
    Set a unique thread ID between 0 and thread_num-1
    Use same tID approach as multiqueue_opt.h
    */
    #ifdef PERF
        void __attribute__ ((noinline)) initTID() {
    #else
        inline void initTID() {
    #endif
            tID = atomic_id++;
        }

    /*
    Use this function to extract a task within the algorithm
    */
    #ifdef PERF
        boost::optional<HeapWithStealingBufferQueue::PQElement> __attribute__ ((noinline)) pop() {
    #else
        inline boost::optional<HeapWithStealingBufferQueue::PQElement> pop() {
    #endif      
            while (true) {
                boost::optional<HeapWithStealingBufferQueue::PQElement> task = deleteTask();
                if (task) { //task found
                    if (decrementedFlag) { //this thread previously was unable to find a task
                        emptyCounter++; //this thread now succeeded in finding a task so we have one more non-empty SMQ for our counter
                        decrementedFlag = 0; //reset the flag
                    }
                    return task; //done, found task
                }
                else { //task not found
                    //also check that the thread's stealingBuffer is non-empty (first task's key should NOT be UNQUEUED, 
                    //or can check stolen flag is false? because that would mean fillBuffer() was called with non-empty task bc that is only place where we set false
                    //but another task could have stolen and then we still have valid tasks in the stealingBuffer? 
                    //No, that cannot occur because that stealing thread would have taken the entire task and still be alive?
                    //But what if another thread decremented emptyCounter earlier, then stole a task from this thread so this thread's stealingBuffer is empty?
                    //That's fine, because then that thread will have non empty pop and thus increment the emptyCounter.
                    //BUT this thread may have exited by then, which is early retirement of this thread.
                    //that should not affect correctness, just performance
                    //but what if we have tasks left in our stolenTasks? we would not reach this point then, if statement above would have been true.

                    // if (!(queues[tID].epoch_stolen & BITMASK_STOLEN) && !decrementedFlag) { //this thread previously did find a task
                    if (!decrementedFlag) { //this thread previously did find a task
                        emptyCounter--; //one more empty thread
                        decrementedFlag = 1; //set the flag
                    }

                    //now check status across all threads
                    //if all threads are unable to find tasks (empty SMQs), then we use this to tell algorithm to terminate
                    if (emptyCounter.load() == 0) {
                        return boost::none;
                    }
                }
            }
        }
};
