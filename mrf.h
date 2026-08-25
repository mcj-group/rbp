#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include <cassert>
#include <iostream>
#include <fstream>
#include <limits>
#include <algorithm>

#include "edge.h"
#include "message.h"
#include "utils.h"

//for MRF-related functionality


//for non-CSR MRFs (i.e., all except LDPC)
class MRF {
    static constexpr uint64_t LENGTH = Edge::LENGTH;

    std::vector<std::array<double,LENGTH> > nodePotentials;
    std::vector<std::array<double,LENGTH> > logNodePotentials;

    // There are two messages per edge, one for each direction
    std::vector<Edge> edges;
    std::vector<Message::Message> messages;
    std::vector<std::vector<Message::Message*> > messagesFrom;
    std::vector<std::vector<Message::Message*> > messagesTo;
    std::vector<std::array<double,LENGTH> > logProductIn;

  public:

    MRF(uint64_t _nodes, uint64_t _edges)
      : nodePotentials(_nodes)
      , logNodePotentials(_nodes)
      , messagesFrom(_nodes)
      , messagesTo(_nodes)
      , logProductIn(_nodes, {0,0})
    {
      edges.reserve(_edges);
      messages.reserve(2 * _edges);
    }

    uint64_t getNodes() const { return logNodePotentials.size(); }


    static constexpr uint64_t getNumberOfValues(uint64_t) {
        return LENGTH;
    }


    void setNodePotential(uint64_t v, std::array<double,LENGTH> potentials);


    void addEdge(uint64_t i, uint64_t j, Edge::array2d_t phi);


    inline std::vector<Message::Message>& getMessages() { return messages; }
    inline const std::vector<Message::Message>& getMessages() const { return messages; }


    inline const std::vector<Message::Message*>& getMessagesFrom(uint64_t v) const {
        return messagesFrom[v];
    }


    inline const std::vector<Message::Message*>& getMessagesTo(uint64_t v) const {
        return messagesTo[v];
    }


    std::array<double, LENGTH> getLogProductIn(uint64_t i) const {
        return logProductIn[i];
    }


    std::array<double, LENGTH> getPartialLogsIn(const Message::Message& m) const {
        uint64_t i = m.i;
        const Message::Message& reverseMessage = *m.reverse;
        std::array<double,LENGTH> partialLogsIn;
        for (uint64_t vali = 0; vali < LENGTH; vali++) {
            partialLogsIn[vali] = logNodePotentials[i][vali]
                                    + (logProductIn[i][vali]
                                    - reverseMessage.logMu[vali]);
        }
        return partialLogsIn;
    }


