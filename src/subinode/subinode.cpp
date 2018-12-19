// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2018-2019 The Subi Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activesubinode.h"
#include "consensus/validation.h"
#include "darksend.h"
#include "init.h"
#include "subinode.h"
#include "subinode-payments.h"
#include "subinode-sync.h"
#include "subinodeman.h"
#include "util.h"
#include "netbase.h"

#include <boost/lexical_cast.hpp>


CSubinode::CSubinode() :
        vin(),
        addr(),
        pubKeyCollateralAddress(),
        pubKeySubinode(),
        lastPing(),
        vchSig(),
        sigTime(GetAdjustedTime()),
        nLastDsq(0),
        nTimeLastChecked(0),
        nTimeLastPaid(0),
        nTimeLastWatchdogVote(0),
        nActiveState(SUBINODE_ENABLED),
        nCacheCollateralBlock(0),
        nBlockLastPaid(0),
        nProtocolVersion(PROTOCOL_VERSION),
        nPoSeBanScore(0),
        nPoSeBanHeight(0),
        fAllowMixingTx(true),
        fUnitTest(false) {}

CSubinode::CSubinode(CService addrNew, CTxIn vinNew, CPubKey pubKeyCollateralAddressNew, CPubKey pubKeySubinodeNew, int nProtocolVersionIn) :
        vin(vinNew),
        addr(addrNew),
        pubKeyCollateralAddress(pubKeyCollateralAddressNew),
        pubKeySubinode(pubKeySubinodeNew),
        lastPing(),
        vchSig(),
        sigTime(GetAdjustedTime()),
        nLastDsq(0),
        nTimeLastChecked(0),
        nTimeLastPaid(0),
        nTimeLastWatchdogVote(0),
        nActiveState(SUBINODE_ENABLED),
        nCacheCollateralBlock(0),
        nBlockLastPaid(0),
        nProtocolVersion(nProtocolVersionIn),
        nPoSeBanScore(0),
        nPoSeBanHeight(0),
        fAllowMixingTx(true),
        fUnitTest(false) {}

CSubinode::CSubinode(const CSubinode &other) :
        vin(other.vin),
        addr(other.addr),
        pubKeyCollateralAddress(other.pubKeyCollateralAddress),
        pubKeySubinode(other.pubKeySubinode),
        lastPing(other.lastPing),
        vchSig(other.vchSig),
        sigTime(other.sigTime),
        nLastDsq(other.nLastDsq),
        nTimeLastChecked(other.nTimeLastChecked),
        nTimeLastPaid(other.nTimeLastPaid),
        nTimeLastWatchdogVote(other.nTimeLastWatchdogVote),
        nActiveState(other.nActiveState),
        nCacheCollateralBlock(other.nCacheCollateralBlock),
        nBlockLastPaid(other.nBlockLastPaid),
        nProtocolVersion(other.nProtocolVersion),
        nPoSeBanScore(other.nPoSeBanScore),
        nPoSeBanHeight(other.nPoSeBanHeight),
        fAllowMixingTx(other.fAllowMixingTx),
        fUnitTest(other.fUnitTest) {}

CSubinode::CSubinode(const CSubinodeBroadcast &mnb) :
        vin(mnb.vin),
        addr(mnb.addr),
        pubKeyCollateralAddress(mnb.pubKeyCollateralAddress),
        pubKeySubinode(mnb.pubKeySubinode),
        lastPing(mnb.lastPing),
        vchSig(mnb.vchSig),
        sigTime(mnb.sigTime),
        nLastDsq(0),
        nTimeLastChecked(0),
        nTimeLastPaid(0),
        nTimeLastWatchdogVote(mnb.sigTime),
        nActiveState(mnb.nActiveState),
        nCacheCollateralBlock(0),
        nBlockLastPaid(0),
        nProtocolVersion(mnb.nProtocolVersion),
        nPoSeBanScore(0),
        nPoSeBanHeight(0),
        fAllowMixingTx(true),
        fUnitTest(false) {}

//CSporkManager sporkManager;
//
// When a new subinode broadcast is sent, update our information
//
bool CSubinode::UpdateFromNewBroadcast(CSubinodeBroadcast &mnb) {
    if (mnb.sigTime <= sigTime && !mnb.fRecovery) return false;

    pubKeySubinode = mnb.pubKeySubinode;
    sigTime = mnb.sigTime;
    vchSig = mnb.vchSig;
    nProtocolVersion = mnb.nProtocolVersion;
    addr = mnb.addr;
    nPoSeBanScore = 0;
    nPoSeBanHeight = 0;
    nTimeLastChecked = 0;
    int nDos = 0;
    if (mnb.lastPing == CSubinodePing() || (mnb.lastPing != CSubinodePing() && mnb.lastPing.CheckAndUpdate(this, true, nDos))) {
        lastPing = mnb.lastPing;
        mnodeman.mapSeenSubinodePing.insert(std::make_pair(lastPing.GetHash(), lastPing));
    }
    // if it matches our Subinode privkey...
    if (fSubiNode && pubKeySubinode == activeSubinode.pubKeySubinode) {
        nPoSeBanScore = -SUBINODE_POSE_BAN_MAX_SCORE;
        if (nProtocolVersion == PROTOCOL_VERSION) {
            // ... and PROTOCOL_VERSION, then we've been remotely activated ...
            activeSubinode.ManageState();
        } else {
            // ... otherwise we need to reactivate our node, do not add it to the list and do not relay
            // but also do not ban the node we get this message from
            //LogPrint("CSubinode::UpdateFromNewBroadcast -- wrong PROTOCOL_VERSION, re-activate your MN: message nProtocolVersion=%d  PROTOCOL_VERSION=%d\n", nProtocolVersion, PROTOCOL_VERSION);
            return false;
        }
    }
    return true;
}

