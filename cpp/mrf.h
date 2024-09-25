/**
 * Created by vaksenov on 23.07.2019.
 * Ported to C++ by Mark Jeffrey 2021.04.27
 */
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "edge.h"
#include "message.h"
#include "utils.h"

class MRF {
    static constexpr uint64_t LENGTH = Edge::LENGTH;

    // [mcj] This first structure seems to be completely unused.
    std::vector<std::array<double,LENGTH> > nodePotentials;
    std::vector<std::array<double,LENGTH> > logNodePotentials;

    // There are two messages per edge, one for each direction
    std::vector<Edge> edges;
    // The java version used a hash map of pair(i,j) -> Message*. This would
    // enable a mapping of edge direction to the given Message, but it didn't
    // seem to be used. I replaced it with a vector of Messages. If it's
    // important, replace the following with an unordered_map
    std::vector<Message> messages;
    std::vector<std::vector<Message*> > messagesFrom;
    std::vector<std::vector<Message*> > messagesTo;
    std::vector<std::array<double,LENGTH> > logProductIn;

  public:

    MRF(uint64_t _nodes, uint64_t _edges)
      : nodePotentials(_nodes)
      , logNodePotentials(_nodes)
      , messagesFrom(_nodes)
      , messagesTo(_nodes)
      , logProductIn(_nodes, {0,0})
    {
      // N.B. each Message holds a reference to its corresponding Edge and
      // reverse Message, but the backing stores for Edge and Message memory is
      // in these vectors. Consequently we *must* reserve enough space for these
      // data structures in advance, as resizing the vector later will break
      // these pointers.
      // TODO(mcj) this design is fragile. The right solution is to replace all
      // address pointers with edge IDs, moving toward a proper compressed
      // sparse row (CSR) format. But that will require a nontrivial re-design
      // of MRF, its interfaces, and the construction of these graphs
      // altogether. I'd like to see us move toward holding MRFs in files in
      // something like the WeightedAdjacencyGraph format from PBBS:
      // https://www.cs.cmu.edu/~pbbs/benchmarks/graphIO.html
      // Of course Markov Random Fields have more annotations per vertex and
      // edge than its typical in graphs.
      edges.reserve(_edges);
      messages.reserve(2 * _edges);
    }

    uint64_t getNodes() const { return logNodePotentials.size(); }


    static constexpr uint64_t getNumberOfValues(uint64_t) {
        return LENGTH;
    }


    void setNodePotential(uint64_t v, std::array<double,LENGTH> potentials);


    void addEdge(uint64_t i, uint64_t j, Edge::array2d_t phi);


    inline std::vector<Message>& getMessages() { return messages; }
    inline const std::vector<Message>& getMessages() const { return messages; }


    inline const std::vector<Message*>& getMessagesFrom(uint64_t v) const {
        return messagesFrom[v];
    }


    inline const std::vector<Message*>& getMessagesTo(uint64_t v) const {
        return messagesTo[v];
    }


    std::array<double, LENGTH> getLogProductIn(uint64_t i) const {
        return logProductIn[i];
    }


    std::array<double, LENGTH> getPartialLogsIn(const Message& m) const {
        uint64_t i = m.i;
        const Message& reverseMessage = *m.reverse;
        std::array<double,LENGTH> partialLogsIn;
        for (uint64_t vali = 0; vali < LENGTH; vali++) {
            partialLogsIn[vali] = logNodePotentials[i][vali]
                                    + (logProductIn[i][vali]
                                    - reverseMessage.logMu[vali]);
        }
        return partialLogsIn;
    }


    std::array<double,LENGTH> getFutureMessage(
            const Message& m, std::array<double,LENGTH> partialLogsIn) const {
        // Use a C++ static array so that we can return it by value, but
        // hopefully just by registers.
        std::array<double,LENGTH> result;

        for (uint64_t valj = 0; valj < LENGTH; valj++) {
            std::array<double,LENGTH> logsIn;
            for (uint64_t vali = 0; vali < LENGTH; vali++) {
                logsIn[vali] = m.getLogPotential(vali, valj)
                                + partialLogsIn[vali];
            }
            result[valj] = utils::logSum(logsIn);
        }
        double logTotalSum = utils::logSum(result);
        for (uint64_t valj = 0; valj < LENGTH; valj++) {
            result[valj] -= logTotalSum;
        }
        return result;
    }


    std::array<double,LENGTH> getFutureMessage(
            const Message& m,
            std::array<double,LENGTH> logProductIn,
            std::array<double,LENGTH> reverseLogMu) const {
        uint64_t i = m.i;
        std::array<double,LENGTH> result;
        for (uint64_t valj = 0; valj < LENGTH; valj++) {
            std::array<double,LENGTH> logsIn;
            for (uint64_t vali = 0; vali < LENGTH; vali++) {
                logsIn[vali] = m.getLogPotential(vali, valj)
                        + logNodePotentials[i][vali]
                        + (logProductIn[vali] - reverseLogMu[vali]);
            }
            result[valj] = utils::logSum(logsIn);
        }
        double logTotalSum = utils::logSum(result);
        for (uint64_t valj = 0; valj < LENGTH; valj++) {
            result[valj] -= logTotalSum;
        }
        return result;
    }


    std::array<double,LENGTH> getFutureMessage(const Message& m) const {
        uint64_t i = m.i;
        const Message& reverseMessage = *m.reverse;

        // Use a C++ static array so that we can return it by value, but
        // hopefully just by registers.
        std::array<double,LENGTH> result;

        for (uint64_t valj = 0; valj < LENGTH; valj++) {
            std::array<double,LENGTH> logsIn;
            for (uint64_t vali = 0; vali < LENGTH; vali++) {
                logsIn[vali] = m.getLogPotential(vali, valj)
                        + logNodePotentials[i][vali]
                        + (logProductIn[i][vali] - reverseMessage.logMu[vali]);
            }
            result[valj] = utils::logSum(logsIn);
        }
        double logTotalSum = utils::logSum(result);
        for (uint64_t valj = 0; valj < LENGTH; valj++) {
            result[valj] -= logTotalSum;
        }
        return result;
    }


    inline void updateMessage(Message& m, std::array<double,LENGTH> newLogMu) {
        uint64_t j = m.j;
        for (uint64_t valj = 0; valj < LENGTH; valj++) {
            logProductIn[j][valj] += -m.logMu[valj] + newLogMu[valj];
        }
        m.logMu = newLogMu;
    }


    inline void updateLogProductIn(
            uint64_t j,
            std::array<double,LENGTH> logMuDelta) {
        for (uint64_t valj = 0; valj < LENGTH; valj++) {
            logProductIn[j][valj] += logMuDelta[valj];
        }
    }


    void updateMessage(Message& m) {
        updateMessage(m, getFutureMessage(m));
    }


    void getNodeProbabilities(
            std::vector<std::array<double,LENGTH> >* answer) const;


  private:
    Message& createMessage(uint64_t i, uint64_t j, Edge& e);
};
