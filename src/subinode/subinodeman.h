// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2018-2019 The Subi Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SUBINODEMAN_H
#define SUBINODEMAN_H

#include "subinode.h"
#include "sync.h"

using namespace std;

class CSubinodeMan;

extern CSubinodeMan mnodeman;

/**
 * Provides a forward and reverse index between MN vin's and integers.
 *
 * This mapping is normally add-only and is expected to be permanent
 * It is only rebuilt if the size of the index exceeds the expected maximum number
 * of MN's and the current number of known MN's.
 *
 * The external interface to this index is provided via delegation by CSubinodeMan
 */
class CSubinodeIndex
{
public: // Types
    typedef std::map<CTxIn,int> index_m_t;

    typedef index_m_t::iterator index_m_it;

    typedef index_m_t::const_iterator index_m_cit;

    typedef std::map<int,CTxIn> rindex_m_t;

    typedef rindex_m_t::iterator rindex_m_it;

    typedef rindex_m_t::const_iterator rindex_m_cit;

private:
    int                  nSize;

    index_m_t            mapIndex;

    rindex_m_t           mapReverseIndex;

public:
    CSubinodeIndex();

    int GetSize() const {
        return nSize;
    }

    /// Retrieve subinode vin by index
    bool Get(int nIndex, CTxIn& vinSubinode) const;

    /// Get index of a subinode vin
    int GetSubinodeIndex(const CTxIn& vinSubinode) const;

    void AddSubinodeVIN(const CTxIn& vinSubinode);

    void Clear();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(mapIndex);
        if(ser_action.ForRead()) {
            RebuildIndex();
        }
    }

private:
    void RebuildIndex();

};

class CSubinodeMan
{
public:
    typedef std::map<CTxIn,int> index_m_t;

    typedef index_m_t::iterator index_m_it;

    typedef index_m_t::const_iterator index_m_cit;

private:
    static const int MAX_EXPECTED_INDEX_SIZE = 30000;

    /// Only allow 1 index rebuild per hour
    static const int64_t MIN_INDEX_REBUILD_TIME = 3600;

    static const std::string SERIALIZATION_VERSION_STRING;

    static const int DSEG_UPDATE_SECONDS        = 3 * 60 * 60;

    static const int LAST_PAID_SCAN_BLOCKS      = 100;

    static const int MIN_POSE_PROTO_VERSION     = 70203;
    static const int MAX_POSE_CONNECTIONS       = 10;
    static const int MAX_POSE_RANK              = 10;
    static const int MAX_POSE_BLOCKS            = 10;

    static const int MNB_RECOVERY_QUORUM_TOTAL      = 10;
    static const int MNB_RECOVERY_QUORUM_REQUIRED   = 6;
    static const int MNB_RECOVERY_MAX_ASK_ENTRIES   = 10;
    static const int MNB_RECOVERY_WAIT_SECONDS      = 60;
    static const int MNB_RECOVERY_RETRY_SECONDS     = 3 * 60 * 60;


    // Keep track of current block index
    const CBlockIndex *pCurrentBlockIndex;

    // map to hold all MNs
    std::vector<CSubinode> vSubinodes;
    // who's asked for the Subinode list and the last time
    std::map<CNetAddr, int64_t> mAskedUsForSubinodeList;
    // who we asked for the Subinode list and the last time
    std::map<CNetAddr, int64_t> mWeAskedForSubinodeList;
    // which Subinodes we've asked for
    std::map<COutPoint, std::map<CNetAddr, int64_t> > mWeAskedForSubinodeListEntry;
    // who we asked for the subinode verification
    std::map<CNetAddr, CSubinodeVerification> mWeAskedForVerification;

    // these maps are used for subinode recovery from SUBINODE_NEW_START_REQUIRED state
    std::map<uint256, std::pair< int64_t, std::set<CNetAddr> > > mMnbRecoveryRequests;
    std::map<uint256, std::vector<CSubinodeBroadcast> > mMnbRecoveryGoodReplies;
    std::list< std::pair<CService, uint256> > listScheduledMnbRequestConnections;

    int64_t nLastIndexRebuildTime;

    CSubinodeIndex indexSubinodes;

    CSubinodeIndex indexSubinodesOld;

    /// Set when index has been rebuilt, clear when read
    bool fIndexRebuilt;

    /// Set when subinodes are added, cleared when CGovernanceManager is notified
    bool fSubinodesAdded;