    std::array<double,LENGTH> getFutureMessage(
            const Message::Message& m, std::array<double,LENGTH> partialLogsIn) const {
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
            const Message::Message& m,
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


    std::array<double,LENGTH> getFutureMessage(const Message::Message& m) const {
        uint64_t i = m.i;
        const Message::Message& reverseMessage = *m.reverse;

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


    inline void updateMessage(Message::Message& m, std::array<double,LENGTH> newLogMu) {
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


    void updateMessage(Message::Message& m) {
        updateMessage(m, getFutureMessage(m));
    }



    void getNodeProbabilities(
            std::vector<std::array<double,LENGTH> >* answer) const;

    void updateNodeSum(int i) {
        for (uint vali = 0; vali < nodePotentials[i].size(); vali++) {
            logProductIn[i][vali] = 0;
        }
        for (Message::Message* message: messagesTo[i]) {
            for (uint vali = 0; vali < nodePotentials[i].size(); vali++) {
                logProductIn[i][vali] += message->logMu[vali];
            }
        }    
    }


  private:
    Message::Message& createMessage(uint64_t i, uint64_t j, Edge& e);
};

//for LDPC, using compressed sparse row (CSR) representation for MRF
class MRF_CSR {

    typedef uint64_t MessageID;
    typedef uint64_t VertexID;

    MessageID msgIDMRF;

    VertexID numVarNodes;
    VertexID numChkNodes;

    std::vector<Message_CSR::Message> messages;

    static constexpr uint64_t VAR_LENGTH = Message_CSR::Message::VAR_LENGTH;
    static constexpr uint64_t CHK_LENGTH = Message_CSR::Message::CHK_LENGTH;
    static constexpr uint64_t CHK_DEG = 6;
    static constexpr uint64_t VAR_DEG = 3;
    static constexpr uint64_t PHI_CHK_LEN = 192;
    static constexpr uint64_t PHI_VAR_LEN = 12;
    
    // Connectivity
    std::vector<VertexID> varIDFrom;    // length=numVarNode
    std::vector<std::array<VertexID,VAR_DEG>> chkIDTo;
    std::vector<VertexID> chkIDFrom;    // length=numChkNode
    std::vector<std::array<VertexID,CHK_DEG>> varIDTo;
    

    // Message Downstream Tracker
    std::vector<std::array<Message_CSR::Message*,VAR_DEG>> messageFromVar;
    std::vector<std::array<Message_CSR::Message*,CHK_DEG>> messageFromChk;
    
    // Message Upstream Tracker
    std::vector<std::array<Message_CSR::Message*,VAR_DEG>> messageToVar;
    std::vector<std::array<Message_CSR::Message*,CHK_DEG>> messageToChk;

    // Message Potential
    std::vector<std::array<double,PHI_CHK_LEN>> logMu_Var2Chk;
    std::vector<std::array<double,PHI_VAR_LEN>> logMu_Chk2Var;    

    // Variable Node Potential
    std::vector<std::array<double,VAR_LENGTH>> logVarNodePot; // Only matters for initialization
    std::vector<std::array<double,VAR_LENGTH>> logVarProductIn; // Previously logProductIn

    // Check Node Potential 
    std::vector<std::array<double,CHK_LENGTH>> logChkNodePot; // Only matters for initialization
    std::vector<std::array<double,CHK_LENGTH>> logChkProductIn; // Previously logProductIn

    // Potential Mask
    std::array<std::array<double,CHK_DEG*2>, CHK_LENGTH> logEdgeMask;


    public:
        MRF_CSR(VertexID _varnodes, VertexID _chknodes, VertexID _edges)
            :
            numVarNodes(_varnodes),
            numChkNodes(_chknodes),
            varIDFrom(_varnodes, 0),  // Initialize vectors
            chkIDTo(_varnodes, std::array<VertexID, VAR_DEG>()),            
            chkIDFrom(_chknodes, 0),
            varIDTo(_chknodes, std::array<VertexID, CHK_DEG>()),
            messageFromVar(_varnodes, std::array<Message_CSR::Message*, VAR_DEG>()),
            messageFromChk(_chknodes, std::array<Message_CSR::Message*, CHK_DEG>()),
            messageToVar(_varnodes, std::array<Message_CSR::Message*, VAR_DEG>()),
            messageToChk(_chknodes, std::array<Message_CSR::Message*, CHK_DEG>()),
            logVarNodePot(_varnodes, std::array<double, VAR_LENGTH>()),
            logVarProductIn(_varnodes, std::array<double, VAR_LENGTH>()),
            logChkNodePot(_chknodes, std::array<double, CHK_LENGTH>()),
            logChkProductIn(_chknodes, std::array<double, CHK_LENGTH>()),
            logMu_Var2Chk(_varnodes, std::array<double, PHI_CHK_LEN>()),
            logMu_Chk2Var(_chknodes, std::array<double, PHI_VAR_LEN>())
        {
            messages.reserve(2*_edges);
            msgIDMRF = 0;
        }

    std::vector<Message_CSR::Message>& getMessages() {return messages;}
    inline const uint64_t getTotalNumNodes() const {return (numVarNodes + numChkNodes); };
    /* Find downstream of the current vertex */
    /*  if:     msg is chk==>var
                    src = chk node index (need to be offset)
                    search through messageFromVar
        else:   msg is var==>chk
                src = chk node index (need to be offset)
                search through messageFromChk
        end
    */
    Message_CSR::Message* getReverseMessage(Message_CSR::Message& m) {
        VertexID msg_end = m.chk2var ? m.j : (m.j - numVarNodes);
        Message_CSR::Message* mRev;
        uint64_t edgeID;
        if (m.chk2var) {
            edgeID = getVar2ChkIndex(msg_end, m.i);
            mRev = messageFromVar[msg_end][edgeID];       
        } else {
            edgeID = getChk2VarIndex(msg_end, m.i);
            mRev = messageFromChk[msg_end][edgeID];
        }
        return mRev;
    }

    std::array<Message_CSR::Message*,VAR_DEG>* getMessageFromVar(VertexID startVarID) {
        return &(messageFromVar[startVarID]);
    }

    std::array<Message_CSR::Message*,CHK_DEG>* getMessageFromChk(VertexID startChkID) {
        startChkID -= numVarNodes;
        return &(messageFromChk[startChkID]);
    }

    std::array<Message_CSR::Message*,VAR_DEG>* getMessageToVar(VertexID EndVarID) {
        return &(messageToVar[EndVarID]);
    }

    std::array<Message_CSR::Message*,CHK_DEG>* getMessageToChk(VertexID EndChkID) {
        EndChkID -= numVarNodes;
        return &(messageToChk[EndChkID]);
    }

    std::vector<double> getLogMu(const Message_CSR::Message& m) {
        VertexID msg_start = m.chk2var ? (m.i - numVarNodes) : m.i;
        VertexID msg_end = m.chk2var ? m.j : (m.j - numVarNodes);
        std::vector<double> curr_logMu;

        uint64_t connectedID; uint64_t offset;
        if (m.chk2var) {
            connectedID = getChk2VarIndex(msg_start, msg_end);
            offset = connectedID*VAR_LENGTH;
            for (uint64_t k=0; k < VAR_LENGTH; k++) {
                curr_logMu.push_back( logMu_Chk2Var[msg_start][offset+k] );
            }
        } else {
            connectedID = getVar2ChkIndex(m.i, m.j);
            offset = connectedID*CHK_LENGTH;
            for (uint64_t k=0; k < CHK_LENGTH; k++) {
                curr_logMu.push_back( logMu_Var2Chk[msg_start][offset+k] );
            }
        }
       
        return curr_logMu;
    }
    VertexID getNumVarNodes() {return numVarNodes;}
    VertexID getNumChkNodes() {return numChkNodes;}


    void setVarNodePotential(VertexID v, const std::vector<double> &potentials) {
        // Setting Variable Node logPotentials
        for (uint64_t i=0; i<VAR_LENGTH;i++) {
            logVarNodePot[v][i] = std::log(potentials[i]);
            logVarProductIn[v][i]= 0.0;
        }
    }

    void setChkNodePotential(VertexID v, const std::vector<double> &potentials) {
        // Setting Check Node logPotentials
        // we need to offset chkNodeID by number of varNode (i.e. 1000)
        VertexID nodeID = v - numVarNodes;
        for (uint64_t i=0; i<CHK_LENGTH; i++) {
            logChkNodePot[nodeID][i] = std::log(potentials[i]);
            logChkProductIn[nodeID][i] = 0.0;
        }
    }

    void addEdge(VertexID chkID, VertexID varID) {
        messages.emplace_back(msgIDMRF++,chkID,varID,true); // forward message
        Message_CSR::Message& mFwd = messages.back();
        messages.emplace_back(msgIDMRF++,varID,chkID,false); // reverse message
        Message_CSR::Message& mRev = messages.back();

        // Index Array
        VertexID chkCnt = varIDFrom[varID]; // forward message 
        chkIDTo.at(varID)[chkCnt] = chkID;
        messageFromVar.at(varID)[chkCnt] = &mRev;
        
        chkID -= numVarNodes;
        VertexID varCnt = chkIDFrom.at(chkID); // reverse message
        varIDTo.at(chkID)[varCnt] = varID;
        messageFromChk.at(chkID)[varCnt] = &mFwd;

        messageToVar.at(varID)[chkCnt] = &mFwd;
        messageToChk.at(chkID)[varCnt] = &mRev; 
        
        // Message Potential
        std::array<double,CHK_LENGTH> logMu_CHK;
        std::fill(std::begin(logMu_CHK), std::end(logMu_CHK), std::log(1.0/CHK_LENGTH));
        uint64_t offset = chkCnt*CHK_LENGTH;
        for (uint64_t k=0; k<CHK_LENGTH; k++) {
            logMu_Var2Chk.at(varID)[offset+k] = logMu_CHK[k];
        }
        varIDFrom.at(varID)++;

        std::array<double,VAR_LENGTH> logMu_VAR;
        std::fill(std::begin(logMu_VAR), std::end(logMu_VAR), std::log(1.0/VAR_LENGTH));
        offset = varCnt*VAR_LENGTH;
        for (uint64_t k=0; k<VAR_LENGTH; k++) {
            logMu_Chk2Var.at(chkID)[offset+k] = logMu_VAR[k];
        }
        chkIDFrom.at(chkID)++;

        // Forward message (CHK --> VAR)
        assert(!logVarNodePot.at(varID).empty());
        for (uint64_t k=0; k<VAR_LENGTH;k++) {
            logVarProductIn.at(varID)[k] += logMu_VAR[k];
        }
        // Reverse message (VAR --> CHK)
        assert(!logChkNodePot.at(chkID).empty());
        for (uint64_t k=0; k<CHK_LENGTH;k++) {
            logChkProductIn.at(chkID)[k] += logMu_CHK[k];
        }
    }

    void createEdgeMask() {
        // Initialization
        for (auto& row : logEdgeMask) {
            row.fill(-std::numeric_limits<double>::infinity());
        }

        // Fill in meaningful potentials
        for (uint32_t j=0; j<CHK_DEG; j++) {
            uint32_t offset = j*2;
            for (uint32_t mask=0; mask < (1U <<CHK_DEG); mask++) {
                logEdgeMask[mask][offset+((mask >> j)&1)] = std::log(1.0);
            }
        }
    }

    // Always go from starting vertex to ending vertex
    uint64_t getChk2VarIndex(VertexID chkID, VertexID varID) const {
        const VertexID* var_connected = varIDTo[chkID].data();
        for (uint64_t edgeID=0; edgeID < CHK_DEG; edgeID++) {
            if (var_connected[edgeID] == varID) {
                return edgeID;
            }
        }
        return std::numeric_limits<int>::max();
    }

    uint64_t getVar2ChkIndex(VertexID varID, VertexID chkID) const {
        const VertexID* chk_connected = chkIDTo[varID].data();
        for (uint64_t edgeID=0; edgeID < VAR_DEG; edgeID++) {
            if (chk_connected[edgeID] == chkID) {
                return edgeID;
            }
        }
        return std::numeric_limits<int>::max();
    }

    /* Return logMu of the future message */
    std::vector<double> getFutureMessage(const Message_CSR::Message& m) const {
        VertexID fwd_start = m.chk2var ? (m.i - numVarNodes) : m.i;
        VertexID fwd_end = m.chk2var ? m.j :  (m.j - numVarNodes);
        VertexID rev_start = fwd_end;
        VertexID rev_end = fwd_start;
        const double* phi_CHK_Ptr = nullptr;
        const double* phi_VAR_Ptr = nullptr;

        uint64_t edgeID; uint64_t connectedID;
        uint64_t offset_Phi;
        uint64_t offset_EdgeMask;
        if (m.chk2var) {
            connectedID = getVar2ChkIndex(fwd_end, m.i);
            edgeID = getChk2VarIndex(fwd_start, m.j);
            offset_Phi = connectedID*CHK_LENGTH;
            offset_EdgeMask = edgeID*2;
            phi_CHK_Ptr = logMu_Var2Chk[rev_start].data();
        } else {
            connectedID = getChk2VarIndex(fwd_end, fwd_start);
            edgeID = connectedID;
            offset_Phi = connectedID*VAR_LENGTH;
            offset_EdgeMask = edgeID*2;
            phi_VAR_Ptr = logMu_Chk2Var[rev_start].data();
        }

        uint64_t out_size = m.chk2var ? VAR_LENGTH : CHK_LENGTH;
        uint64_t in_size = m.chk2var ? CHK_LENGTH : VAR_LENGTH;
        std::vector<double> outPotential(out_size);
        std::vector<double> inPotential(in_size);
        for (uint64_t valj=0; valj < out_size; valj++) {
            for (uint64_t vali=0; vali < in_size; vali++) {
                double edgeMaskVal = m.chk2var ? logEdgeMask[vali][offset_EdgeMask+valj] : logEdgeMask[valj][offset_EdgeMask+vali];
                double nodeMaskVal = m.chk2var ? logChkNodePot[fwd_start][vali] : logVarNodePot[fwd_start][vali];
                double productInVal = m.chk2var ? logChkProductIn[fwd_start][vali] : logVarProductIn[fwd_start][vali];

                if (m.chk2var) {
                    inPotential[vali] = edgeMaskVal + nodeMaskVal + productInVal - phi_CHK_Ptr[offset_Phi+vali];
                } else {
                    inPotential[vali] = edgeMaskVal + nodeMaskVal + productInVal - phi_VAR_Ptr[offset_Phi+vali];
                }
                
            }
            outPotential.at(valj) = utils_CSR::logSum(inPotential);
        }
        double logTotalSum = utils_CSR::logSum(outPotential);
        for (uint64_t valj = 0; valj < outPotential.size(); valj++) {
            outPotential.at(valj) -= logTotalSum;
        }
        return outPotential;
    }

    void getVarNodeProbabilities(std::vector<std::array<double, 2>>* answer) const {
        answer->resize(numVarNodes);
        for (uint64_t i = 0; i < numVarNodes; ++i) {
            std::array<double, 2>& a = (*answer)[i];
            for (uint64_t k = 0; k < 2; ++k) {
                a[k] = logVarNodePot[i][k] + logVarProductIn[i][k];
            }
            double sum = utils_CSR::varLogSum(a);
            for (uint64_t k = 0; k < 2; ++k) {
                a[k] = std::exp(a[k] - sum);
            }
        }
    }
    

    void getFutureMessageAndUpdate(Message_CSR::Message& m) {
        updateMessageAndNodeSum(m, getFutureMessage(m));
    }

    void updateMessageAndNodeSum(Message_CSR::Message& m, const std::vector<double>& newLogMu) {
        VertexID msg_start = m.chk2var ? (m.i - numVarNodes) : m.i; // if chk2var then we need to offset the starting node (chk node) index
        VertexID msg_end = m.chk2var ? m.j : (m.j - numVarNodes);
        
        uint64_t edgeID; uint64_t offset; double oldLogMu;
        if (m.chk2var) {
            edgeID = getChk2VarIndex(msg_start, msg_end);
            offset = edgeID*VAR_LENGTH;
            for (uint64_t k=0; k < VAR_LENGTH; k++) {
                oldLogMu = logMu_Chk2Var[msg_start][offset+k];
                logVarProductIn[msg_end][k] += -oldLogMu + newLogMu[k];
                logMu_Chk2Var[msg_start][offset+k] = newLogMu[k];
            }
        } else {
            edgeID = getVar2ChkIndex(msg_start, m.j);
            offset = edgeID*CHK_LENGTH;
            for (uint64_t k=0; k < CHK_LENGTH; k++) {
                oldLogMu = logMu_Var2Chk[msg_start][offset+k];
                logChkProductIn[msg_end][k] += -oldLogMu + newLogMu[k];
                logMu_Var2Chk[msg_start][offset+k] = newLogMu[k];
            }
        }
    }


    void copyMessage(Message_CSR::Message& m, const std::vector<double>& newLogMu) {
        VertexID msg_start = m.chk2var ? (m.i - numVarNodes) : m.i; // if chk2var then we need to offset the starting node (chk node) index
        VertexID msg_end = m.chk2var ? m.j : (m.j - numVarNodes);
        
        uint64_t edgeID; uint64_t offset;
        if (m.chk2var) {
            edgeID = getChk2VarIndex(msg_start, msg_end);
            offset = edgeID*VAR_LENGTH;
            for (uint64_t k=0; k < VAR_LENGTH; k++) {
                logMu_Chk2Var[msg_start][offset+k] = newLogMu[k];
            }
        } else {
            edgeID = getVar2ChkIndex(msg_start, m.j);
            offset = edgeID*CHK_LENGTH;
            for (uint64_t k=0; k < CHK_LENGTH; k++) {
                logMu_Var2Chk[msg_start][offset+k] = newLogMu[k];
            }
        }
    }

    int findMatchedVarIndex(const std::array<VertexID, CHK_DEG>& connectedVarIDs , VertexID varID) {
        for (int i = 0; i < CHK_DEG; ++i) {
            if (connectedVarIDs[i] == varID) {
                return i; // Return the index if a match is found
            }
        }
        return -1;
    }
    int findMatchedChkIndex(const std::array<VertexID, VAR_DEG>& connectedchkIDs , VertexID chkID) {
        for (int i = 0; i < VAR_DEG; ++i) {
            if (connectedchkIDs[i] == chkID) {
                return i; // Return the index if a match is found
            }
        }
        return -1;
    }

    void updateVarNodeSum(VertexID varID) {
        // Reset
        for (uint64_t k=0; k < logVarNodePot[varID].size(); k++) {
            logVarProductIn[varID][k]  = 0;
        }

        int chkIDConnected = std::numeric_limits<int>::max();
        int offset = -1;
        for (uint64_t i=0; i<VAR_DEG; i++) {
            chkIDConnected = chkIDTo.at(varID)[i];
            chkIDConnected -= numVarNodes;
            offset = findMatchedVarIndex(varIDTo.at(chkIDConnected),varID);
            offset = offset*VAR_LENGTH;
            for (uint64_t val=0; val < VAR_LENGTH; val++) {
                logVarProductIn[varID][val] += logMu_Chk2Var[chkIDConnected][offset+val];
            }
        }
    }
    
    void updateChkNodeSum(VertexID chkID) {
        // Reset
        for (uint64_t k=0; k < logChkNodePot[chkID].size(); k++) {
            logChkProductIn[chkID][k] = 0;
        }

        int varIDConnected = std::numeric_limits<int>::max();
        int offset = -1;
        for (uint64_t i=0; i<CHK_DEG; i++) {
            VertexID origChkID = chkID + numVarNodes;
            varIDConnected = varIDTo.at(chkID)[i];
            offset = findMatchedChkIndex(chkIDTo.at(varIDConnected),origChkID);
            offset = offset*CHK_LENGTH;
            for (uint64_t val=0; val < CHK_LENGTH; val++) {
                logChkProductIn[chkID][val] += logMu_Var2Chk[varIDConnected][offset+val];
            }
        }
    }


    /* Helper function to visualize EdgeMask */
    void printToFile(const std::string& filename) {
        std::ofstream outputFile(filename);
        if (outputFile.is_open()) {
            for (const auto& row : logEdgeMask) {
                for (const auto& element : row) {
                    outputFile << element << ' ';
                }
                outputFile << '\n';
            }
            outputFile.close();
        } else {
            std::cerr << "Unable to open file: " << filename << std::endl;
        }
    }


    inline void printArray(const double* arr, size_t len) const {
        std::cout << "[";
        for (size_t i = 0; i < len; ++i)
        {
            std::cout << arr[i];
            if (i + 1 < len) std::cout << ", ";
        }
        std::cout << "]";
    }

    void printMessagePotentials(size_t maxMsgs = 10) const
    {
        std::cout << "=== Message Potentials ===\n";

        size_t M = std::min(maxMsgs, messages.size());

        for (size_t m = 0; m < M; ++m)
        {
            std::cout << "Msg " << m << " Var→Chk ";
            printArray(logMu_Var2Chk[m].data(), PHI_CHK_LEN);
            std::cout << "\n";

            std::cout << "Msg " << m << " Chk→Var ";
            printArray(logMu_Chk2Var[m].data(), PHI_VAR_LEN);
            std::cout << "\n";
        }
    }

    void printNodePotentials(size_t maxVars = 5, size_t maxChks = 5) const
    {
        std::cout << "=== Variable Node Potentials ===\n";
        for (size_t v = 0; v < std::min(maxVars, logVarProductIn.size()); ++v)
        {
            std::cout << "Var " << v << " ";
            printArray(logVarProductIn[v].data(), VAR_LENGTH);
            std::cout << "\n";
        }

        std::cout << "=== Check Node Potentials ===\n";
        for (size_t c = 0; c < std::min(maxChks, logChkProductIn.size()); ++c)
        {
            std::cout << "Chk " << c << " ";
            printArray(logChkProductIn[c].data(), CHK_LENGTH);
            std::cout << "\n";
        }
    }



};