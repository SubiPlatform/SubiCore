// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2018-2019 The Subi Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activesubinode.h"
#include "addrman.h"
#include "darksend.h"
#include "subinode-payments.h"
#include "subinode-sync.h"
#include "subinodeman.h"
#include "netfulfilledman.h"
#include "util.h"
#include "netmessagemaker.h"

/** Subinode manager */
CSubinodeMan mnodeman;

const std::string CSubinodeMan::SERIALIZATION_VERSION_STRING = "CSubinodeMan-Version-4";

struct CompareLastPaidBlock
{
    bool operator()(const std::pair<int, CSubinode*>& t1,
                    const std::pair<int, CSubinode*>& t2) const
    {
        return (t1.first != t2.first) ? (t1.first < t2.first) : (t1.second->vin < t2.second->vin);
    }
};

struct CompareScoreMN
{
    bool operator()(const std::pair<int64_t, CSubinode*>& t1,
                    const std::pair<int64_t, CSubinode*>& t2) const
    {
        return (t1.first != t2.first) ? (t1.first < t2.first) : (t1.second->vin < t2.second->vin);
    }
};

CSubinodeIndex::CSubinodeIndex()
    : nSize(0),
      mapIndex(),
      mapReverseIndex()
{}

bool CSubinodeIndex::Get(int nIndex, CTxIn& vinSubinode) const
{
    rindex_m_cit it = mapReverseIndex.find(nIndex);
    if(it == mapReverseIndex.end()) {
        return false;
    }
    vinSubinode = it->second;
    return true;
}

int CSubinodeIndex::GetSubinodeIndex(const CTxIn& vinSubinode) const
{
    index_m_cit it = mapIndex.find(vinSubinode);
    if(it == mapIndex.end()) {
        return -1;
    }
    return it->second;
}

void CSubinodeIndex::AddSubinodeVIN(const CTxIn& vinSubinode)
{
    index_m_it it = mapIndex.find(vinSubinode);
    if(it != mapIndex.end()) {
        return;
    }
    int nNextIndex = nSize;
    mapIndex[vinSubinode] = nNextIndex;
    mapReverseIndex[nNextIndex] = vinSubinode;
    ++nSize;
}

void CSubinodeIndex::Clear()
{
    mapIndex.clear();
    mapReverseIndex.clear();
    nSize = 0;
}
struct CompareByAddr

{
    bool operator()(const CSubinode* t1,
                    const CSubinode* t2) const
    {
        return t1->addr < t2->addr;
    }
};

void CSubinodeIndex::RebuildIndex()
{
    nSize = mapIndex.size();
    for(index_m_it it = mapIndex.begin(); it != mapIndex.end(); ++it) {
        mapReverseIndex[it->second] = it->first;
    }
}

CSubinodeMan::CSubinodeMan() : cs(),
  vSubinodes(),
  mAskedUsForSubinodeList(),
  mWeAskedForSubinodeList(),
  mWeAskedForSubinodeListEntry(),
  mWeAskedForVerification(),
  mMnbRecoveryRequests(),
  mMnbRecoveryGoodReplies(),
  listScheduledMnbRequestConnections(),
  nLastIndexRebuildTime(0),
  indexSubinodes(),
  indexSubinodesOld(),
  fIndexRebuilt(false),
  fSubinodesAdded(false),
  fSubinodesRemoved(false),
//  vecDirtyGovernanceObjectHashes(),
  nLastWatchdogVoteTime(0),
  mapSeenSubinodeBroadcast(),
  mapSeenSubinodePing(),
  nDsqCount(0)
{}

bool CSubinodeMan::Add(CSubinode &mn)
{
    LOCK(cs);

    CSubinode *pmn = Find(mn.vin);
    if (pmn == NULL) {
        //LogPrint("subinode", "CSubinodeMan::Add -- Adding new Subinode: addr=%s, %i now\n", mn.addr.ToString(), size() + 1);
        vSubinodes.push_back(mn);
        indexSubinodes.AddSubinodeVIN(mn.vin);
        fSubinodesAdded = true;
        return true;
    }

    return false;
}

void CSubinodeMan::AskForMN(CNode* pnode, const CTxIn &vin)
{
    if(!pnode) return;

    LOCK(cs);

    std::map<COutPoint, std::map<CNetAddr, int64_t> >::iterator it1 = mWeAskedForSubinodeListEntry.find(vin.prevout);
    if (it1 != mWeAskedForSubinodeListEntry.end()) {
        std::map<CNetAddr, int64_t>::iterator it2 = it1->second.find(pnode->addr);
        if (it2 != it1->second.end()) {
            if (GetTime() < it2->second) {
                // we've asked recently, should not repeat too often or we could get banned
                return;
            }
            // we asked this node for this outpoint but it's ok to ask again already
            //LogPrint("CSubinodeMan::AskForMN -- Asking same peer %s for missing subinode entry again: %s\n", pnode->addr.ToString(), vin.prevout.ToStringShort());
        } else {
            // we already asked for this outpoint but not this node
            //LogPrint("CSubinodeMan::AskForMN -- Asking new peer %s for missing subinode entry: %s\n", pnode->addr.ToString(), vin.prevout.ToStringShort());
        }
    } else {
        // we never asked any node for this outpoint
        //LogPrint("CSubinodeMan::AskForMN -- Asking peer %s for missing subinode entry for the first time: %s\n", pnode->addr.ToString(), vin.prevout.ToStringShort());
    }
    mWeAskedForSubinodeListEntry[vin.prevout][pnode->addr] = GetTime() + DSEG_UPDATE_SECONDS;

    const CNetMsgMaker msgMaker(pnode->GetSendVersion());
    g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::DSEG,vin));
}

void CSubinodeMan::Check()
{
    LOCK(cs);

//    //LogPrint("subinode", "CSubinodeMan::Check -- nLastWatchdogVoteTime=%d, IsWatchdogActive()=%d\n", nLastWatchdogVoteTime, IsWatchdogActive());

    BOOST_FOREACH(CSubinode& mn, vSubinodes) {
        mn.Check();
    }
}