    /// Set when subinodes are removed, cleared when CGovernanceManager is notified
    bool fSubinodesRemoved;

    std::vector<uint256> vecDirtyGovernanceObjectHashes;

    int64_t nLastWatchdogVoteTime;

    friend class CSubinodeSync;

public:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;
    // Keep track of all broadcasts I've seen
    std::map<uint256, std::pair<int64_t, CSubinodeBroadcast> > mapSeenSubinodeBroadcast;
    // Keep track of all pings I've seen
    std::map<uint256, CSubinodePing> mapSeenSubinodePing;
    // Keep track of all verifications I've seen
    std::map<uint256, CSubinodeVerification> mapSeenSubinodeVerification;
    // keep track of dsq count to prevent subinodes from gaming darksend queue
    int64_t nDsqCount;


    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        LOCK(cs);
        std::string strVersion;
        if(ser_action.ForRead()) {
            READWRITE(strVersion);
        }
        else {
            strVersion = SERIALIZATION_VERSION_STRING; 
            READWRITE(strVersion);
        }

        READWRITE(vSubinodes);
        READWRITE(mAskedUsForSubinodeList);
        READWRITE(mWeAskedForSubinodeList);
        READWRITE(mWeAskedForSubinodeListEntry);
        READWRITE(mMnbRecoveryRequests);
        READWRITE(mMnbRecoveryGoodReplies);
        READWRITE(nLastWatchdogVoteTime);
        READWRITE(nDsqCount);