//
// Deterministically calculate a given "score" for a Subinode depending on how close it's hash is to
// the proof of work for that block. The further away they are the better, the furthest will win the election
// and get paid this block
//
arith_uint256 CSubinode::CalculateScore(const uint256 &blockHash) {
    uint256 aux = ArithToUint256(UintToArith256(vin.prevout.hash) + vin.prevout.n);

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << blockHash;
    arith_uint256 hash2 = UintToArith256(ss.GetHash());

    CHashWriter ss2(SER_GETHASH, PROTOCOL_VERSION);
    ss2 << blockHash;
    ss2 << aux;
    arith_uint256 hash3 = UintToArith256(ss2.GetHash());

    return (hash3 > hash2 ? hash3 - hash2 : hash2 - hash3);
}

void CSubinode::Check(bool fForce) {
    LOCK(cs);

    if (ShutdownRequested()) return;

    if (!fForce && (GetTime() - nTimeLastChecked < SUBINODE_CHECK_SECONDS)) return;
    nTimeLastChecked = GetTime();

    //LogPrint("subinode", "CSubinode::Check -- Subinode %s is in %s state\n", vin.prevout.ToStringShort(), GetStateString());

    //once spent, stop doing the checks
    if (IsOutpointSpent()) return;

    int nHeight = 0;
    if (!fUnitTest) {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) return;

        Coin coin;
        if (!pcoinsTip->GetCoin(vin.prevout, coin) ||
            /*(unsigned int) vin.prevout.n >= coin.out || */
            coin.out.IsNull()) {
            nActiveState = SUBINODE_OUTPOINT_SPENT;
            //LogPrint("subinode", "CSubinode::Check -- Failed to find Subinode UTXO, subinode=%s\n", vin.prevout.ToStringShort());
            return;
        }

        nHeight = chainActive.Height();
    }

    if (IsPoSeBanned()) {
        if (nHeight < nPoSeBanHeight) return; // too early?
        // Otherwise give it a chance to proceed further to do all the usual checks and to change its state.
        // Subinode still will be on the edge and can be banned back easily if it keeps ignoring mnverify
        // or connect attempts. Will require few mnverify messages to strengthen its position in mn list.
        //LogPrint("CSubinode::Check -- Subinode %s is unbanned and back in list now\n", vin.prevout.ToStringShort());
        DecreasePoSeBanScore();
    } else if (nPoSeBanScore >= SUBINODE_POSE_BAN_MAX_SCORE) {
        nActiveState = SUBINODE_POSE_BAN;
        // ban for the whole payment cycle
        nPoSeBanHeight = nHeight + mnodeman.size();
        //LogPrint("CSubinode::Check -- Subinode %s is banned till block %d now\n", vin.prevout.ToStringShort(), nPoSeBanHeight);
        return;
    }

    int nActiveStatePrev = nActiveState;
    bool fOurSubinode = fSubiNode && activeSubinode.pubKeySubinode == pubKeySubinode;

    // subinode doesn't meet payment protocol requirements ...
    bool fRequireUpdate = nProtocolVersion < mnpayments.GetMinSubinodePaymentsProto() ||
                          // or it's our own node and we just updated it to the new protocol but we are still waiting for activation ...
                          (fOurSubinode && nProtocolVersion < PROTOCOL_VERSION);

    if (fRequireUpdate) {
        nActiveState = SUBINODE_UPDATE_REQUIRED;
        if (nActiveStatePrev != nActiveState) {
            //LogPrint("subinode", "CSubinode::Check -- Subinode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
        }
        return;
    }

    // keep old subinodes on start, give them a chance to receive updates...
    bool fWaitForPing = !subinodeSync.IsSubinodeListSynced() && !IsPingedWithin(SUBINODE_MIN_MNP_SECONDS);

    if (fWaitForPing && !fOurSubinode) {
        // ...but if it was already expired before the initial check - return right away
        if (IsExpired() || IsWatchdogExpired() || IsNewStartRequired()) {
            //LogPrint("subinode", "CSubinode::Check -- Subinode %s is in %s state, waiting for ping\n", vin.prevout.ToStringShort(), GetStateString());
            return;
        }
    }

    // don't expire if we are still in "waiting for ping" mode unless it's our own subinode
    if (!fWaitForPing || fOurSubinode) {

        if (!IsPingedWithin(SUBINODE_NEW_START_REQUIRED_SECONDS)) {
            nActiveState = SUBINODE_NEW_START_REQUIRED;
            if (nActiveStatePrev != nActiveState) {
                //LogPrint("subinode", "CSubinode::Check -- Subinode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
            }
            return;
        }

        bool fWatchdogActive = subinodeSync.IsSynced(chainActive.Height()) && mnodeman.IsWatchdogActive();
        bool fWatchdogExpired = (fWatchdogActive && ((GetTime() - nTimeLastWatchdogVote) > SUBINODE_WATCHDOG_MAX_SECONDS));

//        //LogPrint("subinode", "CSubinode::Check -- outpoint=%s, nTimeLastWatchdogVote=%d, GetTime()=%d, fWatchdogExpired=%d\n",
//                vin.prevout.ToStringShort(), nTimeLastWatchdogVote, GetTime(), fWatchdogExpired);

        if (fWatchdogExpired) {
            nActiveState = SUBINODE_WATCHDOG_EXPIRED;
            if (nActiveStatePrev != nActiveState) {
                //LogPrint("subinode", "CSubinode::Check -- Subinode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
            }
            return;
        }

        if (!IsPingedWithin(SUBINODE_EXPIRATION_SECONDS)) {
            nActiveState = SUBINODE_EXPIRED;
            if (nActiveStatePrev != nActiveState) {
                //LogPrint("subinode", "CSubinode::Check -- Subinode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
            }
            return;
        }
    }

    if (lastPing.sigTime - sigTime < SUBINODE_MIN_MNP_SECONDS) {
        nActiveState = SUBINODE_PRE_ENABLED;
        if (nActiveStatePrev != nActiveState) {
            //LogPrint("subinode", "CSubinode::Check -- Subinode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
        }
        return;
    }

    nActiveState = SUBINODE_ENABLED; // OK
    if (nActiveStatePrev != nActiveState) {
        //LogPrint("subinode", "CSubinode::Check -- Subinode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
    }
}