void CSubinodeMan::CheckAndRemove()
{
    if(!subinodeSync.IsSubinodeListSynced()) return;

    //LogPrint("CSubinodeMan::CheckAndRemove\n");

    {
        // Need LOCK2 here to ensure consistent locking order because code below locks cs_main
        // in CheckMnbAndUpdateSubinodeList()
        LOCK2(cs_main, cs);

        Check();

        // Remove spent subinodes, prepare structures and make requests to reasure the state of inactive ones
        std::vector<CSubinode>::iterator it = vSubinodes.begin();
        std::vector<std::pair<int, CSubinode> > vecSubinodeRanks;
        // ask for up to MNB_RECOVERY_MAX_ASK_ENTRIES subinode entries at a time
        int nAskForMnbRecovery = MNB_RECOVERY_MAX_ASK_ENTRIES;
        while(it != vSubinodes.end()) {
            CSubinodeBroadcast mnb = CSubinodeBroadcast(*it);
            uint256 hash = mnb.GetHash();
            // If collateral was spent ...
            if ((*it).IsOutpointSpent()) {
                //LogPrint("subinode", "CSubinodeMan::CheckAndRemove -- Removing Subinode: %s  addr=%s  %i now\n", (*it).GetStateString(), (*it).addr.ToString(), size() - 1);

                // erase all of the broadcasts we've seen from this txin, ...
                mapSeenSubinodeBroadcast.erase(hash);
                mWeAskedForSubinodeListEntry.erase((*it).vin.prevout);

                // and finally remove it from the list
//                it->FlagGovernanceItemsAsDirty();
                it = vSubinodes.erase(it);
                fSubinodesRemoved = true;
            } else {
                bool fAsk = pCurrentBlockIndex &&
                            (nAskForMnbRecovery > 0) &&
                            subinodeSync.IsSynced(chainActive.Height()) &&
                            it->IsNewStartRequired() &&
                            !IsMnbRecoveryRequested(hash);
                if(fAsk) {
                    // this mn is in a non-recoverable state and we haven't asked other nodes yet
                    std::set<CNetAddr> setRequested;
                    // calulate only once and only when it's needed
                    if(vecSubinodeRanks.empty()) {
                        int nRandomBlockHeight = GetRandInt(pCurrentBlockIndex->nHeight);
                        vecSubinodeRanks = GetSubinodeRanks(nRandomBlockHeight);
                    }
                    bool fAskedForMnbRecovery = false;
                    // ask first MNB_RECOVERY_QUORUM_TOTAL subinodes we can connect to and we haven't asked recently
                    for(int i = 0; setRequested.size() < MNB_RECOVERY_QUORUM_TOTAL && i < (int)vecSubinodeRanks.size(); i++) {
                        // avoid banning
                        if(mWeAskedForSubinodeListEntry.count(it->vin.prevout) && mWeAskedForSubinodeListEntry[it->vin.prevout].count(vecSubinodeRanks[i].second.addr)) continue;
                        // didn't ask recently, ok to ask now
                        CService addr = vecSubinodeRanks[i].second.addr;
                        setRequested.insert(addr);
                        listScheduledMnbRequestConnections.push_back(std::make_pair(addr, hash));
                        fAskedForMnbRecovery = true;
                    }
                    if(fAskedForMnbRecovery) {
                        //LogPrint("subinode", "CSubinodeMan::CheckAndRemove -- Recovery initiated, subinode=%s\n", it->vin.prevout.ToStringShort());
                        nAskForMnbRecovery--;
                    }
                    // wait for mnb recovery replies for MNB_RECOVERY_WAIT_SECONDS seconds
                    mMnbRecoveryRequests[hash] = std::make_pair(GetTime() + MNB_RECOVERY_WAIT_SECONDS, setRequested);
                }
                ++it;
            }
        }

        // proces replies for SUBINODE_NEW_START_REQUIRED subinodes
        //LogPrint("subinode", "CSubinodeMan::CheckAndRemove -- mMnbRecoveryGoodReplies size=%d\n", (int)mMnbRecoveryGoodReplies.size());
        std::map<uint256, std::vector<CSubinodeBroadcast> >::iterator itMnbReplies = mMnbRecoveryGoodReplies.begin();
        while(itMnbReplies != mMnbRecoveryGoodReplies.end()){
            if(mMnbRecoveryRequests[itMnbReplies->first].first < GetTime()) {
                // all nodes we asked should have replied now
                if(itMnbReplies->second.size() >= MNB_RECOVERY_QUORUM_REQUIRED) {
                    // majority of nodes we asked agrees that this mn doesn't require new mnb, reprocess one of new mnbs
                    //LogPrint("subinode", "CSubinodeMan::CheckAndRemove -- reprocessing mnb, subinode=%s\n", itMnbReplies->second[0].vin.prevout.ToStringShort());
                    // mapSeenSubinodeBroadcast.erase(itMnbReplies->first);
                    int nDos;
                    itMnbReplies->second[0].fRecovery = true;
                    CheckMnbAndUpdateSubinodeList(NULL, itMnbReplies->second[0], nDos);
                }
                //LogPrint("subinode", "CSubinodeMan::CheckAndRemove -- removing mnb recovery reply, subinode=%s, size=%d\n", itMnbReplies->second[0].vin.prevout.ToStringShort(), (int)itMnbReplies->second.size());
                mMnbRecoveryGoodReplies.erase(itMnbReplies++);
            } else {
                ++itMnbReplies;
            }
        }
    }
    {
        // no need for cm_main below
        LOCK(cs);

        std::map<uint256, std::pair< int64_t, std::set<CNetAddr> > >::iterator itMnbRequest = mMnbRecoveryRequests.begin();
        while(itMnbRequest != mMnbRecoveryRequests.end()){
            // Allow this mnb to be re-verified again after MNB_RECOVERY_RETRY_SECONDS seconds
            // if mn is still in SUBINODE_NEW_START_REQUIRED state.
            if(GetTime() - itMnbRequest->second.first > MNB_RECOVERY_RETRY_SECONDS) {
                mMnbRecoveryRequests.erase(itMnbRequest++);
            } else {
                ++itMnbRequest;
            }
        }

        // check who's asked for the Subinode list
        std::map<CNetAddr, int64_t>::iterator it1 = mAskedUsForSubinodeList.begin();
        while(it1 != mAskedUsForSubinodeList.end()){
            if((*it1).second < GetTime()) {
                mAskedUsForSubinodeList.erase(it1++);
            } else {
                ++it1;
            }
        }

        // check who we asked for the Subinode list
        it1 = mWeAskedForSubinodeList.begin();
        while(it1 != mWeAskedForSubinodeList.end()){
            if((*it1).second < GetTime()){
                mWeAskedForSubinodeList.erase(it1++);
            } else {
                ++it1;
            }
        }

        // check which Subinodes we've asked for
        std::map<COutPoint, std::map<CNetAddr, int64_t> >::iterator it2 = mWeAskedForSubinodeListEntry.begin();
        while(it2 != mWeAskedForSubinodeListEntry.end()){
            std::map<CNetAddr, int64_t>::iterator it3 = it2->second.begin();
            while(it3 != it2->second.end()){
                if(it3->second < GetTime()){
                    it2->second.erase(it3++);
                } else {
                    ++it3;
                }
            }
            if(it2->second.empty()) {
                mWeAskedForSubinodeListEntry.erase(it2++);
            } else {
                ++it2;
            }
        }

        std::map<CNetAddr, CSubinodeVerification>::iterator it3 = mWeAskedForVerification.begin();
        while(it3 != mWeAskedForVerification.end()){
            if(it3->second.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS) {
                mWeAskedForVerification.erase(it3++);
            } else {
                ++it3;
            }
        }

        // NOTE: do not expire mapSeenSubinodeBroadcast entries here, clean them on mnb updates!

        // remove expired mapSeenSubinodePing
        std::map<uint256, CSubinodePing>::iterator it4 = mapSeenSubinodePing.begin();
        while(it4 != mapSeenSubinodePing.end()){
            if((*it4).second.IsExpired()) {
                //LogPrint("subinode", "CSubinodeMan::CheckAndRemove -- Removing expired Subinode ping: hash=%s\n", (*it4).second.GetHash().ToString());
                mapSeenSubinodePing.erase(it4++);
            } else {
                ++it4;
            }
        }

        // remove expired mapSeenSubinodeVerification
        std::map<uint256, CSubinodeVerification>::iterator itv2 = mapSeenSubinodeVerification.begin();
        while(itv2 != mapSeenSubinodeVerification.end()){
            if((*itv2).second.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS){
                //LogPrint("subinode", "CSubinodeMan::CheckAndRemove -- Removing expired Subinode verification: hash=%s\n", (*itv2).first.ToString());
                mapSeenSubinodeVerification.erase(itv2++);
            } else {
                ++itv2;
            }
        }

        //LogPrint("CSubinodeMan::CheckAndRemove -- %s\n", ToString());

        if(fSubinodesRemoved) {
            CheckAndRebuildSubinodeIndex();
        }
    }

    if(fSubinodesRemoved) {
        NotifySubinodeUpdates();
    }
}

void CSubinodeMan::Clear()
{
    LOCK(cs);
    vSubinodes.clear();
    mAskedUsForSubinodeList.clear();
    mWeAskedForSubinodeList.clear();
    mWeAskedForSubinodeListEntry.clear();
    mapSeenSubinodeBroadcast.clear();
    mapSeenSubinodePing.clear();
    nDsqCount = 0;
    nLastWatchdogVoteTime = 0;
    indexSubinodes.Clear();
    indexSubinodesOld.Clear();
}

int CSubinodeMan::CountSubinodes(int nProtocolVersion)
{
    LOCK(cs);
    int nCount = 0;
    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinSubinodePaymentsProto() : nProtocolVersion;

    BOOST_FOREACH(CSubinode& mn, vSubinodes) {
        if(mn.nProtocolVersion < nProtocolVersion) continue;
        nCount++;
    }

    return nCount;
}

int CSubinodeMan::CountEnabled(int nProtocolVersion)
{
    LOCK(cs);
    int nCount = 0;
    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinSubinodePaymentsProto() : nProtocolVersion;

    BOOST_FOREACH(CSubinode& mn, vSubinodes) {
        if(mn.nProtocolVersion < nProtocolVersion || !mn.IsEnabled()) continue;
        nCount++;
    }

    return nCount;
}

/* Only IPv4 subinodes are allowed in 12.1, saving this for later
int CSubinodeMan::CountByIP(int nNetworkType)
{
    LOCK(cs);
    int nNodeCount = 0;

    BOOST_FOREACH(CSubinode& mn, vSubinodes)
        if ((nNetworkType == NET_IPV4 && mn.addr.IsIPv4()) ||
            (nNetworkType == NET_TOR  && mn.addr.IsTor())  ||
            (nNetworkType == NET_IPV6 && mn.addr.IsIPv6())) {
                nNodeCount++;
        }

    return nNodeCount;
}
*/

void CSubinodeMan::DsegUpdate(CNode* pnode)
{
    LOCK(cs);

    if(Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if(!(pnode->addr.IsRFC1918() || pnode->addr.IsLocal())) {
            std::map<CNetAddr, int64_t>::iterator it = mWeAskedForSubinodeList.find(pnode->addr);
            if(it != mWeAskedForSubinodeList.end() && GetTime() < (*it).second) {
                //LogPrint("CSubinodeMan::DsegUpdate -- we already asked %s for the list; skipping...\n", pnode->addr.ToString());
                return;
            }
        }
    }
    
    const CNetMsgMaker msgMaker(pnode->GetSendVersion());
    g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::DSEG, CTxIn()));
    int64_t askAgain = GetTime() + DSEG_UPDATE_SECONDS;
    mWeAskedForSubinodeList[pnode->addr] = askAgain;

    //LogPrint("subinode", "CSubinodeMan::DsegUpdate -- asked %s for the list\n", pnode->addr.ToString());
}