        READWRITE(mapSeenSubinodeBroadcast);
        READWRITE(mapSeenSubinodePing);
        READWRITE(indexSubinodes);
        if(ser_action.ForRead() && (strVersion != SERIALIZATION_VERSION_STRING)) {
            Clear();
        }
    }

    CSubinodeMan();

    /// Add an entry
    bool Add(CSubinode &mn);

    /// Ask (source) node for mnb
    void AskForMN(CNode *pnode, const CTxIn &vin);
    void AskForMnb(CNode *pnode, const uint256 &hash);

    /// Check all Subinodes
    void Check();

    /// Check all Subinodes and remove inactive
    void CheckAndRemove();

    /// Clear Subinode vector
    void Clear();

    /// Count Subinodes filtered by nProtocolVersion.
    /// Subinode nProtocolVersion should match or be above the one specified in param here.
    int CountSubinodes(int nProtocolVersion = -1);
    /// Count enabled Subinodes filtered by nProtocolVersion.
    /// Subinode nProtocolVersion should match or be above the one specified in param here.
    int CountEnabled(int nProtocolVersion = -1);

    /// Count Subinodes by network type - NET_IPV4, NET_IPV6, NET_TOR
    // int CountByIP(int nNetworkType);

    void DsegUpdate(CNode* pnode);

    /// Find an entry
    CSubinode* Find(const CScript &payee);
    CSubinode* Find(const CTxIn& vin);
    CSubinode* Find(const CPubKey& pubKeySubinode);

    /// Versions of Find that are safe to use from outside the class
    bool Get(const CPubKey& pubKeySubinode, CSubinode& subinode);
    bool Get(const CTxIn& vin, CSubinode& subinode);

    /// Retrieve subinode vin by index
    bool Get(int nIndex, CTxIn& vinSubinode, bool& fIndexRebuiltOut) {
        LOCK(cs);
        fIndexRebuiltOut = fIndexRebuilt;
        return indexSubinodes.Get(nIndex, vinSubinode);
    }

    bool GetIndexRebuiltFlag() {
        LOCK(cs);
        return fIndexRebuilt;
    }

    /// Get index of a subinode vin
    int GetSubinodeIndex(const CTxIn& vinSubinode) {
        LOCK(cs);
        return indexSubinodes.GetSubinodeIndex(vinSubinode);
    }

    /// Get old index of a subinode vin
    int GetSubinodeIndexOld(const CTxIn& vinSubinode) {
        LOCK(cs);
        return indexSubinodesOld.GetSubinodeIndex(vinSubinode);
    }

    /// Get subinode VIN for an old index value
    bool GetSubinodeVinForIndexOld(int nSubinodeIndex, CTxIn& vinSubinodeOut) {
        LOCK(cs);
        return indexSubinodesOld.Get(nSubinodeIndex, vinSubinodeOut);
    }

    /// Get index of a subinode vin, returning rebuild flag
    int GetSubinodeIndex(const CTxIn& vinSubinode, bool& fIndexRebuiltOut) {
        LOCK(cs);
        fIndexRebuiltOut = fIndexRebuilt;
        return indexSubinodes.GetSubinodeIndex(vinSubinode);
    }

    void ClearOldSubinodeIndex() {
        LOCK(cs);
        indexSubinodesOld.Clear();
        fIndexRebuilt = false;
    }

    bool Has(const CTxIn& vin);

    subinode_info_t GetSubinodeInfo(const CTxIn& vin);

    subinode_info_t GetSubinodeInfo(const CPubKey& pubKeySubinode);

    char* GetNotQualifyReason(CSubinode& mn, int nBlockHeight, bool fFilterSigTime, int nMnCount);

    /// Find an entry in the subinode list that is next to be paid
    CSubinode* GetNextSubinodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount);
    /// Same as above but use current block height
    CSubinode* GetNextSubinodeInQueueForPayment(bool fFilterSigTime, int& nCount);

    /// Find a random entry
    CSubinode* FindRandomNotInVec(const std::vector<CTxIn> &vecToExclude, int nProtocolVersion = -1);

    std::vector<CSubinode> GetFullSubinodeVector() { return vSubinodes; }

    std::vector<std::pair<int, CSubinode> > GetSubinodeRanks(int nBlockHeight = -1, int nMinProtocol=0);
    int GetSubinodeRank(const CTxIn &vin, int nBlockHeight, int nMinProtocol=0, bool fOnlyActive=true);
    CSubinode* GetSubinodeByRank(int nRank, int nBlockHeight, int nMinProtocol=0, bool fOnlyActive=true);

    void ProcessSubinodeConnections();
    std::pair<CService, std::set<uint256> > PopScheduledMnbRequestConnection();

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    void DoFullVerificationStep();
    void CheckSameAddr();
    bool SendVerifyRequest(const CAddress& addr, const std::vector<CSubinode*>& vSortedByAddr);
    void SendVerifyReply(CNode* pnode, CSubinodeVerification& mnv);
    void ProcessVerifyReply(CNode* pnode, CSubinodeVerification& mnv);
    void ProcessVerifyBroadcast(CNode* pnode, const CSubinodeVerification& mnv);

    /// Return the number of (unique) Subinodes
    int size() { return vSubinodes.size(); }

    std::string ToString() const;

    /// Update subinode list and maps using provided CSubinodeBroadcast
    void UpdateSubinodeList(CSubinodeBroadcast mnb);
    /// Perform complete check and only then update list and maps
    bool CheckMnbAndUpdateSubinodeList(CNode* pfrom, CSubinodeBroadcast mnb, int& nDos);
    bool IsMnbRecoveryRequested(const uint256& hash) { return mMnbRecoveryRequests.count(hash); }

    void UpdateLastPaid();

    void CheckAndRebuildSubinodeIndex();

    void AddDirtyGovernanceObjectHash(const uint256& nHash)
    {
        LOCK(cs);
        vecDirtyGovernanceObjectHashes.push_back(nHash);
    }

    std::vector<uint256> GetAndClearDirtyGovernanceObjectHashes()
    {
        LOCK(cs);
        std::vector<uint256> vecTmp = vecDirtyGovernanceObjectHashes;
        vecDirtyGovernanceObjectHashes.clear();
        return vecTmp;;
    }

    bool IsWatchdogActive();
    void UpdateWatchdogVoteTime(const CTxIn& vin);
    bool AddGovernanceVote(const CTxIn& vin, uint256 nGovernanceObjectHash);
    void RemoveGovernanceObject(uint256 nGovernanceObjectHash);

    void CheckSubinode(const CTxIn& vin, bool fForce = false);
    void CheckSubinode(const CPubKey& pubKeySubinode, bool fForce = false);

    int GetSubinodeState(const CTxIn& vin);
    int GetSubinodeState(const CPubKey& pubKeySubinode);

    bool IsSubinodePingedWithin(const CTxIn& vin, int nSeconds, int64_t nTimeToCheckAt = -1);
    void SetSubinodeLastPing(const CTxIn& vin, const CSubinodePing& mnp);

    void UpdatedBlockTip(const CBlockIndex *pindex);

    /**
     * Called to notify CGovernanceManager that the subinode index has been updated.
     * Must be called while not holding the CSubinodeMan::cs mutex
     */
    void NotifySubinodeUpdates();
};

#endif