bool CSubinode::IsValidNetAddr() {
    return IsValidNetAddr(addr);
}

bool CSubinode::IsValidForPayment() {
    if (nActiveState == SUBINODE_ENABLED) {
        return true;
    }
//    if(!sporkManager.IsSporkActive(SPORK_14_REQUIRE_SENTINEL_FLAG) &&
//       (nActiveState == SUBINODE_WATCHDOG_EXPIRED)) {
//        return true;
//    }

    return false;
}

bool CSubinode::IsValidNetAddr(CService addrIn) {
    // TODO: regtest is fine with any addresses for now,
    // should probably be a bit smarter if one day we start to implement tests for this
    return Params().NetworkIDString() == CBaseChainParams::REGTEST ||
           (addrIn.IsIPv4() && IsReachable(addrIn) && addrIn.IsRoutable());
}

subinode_info_t CSubinode::GetInfo() {
    subinode_info_t info;
    info.vin = vin;
    info.addr = addr;
    info.pubKeyCollateralAddress = pubKeyCollateralAddress;
    info.pubKeySubinode = pubKeySubinode;
    info.sigTime = sigTime;
    info.nLastDsq = nLastDsq;
    info.nTimeLastChecked = nTimeLastChecked;
    info.nTimeLastPaid = nTimeLastPaid;
    info.nTimeLastWatchdogVote = nTimeLastWatchdogVote;
    info.nTimeLastPing = lastPing.sigTime;
    info.nActiveState = nActiveState;
    info.nProtocolVersion = nProtocolVersion;
    info.fInfoValid = true;
    return info;
}

std::string CSubinode::StateToString(int nStateIn) {
    switch (nStateIn) {
        case SUBINODE_PRE_ENABLED:
            return "PRE_ENABLED";
        case SUBINODE_ENABLED:
            return "ENABLED";
        case SUBINODE_EXPIRED:
            return "EXPIRED";
        case SUBINODE_OUTPOINT_SPENT:
            return "OUTPOINT_SPENT";
        case SUBINODE_UPDATE_REQUIRED:
            return "UPDATE_REQUIRED";
        case SUBINODE_WATCHDOG_EXPIRED:
            return "WATCHDOG_EXPIRED";
        case SUBINODE_NEW_START_REQUIRED:
            return "NEW_START_REQUIRED";
        case SUBINODE_POSE_BAN:
            return "POSE_BAN";
        default:
            return "UNKNOWN";
    }
}

std::string CSubinode::GetStateString() const {
    return StateToString(nActiveState);
}

std::string CSubinode::GetStatus() const {
    // TODO: return smth a bit more human readable here
    return GetStateString();
}

std::string CSubinode::ToString() const {
    std::string str;
    str += "subinode{";
    str += addr.ToString();
    str += " ";
    str += std::to_string(nProtocolVersion);
    str += " ";
    str += vin.prevout.ToStringShort();
    str += " ";
    str += CBitcoinAddress(pubKeyCollateralAddress.GetID()).ToString();
    str += " ";
    str += std::to_string(lastPing == CSubinodePing() ? sigTime : lastPing.sigTime);
    str += " ";
    str += std::to_string(lastPing == CSubinodePing() ? 0 : lastPing.sigTime - sigTime);
    str += " ";
    str += std::to_string(nBlockLastPaid);
    str += "}\n";
    return str;
}