CSubinode* CSubinodeMan::Find(const CScript &payee)
{
    LOCK(cs);

    BOOST_FOREACH(CSubinode& mn, vSubinodes)
    {
        if(GetScriptForDestination(mn.pubKeyCollateralAddress.GetID()) == payee)
            return &mn;
    }
    return NULL;
}

CSubinode* CSubinodeMan::Find(const CTxIn &vin)
{
    LOCK(cs);

    BOOST_FOREACH(CSubinode& mn, vSubinodes)
    {
        if(mn.vin.prevout == vin.prevout)
            return &mn;
    }
    return NULL;
}

CSubinode* CSubinodeMan::Find(const CPubKey &pubKeySubinode)
{
    LOCK(cs);

    BOOST_FOREACH(CSubinode& mn, vSubinodes)
    {
        if(mn.pubKeySubinode == pubKeySubinode)
            return &mn;
    }
    return NULL;
}

bool CSubinodeMan::Get(const CPubKey& pubKeySubinode, CSubinode& subinode)
{
    // Theses mutexes are recursive so double locking by the same thread is safe.
    LOCK(cs);
    CSubinode* pMN = Find(pubKeySubinode);
    if(!pMN)  {
        return false;
    }
    subinode = *pMN;
    return true;
}

bool CSubinodeMan::Get(const CTxIn& vin, CSubinode& subinode)
{
    // Theses mutexes are recursive so double locking by the same thread is safe.
    LOCK(cs);
    CSubinode* pMN = Find(vin);
    if(!pMN)  {
        return false;
    }
    subinode = *pMN;
    return true;
}

subinode_info_t CSubinodeMan::GetSubinodeInfo(const CTxIn& vin)
{
    subinode_info_t info;
    LOCK(cs);
    CSubinode* pMN = Find(vin);
    if(!pMN)  {
        return info;
    }
    info = pMN->GetInfo();
    return info;
}

subinode_info_t CSubinodeMan::GetSubinodeInfo(const CPubKey& pubKeySubinode)
{
    subinode_info_t info;
    LOCK(cs);
    CSubinode* pMN = Find(pubKeySubinode);
    if(!pMN)  {
        return info;
    }
    info = pMN->GetInfo();
    return info;
}

bool CSubinodeMan::Has(const CTxIn& vin)
{
    LOCK(cs);
    CSubinode* pMN = Find(vin);
    return (pMN != NULL);
}

char* CSubinodeMan::GetNotQualifyReason(CSubinode& mn, int nBlockHeight, bool fFilterSigTime, int nMnCount)
{
    if (!mn.IsValidForPayment()) {
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'not valid for payment'");
        return reasonStr;
    }
    // //check protocol version
    if (mn.nProtocolVersion < mnpayments.GetMinSubinodePaymentsProto()) {
        // //LogPrint("Invalid nProtocolVersion!\n");
        // //LogPrint("mn.nProtocolVersion=%s!\n", mn.nProtocolVersion);
        // //LogPrint("mnpayments.GetMinSubinodePaymentsProto=%s!\n", mnpayments.GetMinSubinodePaymentsProto());
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'Invalid nProtocolVersion', nProtocolVersion=%d", mn.nProtocolVersion);
        return reasonStr;
    }
    //it's in the list (up to 8 entries ahead of current block to allow propagation) -- so let's skip it
    if (mnpayments.IsScheduled(mn, nBlockHeight)) {
        // //LogPrint("mnpayments.IsScheduled!\n");
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'is scheduled'");
        return reasonStr;
    }
    //it's too new, wait for a cycle
    if (fFilterSigTime && mn.sigTime + (nMnCount * 2.6 * 60) > GetAdjustedTime()) {
        // //LogPrint("it's too new, wait for a cycle!\n");
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'too new', sigTime=%s, will be qualifed after=%s",
                DateTimeStrFormat("%Y-%m-%d %H:%M UTC", mn.sigTime).c_str(), DateTimeStrFormat("%Y-%m-%d %H:%M UTC", mn.sigTime + (nMnCount * 2.6 * 60)).c_str());
        return reasonStr;
    }
    //make sure it has at least as many confirmations as there are subinodes
    if (mn.GetCollateralAge() < nMnCount) {
        // //LogPrint("mn.GetCollateralAge()=%s!\n", mn.GetCollateralAge());
        // //LogPrint("nMnCount=%s!\n", nMnCount);
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'collateralAge < znCount', collateralAge=%d, znCount=%d", mn.GetCollateralAge(), nMnCount);
        return reasonStr;
    }
    return NULL;
}

//
// Deterministically select the oldest/best subinode to pay on the network
//
CSubinode* CSubinodeMan::GetNextSubinodeInQueueForPayment(bool fFilterSigTime, int& nCount)
{
    if(!pCurrentBlockIndex) {
        nCount = 0;
        return NULL;
    }
    return GetNextSubinodeInQueueForPayment(pCurrentBlockIndex->nHeight, fFilterSigTime, nCount);
}

CSubinode* CSubinodeMan::GetNextSubinodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount)
{
    // Need LOCK2 here to ensure consistent locking order because the GetBlockHash call below locks cs_main
    LOCK2(cs_main,cs);

    CSubinode *pBestSubinode = NULL;
    std::vector<std::pair<int, CSubinode*> > vecSubinodeLastPaid;

    /*
        Make a vector with all of the last paid times
    */
    //LogPrintf("\nSubinode InQueueForPayment \n");
    int nMnCount = CountEnabled();
    int index = 0;
    BOOST_FOREACH(CSubinode &mn, vSubinodes)
    {
        index += 1;

        char* reasonStr = GetNotQualifyReason(mn, nBlockHeight, fFilterSigTime, nMnCount);
        if (reasonStr != NULL) {
            //LogPrint("subinodeman", "Subinode, %s, addr(%s), qualify %s\n",
                     //mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString(), reasonStr);
            delete [] reasonStr;
            continue;
        }
        //LogPrintf("\nNODE Last Paid\n");
        vecSubinodeLastPaid.push_back(std::make_pair(mn.GetLastPaidBlock(), &mn));
    }
    nCount = (int)vecSubinodeLastPaid.size();

    //when the network is in the process of upgrading, don't penalize nodes that recently restarted
    if(fFilterSigTime && nCount < nMnCount / 3) {
        LogPrintf("Need Return, nCount=%s, nMnCount/3=%s\n", nCount, nMnCount/3);
        return GetNextSubinodeInQueueForPayment(nBlockHeight, false, nCount);
    }

    // Sort them low to high
    sort(vecSubinodeLastPaid.begin(), vecSubinodeLastPaid.end(), CompareLastPaidBlock());

    uint256 blockHash;
    if(!GetBlockHash(blockHash, nBlockHeight - 100)) {
        LogPrintf("CSubinode::GetNextSubinodeInQueueForPayment -- ERROR: GetBlockHash() failed at nBlockHeight %d\n", (nBlockHeight - 100));
        return NULL;
    }
    // Look at 1/10 of the oldest nodes (by last payment), calculate their scores and pay the best one
    //  -- This doesn't look at who is being paid in the +8-10 blocks, allowing for double payments very rarely
    //  -- 1/100 payments should be a double payment on mainnet - (1/(3000/10))*2
    //  -- (chance per block * chances before IsScheduled will fire)
    int nTenthNetwork = nMnCount/10;
    int nCountTenth = 0;
    arith_uint256 nHighest = 0;
    BOOST_FOREACH (PAIRTYPE(int, CSubinode*)& s, vecSubinodeLastPaid){
        arith_uint256 nScore = s.second->CalculateScore(blockHash);
        if(nScore > nHighest){
            nHighest = nScore;
            pBestSubinode = s.second;
        }
        nCountTenth++;
        if(nCountTenth >= nTenthNetwork) break;
    }
    return pBestSubinode;
}

CSubinode* CSubinodeMan::FindRandomNotInVec(const std::vector<CTxIn> &vecToExclude, int nProtocolVersion)
{
    LOCK(cs);

    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinSubinodePaymentsProto() : nProtocolVersion;

    int nCountEnabled = CountEnabled(nProtocolVersion);
    int nCountNotExcluded = nCountEnabled - vecToExclude.size();

    //LogPrint("CSubinodeMan::FindRandomNotInVec -- %d enabled subinodes, %d subinodes to choose from\n", nCountEnabled, nCountNotExcluded);
    if(nCountNotExcluded < 1) return NULL;

    // fill a vector of pointers
    std::vector<CSubinode*> vpSubinodesShuffled;
    BOOST_FOREACH(CSubinode &mn, vSubinodes) {
        vpSubinodesShuffled.push_back(&mn);
    }

    InsecureRand insecureRand;
    // shuffle pointers
    std::random_shuffle(vpSubinodesShuffled.begin(), vpSubinodesShuffled.end(), insecureRand);
    bool fExclude;

    // loop through
    BOOST_FOREACH(CSubinode* pmn, vpSubinodesShuffled) {
        if(pmn->nProtocolVersion < nProtocolVersion || !pmn->IsEnabled()) continue;
        fExclude = false;
        BOOST_FOREACH(const CTxIn &txinToExclude, vecToExclude) {
            if(pmn->vin.prevout == txinToExclude.prevout) {
                fExclude = true;
                break;
            }
        }
        if(fExclude) continue;
        // found the one not in vecToExclude
        //LogPrint("subinode", "CSubinodeMan::FindRandomNotInVec -- found, subinode=%s\n", pmn->vin.prevout.ToStringShort());
        return pmn;
    }

    //LogPrint("subinode", "CSubinodeMan::FindRandomNotInVec -- failed\n");
    return NULL;
}

