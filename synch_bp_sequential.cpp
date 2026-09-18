#include "synch_bp_sequential.h"

namespace synch_bp_sequential {
    static const Message::Message* baseMessage;
    
    static inline uint64_t id(const Message::Message* m) {
        return std::distance(baseMessage, m);
    }

    double totalError(MRF *mrf, std::vector<Message::Message> messages) {
        double result = 0;
        for (Message::Message message : messages) {
            result += utils::distance(message.logMu, mrf->getFutureMessage(message));
        }
        return result;
    }

    void solve(MRF *mrf, double sensitivity, std::vector<std::array<double,2>> *answer, perf_metrics &metrics) {
        synch_bp_sequential::baseMessage = mrf->getMessages().data();
        auto &Messages = mrf->getMessages();
        const uint64_t numNodes = mrf->getNodes();

        std::vector<std::array<double, 2>> new_mu(Messages.size());

        auto startTime = std::chrono::high_resolution_clock::now();
        uint64_t iterations = 0;
        uint64_t updates = 0;
        bool updated = true;
        double dist = 0;
        while (updated) {

            // if (iterations % 10 == 0) {
            //     std::cout << "Current error: " << totalError(mrf, Messages) << std::endl;
            // }

            updated = false;
            ++iterations;
            for (Message::Message &msg : Messages)
            {
                new_mu[id(&msg)] = mrf->getFutureMessage(msg);
            }

            bool updatedFlag = false;
            for (Message::Message &msg : Messages) {
                std::array<double, 2> old_mu = msg.logMu; //mrf->getLogMu(msg);
                dist = utils::distance(old_mu, new_mu[id(&msg)]); //logDifference in ldpc utils.h
                if (dist > sensitivity) { 
                    updatedFlag = true;
                }
                updates++;
                mrf->updateMessage(msg, new_mu[id(&msg)]);
            }
            updated = updatedFlag;
            for (uint64_t nodeIdx = 0; nodeIdx < numNodes; nodeIdx++) {
                mrf->updateNodeSum(nodeIdx);
            }
        }
        auto endTime = std::chrono::high_resolution_clock::now();
        auto runtime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        // Update metrics
        metrics.runtime_ms = runtime_ms.count();
        metrics.num_updates = updates;
        
        mrf->getNodeProbabilities(answer);
    }
} //namespace synch_bp_sequential

namespace synch_bp_sequential_CSR {
    static const Message_CSR::Message* baseMessage;
    
    static inline uint64_t id(const Message_CSR::Message* m) {
        return std::distance(baseMessage, m);
    }

    void solve(MRF_CSR *mrf, double sensitivity, 
        std::vector<std::array<double,2>>* answer, perf_metrics &metrics) {
        synch_bp_sequential_CSR::baseMessage = mrf->getMessages().data();
        /* Messages */
        auto &Messages = mrf->getMessages();
        const uint64_t numChkNodes = mrf->getNumChkNodes();
        const uint64_t numVarNodes = mrf->getNumVarNodes();

        std::vector<std::vector<double>> new_mu(Messages.size());

        auto startTime = std::chrono::high_resolution_clock::now();
        std::chrono::minutes timeLimit(30); // 30-min timer
        uint64_t iterations = 0;
        uint64_t updates = 0;
        bool updated = true;
        double dist = 0;

        while (updated) 
        {
            updated = false;
            ++iterations;

            for (Message_CSR::Message &msg : Messages) {
                new_mu[msg.id] = mrf->getFutureMessage(msg);
            }

            bool updatedFlag = false;
            for (Message_CSR::Message &msg : Messages) {
                std::vector<double> old_mu = mrf->getLogMu(msg);
                dist = utils_CSR::logDifference(old_mu, new_mu[msg.id]); //logDifference in ldpc utils.h
                if (dist > sensitivity) { 
                    updatedFlag = true;
                }
                updates++;
                mrf->copyMessage(msg, new_mu[msg.id]);
            }
            updated = updatedFlag;

            // Update var and chk nodes
            for (uint64_t nodeIdx = 0; nodeIdx < numVarNodes; ++nodeIdx) {
                mrf->updateVarNodeSum(nodeIdx);
            }
            for (uint64_t nodeIdx = 0; nodeIdx < numChkNodes; ++nodeIdx) {
                mrf->updateChkNodeSum(nodeIdx);
            }

            // Check elapsed time
            auto currentTime = std::chrono::high_resolution_clock::now();
            auto elapsedTime = std::chrono::duration_cast<std::chrono::minutes>(currentTime - startTime);
            if (elapsedTime >= timeLimit) {
                std::cout << "Timer expired: 30 minutes have passed." << std::endl;
                break;
            }
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto runtime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime-startTime);
        // std::cout << "Elapsed Time " << ms.count() << "ms" << std::endl; //runtime_ms

        // std::cout << "Updates " << updates << std::endl;
        // runtime_ms = ms.count();
        // num_updates = updates;
        // std::cout << "Skips " << skips << std::endl; 
        // Update metrics
        metrics.runtime_ms = runtime_ms.count();
        metrics.num_updates = updates;

        mrf->getVarNodeProbabilities(answer);
    }
} //namespace synch_bp_sequential_CSR