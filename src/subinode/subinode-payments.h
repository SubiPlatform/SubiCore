// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2018-2019 The Subi Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SUBINODE_PAYMENTS_H
#define SUBINODE_PAYMENTS_H

#include "util.h"
#include "core_io.h"
#include "key.h"
#include "validation.h"
#include "subinode.h"
#include "utilstrencodings.h"

class CSubinodePayments;
class CSubinodePaymentVote;
class CSubinodeBlockPayees;

static const int MNPAYMENTS_SIGNATURES_REQUIRED         = 6;
static const int MNPAYMENTS_SIGNATURES_TOTAL            = 10;

//! minimum peer version that can receive and send subinode payment messages,
//  vote for subinode and be elected as a payment winner
// V1 - Last protocol version before update
// V2 - Newest protocol version
static const int MIN_SUBINODE_PAYMENT_PROTO_VERSION_1 = 70020;
static const int MIN_SUBINODE_PAYMENT_PROTO_VERSION_2 = 70021;

extern CCriticalSection cs_vecPayees;
extern CCriticalSection cs_mapSubinodeBlocks;
extern CCriticalSection cs_mapSubinodePayeeVotes;

extern CSubinodePayments mnpayments;

/// TODO: all 4 functions do not belong here really, they should be refactored/moved somewhere (main.cpp ?)
bool IsBlockValueValid(const CBlock& block, int nBlockHeight, CAmount blockReward, std::string &strErrorRet);
bool IsBlockPayeeValid(const CTransaction& txNew, int nBlockHeight, CAmount blockReward);
void FillBlockPayments(CMutableTransaction& txNew, int nBlockHeight, CAmount blockReward, CTxOut& txoutSubinodeRet, std::vector<CTxOut>& voutSuperblockRet);
std::string GetRequiredPaymentsString(int nBlockHeight);

class CSubinodePayee
{
private:
    CScript scriptPubKey;
    std::vector<uint256> vecVoteHashes;

public:
    CSubinodePayee() :
        scriptPubKey(),
        vecVoteHashes()
        {}

    CSubinodePayee(CScript payee, uint256 hashIn) :
        scriptPubKey(payee),
        vecVoteHashes()
    {
        vecVoteHashes.push_back(hashIn);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(*(CScriptBase*)(&scriptPubKey));
        READWRITE(vecVoteHashes);
    }

    CScript GetPayee() { return scriptPubKey; }

    void AddVoteHash(uint256 hashIn) { vecVoteHashes.push_back(hashIn); }
    std::vector<uint256> GetVoteHashes() { return vecVoteHashes; }
    int GetVoteCount() { return vecVoteHashes.size(); }
    std::string ToString() const;
};

// Keep track of votes for payees from subinodes
class CSubinodeBlockPayees
{
public:
    int nBlockHeight;
    std::vector<CSubinodePayee> vecPayees;

    CSubinodeBlockPayees() :
        nBlockHeight(0),
        vecPayees()
        {}
    CSubinodeBlockPayees(int nBlockHeightIn) :
        nBlockHeight(nBlockHeightIn),
        vecPayees()
        {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(nBlockHeight);
        READWRITE(vecPayees);
    }

    void AddPayee(const CSubinodePaymentVote& vote);
    bool GetBestPayee(CScript& payeeRet);
    bool HasPayeeWithVotes(CScript payeeIn, int nVotesReq);

    bool IsTransactionValid(const CTransaction& txNew);

    std::string GetRequiredPaymentsString();
};

// vote for the winning payment
class CSubinodePaymentVote
{
public:
    CTxIn vinSubinode;

    int nBlockHeight;
    CScript payee;
    std::vector<unsigned char> vchSig;

    CSubinodePaymentVote() :
        vinSubinode(),
        nBlockHeight(0),
        payee(),
        vchSig()
        {}

    CSubinodePaymentVote(CTxIn vinSubinode, int nBlockHeight, CScript payee) :
        vinSubinode(vinSubinode),
        nBlockHeight(nBlockHeight),
        payee(payee),
        vchSig()
        {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(vinSubinode);
        READWRITE(nBlockHeight);
        READWRITE(*(CScriptBase*)(&payee));
        READWRITE(vchSig);
    }

    uint256 GetHash() const {
        CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
        ss << *(CScriptBase*)(&payee);
        ss << nBlockHeight;
        ss << vinSubinode.prevout;
        return ss.GetHash();
    }

    bool Sign();
    bool CheckSignature(const CPubKey& pubKeySubinode, int nValidationHeight, int &nDos);

    bool IsValid(CNode* pnode, int nValidationHeight, std::string& strError);
    void Relay();

    bool IsVerified() { return !vchSig.empty(); }
    void MarkAsNotVerified() { vchSig.clear(); }

    std::string ToString() const;
};

//
// Subinode Payments Class
// Keeps track of who should get paid for which blocks
//

class CSubinodePayments
{
private:
    // subinode count times nStorageCoeff payments blocks should be stored ...
    const float nStorageCoeff;
    // ... but at least nMinBlocksToStore (payments blocks)
    const int nMinBlocksToStore;

    // Keep track of current block index
    const CBlockIndex *pCurrentBlockIndex;

public:
    std::map<uint256, CSubinodePaymentVote> mapSubinodePaymentVotes;
    std::map<int, CSubinodeBlockPayees> mapSubinodeBlocks;
    std::map<COutPoint, int> mapSubinodesLastVote;

    CSubinodePayments() : nStorageCoeff(1.25), nMinBlocksToStore(5000) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(mapSubinodePaymentVotes);
        READWRITE(mapSubinodeBlocks);
    }

    void Clear();

    bool AddPaymentVote(const CSubinodePaymentVote& vote);
    bool HasVerifiedPaymentVote(uint256 hashIn);
    bool ProcessBlock(int nBlockHeight);

    void Sync(CNode* node);
    void RequestLowDataPaymentBlocks(CNode* pnode);
    void CheckAndRemove();

    bool GetBlockPayee(int nBlockHeight, CScript& payee);
    bool IsTransactionValid(const CTransaction& txNew, int nBlockHeight);
    bool IsScheduled(CSubinode& mn, int nNotBlockHeight);

    bool CanVote(COutPoint outSubinode, int nBlockHeight);

    int GetMinSubinodePaymentsProto();
    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    std::string GetRequiredPaymentsString(int nBlockHeight);
    void FillBlockPayee(CMutableTransaction& txNew, int nBlockHeight, CAmount blockReward, CTxOut& txoutSubinodeRet);
    std::string ToString() const;

    int GetBlockCount() { return mapSubinodeBlocks.size(); }
    int GetVoteCount() { return mapSubinodePaymentVotes.size(); }

    bool IsEnoughData();
    int GetStorageLimit();

    void UpdatedBlockTip(const CBlockIndex *pindex);
};

#endif