int CSubinodeMan::GetSubinodeRank(const CTxIn& vin, int nBlockHeight, int nMinProtocol, bool fOnlyActive)
{
    std::vector<std::pair<int64_t, CSubinode*> > vecSubinodeScores;

    //make sure we know about this block
    uint256 blockHash = uint256();
    if(!GetBlockHash(blockHash, nBlockHeight)) return -1;

    LOCK(cs);

    // scan for winner
    BOOST_FOREACH(CSubinode& mn, vSubinodes) {
        if(mn.nProtocolVersion < nMinProtocol) continue;
        if(fOnlyActive) {
            if(!mn.IsEnabled()) continue;
        }
        else {
            if(!mn.IsValidForPayment()) continue;
        }
        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecSubinodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecSubinodeScores.rbegin(), vecSubinodeScores.rend(), CompareScoreMN());

    int nRank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CSubinode*)& scorePair, vecSubinodeScores) {
        nRank++;
        if(scorePair.second->vin.prevout == vin.prevout) return nRank;
    }

    return -1;
}

std::vector<std::pair<int, CSubinode> > CSubinodeMan::GetSubinodeRanks(int nBlockHeight, int nMinProtocol)
{
    std::vector<std::pair<int64_t, CSubinode*> > vecSubinodeScores;
    std::vector<std::pair<int, CSubinode> > vecSubinodeRanks;

    //make sure we know about this block
    uint256 blockHash = uint256();
    if(!GetBlockHash(blockHash, nBlockHeight)) return vecSubinodeRanks;

    LOCK(cs);

    // scan for winner
    BOOST_FOREACH(CSubinode& mn, vSubinodes) {

        if(mn.nProtocolVersion < nMinProtocol || !mn.IsEnabled()) continue;

        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecSubinodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecSubinodeScores.rbegin(), vecSubinodeScores.rend(), CompareScoreMN());

    int nRank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CSubinode*)& s, vecSubinodeScores) {
        nRank++;
        vecSubinodeRanks.push_back(std::make_pair(nRank, *s.second));
    }

    return vecSubinodeRanks;
}

CSubinode* CSubinodeMan::GetSubinodeByRank(int nRank, int nBlockHeight, int nMinProtocol, bool fOnlyActive)
{
    std::vector<std::pair<int64_t, CSubinode*> > vecSubinodeScores;

    LOCK(cs);

    uint256 blockHash;
    if(!GetBlockHash(blockHash, nBlockHeight)) {
        //LogPrint("CSubinode::GetSubinodeByRank -- ERROR: GetBlockHash() failed at nBlockHeight %d\n", nBlockHeight);
        return NULL;
    }

    // Fill scores
    BOOST_FOREACH(CSubinode& mn, vSubinodes) {

        if(mn.nProtocolVersion < nMinProtocol) continue;
        if(fOnlyActive && !mn.IsEnabled()) continue;

        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecSubinodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecSubinodeScores.rbegin(), vecSubinodeScores.rend(), CompareScoreMN());

    int rank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CSubinode*)& s, vecSubinodeScores){
        rank++;
        if(rank == nRank) {
            return s.second;
        }
    }

    return NULL;
}

void CSubinodeMan::ProcessSubinodeConnections()
{
    //we don't care about this for regtest
    if(Params().NetworkIDString() == CBaseChainParams::REGTEST) return;

    g_connman->ForEachNode([](CNode* pnode){
        if(!(darkSendPool.pSubmittedToSubinode != NULL && pnode->addr == darkSendPool.pSubmittedToSubinode->addr))
            if(pnode->fSubinode) {
                // //LogPrint("Closing Subinode connection: peer=%d, addr=%s\n", pnode->GetId(), pnode->addr.ToString());
                pnode->fDisconnect = true;
            }
    });
    /*
    BOOST_FOREACH(CNode* pnode, g_connman->vNodes) {
        if(pnode->fSubinode) {
            if(darkSendPool.pSubmittedToSubinode != NULL && pnode->addr == darkSendPool.pSubmittedToSubinode->addr) continue;
            // //LogPrint("Closing Subinode connection: peer=%d, addr=%s\n", pnode->GetId(), pnode->addr.ToString());
            pnode->fDisconnect = true;
        }
    }
    */
}

std::pair<CService, std::set<uint256> > CSubinodeMan::PopScheduledMnbRequestConnection()
{
    LOCK(cs);
    if(listScheduledMnbRequestConnections.empty()) {
        return std::make_pair(CService(), std::set<uint256>());
    }

    std::set<uint256> setResult;

    listScheduledMnbRequestConnections.sort();
    std::pair<CService, uint256> pairFront = listScheduledMnbRequestConnections.front();

    // squash hashes from requests with the same CService as the first one into setResult
    std::list< std::pair<CService, uint256> >::iterator it = listScheduledMnbRequestConnections.begin();
    while(it != listScheduledMnbRequestConnections.end()) {
        if(pairFront.first == it->first) {
            setResult.insert(it->second);
            it = listScheduledMnbRequestConnections.erase(it);
        } else {
            // since list is sorted now, we can be sure that there is no more hashes left
            // to ask for from this addr
            break;
        }
    }
    return std::make_pair(pairFront.first, setResult);
}


