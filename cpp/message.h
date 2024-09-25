/**
 * Created by vaksenov on 23.07.2019.
 * Ported to C++ by Mark Jeffrey 2021.04.26
 */
#pragma once

#include <array>

#include "edge.h"

#ifdef COMPETITION_RUNTIME
struct Message {
#else
// Since Message::logMu is read/written in the BP algorithms, we must ensure
// that the Message structure is aligned to a power of two size to avoid false
// conflicts in Swarm implementations
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

    // TODO(mcj) For relaxed RMF, unlikely we'll need it
    //int fromId, toId;

    Message(uint64_t i, uint64_t j, Edge& e);

    inline double getPotential(uint64_t vi, uint64_t vj) const {
        return e.getPotential(i, j, vi, vj);
    }

    inline double getLogPotential(uint64_t vi, uint64_t vj) const {
        return e.getLogPotential(i, j, vi, vj);
    }
};
