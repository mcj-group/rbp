/**
 * Created by vaksenov on 24.07.2019.
 * Ported to C++ by Mark Jeffrey 2021.04.29
 */

#include <array>
#include <boost/heap/fibonacci_heap.hpp>
#include <cstdlib>
#include <iostream>
#include <tuple>
#include <vector>

#include "message.h"
#include "mrf.h"
#include "residual_bp.h"

namespace residual_bp {

// We use a Boost heap below since it implements an "update" function, whereas
// std::priority_queue does not. We leverage the lexicographical comparison of
// std::tuple for ordering, but we need to specify the use of std::less.
// Inexplicably, this is how we make a max heap with the Boost fibonacci_heap
// rather than std::greater. I don't get it...
using PQElement = std::tuple<double, Message*>;
using PQ = boost::heap::fibonacci_heap<
            PQElement,
            boost::heap::compare<std::less<PQElement> >
            >;
// TODO Use a d_ary heap since it has a reserve() method?
// <boost/heap/d_ary_heap.hpp>


// [mcj] Since the Java ResidualBP class is basically a singleton with little
// abstraction, let's not bother with OOO for it.
static double sensitivity;
static MRF* mrf;
static const Message* baseMessage;
static std::vector<PQ::handle_type> pqHandles;


static inline double priority(const Message* m) {
    return utils::distance(m->logMu, mrf->getFutureMessage(*m));
}


static inline uint64_t id(const Message* m) {
    return std::distance(baseMessage, m);
}


void solve(MRF* mrf, double sensitivity,
           std::vector<std::array<double,2> >* answer) {
    std::cout << "Running the sequential residual BP algorithm" << std::endl;

    residual_bp::sensitivity = sensitivity;
    residual_bp::mrf = mrf;
    residual_bp::baseMessage = mrf->getMessages().data();

    auto& messages = mrf->getMessages();
    pqHandles.reserve(messages.size());

    PQ pq;
    for (Message& message : messages) {
        auto handle = pq.push(std::make_tuple(priority(&message), &message));
        pqHandles.push_back(handle);
    }

    int it = 0;
    while (!pq.empty()) {
        double prio;
        Message* m;
        std::tie(prio, m) = pq.top();
        if (prio <= sensitivity) break;

        mrf->updateMessage(*m, mrf->getFutureMessage(*m));
        pq.update(pqHandles[id(m)], std::make_tuple(0.0, m));
        for (Message* affected : mrf->getMessagesFrom(m->j)) {
            pq.update(pqHandles[id(affected)],
                      std::make_tuple(priority(affected), affected));
        }
        it++;
    }

    std::cout << "Updates " << it << std::endl;

    mrf->getNodeProbabilities(answer);
}

} // namespace residual_bp