void CSubinodeMan::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{

//    //LogPrint("subinode", "CSubinodeMan::ProcessMessage, strCommand=%s\n", strCommand);
    if(fLiteMode) return; // disable all Dash specific functionality
    if(!subinodeSync.IsBlockchainSynced()) return;

    if (strCommand == NetMsgType::MNANNOUNCE) { //Subinode Broadcast
        CSubinodeBroadcast mnb;
        vRecv >> mnb;

        pfrom->setAskFor.erase(mnb.GetHash());

        //LogPrint("MNANNOUNCE -- Subinode announce, subinode=%s\n", mnb.vin.prevout.ToStringShort());

        int nDos = 0;

        if (CheckMnbAndUpdateSubinodeList(pfrom, mnb, nDos)) {
            // use announced Subinode as a peer
            g_connman->addrman.Add(CAddress(mnb.addr, NODE_NETWORK), pfrom->addr, 2*60*60);
        } else if(nDos > 0) {
            Misbehaving(pfrom->GetId(), nDos);
        }

        if(fSubinodesAdded) {
            NotifySubinodeUpdates();
        }
    } else if (strCommand == NetMsgType::MNPING) { //Subinode Ping

        CSubinodePing mnp;
        vRecv >> mnp;

        uint256 nHash = mnp.GetHash();

        pfrom->setAskFor.erase(nHash);

        //LogPrint("subinode", "MNPING -- Subinode ping, subinode=%s\n", mnp.vin.prevout.ToStringShort());

        // Need LOCK2 here to ensure consistent locking order because the CheckAndUpdate call below locks cs_main
        LOCK2(cs_main, cs);

        if(mapSeenSubinodePing.count(nHash)) return; //seen
        mapSeenSubinodePing.insert(std::make_pair(nHash, mnp));

        //LogPrint("subinode", "MNPING -- Subinode ping, subinode=%s new\n", mnp.vin.prevout.ToStringShort());

        // see if we have this Subinode
        CSubinode* pmn = mnodeman.Find(mnp.vin);

        // too late, new MNANNOUNCE is required
        if(pmn && pmn->IsNewStartRequired()) return;

        int nDos = 0;
        if(mnp.CheckAndUpdate(pmn, false, nDos)) return;

        if(nDos > 0) {
            // if anything significant failed, mark that node
            Misbehaving(pfrom->GetId(), nDos);
        } else if(pmn != NULL) {
            // nothing significant failed, mn is a known one too
            return;
        }

        // something significant is broken or mn is unknown,
        // we might have to ask for a subinode entry once
        AskForMN(pfrom, mnp.vin);

    } else if (strCommand == NetMsgType::DSEG) { //Get Subinode list or specific entry
        // Ignore such requests until we are fully synced.
        // We could start processing this after subinode list is synced
        // but this is a heavy one so it's better to finish sync first.
        if (!subinodeSync.IsSynced(chainActive.Height())) return;

        CTxIn vin;
        vRecv >> vin;

        //LogPrint("subinode", "DSEG -- Subinode list, subinode=%s\n", vin.prevout.ToStringShort());

        LOCK(cs);

        if(vin == CTxIn()) { //only should ask for this once
            //local network
            bool isLocal = (pfrom->addr.IsRFC1918() || pfrom->addr.IsLocal());

            if(!isLocal && Params().NetworkIDString() == CBaseChainParams::MAIN) {
                std::map<CNetAddr, int64_t>::iterator i = mAskedUsForSubinodeList.find(pfrom->addr);
                if (i != mAskedUsForSubinodeList.end()){
                    int64_t t = (*i).second;
                    if (GetTime() < t) {
                        Misbehaving(pfrom->GetId(), 34);
                        //LogPrint("DSEG -- peer already asked me for the list, peer=%d\n", pfrom->GetId());
                        return;
                    }
                }
                int64_t askAgain = GetTime() + DSEG_UPDATE_SECONDS;
                mAskedUsForSubinodeList[pfrom->addr] = askAgain;
            }
        } //else, asking for a specific node which is ok

        int nInvCount = 0;

        BOOST_FOREACH(CSubinode& mn, vSubinodes) {
            if (vin != CTxIn() && vin != mn.vin) continue; // asked for specific vin but we are not there yet
            if (mn.addr.IsRFC1918() || mn.addr.IsLocal()) continue; // do not send local network subinode
            if (mn.IsUpdateRequired()) continue; // do not send outdated subinodes

            //LogPrint("subinode", "DSEG -- Sending Subinode entry: subinode=%s  addr=%s\n", mn.vin.prevout.ToStringShort(), mn.addr.ToString());
            CSubinodeBroadcast mnb = CSubinodeBroadcast(mn);
            uint256 hash = mnb.GetHash();
            pfrom->PushInventory(CInv(MSG_SUBINODE_ANNOUNCE, hash));
            pfrom->PushInventory(CInv(MSG_SUBINODE_PING, mn.lastPing.GetHash()));
            nInvCount++;

            if (!mapSeenSubinodeBroadcast.count(hash)) {
                mapSeenSubinodeBroadcast.insert(std::make_pair(hash, std::make_pair(GetTime(), mnb)));
            }

            if (vin == mn.vin) {
                //LogPrint("DSEG -- Sent 1 Subinode inv to peer %d\n", pfrom->GetId());
                return;
            }
        }

        if(vin == CTxIn()) {
            const CNetMsgMaker msgMaker(pfrom->GetSendVersion());
            g_connman->PushMessage(pfrom, msgMaker.Make(NetMsgType::SYNCSTATUSCOUNT, SUBINODE_SYNC_LIST, nInvCount));
            //LogPrint("DSEG -- Sent %d Subinode invs to peer %d\n", nInvCount, pfrom->GetId());
            return;
        }
        // smth weird happen - someone asked us for vin we have no idea about?
        //LogPrint("subinode", "DSEG -- No invs sent to peer %d\n", pfrom->GetId());

    } else if (strCommand == NetMsgType::MNVERIFY) { // Subinode Verify

        // Need LOCK2 here to ensure consistent locking order because the all functions below call GetBlockHash which locks cs_main
        LOCK2(cs_main, cs);

        CSubinodeVerification mnv;
        vRecv >> mnv;

        if(mnv.vchSig1.empty()) {
            // CASE 1: someone asked me to verify myself /IP we are using/
            SendVerifyReply(pfrom, mnv);
        } else if (mnv.vchSig2.empty()) {
            // CASE 2: we _probably_ got verification we requested from some subinode
            ProcessVerifyReply(pfrom, mnv);
        } else {
            // CASE 3: we _probably_ got verification broadcast signed by some subinode which verified another one
            ProcessVerifyBroadcast(pfrom, mnv);
        }
    }
}

// Verification of subinodes via unique direct requests.

void CSubinodeMan::DoFullVerificationStep()
{
    if(activeSubinode.vin == CTxIn()) return;
    if(!subinodeSync.IsSynced(chainActive.Height())) return;

    std::vector<std::pair<int, CSubinode> > vecSubinodeRanks = GetSubinodeRanks(pCurrentBlockIndex->nHeight - 1, MIN_POSE_PROTO_VERSION);

    {
    LOCK(cs);

    int nCount = 0;

    int nMyRank = -1;
    int nRanksTotal = (int)vecSubinodeRanks.size();

    // send verify requests only if we are in top MAX_POSE_RANK
    std::vector<std::pair<int, CSubinode> >::iterator it = vecSubinodeRanks.begin();
    while(it != vecSubinodeRanks.end()) {
        if(it->first > MAX_POSE_RANK) {
            //LogPrint("subinode", "CSubinodeMan::DoFullVerificationStep -- Must be in top %d to send verify request\n",
                    //    (int)MAX_POSE_RANK);
            return;
        }
        if(it->second.vin == activeSubinode.vin) {
            nMyRank = it->first;
            //LogPrint("subinode", "CSubinodeMan::DoFullVerificationStep -- Found self at rank %d/%d, verifying up to %d subinodes\n",
                    //    nMyRank, nRanksTotal, (int)MAX_POSE_CONNECTIONS);
            break;
        }
        ++it;
    }

    // edge case: list is too short and this subinode is not enabled
    if(nMyRank == -1) return;

    // send verify requests to up to MAX_POSE_CONNECTIONS subinodes
    // starting from MAX_POSE_RANK + nMyRank and using MAX_POSE_CONNECTIONS as a step
    int nOffset = MAX_POSE_RANK + nMyRank - 1;
    if(nOffset >= (int)vecSubinodeRanks.size()) return;

    std::vector<CSubinode*> vSortedByAddr;
    BOOST_FOREACH(CSubinode& mn, vSubinodes) {
        vSortedByAddr.push_back(&mn);
    }

    sort(vSortedByAddr.begin(), vSortedByAddr.end(), CompareByAddr());

    it = vecSubinodeRanks.begin() + nOffset;
    while(it != vecSubinodeRanks.end()) {
        if(it->second.IsPoSeVerified() || it->second.IsPoSeBanned()) {
            //LogPrint("subinode", "CSubinodeMan::DoFullVerificationStep -- Already %s%s%s subinode %s address %s, skipping...\n",
//                        it->second.IsPoSeVerified() ? "verified" : "",
//                        it->second.IsPoSeVerified() && it->second.IsPoSeBanned() ? " and " : "",
//                        it->second.IsPoSeBanned() ? "banned" : "",
//                        it->second.vin.prevout.ToStringShort(), it->second.addr.ToString());
            nOffset += MAX_POSE_CONNECTIONS;
            if(nOffset >= (int)vecSubinodeRanks.size()) break;
            it += MAX_POSE_CONNECTIONS;
            continue;
        }
        //LogPrint("subinode", "CSubinodeMan::DoFullVerificationStep -- Verifying subinode %s rank %d/%d address %s\n",
                 //   it->second.vin.prevout.ToStringShort(), it->first, nRanksTotal, it->second.addr.ToString());
        if(SendVerifyRequest(CAddress(it->second.addr, NODE_NETWORK), vSortedByAddr)) {
            nCount++;
            if(nCount >= MAX_POSE_CONNECTIONS) break;
        }
        nOffset += MAX_POSE_CONNECTIONS;
        if(nOffset >= (int)vecSubinodeRanks.size()) break;
        it += MAX_POSE_CONNECTIONS;
    }
    }
    //LogPrint("subinode", "CSubinodeMan::DoFullVerificationStep -- Sent verification requests to %d subinodes\n", nCount);
}

// This function tries to find subinodes with the same addr,
// find a verified one and ban all the other. If there are many nodes
// with the same addr but none of them is verified yet, then none of them are banned.
// It could take many times to run this before most of the duplicate nodes are banned.

void CSubinodeMan::CheckSameAddr()
{
    if(!subinodeSync.IsSynced(chainActive.Height()) || vSubinodes.empty()) return;

    std::vector<CSubinode*> vBan;
    std::vector<CSubinode*> vSortedByAddr;

    {
        LOCK(cs);

        CSubinode* pprevSubinode = NULL;
        CSubinode* pverifiedSubinode = NULL;

        BOOST_FOREACH(CSubinode& mn, vSubinodes) {
            vSortedByAddr.push_back(&mn);
        }

        sort(vSortedByAddr.begin(), vSortedByAddr.end(), CompareByAddr());

        BOOST_FOREACH(CSubinode* pmn, vSortedByAddr) {
            // check only (pre)enabled subinodes
            if(!pmn->IsEnabled() && !pmn->IsPreEnabled()) continue;
            // initial step
            if(!pprevSubinode) {
                pprevSubinode = pmn;
                pverifiedSubinode = pmn->IsPoSeVerified() ? pmn : NULL;
                continue;
            }
            // second+ step
            if(pmn->addr == pprevSubinode->addr) {
                if(pverifiedSubinode) {
                    // another subinode with the same ip is verified, ban this one
                    vBan.push_back(pmn);
                } else if(pmn->IsPoSeVerified()) {
                    // this subinode with the same ip is verified, ban previous one
                    vBan.push_back(pprevSubinode);
                    // and keep a reference to be able to ban following subinodes with the same ip
                    pverifiedSubinode = pmn;
                }
            } else {
                pverifiedSubinode = pmn->IsPoSeVerified() ? pmn : NULL;
            }
            pprevSubinode = pmn;
        }
    }

    // ban duplicates
    BOOST_FOREACH(CSubinode* pmn, vBan) {
        //LogPrint("CSubinodeMan::CheckSameAddr -- increasing PoSe ban score for subinode %s\n", pmn->vin.prevout.ToStringShort());
        pmn->IncreasePoSeBanScore();
    }
}

