#include <cassert>

#include "mrf.h"

void MRF::setNodePotential(uint64_t v, std::array<double,LENGTH> potentials) {
    for (uint64_t i = 0; i < LENGTH; i++) {
        nodePotentials.at(v)[i] = potentials[i];
        logNodePotentials.at(v)[i] = std::log(potentials[i]);
    }
}


void MRF::addEdge(uint64_t i, uint64_t j, Edge::array2d_t phi) {
    // Did you pre-allocate enough edges in the constructor of MRF?
    assert(edges.size() < edges.capacity());
    edges.emplace_back(i, j, phi);
    Edge& e = edges.back();

    // Did you pre-allocate enough edges in the constructor of MRF?
    assert(messages.size() < messages.capacity() - 1);
    Message& m1 = createMessage(i, j, e);
    Message& m2 = createMessage(j, i, e);

    m1.reverse = &m2;
    m2.reverse = &m1;
}


void MRF::getNodeProbabilities(
        std::vector<std::array<double,LENGTH> >* answer) const {
    uint64_t nodes = getNodes();
    answer->resize(nodes, {0,0});
    for (uint64_t i = 0; i < nodes; i++) {
        std::array<double,LENGTH>& a = (*answer)[i];
        for (uint64_t vali = 0; vali < LENGTH; vali++) {
            a[vali] = logNodePotentials[i][vali] + logProductIn[i][vali];
        }
        double sum = utils::logSum(a);
        for (uint64_t vali = 0; vali < LENGTH; vali++) {
            a[vali] -= sum;
            a[vali] = std::exp(a[vali]);
        }
    }
}


Message& MRF::createMessage(uint64_t i, uint64_t j, Edge& e) {
    messages.emplace_back(i, j, e);
    Message& m = messages.back();
    messagesFrom[i].push_back(&m);
    messagesTo[j].push_back(&m);
    for (uint64_t k = 0; k < LENGTH; k++) {
        logProductIn[j][k] += m.logMu[k];
    }
    return m;
}
