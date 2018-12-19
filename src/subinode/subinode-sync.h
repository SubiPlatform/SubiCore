// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2018-2019 The Subi Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef SUBINODE_SYNC_H
#define SUBINODE_SYNC_H

#include "chain.h"
#include "net.h"
#include  "utiltime.h"
#include <univalue.h>

class CSubinodeSync;

static const int SUBINODE_SYNC_FAILED          = -1;
static const int SUBINODE_SYNC_INITIAL         = 0;
static const int SUBINODE_SYNC_SPORKS          = 1;
static const int SUBINODE_SYNC_LIST            = 2;
static const int SUBINODE_SYNC_MNW             = 3;
static const int SUBINODE_SYNC_FINISHED        = 999;

static const int SUBINODE_SYNC_TICK_SECONDS    = 6;
static const int SUBINODE_SYNC_TIMEOUT_SECONDS = 30; // our blocks are 2 minutes so 30 seconds should be fine

static const int SUBINODE_SYNC_ENOUGH_PEERS    = 1;  //Mainnet PARAMS
static const int SUBINODE_SYNC_ENOUGH_PEERS_TESTNET    = 1;  //Testnet PARAMS

extern CSubinodeSync subinodeSync;

//
// CSubinodeSync : Sync subinode assets in stages
//

class CSubinodeSync
{
private:
    // Keep track of current asset
    int nRequestedSubinodeAssets;
    // Count peers we've requested the asset from
    int nRequestedSubinodeAttempt;

    // Time when current subinode asset sync started
    int64_t nTimeAssetSyncStarted;

    // Last time when we received some subinode asset ...
    int64_t nTimeLastSubinodeList;
    int64_t nTimeLastPaymentVote;
    int64_t nTimeLastGovernanceItem;
    // ... or failed
    int64_t nTimeLastFailure;

    // How many times we failed
    int nCountFailures;

    // Keep track of current block index
    const CBlockIndex *pCurrentBlockIndex;

    bool CheckNodeHeight(CNode* pnode, bool fDisconnectStuckNodes = false);
    void Fail();
    void ClearFulfilledRequests();

public:
    CSubinodeSync() { Reset(); }

    void AddedSubinodeList() { nTimeLastSubinodeList = GetTime(); }
    void AddedPaymentVote() { nTimeLastPaymentVote = GetTime(); }
    void AddedGovernanceItem() { nTimeLastGovernanceItem = GetTime(); }

    void SendGovernanceSyncRequest(CNode* pnode);

    bool IsFailed() { return nRequestedSubinodeAssets == SUBINODE_SYNC_FAILED; }
    bool IsBlockchainSynced(bool fBlockAccepted = false);
    bool IsSubinodeListSynced() { return nRequestedSubinodeAssets > SUBINODE_SYNC_LIST; }
    bool IsWinnersListSynced() { return nRequestedSubinodeAssets > SUBINODE_SYNC_MNW; }
    bool IsSynced(int nHeight) { return (nHeight >= 6) ? nRequestedSubinodeAssets == SUBINODE_SYNC_FINISHED : true; }

    int GetAssetID() { return nRequestedSubinodeAssets; }
    int GetAttempt() { return nRequestedSubinodeAttempt; }
    std::string GetAssetName();
    std::string GetSyncStatus();

    void Reset();
    void SwitchToNextAsset();

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    void ProcessTick();

    void UpdatedBlockTip(const CBlockIndex *pindex);
};

#endif