int CSubinode::GetCollateralAge() {
    int nHeight;
    {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain || !chainActive.Tip()) return -1;
        nHeight = chainActive.Height();
    }

    if (nCacheCollateralBlock == 0) {
        int nInputAge = GetInputAge(vin);
        if (nInputAge > 0) {
            nCacheCollateralBlock = nHeight - nInputAge;
        } else {
            return nInputAge;
        }
    }

    return nHeight - nCacheCollateralBlock;
}

void CSubinode::UpdateLastPaid(const CBlockIndex *pindex, int nMaxBlocksToScanBack) {
    if (!pindex) {
        //LogPrint("CSubinode::UpdateLastPaid pindex is NULL\n");
        return;
    }

    const CBlockIndex *BlockReading = pindex;

    CScript mnpayee = GetScriptForDestination(pubKeyCollateralAddress.GetID());
    //LogPrint("subinode", "CSubinode::UpdateLastPaidBlock -- searching for block with payment to %s\n", vin.prevout.ToStringShort());

    LOCK(cs_mapSubinodeBlocks);

    for (int i = 0; BlockReading && BlockReading->nHeight > nBlockLastPaid && i < nMaxBlocksToScanBack; i++) {
//        //LogPrint("mnpayments.mapSubinodeBlocks.count(BlockReading->nHeight)=%s\n", mnpayments.mapSubinodeBlocks.count(BlockReading->nHeight));
//        //LogPrint("mnpayments.mapSubinodeBlocks[BlockReading->nHeight].HasPayeeWithVotes(mnpayee, 2)=%s\n", mnpayments.mapSubinodeBlocks[BlockReading->nHeight].HasPayeeWithVotes(mnpayee, 2));
        if (mnpayments.mapSubinodeBlocks.count(BlockReading->nHeight) &&
            mnpayments.mapSubinodeBlocks[BlockReading->nHeight].HasPayeeWithVotes(mnpayee, 2)) {
            // //LogPrint("i=%s, BlockReading->nHeight=%s\n", i, BlockReading->nHeight);
            CBlock block;
            if (!ReadBlockFromDisk(block, BlockReading, Params().GetConsensus())) // shouldn't really happen
            {
                //LogPrint("ReadBlockFromDisk failed\n");
                continue;
            }

            CAmount nSubinodePayment = GetSubinodePayment(BlockReading->nHeight, block.vtx[0]->GetValueOut());

            BOOST_FOREACH(CTxOut txout, block.vtx[0]->vout)
            if (mnpayee == txout.scriptPubKey && nSubinodePayment == txout.nValue) {
                nBlockLastPaid = BlockReading->nHeight;
                nTimeLastPaid = BlockReading->nTime;
                //LogPrint("subinode", "CSubinode::UpdateLastPaidBlock -- searching for block with payment to %s -- found new %d\n", vin.prevout.ToStringShort(), nBlockLastPaid);
                return;
            }
        }

        if (BlockReading->pprev == NULL) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    // Last payment for this subinode wasn't found in latest mnpayments blocks
    // or it was found in mnpayments blocks but wasn't found in the blockchain.
    // //LogPrint("subinode", "CSubinode::UpdateLastPaidBlock -- searching for block with payment to %s -- keeping old %d\n", vin.prevout.ToStringShort(), nBlockLastPaid);
}

bool CSubinodeBroadcast::Create(std::string strService, std::string strKeySubinode, std::string strTxHash, std::string strOutputIndex, std::string &strErrorRet, CSubinodeBroadcast &mnbRet, bool fOffline) {
    //LogPrint("CSubinodeBroadcast::Create\n");
    CTxIn txin;
    CPubKey pubKeyCollateralAddressNew;
    CKey keyCollateralAddressNew;
    CPubKey pubKeySubinodeNew;
    CKey keySubinodeNew;
    //need correct blocks to send ping
    if (!fOffline && !subinodeSync.IsBlockchainSynced()) {
        strErrorRet = "Sync in progress. Must wait until sync is complete to start Subinode";
        //LogPrint("CSubinodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    //TODO
    if (!darkSendSigner.GetKeysFromSecret(strKeySubinode, keySubinodeNew, pubKeySubinodeNew)) {
        strErrorRet = strprintf("Invalid subinode key %s", strKeySubinode);
        //LogPrint("CSubinodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    if (!vpwallets.front()->GetSubinodeVinAndKeys(txin, pubKeyCollateralAddressNew, keyCollateralAddressNew, strTxHash, strOutputIndex)) {
        strErrorRet = strprintf("Could not allocate txin %s:%s for subinode %s", strTxHash, strOutputIndex, strService);
        //LogPrint("CSubinodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }


    CService addr;
    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if (!Lookup(strService.c_str(), addr, mainnetDefaultPort, false)) {
        return InitError(strprintf(_("CSubinodeBroadcast Create(): Invalid subinode broadcast: '%s'"), strService));
    }

    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if (addr.GetPort() != mainnetDefaultPort) {
            strErrorRet = strprintf("Invalid port %u for subinode %s, only %d is supported on mainnet.", addr.GetPort(), strService, mainnetDefaultPort);
            //LogPrint("CSubinodeBroadcast::Create -- %s\n", strErrorRet);
            return false;
        }
    } else if (addr.GetPort() == mainnetDefaultPort) {
        strErrorRet = strprintf("Invalid port %u for subinode %s, %d is the only supported on mainnet.", addr.GetPort(), strService, mainnetDefaultPort);
        //LogPrint("CSubinodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    return Create(txin, addr, keyCollateralAddressNew, pubKeyCollateralAddressNew, keySubinodeNew, pubKeySubinodeNew, strErrorRet, mnbRet);
}

bool CSubinodeBroadcast::Create(CTxIn txin, CService service, CKey keyCollateralAddressNew, CPubKey pubKeyCollateralAddressNew, CKey keySubinodeNew, CPubKey pubKeySubinodeNew, std::string &strErrorRet, CSubinodeBroadcast &mnbRet) {
    // wait for reindex and/or import to finish
    if (fImporting || fReindex) return false;

    //LogPrint("subinode", "CSubinodeBroadcast::Create -- pubKeyCollateralAddressNew = %s, pubKeySubinodeNew.GetID() = %s\n",
             //CBitcoinAddress(pubKeyCollateralAddressNew.GetID()).ToString(),
             //pubKeySubinodeNew.GetID().ToString());


    CSubinodePing mnp(txin);
    if (!mnp.Sign(keySubinodeNew, pubKeySubinodeNew)) {
        strErrorRet = strprintf("Failed to sign ping, subinode=%s", txin.prevout.ToStringShort());
        //LogPrint("CSubinodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CSubinodeBroadcast();
        return false;
    }

    mnbRet = CSubinodeBroadcast(service, txin, pubKeyCollateralAddressNew, pubKeySubinodeNew, PROTOCOL_VERSION);

    if (!mnbRet.IsValidNetAddr()) {
        strErrorRet = strprintf("Invalid IP address, subinode=%s", txin.prevout.ToStringShort());
        //LogPrint("CSubinodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CSubinodeBroadcast();
        return false;
    }

    mnbRet.lastPing = mnp;
    if (!mnbRet.Sign(keyCollateralAddressNew)) {
        strErrorRet = strprintf("Failed to sign broadcast, subinode=%s", txin.prevout.ToStringShort());
        //LogPrint("CSubinodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CSubinodeBroadcast();
        return false;
    }

    return true;
}

bool CSubinodeBroadcast::SimpleCheck(int &nDos) {
    nDos = 0;

    // make sure addr is valid
    if (!IsValidNetAddr()) {
        //LogPrint("CSubinodeBroadcast::SimpleCheck -- Invalid addr, rejected: subinode=%s  addr=%s\n",
                  //vin.prevout.ToStringShort(), addr.ToString());
        return false;
    }

    // make sure signature isn't in the future (past is OK)
    if (sigTime > GetAdjustedTime() + 60 * 60) {
        //LogPrint("CSubinodeBroadcast::SimpleCheck -- Signature rejected, too far into the future: subinode=%s\n", vin.prevout.ToStringShort());
        nDos = 1;
        return false;
    }

    // empty ping or incorrect sigTime/unknown blockhash
    if (lastPing == CSubinodePing() || !lastPing.SimpleCheck(nDos)) {
        // one of us is probably forked or smth, just mark it as expired and check the rest of the rules
        nActiveState = SUBINODE_EXPIRED;
    }

    if (nProtocolVersion < mnpayments.GetMinSubinodePaymentsProto()) {
        //LogPrint("CSubinodeBroadcast::SimpleCheck -- ignoring outdated Subinode: subinode=%s  nProtocolVersion=%d\n", vin.prevout.ToStringShort(), nProtocolVersion);
        return false;
    }

    CScript pubkeyScript;
    pubkeyScript = GetScriptForDestination(pubKeyCollateralAddress.GetID());

    if (pubkeyScript.size() != 25) {
        //LogPrint("CSubinodeBroadcast::SimpleCheck -- pubKeyCollateralAddress has the wrong size\n");
        nDos = 100;
        return false;
    }

    CScript pubkeyScript2;
    pubkeyScript2 = GetScriptForDestination(pubKeySubinode.GetID());

    if (pubkeyScript2.size() != 25) {
        //LogPrint("CSubinodeBroadcast::SimpleCheck -- pubKeySubinode has the wrong size\n");
        nDos = 100;
        return false;
    }

    if (!vin.scriptSig.empty()) {
        //LogPrint("CSubinodeBroadcast::SimpleCheck -- Ignore Not Empty ScriptSig %s\n", vin.ToString());
        nDos = 100;
        return false;
    }

    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if (addr.GetPort() != mainnetDefaultPort) return false;
    } else if (addr.GetPort() == mainnetDefaultPort) return false;

    return true;
}

bool CSubinodeBroadcast::Update(CSubinode *pmn, int &nDos) {
    nDos = 0;

    if (pmn->sigTime == sigTime && !fRecovery) {
        // mapSeenSubinodeBroadcast in CSubinodeMan::CheckMnbAndUpdateSubinodeList should filter legit duplicates
        // but this still can happen if we just started, which is ok, just do nothing here.
        return false;
    }

    // this broadcast is older than the one that we already have - it's bad and should never happen
    // unless someone is doing something fishy
    if (pmn->sigTime > sigTime) {
        //LogPrint("CSubinodeBroadcast::Update -- Bad sigTime %d (existing broadcast is at %d) for Subinode %s %s\n",
                  //sigTime, pmn->sigTime, vin.prevout.ToStringShort(), addr.ToString());
        return false;
    }

    pmn->Check();

    // subinode is banned by PoSe
    if (pmn->IsPoSeBanned()) {
        //LogPrint("CSubinodeBroadcast::Update -- Banned by PoSe, subinode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    // IsVnAssociatedWithPubkey is validated once in CheckOutpoint, after that they just need to match
    if (pmn->pubKeyCollateralAddress != pubKeyCollateralAddress) {
        //LogPrint("CSubinodeBroadcast::Update -- Got mismatched pubKeyCollateralAddress and vin\n");
        nDos = 33;
        return false;
    }

    if (!CheckSignature(nDos)) {
        //LogPrint("CSubinodeBroadcast::Update -- CheckSignature() failed, subinode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    // if ther was no subinode broadcast recently or if it matches our Subinode privkey...
    if (!pmn->IsBroadcastedWithin(SUBINODE_MIN_MNB_SECONDS) || (fSubiNode && pubKeySubinode == activeSubinode.pubKeySubinode)) {
        // take the newest entry
        //LogPrint("CSubinodeBroadcast::Update -- Got UPDATED Subinode entry: addr=%s\n", addr.ToString());
        if (pmn->UpdateFromNewBroadcast((*this))) {
            pmn->Check();
            RelaySubiNode();
        }
        subinodeSync.AddedSubinodeList();
    }

    return true;
}

bool CSubinodeBroadcast::CheckOutpoint(int &nDos) {
    // we are a subinode with the same vin (i.e. already activated) and this mnb is ours (matches our Subinode privkey)
    // so nothing to do here for us
    if (fSubiNode && vin.prevout == activeSubinode.vin.prevout && pubKeySubinode == activeSubinode.pubKeySubinode) {
        return false;
    }

    if (!CheckSignature(nDos)) {
        //LogPrint("CSubinodeBroadcast::CheckOutpoint -- CheckSignature() failed, subinode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) {
            // not mnb fault, let it to be checked again later
            //LogPrint("subinode", "CSubinodeBroadcast::CheckOutpoint -- Failed to aquire lock, addr=%s", addr.ToString());
            mnodeman.mapSeenSubinodeBroadcast.erase(GetHash());
            return false;
        }

        /*
        CCoins coins;
        if (!pcoinsTip->GetCoins(vin.prevout.hash, coins) ||
            (unsigned int) vin.prevout.n >= coins.vout.size() ||
            coins.vout[vin.prevout.n].IsNull()) {
            //LogPrint("subinode", "CSubinodeBroadcast::CheckOutpoint -- Failed to find Subinode UTXO, subinode=%s\n", vin.prevout.ToStringShort());
            return false;
        }
        */

        Coin coin;
        if (!pcoinsTip->GetCoin(vin.prevout, coin) ||
            /*(unsigned int) vin.prevout.n >= coin.out || */
            coin.out.IsNull()) {
            //LogPrint("subinode", "CSubinode::Check -- Failed to find Subinode UTXO, subinode=%s\n", vin.prevout.ToStringShort());
            return false;
        }
        if (coin.out.nValue != SUBINODE_COIN_REQUIRED * COIN) {
            //LogPrint("subinode", "CSubinodeBroadcast::CheckOutpoint -- Failed to find Subinode UTXO, subinode=%s\n", vin.prevout.ToStringShort());
            return false;
        }
        if (chainActive.Height() - coin.nHeight + 1 < Params().GetConsensus().nSubinodeMinimumConfirmations) {
            //LogPrint("CSubinodeBroadcast::CheckOutpoint -- Subinode UTXO must have at least %d confirmations, subinode=%s\n",
                      //Params().GetConsensus().nSubinodeMinimumConfirmations, vin.prevout.ToStringShort());
            // maybe we miss few blocks, let this mnb to be checked again later
            mnodeman.mapSeenSubinodeBroadcast.erase(GetHash());
            return false;
        }
    }

    //LogPrint("subinode", "CSubinodeBroadcast::CheckOutpoint -- Subinode UTXO verified\n");

    // make sure the vout that was signed is related to the transaction that spawned the Subinode
    //  - this is expensive, so it's only done once per Subinode
    if (!darkSendSigner.IsVinAssociatedWithPubkey(vin, pubKeyCollateralAddress)) {
        //LogPrint("CSubinodeMan::CheckOutpoint -- Got mismatched pubKeyCollateralAddress and vin\n");
        nDos = 33;
        return false;
    }

    // verify that sig time is legit in past
    // should be at least not earlier than block when 10000 SUBI tx got nSubinodeMinimumConfirmations
    uint256 hashBlock = uint256();
    CTransactionRef tx2;
    GetTransaction(vin.prevout.hash, tx2, Params().GetConsensus(), hashBlock, true);
    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end() && (*mi).second) {
            CBlockIndex *pMNIndex = (*mi).second; // block for 10k SUBI tx -> 1 confirmation
            CBlockIndex *pConfIndex = chainActive[pMNIndex->nHeight + Params().GetConsensus().nSubinodeMinimumConfirmations - 1]; // block where tx got nSubinodeMinimumConfirmations
            if (pConfIndex->GetBlockTime() > sigTime) {
                //LogPrint("CSubinodeBroadcast::CheckOutpoint -- Bad sigTime %d (%d conf block is at %d) for Subinode %s %s\n",
                          //sigTime, Params().GetConsensus().nSubinodeMinimumConfirmations, pConfIndex->GetBlockTime(), vin.prevout.ToStringShort(), addr.ToString());
                return false;
            }
        }
    }

    return true;
}

bool CSubinodeBroadcast::Sign(CKey &keyCollateralAddress) {
    std::string strError;
    std::string strMessage;

    sigTime = GetAdjustedTime();

    strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) +
                 pubKeyCollateralAddress.GetID().ToString() + pubKeySubinode.GetID().ToString() +
                 boost::lexical_cast<std::string>(nProtocolVersion);

    if (!darkSendSigner.SignMessage(strMessage, vchSig, keyCollateralAddress)) {
        //LogPrint("CSubinodeBroadcast::Sign -- SignMessage() failed\n");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError)) {
        //LogPrint("CSubinodeBroadcast::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CSubinodeBroadcast::CheckSignature(int &nDos) {
    std::string strMessage;
    std::string strError = "";
    nDos = 0;

    strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) +
                 pubKeyCollateralAddress.GetID().ToString() + pubKeySubinode.GetID().ToString() +
                 boost::lexical_cast<std::string>(nProtocolVersion);

    //LogPrint("subinode", "CSubinodeBroadcast::CheckSignature -- strMessage: %s  pubKeyCollateralAddress address: %s  sig: %s\n", strMessage, CBitcoinAddress(pubKeyCollateralAddress.GetID()).ToString(), EncodeBase64(&vchSig[0], vchSig.size()));

    if (!darkSendSigner.VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError)) {
        //LogPrint("CSubinodeBroadcast::CheckSignature -- Got bad Subinode announce signature, error: %s\n", strError);
        nDos = 100;
        return false;
    }

    return true;
}