bool CSubinodeMan::SendVerifyRequest(const CAddress& addr, const std::vector<CSubinode*>& vSortedByAddr)
{
    if(netfulfilledman.HasFulfilledRequest(addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request")) {
        // we already asked for verification, not a good idea to do this too often, skip it
        //LogPrint("subinode", "CSubinodeMan::SendVerifyRequest -- too many requests, skipping... addr=%s\n", addr.ToString());
        return false;
    }

    CNode* pnode = g_connman->ConnectNode(addr, NULL, false, true);
    if(pnode == NULL) {
        //LogPrint("CSubinodeMan::SendVerifyRequest -- can't connect to node to verify it, addr=%s\n", addr.ToString());
        return false;
    }

    netfulfilledman.AddFulfilledRequest(addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request");
    // use random nonce, store it and require node to reply with correct one later
    CSubinodeVerification mnv(addr, GetRandInt(999999), pCurrentBlockIndex->nHeight - 1);
    mWeAskedForVerification[addr] = mnv;
    //LogPrint("CSubinodeMan::SendVerifyRequest -- verifying node using nonce %d addr=%s\n", mnv.nonce, addr.ToString());
    const CNetMsgMaker msgMaker(pnode->GetSendVersion());
    g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::MNVERIFY, mnv));


    return true;
}

void CSubinodeMan::SendVerifyReply(CNode* pnode, CSubinodeVerification& mnv)
{
    // only subinodes can sign this, why would someone ask regular node?
    if(!fSubiNode) {
        // do not ban, malicious node might be using my IP
        // and trying to confuse the node which tries to verify it
        return;
    }

    if(netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-reply")) {
//        // peer should not ask us that often
        //LogPrint("SubinodeMan::SendVerifyReply -- ERROR: peer already asked me recently, peer=%d\n", pnode->GetId());
        Misbehaving(pnode->GetId(), 20);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight)) {
        //LogPrint("SubinodeMan::SendVerifyReply -- can't get block hash for unknown block height %d, peer=%d\n", mnv.nBlockHeight, pnode->GetId());
        return;
    }

    std::string strMessage = strprintf("%s%d%s", activeSubinode.service.ToString(), mnv.nonce, blockHash.ToString());

    if(!darkSendSigner.SignMessage(strMessage, mnv.vchSig1, activeSubinode.keySubinode)) {
        //LogPrint("SubinodeMan::SendVerifyReply -- SignMessage() failed\n");
        return;
    }

    std::string strError;

    if(!darkSendSigner.VerifyMessage(activeSubinode.pubKeySubinode, mnv.vchSig1, strMessage, strError)) {
        //LogPrint("SubinodeMan::SendVerifyReply -- VerifyMessage() failed, error: %s\n", strError);
        return;
    }

    const CNetMsgMaker msgMaker(pnode->GetSendVersion());
    g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::MNVERIFY, mnv));
    netfulfilledman.AddFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-reply");
}

void CSubinodeMan::ProcessVerifyReply(CNode* pnode, CSubinodeVerification& mnv)
{
    std::string strError;

    // did we even ask for it? if that's the case we should have matching fulfilled request
    if(!netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request")) {
        //LogPrint("CSubinodeMan::ProcessVerifyReply -- ERROR: we didn't ask for verification of %s, peer=%d\n", pnode->addr.ToString(), pnode->GetId());
        Misbehaving(pnode->GetId(), 20);
        return;
    }

    // Received nonce for a known address must match the one we sent
    if(mWeAskedForVerification[pnode->addr].nonce != mnv.nonce) {
        //LogPrint("CSubinodeMan::ProcessVerifyReply -- ERROR: wrong nounce: requested=%d, received=%d, peer=%d\n",
                   // mWeAskedForVerification[pnode->addr].nonce, mnv.nonce, pnode->GetId());
        Misbehaving(pnode->GetId(), 20);
        return;
    }

    // Received nBlockHeight for a known address must match the one we sent
    if(mWeAskedForVerification[pnode->addr].nBlockHeight != mnv.nBlockHeight) {
        //LogPrint("CSubinodeMan::ProcessVerifyReply -- ERROR: wrong nBlockHeight: requested=%d, received=%d, peer=%d\n",
                   // mWeAskedForVerification[pnode->addr].nBlockHeight, mnv.nBlockHeight, pnode->GetId());
        Misbehaving(pnode->GetId(), 20);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight)) {
        // this shouldn't happen...
        //LogPrint("SubinodeMan::ProcessVerifyReply -- can't get block hash for unknown block height %d, peer=%d\n", mnv.nBlockHeight, pnode->GetId());
        return;
    }

//    // we already verified this address, why node is spamming?
    if(netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-done")) {
        //LogPrint("CSubinodeMan::ProcessVerifyReply -- ERROR: already verified %s recently\n", pnode->addr.ToString());
        Misbehaving(pnode->GetId(), 20);
        return;
    }

    {
        LOCK(cs);

        CSubinode* prealSubinode = NULL;
        std::vector<CSubinode*> vpSubinodesToBan;
        std::vector<CSubinode>::iterator it = vSubinodes.begin();
        std::string strMessage1 = strprintf("%s%d%s", pnode->addr.ToString(), mnv.nonce, blockHash.ToString());
        while(it != vSubinodes.end()) {
            if(CAddress(it->addr, NODE_NETWORK) == pnode->addr) {
                if(darkSendSigner.VerifyMessage(it->pubKeySubinode, mnv.vchSig1, strMessage1, strError)) {
                    // found it!
                    prealSubinode = &(*it);
                    if(!it->IsPoSeVerified()) {
                        it->DecreasePoSeBanScore();
                    }
                    netfulfilledman.AddFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-done");

                    // we can only broadcast it if we are an activated subinode
                    if(activeSubinode.vin == CTxIn()) continue;
                    // update ...
                    mnv.addr = it->addr;
                    mnv.vin1 = it->vin;
                    mnv.vin2 = activeSubinode.vin;
                    std::string strMessage2 = strprintf("%s%d%s%s%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString(),
                                            mnv.vin1.prevout.ToStringShort(), mnv.vin2.prevout.ToStringShort());
                    // ... and sign it
                    if(!darkSendSigner.SignMessage(strMessage2, mnv.vchSig2, activeSubinode.keySubinode)) {
                        //LogPrint("SubinodeMan::ProcessVerifyReply -- SignMessage() failed\n");
                        return;
                    }

                    std::string strError;

                    if(!darkSendSigner.VerifyMessage(activeSubinode.pubKeySubinode, mnv.vchSig2, strMessage2, strError)) {
                        //LogPrint("SubinodeMan::ProcessVerifyReply -- VerifyMessage() failed, error: %s\n", strError);
                        return;
                    }

                    mWeAskedForVerification[pnode->addr] = mnv;
                    mnv.Relay();

                } else {
                    vpSubinodesToBan.push_back(&(*it));
                }
            }
            ++it;
        }
        // no real subinode found?...
        if(!prealSubinode) {
            // this should never be the case normally,
            // only if someone is trying to game the system in some way or smth like that
            //LogPrint("CSubinodeMan::ProcessVerifyReply -- ERROR: no real subinode found for addr %s\n", pnode->addr.ToString());
            Misbehaving(pnode->GetId(), 20);
            return;
        }
        //LogPrint("CSubinodeMan::ProcessVerifyReply -- verified real subinode %s for addr %s\n",
                   // prealSubinode->vin.prevout.ToStringShort(), pnode->addr.ToString());
        // increase ban score for everyone else
        BOOST_FOREACH(CSubinode* pmn, vpSubinodesToBan) {
            pmn->IncreasePoSeBanScore();
            //LogPrint("subinode", "CSubinodeMan::ProcessVerifyBroadcast -- increased PoSe ban score for %s addr %s, new score %d\n",
                        //prealSubinode->vin.prevout.ToStringShort(), pnode->addr.ToString(), pmn->nPoSeBanScore);
        }
        //LogPrint("CSubinodeMan::ProcessVerifyBroadcast -- PoSe score increased for %d fake subinodes, addr %s\n",
                   // (int)vpSubinodesToBan.size(), pnode->addr.ToString());
    }
}

