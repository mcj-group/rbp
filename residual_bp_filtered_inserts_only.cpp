#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <queue>
#include <tuple>
#include <vector>

#include "message.h"
#include "mrf.h"
#include "residual_bp_filtered_inserts_only.h"

//for CSR representation
#define VAR_DEG 3
#define CHK_DEG 6


namespace residual_bp_filtered_inserts_only {

// This algorithm also schedules its updates of messages using a max heap that
// still does not permit changes to the heap. However, unlike
// residual_bp_inserts_only, it uses an auxiliary data structure, a vector of
// scheduled message lookaheads, to filter out unneeded events or delay the
// enqueue of early events. This has two positive impacts:
// (1) it reduces the pressure on the scheduling queue
// (2) it reduces the number of calls to the expensive MRF::getFutureMessage.

using PQElement = std::tuple<double, Message::Message*>;
using PQ = std::priority_queue<PQElement>;

struct Lookahead
{
    double priority;
    std::array<double, 2> nlm;
};

static MRF* mrf;
static const Message::Message* baseMessage;
static std::vector<Lookahead> lookaheads;


static inline uint64_t id(const Message::Message* m) {
    return std::distance(baseMessage, m);
}


template <class T>
static inline double priority(const Message::Message* m, T futureMessage) {
    return utils::distance(m->logMu, futureMessage);
}


void solve(MRF* mrf, double sensitivity,
        std::vector<std::array<double,2> >* answer,
        perf_metrics &metrics) {
    std::cout << "Running the sequential residual BP algorithm "
              << "with a std::priority_queue and an auxiliary "
              << "std::vector of per-message lookaheads"
              << std::endl;

    auto& messages = mrf->getMessages();

    residual_bp_filtered_inserts_only::mrf = mrf;
    residual_bp_filtered_inserts_only::lookaheads.resize(messages.size(), {0.0, {0.0, 0.0}});
    residual_bp_filtered_inserts_only::baseMessage = messages.data();

    auto startTime = std::chrono::high_resolution_clock::now();

    PQ pq;
    for (Message::Message& m: messages) {
        auto nlm = mrf->getFutureMessage(m);
        double prio = priority(&m, nlm);
        if (prio > sensitivity) {
            lookaheads.at(id(&m)).priority = prio;
            lookaheads.at(id(&m)).nlm = nlm;
            pq.push(std::make_tuple(prio, &m));
        }
    }

    int updates = 0;
    int skips = 0;
    while (!pq.empty()) {
        double pushedPrio;
        Message::Message* m;
        std::tie(pushedPrio, m) = pq.top();
        pq.pop();

        uint64_t mid = id(m);
        double curPrio = lookaheads[mid].priority;
        assert(curPrio <= pushedPrio);
        assert(sensitivity < pushedPrio);
        if (curPrio < pushedPrio) {
            // The message, m, lost priority relative to when this event was
            // created, so skip this early update, but reschedule the message at
            // the intended priority, because the neighbor deferred to us.
            if (curPrio > sensitivity) {
                pq.push(std::make_tuple(curPrio, m));
            }
            skips++;
        } else {
            auto futureMessage = lookaheads[mid].nlm;
            assert(curPrio == priority(m, futureMessage));
            mrf->updateMessage(*m, futureMessage);
            updates++;
            lookaheads[mid].priority = 0.0;

            for (Message::Message* nbr : mrf->getMessagesFrom(m->j)) {
                auto nlm = mrf->getFutureMessage(*nbr);
                double nbrNewPrio = priority(nbr, nlm);
                uint64_t nbrid = id(nbr);
                double nbrCurPrio = lookaheads[nbrid].priority;
                if (nbrCurPrio < nbrNewPrio) {
                    if (nbrNewPrio > sensitivity) {
                        lookaheads[nbrid].priority = nbrNewPrio;
                        lookaheads[nbrid].nlm = nlm;
                        pq.push(std::make_tuple(nbrNewPrio, nbr));
                    }
                } else {
                    // There already exists an event for the message. Let's
                    // avoid putting pressure on the queue, and defer to that
                    // overly urgent message re-queue for the right priority,
                    // using the lookaheads vector.
                    if (nbrCurPrio > sensitivity) {
                        // There is no use in writing a value lower than
                        // sensitivity if lookaheads[id(nbr)] is already below
                        // sensitivity
                        lookaheads[nbrid].priority = nbrNewPrio;
                        lookaheads[nbrid].nlm = nlm;
                    }
                }
            }
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto runtime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(endTime-startTime);

    // Update metrics
    metrics.runtime_ms = runtime_ms.count();
    metrics.num_updates = updates;
    mrf->getNodeProbabilities(answer);
}

}

namespace residual_bp_filtered_inserts_only_CSR {

    // This algorithm also schedules its updates of messages using a max heap that
    // still does not permit changes to the heap. However, unlike
    // residual_bp_inserts_only, it uses an auxiliary data structure, a vector of
    // scheduled message lookaheads, to filter out unneeded events or delay the
    // enqueue of early events. This has two positive impacts:
    // (1) it reduces the pressure on the scheduling queue
    // (2) it reduces the number of calls to the expensive MRF::getFutureMessage.

    using PQElement = std::tuple<double, Message_CSR::Message *>;
    using PQ = std::priority_queue<PQElement>;
    static constexpr uint64_t VAR_LENGTH = 2;
    static constexpr uint64_t CHK_LENGTH = 64;

    struct Lookahead
    {
        double priority;
        std::vector<double> nlm;
    };

    static MRF_CSR *mrf;
    static const Message_CSR::Message *baseMessage;
    static std::vector<Lookahead> lookaheads;

    static inline uint64_t id(const Message_CSR::Message *m)
    {
        return std::distance(baseMessage, m);
    }

    template <class T>
    static inline double priority(const Message_CSR::Message *m, T futureMessage)
    {
        return utils_CSR::logDifference(mrf->getLogMu(*m), futureMessage);
    }

    void solve(MRF_CSR *mrf, double sensitivity,
            std::vector<std::array<double,2>>* answer, perf_metrics &metrics)
    {
        std::cout << "Running the sequential residual BP algorithm "
                  << "with a std::priority_queue and an auxiliary "
                  << "std::vector of per-message lookaheads"
                  << std::endl;

        auto &messages = mrf->getMessages();
        std::array<Message_CSR::Message *, VAR_DEG> *msgFromVar = nullptr;
        std::array<Message_CSR::Message *, CHK_DEG> *msgFromChk = nullptr;

        residual_bp_filtered_inserts_only_CSR::mrf = mrf;
        residual_bp_filtered_inserts_only_CSR::lookaheads.resize(messages.size(), {0.0, std::vector<double>{}});
        residual_bp_filtered_inserts_only_CSR::baseMessage = messages.data();

        PQ pq;

        //include message insertion in runtime for fairness with smq and mq
        auto startTime = std::chrono::high_resolution_clock::now();

        for (Message_CSR::Message &m : messages)
        {
            auto nlm = mrf->getFutureMessage(m);
            double prio = priority(&m, nlm);
            if (prio > sensitivity)
            {
                lookaheads.at(id(&m)).priority = prio;
                lookaheads.at(id(&m)).nlm = nlm;
                pq.push(std::make_tuple(prio, &m));
            }
        }

        int updates = 0;
        int skips = 0;
        while (!pq.empty())
        {
            double pushedPrio;
            
            Message_CSR::Message *m;
            std::tie(pushedPrio, m) = pq.top();
            pq.pop();

            uint64_t mid = id(m);
            double curPrio = lookaheads[mid].priority;
            assert(curPrio <= pushedPrio);
            assert(sensitivity < pushedPrio);
            if (curPrio < pushedPrio)
            {
                // The message, m, lost priority relative to when this event was
                // created, so skip this early update, but reschedule the message at
                // the intended priority, because the neighbor deferred to us.
                if (curPrio > sensitivity)
                {
                    pq.push(std::make_tuple(curPrio, m));
                }
                skips++;
            }
            else {
                // Here we *HAVE NOT* updated the message yet (this is diff from relaxed-rbp)
                auto futureMessage = lookaheads[mid].nlm;
                //assert(curPrio == priority(m, futureMessage)); //commenting out because increases runtime
                mrf->updateMessageAndNodeSum(*m, futureMessage);
                updates++;
                lookaheads[mid].priority = 0.0;
                // Here we *HAVE* updated the message
                if (m->chk2var)
                {
                    msgFromVar = mrf->getMessageFromVar(m->j);
                    for (Message_CSR::Message *nbr : *msgFromVar)
                    {
                        auto nlm = mrf->getFutureMessage(*nbr);
                        double nbrNewPrio = priority(nbr, nlm);
                        uint64_t nbrid = id(nbr);
                        double nbrCurPrio = lookaheads[nbrid].priority;
                        if (nbrCurPrio < nbrNewPrio)
                        {
                            if (nbrNewPrio > sensitivity)
                            {
                                lookaheads[nbrid].priority = nbrNewPrio;
                                lookaheads[nbrid].nlm = nlm;
                                pq.push(std::make_tuple(nbrNewPrio, nbr));
                            }
                        }
                        else
                        {
                            // There already exists an event for the message. Let's
                            // avoid putting pressure on the queue, and defer to that
                            // overly urgent message re-queue for the right priority,
                            // using the lookaheads vector.
                            if (nbrCurPrio > sensitivity)
                            {
                                // There is no use in writing a value lower than
                                // sensitivity if lookaheads[id(nbr)] is already below
                                // sensitivity
                                lookaheads[nbrid].priority = nbrNewPrio;
                                lookaheads[nbrid].nlm = nlm;
                            }
                        }
                    }
                }
                else
                {
                    msgFromChk = mrf->getMessageFromChk(m->j);
                    for (Message_CSR::Message *nbr : *msgFromChk)
                    {
                        auto nlm = mrf->getFutureMessage(*nbr);
                        double nbrNewPrio = priority(nbr, nlm);
                        uint64_t nbrid = id(nbr);
                        double nbrCurPrio = lookaheads[nbrid].priority;
                        if (nbrCurPrio < nbrNewPrio)
                        {
                            if (nbrNewPrio > sensitivity)
                            {
                                lookaheads[nbrid].priority = nbrNewPrio;
                                lookaheads[nbrid].nlm = nlm;
                                pq.push(std::make_tuple(nbrNewPrio, nbr));
                            }
                        }
                        else
                        {
                            // There already exists an event for the message. Let's
                            // avoid putting pressure on the queue, and defer to that
                            // overly urgent message re-queue for the right priority,
                            // using the lookaheads vector.
                            if (nbrCurPrio > sensitivity)
                            {
                                // There is no use in writing a value lower than
                                // sensitivity if lookaheads[id(nbr)] is already below
                                // sensitivity
                                lookaheads[nbrid].priority = nbrNewPrio;
                                lookaheads[nbrid].nlm = nlm;
                            }
                        }
                    }
                }
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
        metrics.num_skips = skips;
        mrf->getVarNodeProbabilities(answer);
    }

}