void CSubinodeBroadcast::RelaySubiNode() {
    //LogPrint("CSubinodeBroadcast::RelaySubiNode\n");
    CInv inv(MSG_SUBINODE_ANNOUNCE, GetHash());
    g_connman->RelayInv(inv);
}

CSubinodePing::CSubinodePing(CTxIn &vinNew) {
    LOCK(cs_main);
    if (!chainActive.Tip() || chainActive.Height() < 12) return;

    vin = vinNew;
    blockHash = chainActive[chainActive.Height() - 12]->GetBlockHash();
    sigTime = GetAdjustedTime();
    vchSig = std::vector < unsigned char > ();
}

bool CSubinodePing::Sign(CKey &keySubinode, CPubKey &pubKeySubinode) {
    std::string strError;
    std::string strSubiNodeSignMessage;

    sigTime = GetAdjustedTime();
    std::string strMessage = vin.ToString() + blockHash.ToString() + boost::lexical_cast<std::string>(sigTime);

    if (!darkSendSigner.SignMessage(strMessage, vchSig, keySubinode)) {
        //LogPrint("CSubinodePing::Sign -- SignMessage() failed\n");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(pubKeySubinode, vchSig, strMessage, strError)) {
        //LogPrint("CSubinodePing::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CSubinodePing::CheckSignature(CPubKey &pubKeySubinode, int &nDos) {
    std::string strMessage = vin.ToString() + blockHash.ToString() + boost::lexical_cast<std::string>(sigTime);
    std::string strError = "";
    nDos = 0;

    if (!darkSendSigner.VerifyMessage(pubKeySubinode, vchSig, strMessage, strError)) {
        //LogPrint("CSubinodePing::CheckSignature -- Got bad Subinode ping signature, subinode=%s, error: %s\n", vin.prevout.ToStringShort(), strError);
        nDos = 33;
        return false;
    }
    return true;
}

bool CSubinodePing::SimpleCheck(int &nDos) {
    // don't ban by default
    nDos = 0;

    if (sigTime > GetAdjustedTime() + 60 * 60) {
        //LogPrint("CSubinodePing::SimpleCheck -- Signature rejected, too far into the future, subinode=%s\n", vin.prevout.ToStringShort());
        nDos = 1;
        return false;
    }

    {
//        LOCK(cs_main);
        AssertLockHeld(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(blockHash);
        if (mi == mapBlockIndex.end()) {
            //LogPrint("subinode", "CSubinodePing::SimpleCheck -- Subinode ping is invalid, unknown block hash: subinode=%s blockHash=%s\n", vin.prevout.ToStringShort(), blockHash.ToString());
            // maybe we stuck or forked so we shouldn't ban this node, just fail to accept this ping
            // TODO: or should we also request this block?
            return false;
        }
    }
    //LogPrint("subinode", "CSubinodePing::SimpleCheck -- Subinode ping verified: subinode=%s  blockHash=%s  sigTime=%d\n", vin.prevout.ToStringShort(), blockHash.ToString(), sigTime);
    return true;
}

bool CSubinodePing::CheckAndUpdate(CSubinode *pmn, bool fFromNewBroadcast, int &nDos) {
    // don't ban by default
    nDos = 0;

    if (!SimpleCheck(nDos)) {
        return false;
    }

    if (pmn == NULL) {
        //LogPrint("subinode", "CSubinodePing::CheckAndUpdate -- Couldn't find Subinode entry, subinode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    if (!fFromNewBroadcast) {
        if (pmn->IsUpdateRequired()) {
            //LogPrint("subinode", "CSubinodePing::CheckAndUpdate -- subinode protocol is outdated, subinode=%s\n", vin.prevout.ToStringShort());
            return false;
        }

        if (pmn->IsNewStartRequired()) {
            //LogPrint("subinode", "CSubinodePing::CheckAndUpdate -- subinode is completely expired, new start is required, subinode=%s\n", vin.prevout.ToStringShort());
            return false;
        }
    }

    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(blockHash);
        if ((*mi).second && (*mi).second->nHeight < chainActive.Height() - 24) {
            //LogPrint("CSubinodePing::CheckAndUpdate -- Subinode ping is invalid, block hash is too old: subinode=%s  blockHash=%s\n", vin.prevout.ToStringShort(), blockHash.ToString());
            // nDos = 1;
            return false;
        }
    }

    //LogPrint("subinode", "CSubinodePing::CheckAndUpdate -- New ping: subinode=%s  blockHash=%s  sigTime=%d\n", vin.prevout.ToStringShort(), blockHash.ToString(), sigTime);

    // //LogPrint("mnping - Found corresponding mn for vin: %s\n", vin.prevout.ToStringShort());
    // update only if there is no known ping for this subinode or
    // last ping was more then SUBINODE_MIN_MNP_SECONDS-60 ago comparing to this one
    if (pmn->IsPingedWithin(SUBINODE_MIN_MNP_SECONDS - 60, sigTime)) {
        //LogPrint("subinode", "CSubinodePing::CheckAndUpdate -- Subinode ping arrived too early, subinode=%s\n", vin.prevout.ToStringShort());
        //nDos = 1; //disable, this is happening frequently and causing banned peers
        return false;
    }

    if (!CheckSignature(pmn->pubKeySubinode, nDos)) return false;

    // so, ping seems to be ok

    // if we are still syncing and there was no known ping for this mn for quite a while
    // (NOTE: assuming that SUBINODE_EXPIRATION_SECONDS/2 should be enough to finish mn list sync)
    if (!subinodeSync.IsSubinodeListSynced() && !pmn->IsPingedWithin(SUBINODE_EXPIRATION_SECONDS / 2)) {
        // let's bump sync timeout
        //LogPrint("subinode", "CSubinodePing::CheckAndUpdate -- bumping sync timeout, subinode=%s\n", vin.prevout.ToStringShort());
        subinodeSync.AddedSubinodeList();
    }

    // let's store this ping as the last one
    //LogPrint("subinode", "CSubinodePing::CheckAndUpdate -- Subinode ping accepted, subinode=%s\n", vin.prevout.ToStringShort());
    pmn->lastPing = *this;

    // and update subinodeman.mapSeenSubinodeBroadcast.lastPing which is probably outdated
    CSubinodeBroadcast mnb(*pmn);
    uint256 hash = mnb.GetHash();
    if (mnodeman.mapSeenSubinodeBroadcast.count(hash)) {
        mnodeman.mapSeenSubinodeBroadcast[hash].second.lastPing = *this;
    }

    pmn->Check(true); // force update, ignoring cache
    if (!pmn->IsEnabled()) return false;

    //LogPrint("subinode", "CSubinodePing::CheckAndUpdate -- Subinode ping acceepted and relayed, subinode=%s\n", vin.prevout.ToStringShort());
    Relay();

    return true;
}

void CSubinodePing::Relay() {
    CInv inv(MSG_SUBINODE_PING, GetHash());
    g_connman->RelayInv(inv);
}

void CSubinode::UpdateWatchdogVoteTime() {
    LOCK(cs);
    nTimeLastWatchdogVote = GetTime();
}