void CSubinodeMan::ProcessVerifyBroadcast(CNode* pnode, const CSubinodeVerification& mnv)
{
    std::string strError;

    if(mapSeenSubinodeVerification.find(mnv.GetHash()) != mapSeenSubinodeVerification.end()) {
        // we already have one
        return;
    }
    mapSeenSubinodeVerification[mnv.GetHash()] = mnv;

    // we don't care about history
    if(mnv.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS) {
        //LogPrint("subinode", "SubinodeMan::ProcessVerifyBroadcast -- Outdated: current block %d, verification block %d, peer=%d\n",
                  //  pCurrentBlockIndex->nHeight, mnv.nBlockHeight, pnode->GetId());
        return;
    }

    if(mnv.vin1.prevout == mnv.vin2.prevout) {
        //LogPrint("subinode", "SubinodeMan::ProcessVerifyBroadcast -- ERROR: same vins %s, peer=%d\n",
                   // mnv.vin1.prevout.ToStringShort(), pnode->GetId());
        // that was NOT a good idea to cheat and verify itself,
        // ban the node we received such message from
        Misbehaving(pnode->GetId(), 100);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight)) {
        // this shouldn't happen...
        //LogPrint("SubinodeMan::ProcessVerifyBroadcast -- Can't get block hash for unknown block height %d, peer=%d\n", mnv.nBlockHeight, pnode->GetId());
        return;
    }

    int nRank = GetSubinodeRank(mnv.vin2, mnv.nBlockHeight, MIN_POSE_PROTO_VERSION);

    if (nRank == -1) {
        //LogPrint("subinode", "CSubinodeMan::ProcessVerifyBroadcast -- Can't calculate rank for subinode %s\n",
                  //  mnv.vin2.prevout.ToStringShort());
        return;
    }

    if(nRank > MAX_POSE_RANK) {
        //LogPrint("subinode", "CSubinodeMan::ProcessVerifyBroadcast -- Mastrernode %s is not in top %d, current rank %d, peer=%d\n",
                  //  mnv.vin2.prevout.ToStringShort(), (int)MAX_POSE_RANK, nRank, pnode->GetId());
        return;
    }

    {
        LOCK(cs);

        std::string strMessage1 = strprintf("%s%d%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString());
        std::string strMessage2 = strprintf("%s%d%s%s%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString(),
                                mnv.vin1.prevout.ToStringShort(), mnv.vin2.prevout.ToStringShort());

        CSubinode* pmn1 = Find(mnv.vin1);
        if(!pmn1) {
            //LogPrint("CSubinodeMan::ProcessVerifyBroadcast -- can't find subinode1 %s\n", mnv.vin1.prevout.ToStringShort());
            return;
        }

        CSubinode* pmn2 = Find(mnv.vin2);
        if(!pmn2) {
            //LogPrint("CSubinodeMan::ProcessVerifyBroadcast -- can't find subinode2 %s\n", mnv.vin2.prevout.ToStringShort());
            return;
        }

        if(pmn1->addr != mnv.addr) {
            //LogPrint("CSubinodeMan::ProcessVerifyBroadcast -- addr %s do not match %s\n", mnv.addr.ToString(), pnode->addr.ToString());
            return;
        }

        if(darkSendSigner.VerifyMessage(pmn1->pubKeySubinode, mnv.vchSig1, strMessage1, strError)) {
            //LogPrint("SubinodeMan::ProcessVerifyBroadcast -- VerifyMessage() for subinode1 failed, error: %s\n", strError);
            return;
        }

        if(darkSendSigner.VerifyMessage(pmn2->pubKeySubinode, mnv.vchSig2, strMessage2, strError)) {
            //LogPrint("SubinodeMan::ProcessVerifyBroadcast -- VerifyMessage() for subinode2 failed, error: %s\n", strError);
            return;
        }

        if(!pmn1->IsPoSeVerified()) {
            pmn1->DecreasePoSeBanScore();
        }
        mnv.Relay();

        //LogPrint("CSubinodeMan::ProcessVerifyBroadcast -- verified subinode %s for addr %s\n",
                   // pmn1->vin.prevout.ToStringShort(), pnode->addr.ToString());

        // increase ban score for everyone else with the same addr
        int nCount = 0;
        BOOST_FOREACH(CSubinode& mn, vSubinodes) {
            if(mn.addr != mnv.addr || mn.vin.prevout == mnv.vin1.prevout) continue;
            mn.IncreasePoSeBanScore();
            nCount++;
            //LogPrint("subinode", "CSubinodeMan::ProcessVerifyBroadcast -- increased PoSe ban score for %s addr %s, new score %d\n",
                      //  mn.vin.prevout.ToStringShort(), mn.addr.ToString(), mn.nPoSeBanScore);
        }
        //LogPrint("CSubinodeMan::ProcessVerifyBroadcast -- PoSe score incresed for %d fake subinodes, addr %s\n",
                    //nCount, pnode->addr.ToString());
    }
}

std::string CSubinodeMan::ToString() const
{
    std::ostringstream info;

    info << "Subinodes: " << (int)vSubinodes.size() <<
            ", peers who asked us for Subinode list: " << (int)mAskedUsForSubinodeList.size() <<
            ", peers we asked for Subinode list: " << (int)mWeAskedForSubinodeList.size() <<
            ", entries in Subinode list we asked for: " << (int)mWeAskedForSubinodeListEntry.size() <<
            ", subinode index size: " << indexSubinodes.GetSize() <<
            ", nDsqCount: " << (int)nDsqCount;

    return info.str();
}

void CSubinodeMan::UpdateSubinodeList(CSubinodeBroadcast mnb)
{
    try {
        //LogPrint("CSubinodeMan::UpdateSubinodeList\n");
        LOCK2(cs_main, cs);
        mapSeenSubinodePing.insert(std::make_pair(mnb.lastPing.GetHash(), mnb.lastPing));
        mapSeenSubinodeBroadcast.insert(std::make_pair(mnb.GetHash(), std::make_pair(GetTime(), mnb)));

        //LogPrint("CSubinodeMan::UpdateSubinodeList -- subinode=%s  addr=%s\n", mnb.vin.prevout.ToStringShort(), mnb.addr.ToString());

        CSubinode *pmn = Find(mnb.vin);
        if (pmn == NULL) {
            CSubinode mn(mnb);
            if (Add(mn)) {
                subinodeSync.AddedSubinodeList();
            }
        } else {
            CSubinodeBroadcast mnbOld = mapSeenSubinodeBroadcast[CSubinodeBroadcast(*pmn).GetHash()].second;
            if (pmn->UpdateFromNewBroadcast(mnb)) {
                subinodeSync.AddedSubinodeList();
                mapSeenSubinodeBroadcast.erase(mnbOld.GetHash());
            }
        }
    } catch (const std::exception &e) {
        PrintExceptionContinue(&e, "UpdateSubinodeList");
    }
}

