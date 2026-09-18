#pragma once

#include <array>

#include "edge.h"

//for non-CSR MRFs (i.e., all except LDPC)
namespace Message {
    #ifdef COMPETITION_RUNTIME
    struct Message {
    #else
    struct alignas(64) Message {
    #endif

        static constexpr uint64_t LENGTH = Edge::LENGTH;

        uint64_t i, j;
        Edge& e;
        Message* reverse;
    #ifndef COMPETITION_RUNTIME
        // All previous variables are read-only but logMu is read-write. To avoid
        // false aborts due to false sharing, we add padding to push logMu onto its
        // own cache line.
        uint8_t padding[32];
    #endif
        std::array<double,LENGTH> logMu;

        //int fromId, toId;

        Message(uint64_t i, uint64_t j, Edge& e);

        inline double getPotential(uint64_t vi, uint64_t vj) const {
            return e.getPotential(i, j, vi, vj);
        }

        inline double getLogPotential(uint64_t vi, uint64_t vj) const {
            return e.getLogPotential(i, j, vi, vj);
        }
    };
}

//for CSR MRFs (LDPC)
namespace Message_CSR {

    struct alignas(64) Message {
        uint64_t id;
        uint64_t i,j;
        bool chk2var;
        
        public:
            static constexpr uint64_t VAR_LENGTH = 2;
            static constexpr uint64_t CHK_LENGTH = 64;

        public:
            Message(uint64_t msg_id, uint64_t i, uint64_t j, bool chk2var)
            : id(msg_id), i(i), j(j), chk2var(chk2var)
            { }
    };
    
}
