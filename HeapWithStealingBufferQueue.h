#pragma once
#include <queue>
#include <cstdlib>
#include <iostream>
#include <tuple>
#include <vector>
#include <algorithm>
#include <mutex>
#include <random>
#include <atomic>
#include <thread>
#include <cassert>
#include <array>
#include <boost/optional.hpp>

#define SIZE_STEAL 4 //open to experimentation -- must be set to 0 if using only one thread
#define BITMASK_STOLEN 0x0000000000000001
#define BITMASK_SET_STOLEN_FALSE 0xfffffffffffffffe

/*
Avoiding use of std::optional or boost::optional bc of high overhead
*/

class HeapWithStealingBufferQueue {
    public:
        //only stolenTasks/stealingBuffer can contain UNQUEUED tasks because used to guarantee stealingBuffer size. PQ does not face this.
        static constexpr uint64_t UNQUEUED = UINT64_MAX;
        struct Node;
        typedef std::tuple<double, uint64_t> PQElement; //double is priority, uint64_t is key to index into nodes std::vector, bc nodes is max_size based on # of msgs
        typedef std::priority_queue<PQElement> PQ;

        PQ q;
        std::array<PQElement, SIZE_STEAL> stealingBuffer; //using std::array for hopefully better perf than std::vector

        //LSB is stolen flag (1 if steal from this SMQ happened in current epoch, else 0)
        //Rest of the bits are current epoch (time stamp that is incremented every time any SMQ fills its stealingBuffer
        std::atomic<uint64_t> epoch_stolen{0x0000000000000001}; 

        // HeapWithStealingBufferQueue(); //constructor
        // ~HeapWithStealingBufferQueue(); //destructor
        // void addLocal(PQElement task); //equivalent to(uint64_t key, double priority);
        // boost::optional<PQElement> extractTopLocal();
        // boost::optional<PQElement> top();
        // std::vector<PQElement> steal(int size);
        // void fillBuffer(); 

        bool debug_heap = false;

        //note that top does not remove the first element, just returns it bc we read the priority to choose
        //if we want to steal it in trySteal

        HeapWithStealingBufferQueue() { }

        ~HeapWithStealingBufferQueue() { }

        void
        addLocal(PQElement task) { //equivalent to(uint64_t key, double priority);
            
            q.push(task);
            if (epoch_stolen.load() & BITMASK_STOLEN) { //stolen
                this->fillBuffer();
            }
            
        }

        boost::optional<PQElement>
        extractTopLocal() {
            if (epoch_stolen.load() & BITMASK_STOLEN) { //stolen) {
                this->fillBuffer();
            }
            if (!q.empty()) {
                PQElement ret = q.top(); //bc pop does not return value
                // if ( (ret > stealingBuffer[0]) || (std::get<1>(stealingBuffer[0]) == UNQUEUED) || (epoch_stolen.load() & BITMASK_STOLEN) ) { //compares priorities (first elem of each tuple), or if someone stole from stealingBuffer anyways
                    //only return from its own queue if the task is higher priority than something in its own stealingBuffer
                    q.pop();
                    return ret;
                // }
            }
            //else
            if (debug_heap) std::cout << "extractTopLocal: empty q" << std::endl;
            return boost::none;
        }

        //used to peek top prio elem (head) of stealingBuffer for other threads to decide to steal or not
        boost::optional<PQElement>
        top() {
            //must return pointer to allow for NULL to be assigned
            uint64_t curr_epoch;
            while (true) {
                curr_epoch = epoch_stolen.load() >> 1; //epoch;
                boost::optional<PQElement> top;
                if (epoch_stolen.load() & BITMASK_STOLEN) { //someone already stole on present epoch/time
                    //top = std::make_tuple(0.0, UNQUEUED); //nullptr);
                    return boost::none; //top;
                }
                if (SIZE_STEAL > 0) { //skip edgecase where smq config has size zero stealingBuffer
                    top = stealingBuffer[0]; //first elem in stealingBuffer has highest priority
                }
                else if (!q.empty()) { //check the heap if edgecase where smq config has size zero stealingBuffer
                    top = q.top(); //returns max elem in heap
                }
                else {
                    top = boost::none;
                }
                if (curr_epoch != (epoch_stolen.load() >> 1)) { //another thread stole from our stealingBuffer so try again
                    continue; //break this iteration of while loop and go to next iteration
                }

                if (top) { //check that not boost::none
                    uint64_t key = std::get<1>(top.get()); 
                    if (key == UNQUEUED) { //we selected a filler task inserted by fillBuffer(), so discard 
                        return boost::none;
                }
                }
                return top;
            }

        }

        std::vector<PQElement>
        steal() {
            while (SIZE_STEAL > 0) { //infinite loop, unless edgecase of zero sized stealingBuffer due to smq config
                uint64_t curr_epoch = epoch_stolen.load() >> 1; //epoch;
                if (epoch_stolen.load() & BITMASK_STOLEN) { //stolen) {
                    //std::cout << "tasks[0]: " << std::get<0>(tasks[0]) << std::endl;
                    return std::vector<PQElement>(); //tasks; //cannot steal so return empty vector
                }

                //copy array into vector (because need vector as return type in smq)
                //note that this syntax is different from regular array (used std::array)
                std::vector<PQElement> tasks; //empty vector
                tasks.insert(tasks.end(), &stealingBuffer[0], &stealingBuffer[SIZE_STEAL]); //tasks = stealingBuffer

                //atomic implementation
                bool skip = false;
                uint64_t temp_epoch_stolen = epoch_stolen.load();
                do {
                    if ( ((temp_epoch_stolen >> 1) != curr_epoch) || (temp_epoch_stolen & BITMASK_STOLEN)) {
                        skip = true;
                        break;
                    }

                } while (!(epoch_stolen.compare_exchange_weak(temp_epoch_stolen, temp_epoch_stolen | BITMASK_STOLEN))); //try to atomically set stolen to true
                if (skip) {
                    continue; //repeat outer while loop
                }
                if (debug_heap)  {
                    std::cout << "tasks in HeapWithStealingBufferQueue.cpp steal(): " << std::endl;
                    for (unsigned long i = 0; i < tasks.size(); i++) {
                        std::cout << std::get<0>(tasks[i]) << " " ;
                    }
                    std::cout << std::endl;
                }
                //std::cout << "tasks[0]: " << std::get<0>(tasks[0]) << std::endl;
                return tasks;
            }
        }

        void
        fillBuffer() {
            int inserted_non_filler_task = 0;
            for (int i = 0; i < SIZE_STEAL; i++) {
                if (q.empty()) {
                    //insert filler element to ensure size of stealingBuffer == SIZE_STEAL
                    stealingBuffer[i] = {0, UNQUEUED}; //break;
                }
                else { //extractTop aka store and pop
                    PQElement task = q.top(); //since popping from a priority queue, stealingBuffer is also sorted high to low prio
                    q.pop();
                    stealingBuffer[i] = task;
                    inserted_non_filler_task = 1;
                }
            }
            //only reset stolen flag and increment epoch if stealingBuffer is not entirely made of filler tasks
            //this prevents stealingBuffer from having useless tasks
            //keeping stolen set as true will allow this HeapWithStealingBufferQueue's thread to keep trying to fill the buffer
            if (inserted_non_filler_task) {
                epoch_stolen = (((epoch_stolen.load() >> 1) + 1) << 1) | (epoch_stolen.load() & BITMASK_STOLEN); //epoch++, and restore the LSB stolen flag
                epoch_stolen = epoch_stolen.load() & BITMASK_SET_STOLEN_FALSE; //stolen = false;
            }
        }
};