bool CSubinodeMan::CheckMnbAndUpdateSubinodeList(CNode* pfrom, CSubinodeBroadcast mnb, int& nDos)
{
    // Need LOCK2 here to ensure consistent locking order because the SimpleCheck call below locks cs_main
    LOCK(cs_main);

    {
        LOCK(cs);
        nDos = 0;
        //LogPrint("subinode", "CSubinodeMan::CheckMnbAndUpdateSubinodeList -- subinode=%s\n", mnb.vin.prevout.ToStringShort());

        uint256 hash = mnb.GetHash();
        if (mapSeenSubinodeBroadcast.count(hash) && !mnb.fRecovery) { //seen
            //LogPrint("subinode", "CSubinodeMan::CheckMnbAndUpdateSubinodeList -- subinode=%s seen\n", mnb.vin.prevout.ToStringShort());
            // less then 2 pings left before this MN goes into non-recoverable state, bump sync timeout
            if (GetTime() - mapSeenSubinodeBroadcast[hash].first > SUBINODE_NEW_START_REQUIRED_SECONDS - SUBINODE_MIN_MNP_SECONDS * 2) {
                //LogPrint("subinode", "CSubinodeMan::CheckMnbAndUpdateSubinodeList -- subinode=%s seen update\n", mnb.vin.prevout.ToStringShort());
                mapSeenSubinodeBroadcast[hash].first = GetTime();
                subinodeSync.AddedSubinodeList();
            }
            // did we ask this node for it?
            if (pfrom && IsMnbRecoveryRequested(hash) && GetTime() < mMnbRecoveryRequests[hash].first) {
                //LogPrint("subinode", "CSubinodeMan::CheckMnbAndUpdateSubinodeList -- mnb=%s seen request\n", hash.ToString());
                if (mMnbRecoveryRequests[hash].second.count(pfrom->addr)) {
                    //LogPrint("subinode", "CSubinodeMan::CheckMnbAndUpdateSubinodeList -- mnb=%s seen request, addr=%s\n", hash.ToString(), pfrom->addr.ToString());
                    // do not allow node to send same mnb multiple times in recovery mode
                    mMnbRecoveryRequests[hash].second.erase(pfrom->addr);
                    // does it have newer lastPing?
                    if (mnb.lastPing.sigTime > mapSeenSubinodeBroadcast[hash].second.lastPing.sigTime) {
                        // simulate Check
                        CSubinode mnTemp = CSubinode(mnb);
                        mnTemp.Check();
                        //LogPrint("subinode", "CSubinodeMan::CheckMnbAndUpdateSubinodeList -- mnb=%s seen request, addr=%s, better lastPing: %d min ago, projected mn state: %s\n", hash.ToString(), pfrom->addr.ToString(), (GetTime() - mnb.lastPing.sigTime) / 60, mnTemp.GetStateString());
                        if (mnTemp.IsValidStateForAutoStart(mnTemp.nActiveState)) {
                            // this node thinks it's a good one
                            //LogPrint("subinode", "CSubinodeMan::CheckMnbAndUpdateSubinodeList -- subinode=%s seen good\n", mnb.vin.prevout.ToStringShort());
                            mMnbRecoveryGoodReplies[hash].push_back(mnb);
                        }
                    }
                }
            }
            return true;
        }
        mapSeenSubinodeBroadcast.insert(std::make_pair(hash, std::make_pair(GetTime(), mnb)));

        //LogPrint("subinode", "CSubinodeMan::CheckMnbAndUpdateSubinodeList -- subinode=%s new\n", mnb.vin.prevout.ToStringShort());

        if (!mnb.SimpleCheck(nDos)) {
            //LogPrint("subinode", "CSubinodeMan::CheckMnbAndUpdateSubinodeList -- SimpleCheck() failed, subinode=%s\n", mnb.vin.prevout.ToStringShort());
            return false;
        }

        // search Subinode list
        CSubinode *pmn = Find(mnb.vin);
        if (pmn) {
            CSubinodeBroadcast mnbOld = mapSeenSubinodeBroadcast[CSubinodeBroadcast(*pmn).GetHash()].second;
            if (!mnb.Update(pmn, nDos)) {
                //LogPrint("subinode", "CSubinodeMan::CheckMnbAndUpdateSubinodeList -- Update() failed, subinode=%s\n", mnb.vin.prevout.ToStringShort());
                return false;
            }
            if (hash != mnbOld.GetHash()) {
                mapSeenSubinodeBroadcast.erase(mnbOld.GetHash());
            }
        }
    } // end of LOCK(cs);

    if(mnb.CheckOutpoint(nDos)) {
        Add(mnb);
        subinodeSync.AddedSubinodeList();
        // if it matches our Subinode privkey...
        if(fSubiNode && mnb.pubKeySubinode == activeSubinode.pubKeySubinode) {
            mnb.nPoSeBanScore = -SUBINODE_POSE_BAN_MAX_SCORE;
            if(mnb.nProtocolVersion == PROTOCOL_VERSION) {
                // ... and PROTOCOL_VERSION, then we've been remotely activated ...
                //LogPrint("CSubinodeMan::CheckMnbAndUpdateSubinodeList -- Got NEW Subinode entry: subinode=%s  sigTime=%lld  addr=%s\n",
                            //mnb.vin.prevout.ToStringShort(), mnb.sigTime, mnb.addr.ToString());
                activeSubinode.ManageState();
            } else {
                // ... otherwise we need to reactivate our node, do not add it to the list and do not relay
                // but also do not ban the node we get this message from
                //LogPrint("CSubinodeMan::CheckMnbAndUpdateSubinodeList -- wrong PROTOCOL_VERSION, re-activate your MN: message nProtocolVersion=%d  PROTOCOL_VERSION=%d\n", mnb.nProtocolVersion, PROTOCOL_VERSION);
                return false;
            }
        }
        mnb.RelaySubiNode();
    } else {
        //LogPrint("CSubinodeMan::CheckMnbAndUpdateSubinodeList -- Rejected Subinode entry: %s  addr=%s\n", mnb.vin.prevout.ToStringShort(), mnb.addr.ToString());
        return false;
    }

    return true;
}

void CSubinodeMan::UpdateLastPaid()
{
    LOCK(cs);
    if(fLiteMode) return;
    if(!pCurrentBlockIndex) {
        // //LogPrint("CSubinodeMan::UpdateLastPaid, pCurrentBlockIndex=NULL\n");
        return;
    }

    static bool IsFirstRun = true;
    // Do full scan on first run or if we are not a subinode
    // (MNs should update this info on every block, so limited scan should be enough for them)
    int nMaxBlocksToScanBack = (IsFirstRun || !fSubiNode) ? mnpayments.GetStorageLimit() : LAST_PAID_SCAN_BLOCKS;

    //LogPrint("mnpayments", "CSubinodeMan::UpdateLastPaid -- nHeight=%d, nMaxBlocksToScanBack=%d, IsFirstRun=%s\n",
                            // pCurrentBlockIndex->nHeight, nMaxBlocksToScanBack, IsFirstRun ? "true" : "false");

    BOOST_FOREACH(CSubinode& mn, vSubinodes) {
        mn.UpdateLastPaid(pCurrentBlockIndex, nMaxBlocksToScanBack);
    }

    // every time is like the first time if winners list is not synced
    IsFirstRun = !subinodeSync.IsWinnersListSynced();
}

void CSubinodeMan::CheckAndRebuildSubinodeIndex()
{
    LOCK(cs);

    if(GetTime() - nLastIndexRebuildTime < MIN_INDEX_REBUILD_TIME) {
        return;
    }

    if(indexSubinodes.GetSize() <= MAX_EXPECTED_INDEX_SIZE) {
        return;
    }

    if(indexSubinodes.GetSize() <= int(vSubinodes.size())) {
        return;
    }

    indexSubinodesOld = indexSubinodes;
    indexSubinodes.Clear();
    for(size_t i = 0; i < vSubinodes.size(); ++i) {
        indexSubinodes.AddSubinodeVIN(vSubinodes[i].vin);
    }

    fIndexRebuilt = true;
    nLastIndexRebuildTime = GetTime();
}

void CSubinodeMan::UpdateWatchdogVoteTime(const CTxIn& vin)
{
    LOCK(cs);
    CSubinode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->UpdateWatchdogVoteTime();
    nLastWatchdogVoteTime = GetTime();
}

bool CSubinodeMan::IsWatchdogActive()
{
    LOCK(cs);
    // Check if any subinodes have voted recently, otherwise return false
    return (GetTime() - nLastWatchdogVoteTime) <= SUBINODE_WATCHDOG_MAX_SECONDS;
}

void CSubinodeMan::CheckSubinode(const CTxIn& vin, bool fForce)
{
    LOCK(cs);
    CSubinode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->Check(fForce);
}

void CSubinodeMan::CheckSubinode(const CPubKey& pubKeySubinode, bool fForce)
{
    LOCK(cs);
    CSubinode* pMN = Find(pubKeySubinode);
    if(!pMN)  {
        return;
    }
    pMN->Check(fForce);
}

int CSubinodeMan::GetSubinodeState(const CTxIn& vin)
{
    LOCK(cs);
    CSubinode* pMN = Find(vin);
    if(!pMN)  {
        return CSubinode::SUBINODE_NEW_START_REQUIRED;
    }
    return pMN->nActiveState;
}

int CSubinodeMan::GetSubinodeState(const CPubKey& pubKeySubinode)
{
    LOCK(cs);
    CSubinode* pMN = Find(pubKeySubinode);
    if(!pMN)  {
        return CSubinode::SUBINODE_NEW_START_REQUIRED;
    }
    return pMN->nActiveState;
}

bool CSubinodeMan::IsSubinodePingedWithin(const CTxIn& vin, int nSeconds, int64_t nTimeToCheckAt)
{
    LOCK(cs);
    CSubinode* pMN = Find(vin);
    if(!pMN) {
        return false;
    }
    return pMN->IsPingedWithin(nSeconds, nTimeToCheckAt);
}

void CSubinodeMan::SetSubinodeLastPing(const CTxIn& vin, const CSubinodePing& mnp)
{
    LOCK(cs);
    CSubinode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->lastPing = mnp;
    mapSeenSubinodePing.insert(std::make_pair(mnp.GetHash(), mnp));

    CSubinodeBroadcast mnb(*pMN);
    uint256 hash = mnb.GetHash();
    if(mapSeenSubinodeBroadcast.count(hash)) {
        mapSeenSubinodeBroadcast[hash].second.lastPing = mnp;
    }
}

void CSubinodeMan::UpdatedBlockTip(const CBlockIndex *pindex)
{
    pCurrentBlockIndex = pindex;
    //LogPrint("subinode", "CSubinodeMan::UpdatedBlockTip -- pCurrentBlockIndex->nHeight=%d\n", pCurrentBlockIndex->nHeight);

    CheckSameAddr();

    if(fSubiNode) {
        // normal wallet does not need to update this every block, doing update on rpc call should be enough
        UpdateLastPaid();
    }
}

void CSubinodeMan::NotifySubinodeUpdates()
{
    // Avoid double locking
    bool fSubinodesAddedLocal = false;
    bool fSubinodesRemovedLocal = false;
    {
        LOCK(cs);
        fSubinodesAddedLocal = fSubinodesAdded;
        fSubinodesRemovedLocal = fSubinodesRemoved;
    }

    if(fSubinodesAddedLocal) {
//        governance.CheckSubinodeOrphanObjects();
//        governance.CheckSubinodeOrphanVotes();
    }
    if(fSubinodesRemovedLocal) {
//        governance.UpdateCachesAndClean();
    }

    LOCK(cs);
    fSubinodesAdded = false;
    fSubinodesRemoved = false;
}
