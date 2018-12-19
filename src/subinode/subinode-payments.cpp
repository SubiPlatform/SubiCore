// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2018-2019 The Subi Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activesubinode.h"
#include "darksend.h"
#include "subinode-payments.h"
#include "subinode-sync.h"
#include "subinodeman.h"
#include "netfulfilledman.h"
#include "spork.h"
#include "util.h"
#include "netmessagemaker.h"
#include "chainparams.h"

#include <boost/lexical_cast.hpp>

/** Object for who's going to get paid on which blocks */
CSubinodePayments mnpayments;

CCriticalSection cs_vecPayees;
CCriticalSection cs_mapSubinodeBlocks;
CCriticalSection cs_mapSubinodePaymentVotes;

/**
* IsBlockValueValid
*
*   Determine if coinbase outgoing created money is the correct value
*
*   Why is this needed?
*   - In Dash some blocks are superblocks, which output much higher amounts of coins
*   - Otherblocks are 10% lower in outgoing value, so in total, no extra coins are created
*   - When non-superblocks are detected, the normal schedule should be maintained
*/

bool IsBlockValueValid(const CBlock &block, int nBlockHeight, CAmount blockReward, std::string &strErrorRet) {
    strErrorRet = "";
    bool isBlockRewardValueMet = block.vtx[0]->IsCoinStake() ? true :  (block.vtx[0]->GetValueOut() <= blockReward);
    //LogPrintf("IsBlockValueValid(): value-out=%llf, block-reward=%llf \n", block.vtx[0]->GetValueOut(), blockReward);
    //if (fDebug) //LogPrint("block.vtx[0].GetValueOut() %lld <= blockReward %lld\n", block.vtx[0]->GetValueOut(), blockReward);


    if (!subinodeSync.IsSynced(chainActive.Height())) {

        if (!isBlockRewardValueMet) {
            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, only regular blocks are allowed at this height",
                                    nBlockHeight, block.vtx[0]->GetValueOut(), blockReward);
        }
        // it MUST be a regular block otherwise
        return isBlockRewardValueMet;
    }

    // we are synced, let's try to check as much data as we can

    if (sporkManager.IsSporkActive(SPORK_9_SUPERBLOCKS_ENABLED)) {
    } else {
//        // should NOT allow superblocks at all, when superblocks are disabled
        //LogPrint("gobject", "IsBlockValueValid -- Superblocks are disabled, no superblocks allowed\n");
        if (!isBlockRewardValueMet) {
            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, superblocks are disabled",
                                    nBlockHeight, block.vtx[0]->GetValueOut(), blockReward);
        }
    }

    // it MUST be a regular block
    return isBlockRewardValueMet;
}

bool IsBlockPayeeValid(const CTransaction &txNew, int nBlockHeight, CAmount blockReward) {
    // we can only check subinode payment /
    const Consensus::Params &consensusParams = Params().GetConsensus();

    if (nBlockHeight < consensusParams.nSubinodePaymentsStartBlock) {
        //there is no budget data to use to check anything, let's just accept the longest chain
        //if (fDebug) //LogPrint("IsBlockPayeeValid -- subinode isn't start\n");
        return true;
    }
    if (!subinodeSync.IsSynced(chainActive.Height())) {
        //there is no budget data to use to check anything, let's just accept the longest chain
        //if (fDebug) //LogPrint("IsBlockPayeeValid -- WARNING: Client not synced, skipping block payee checks\n");
        return true;
    }

    //check for subinode payee
    if (mnpayments.IsTransactionValid(txNew, nBlockHeight)) {
        //LogPrint("mnpayments", "IsBlockPayeeValid -- Valid subinode payment at height %d: %s", nBlockHeight, txNew.ToString());
        return true;
    } else {
        if(sporkManager.IsSporkActive(SPORK_8_SUBINODE_PAYMENT_ENFORCEMENT)){
            return false;
        } else {
            //LogPrint("SubiNode payment enforcement is disabled, accepting block\n");
            return true;
        }
    }
}

void FillBlockPayments(CMutableTransaction &txNew, int nBlockHeight, CAmount subinodePayment, CTxOut &txoutSubinodeRet, std::vector <CTxOut> &voutSuperblockRet) {

    // FILL BLOCK PAYEE WITH SUBINODE PAYMENT OTHERWISE
    mnpayments.FillBlockPayee(txNew, nBlockHeight, subinodePayment, txoutSubinodeRet);
    //LogPrint("mnpayments", "FillBlockPayments -- nBlockHeight %d subinodePayment %lld txoutSubinodeRet %s txNew %s",
             //nBlockHeight, subinodePayment, txoutSubinodeRet.ToString(), txNew.ToString());
}

std::string GetRequiredPaymentsString(int nBlockHeight) {
    // IF WE HAVE A ACTIVATED TRIGGER FOR THIS HEIGHT - IT IS A SUPERBLOCK, GET THE REQUIRED PAYEES
//    if(CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
//        return CSuperblockManager::GetRequiredPaymentsString(nBlockHeight);
//    }

    // OTHERWISE, PAY SUBINODE
    return mnpayments.GetRequiredPaymentsString(nBlockHeight);
}

void CSubinodePayments::Clear() {
    LOCK2(cs_mapSubinodeBlocks, cs_mapSubinodePaymentVotes);
    mapSubinodeBlocks.clear();
    mapSubinodePaymentVotes.clear();
}

bool CSubinodePayments::CanVote(COutPoint outSubinode, int nBlockHeight) {
    LOCK(cs_mapSubinodePaymentVotes);

    if (mapSubinodesLastVote.count(outSubinode) && mapSubinodesLastVote[outSubinode] == nBlockHeight) {
        return false;
    }

    //record this subinode voted
    mapSubinodesLastVote[outSubinode] = nBlockHeight;
    return true;
}

std::string CSubinodePayee::ToString() const {
    CTxDestination address1;
    ExtractDestination(scriptPubKey, address1);
    CBitcoinAddress address2(address1);
    std::string str;
    str += "(address: ";
    str += address2.ToString();
    str += ")\n";
    return str;
}

/**
*   FillBlockPayee
*
*   Fill Subinode ONLY payment block
*/

void CSubinodePayments::FillBlockPayee(CMutableTransaction &txNew, int nBlockHeight, CAmount subinodePayment, CTxOut &txoutSubinodeRet) {
    // make sure it's not filled yet
    txoutSubinodeRet = CTxOut();

    CScript payee;
    bool foundMaxVotedPayee = true;

    if (!mnpayments.GetBlockPayee(nBlockHeight, payee)) {
        // no subinode detected...
        //LogPrint("No SubiNode detected...\n");
        foundMaxVotedPayee = false;
        int nCount = 0;
        CSubinode *winningNode = mnodeman.GetNextSubinodeInQueueForPayment(nBlockHeight, true, nCount);
        if (!winningNode) {
            // ...and we can't calculate it on our own
            //LogPrint("CSubinodePayments::FillBlockPayee -- Failed to detect subinode to pay\n");
            return;
        }
        // fill payee with locally calculated winner and hope for the best
        payee = GetScriptForDestination(winningNode->pubKeyCollateralAddress.GetID());
        //LogPrint("payee=%s\n", winningNode->ToString());
    }
    txoutSubinodeRet = CTxOut(subinodePayment, payee);
    txNew.vout.push_back(txoutSubinodeRet);

    CTxDestination address1;
    ExtractDestination(payee, address1);
    CBitcoinAddress address2(address1);
    if (foundMaxVotedPayee) {
        //LogPrint("CSubinodePayments::FillBlockPayee::foundMaxVotedPayee -- Subinode payment %lld to %s\n", subinodePayment, address2.ToString());
    } else {
        //LogPrint("CSubinodePayments::FillBlockPayee -- Subinode payment %lld to %s\n", subinodePayment, address2.ToString());
    }

}

int CSubinodePayments::GetMinSubinodePaymentsProto() {
    if(chainActive.Height() > Params().GetConsensus().nStartShadeFeeDistribution)
        return MIN_SUBINODE_PAYMENT_PROTO_VERSION_2;

    return sporkManager.IsSporkActive(SPORK_10_SUBINODE_PAY_UPDATED_NODES)
           ? MIN_SUBINODE_PAYMENT_PROTO_VERSION_2
           : MIN_SUBINODE_PAYMENT_PROTO_VERSION_1;
}

void CSubinodePayments::ProcessMessage(CNode *pfrom, std::string &strCommand, CDataStream &vRecv) {

    //LogPrintf("CSubinodePayments::ProcessMessage strCommand=%s\n", strCommand);
    // Ignore any payments messages until subinode list is synced
    if (!subinodeSync.IsSubinodeListSynced()) return;

    if (fLiteMode) return; // disable all Dash specific functionality

    if (strCommand == NetMsgType::SUBINODEPAYMENTSYNC) { //Subinode Payments Request Sync

        // Ignore such requests until we are fully synced.
        // We could start processing this after subinode list is synced
        // but this is a heavy one so it's better to finish sync first.
        if (!subinodeSync.IsSynced(chainActive.Height())) return;

        int nCountNeeded;
        vRecv >> nCountNeeded;

        if (netfulfilledman.HasFulfilledRequest(pfrom->addr, NetMsgType::SUBINODEPAYMENTSYNC)) {
            // Asking for the payments list multiple times in a short period of time is no good
            //LogPrintf("SUBINODEPAYMENTSYNC -- peer already asked me for the list\n");
            Misbehaving(pfrom->GetId(), 20);
            return;
        }
        netfulfilledman.AddFulfilledRequest(pfrom->addr, NetMsgType::SUBINODEPAYMENTSYNC);

        Sync(pfrom);
        //LogPrintf("mnpayments SUBINODEPAYMENTSYNC -- Sent Subinode payment votes to peer \n");

    } else if (strCommand == NetMsgType::SUBINODEPAYMENTVOTE) { // Subinode Payments Vote for the Winner

        CSubinodePaymentVote vote;
        vRecv >> vote;

        if (pfrom->nVersion < GetMinSubinodePaymentsProto()) return;

        if (!pCurrentBlockIndex) return;

        uint256 nHash = vote.GetHash();

        pfrom->setAskFor.erase(nHash);

        {
            LOCK(cs_mapSubinodePaymentVotes);
            if (mapSubinodePaymentVotes.count(nHash)) {
                //LogPrintf("mnpayments SUBINODEPAYMENTVOTE -- nHeight=%d seen\n", pCurrentBlockIndex->nHeight);
                return;
            }

            // Avoid processing same vote multiple times
            mapSubinodePaymentVotes[nHash] = vote;
            // but first mark vote as non-verified,
            // AddPaymentVote() below should take care of it if vote is actually ok
            mapSubinodePaymentVotes[nHash].MarkAsNotVerified();
        }

        int nFirstBlock = pCurrentBlockIndex->nHeight - GetStorageLimit();
        if (vote.nBlockHeight < nFirstBlock || vote.nBlockHeight > pCurrentBlockIndex->nHeight + 20) {
            //LogPrintf("mnpaymentsSUBINODEPAYMENTVOTE -- vote out of range: nFirstBlock=%d, nBlockHeight=%d, nHeight=%d\n", nFirstBlock, vote.nBlockHeight, pCurrentBlockIndex->nHeight);
            return;
        }

        std::string strError = "";
        if (!vote.IsValid(pfrom, pCurrentBlockIndex->nHeight, strError)) {
            //LogPrintf("mnpayments SUBINODEPAYMENTVOTE -- invalid message, error: %s\n", strError);
            return;
        }

        if (!CanVote(vote.vinSubinode.prevout, vote.nBlockHeight)) {
            //LogPrintf("SUBINODEPAYMENTVOTE -- subinode already voted, subinode\n");
            return;
        }

        subinode_info_t mnInfo = mnodeman.GetSubinodeInfo(vote.vinSubinode);
        if (!mnInfo.fInfoValid) {
            // mn was not found, so we can't check vote, some info is probably missing
            //LogPrintf("SUBINODEPAYMENTVOTE -- subinode is missing \n");
            mnodeman.AskForMN(pfrom, vote.vinSubinode);
            return;
        }

        int nDos = 0;
        if (!vote.CheckSignature(mnInfo.pubKeySubinode, pCurrentBlockIndex->nHeight, nDos)) {
            if (nDos) {
                //LogPrintf("SUBINODEPAYMENTVOTE -- ERROR: invalid signature\n");
                Misbehaving(pfrom->GetId(), nDos);
            } else {
                // only warn about anything non-critical (i.e. nDos == 0) in debug mode
                //LogPrintf("mnpayments SUBINODEPAYMENTVOTE -- WARNING: invalid signature\n");
            }
            // Either our info or vote info could be outdated.
            // In case our info is outdated, ask for an update,
            mnodeman.AskForMN(pfrom, vote.vinSubinode);
            // but there is nothing we can do if vote info itself is outdated
            // (i.e. it was signed by a mn which changed its key),
            // so just quit here.
            return;
        }

        CTxDestination address1;
        ExtractDestination(vote.payee, address1);
        CBitcoinAddress address2(address1);

        //LogPrintf("mnpayments SUBINODEPAYMENTVOTE -- vote: address=%s, nBlockHeight=%d, nHeight=%d, prevout=%s\n", address2.ToString(), vote.nBlockHeight, pCurrentBlockIndex->nHeight, vote.vinSubinode.prevout.ToStringShort());

        if (AddPaymentVote(vote)) {
            vote.Relay();
            subinodeSync.AddedPaymentVote();
        }
    }
}

bool CSubinodePaymentVote::Sign() {
    std::string strError;
    std::string strMessage = vinSubinode.prevout.ToStringShort() +
                             boost::lexical_cast<std::string>(nBlockHeight) +
                             ScriptToAsmStr(payee);

    if (!darkSendSigner.SignMessage(strMessage, vchSig, activeSubinode.keySubinode)) {
        //LogPrint("CSubinodePaymentVote::Sign -- SignMessage() failed\n");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(activeSubinode.pubKeySubinode, vchSig, strMessage, strError)) {
        //LogPrint("CSubinodePaymentVote::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CSubinodePayments::GetBlockPayee(int nBlockHeight, CScript &payee) {
    if (mapSubinodeBlocks.count(nBlockHeight)) {
        return mapSubinodeBlocks[nBlockHeight].GetBestPayee(payee);
    }

    return false;
}

// Is this subinode scheduled to get paid soon?
// -- Only look ahead up to 8 blocks to allow for propagation of the latest 2 blocks of votes
bool CSubinodePayments::IsScheduled(CSubinode &mn, int nNotBlockHeight) {
    LOCK(cs_mapSubinodeBlocks);

    if (!pCurrentBlockIndex) return false;

    CScript mnpayee;
    mnpayee = GetScriptForDestination(mn.pubKeyCollateralAddress.GetID());

    CScript payee;
    for (int64_t h = pCurrentBlockIndex->nHeight; h <= pCurrentBlockIndex->nHeight + 8; h++) {
        if (h == nNotBlockHeight) continue;
        if (mapSubinodeBlocks.count(h) && mapSubinodeBlocks[h].GetBestPayee(payee) && mnpayee == payee) {
            return true;
        }
    }

    return false;
}

bool CSubinodePayments::AddPaymentVote(const CSubinodePaymentVote &vote) {
    //LogPrintf("\nsubinode-payments CSubinodePayments::AddPaymentVote\n");
    uint256 blockHash = uint256();
    if (!GetBlockHash(blockHash, vote.nBlockHeight - 100)){
        LogPrintf("\nsubinode-payments CSubinodePayments::Invalid Hash\n");
        return false;
    }
    if (HasVerifiedPaymentVote(vote.GetHash())) return false;

    LOCK2(cs_mapSubinodeBlocks, cs_mapSubinodePaymentVotes);

    mapSubinodePaymentVotes[vote.GetHash()] = vote;

    if (!mapSubinodeBlocks.count(vote.nBlockHeight)) {
        CSubinodeBlockPayees blockPayees(vote.nBlockHeight);
        mapSubinodeBlocks[vote.nBlockHeight] = blockPayees;
    }

    mapSubinodeBlocks[vote.nBlockHeight].AddPayee(vote);

    return true;
}

bool CSubinodePayments::HasVerifiedPaymentVote(uint256 hashIn) {
    LOCK(cs_mapSubinodePaymentVotes);
    std::map<uint256, CSubinodePaymentVote>::iterator it = mapSubinodePaymentVotes.find(hashIn);
    return it != mapSubinodePaymentVotes.end() && it->second.IsVerified();
}

void CSubinodeBlockPayees::AddPayee(const CSubinodePaymentVote &vote) {
    LOCK(cs_vecPayees);

    BOOST_FOREACH(CSubinodePayee & payee, vecPayees)
    {
        if (payee.GetPayee() == vote.payee) {
            payee.AddVoteHash(vote.GetHash());
            return;
        }
    }
    CSubinodePayee payeeNew(vote.payee, vote.GetHash());
    vecPayees.push_back(payeeNew);
}

bool CSubinodeBlockPayees::GetBestPayee(CScript &payeeRet) {
    LOCK(cs_vecPayees);
    //LogPrint("mnpayments", "CSubinodeBlockPayees::GetBestPayee, vecPayees.size()=%s\n", vecPayees.size());
    if (!vecPayees.size()) {
        //LogPrint("mnpayments", "CSubinodeBlockPayees::GetBestPayee -- ERROR: couldn't find any payee\n");
        return false;
    }

    int nVotes = -1;
    BOOST_FOREACH(CSubinodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() > nVotes) {
            payeeRet = payee.GetPayee();
            nVotes = payee.GetVoteCount();
        }
    }

    return (nVotes > -1);
}

bool CSubinodeBlockPayees::HasPayeeWithVotes(CScript payeeIn, int nVotesReq) {
    LOCK(cs_vecPayees);

    BOOST_FOREACH(CSubinodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() >= nVotesReq && payee.GetPayee() == payeeIn) {
            return true;
        }
    }

//    //LogPrint("mnpayments", "CSubinodeBlockPayees::HasPayeeWithVotes -- ERROR: couldn't find any payee with %d+ votes\n", nVotesReq);
    return false;
}

bool CSubinodeBlockPayees::IsTransactionValid(const CTransaction &txNew) {
    LOCK(cs_vecPayees);

    int nMaxSignatures = 0;
    std::string strPayeesPossible = "";

    CAmount nSubinodePayment = GetSubinodePayment(nBlockHeight, txNew.GetValueOut());

    //require at least MNPAYMENTS_SIGNATURES_REQUIRED signatures

    BOOST_FOREACH(CSubinodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() >= nMaxSignatures) {
            nMaxSignatures = payee.GetVoteCount();
        }
    }

    // if we don't have at least MNPAYMENTS_SIGNATURES_REQUIRED signatures on a payee, approve whichever is the longest chain
    if (nMaxSignatures < MNPAYMENTS_SIGNATURES_REQUIRED) return true;

    bool hasValidPayee = false;

    BOOST_FOREACH(CSubinodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() >= MNPAYMENTS_SIGNATURES_REQUIRED) {
            hasValidPayee = true;

            BOOST_FOREACH(CTxOut txout, txNew.vout) {
                if (payee.GetPayee() == txout.scriptPubKey && nSubinodePayment == txout.nValue) {
                    //LogPrint("mnpayments", "CSubinodeBlockPayees::IsTransactionValid -- Found required payment\n");
                    return true;
                }
            }

            CTxDestination address1;
            ExtractDestination(payee.GetPayee(), address1);
            CBitcoinAddress address2(address1);

            if (strPayeesPossible == "") {
                strPayeesPossible = address2.ToString();
            } else {
                strPayeesPossible += "," + address2.ToString();
            }
        }
    }

    if (!hasValidPayee) return true;

    return false;
}

std::string CSubinodeBlockPayees::GetRequiredPaymentsString() {
    LOCK(cs_vecPayees);

    std::string strRequiredPayments = "Unknown";

    BOOST_FOREACH(CSubinodePayee & payee, vecPayees)
    {
        CTxDestination address1;
        ExtractDestination(payee.GetPayee(), address1);
        CBitcoinAddress address2(address1);

        if (strRequiredPayments != "Unknown") {
            strRequiredPayments += ", " + address2.ToString() + ":" + boost::lexical_cast<std::string>(payee.GetVoteCount());
        } else {
            strRequiredPayments = address2.ToString() + ":" + boost::lexical_cast<std::string>(payee.GetVoteCount());
        }
    }

    return strRequiredPayments;
}

std::string CSubinodePayments::GetRequiredPaymentsString(int nBlockHeight) {
    LOCK(cs_mapSubinodeBlocks);

    if (mapSubinodeBlocks.count(nBlockHeight)) {
        return mapSubinodeBlocks[nBlockHeight].GetRequiredPaymentsString();
    }

    return "Unknown";
}

bool CSubinodePayments::IsTransactionValid(const CTransaction &txNew, int nBlockHeight) {
    LOCK(cs_mapSubinodeBlocks);

    if (mapSubinodeBlocks.count(nBlockHeight)) {
        return mapSubinodeBlocks[nBlockHeight].IsTransactionValid(txNew);
    }

    return true;
}

void CSubinodePayments::CheckAndRemove() {
    if (!pCurrentBlockIndex) return;

    LOCK2(cs_mapSubinodeBlocks, cs_mapSubinodePaymentVotes);

    int nLimit = GetStorageLimit();

    std::map<uint256, CSubinodePaymentVote>::iterator it = mapSubinodePaymentVotes.begin();
    while (it != mapSubinodePaymentVotes.end()) {
        CSubinodePaymentVote vote = (*it).second;

        if (pCurrentBlockIndex->nHeight - vote.nBlockHeight > nLimit) {
            //LogPrint("mnpayments", "CSubinodePayments::CheckAndRemove -- Removing old Subinode payment: nBlockHeight=%d\n", vote.nBlockHeight);
            mapSubinodePaymentVotes.erase(it++);
            mapSubinodeBlocks.erase(vote.nBlockHeight);
        } else {
            ++it;
        }
    }
    //LogPrint("CSubinodePayments::CheckAndRemove -- %s\n", ToString());
}

bool CSubinodePaymentVote::IsValid(CNode *pnode, int nValidationHeight, std::string &strError) {
    CSubinode *pmn = mnodeman.Find(vinSubinode);

    if (!pmn) {
        strError = strprintf("Unknown Subinode: prevout=%s", vinSubinode.prevout.ToStringShort());
        // Only ask if we are already synced and still have no idea about that Subinode
        if (subinodeSync.IsSubinodeListSynced()) {
            mnodeman.AskForMN(pnode, vinSubinode);
        }

        return false;
    }

    int nMinRequiredProtocol;
    if (nBlockHeight >= nValidationHeight) {
        // new votes must comply SPORK_10_SUBINODE_PAY_UPDATED_NODES rules
        nMinRequiredProtocol = mnpayments.GetMinSubinodePaymentsProto();
    } else {
        // allow non-updated subinodes for old blocks
        nMinRequiredProtocol = MIN_SUBINODE_PAYMENT_PROTO_VERSION_1;
    }

    if (pmn->nProtocolVersion < nMinRequiredProtocol) {
        strError = strprintf("Subinode protocol is too old: nProtocolVersion=%d, nMinRequiredProtocol=%d", pmn->nProtocolVersion, nMinRequiredProtocol);
        return false;
    }

    // Only subinodes should try to check subinode rank for old votes - they need to pick the right winner for future blocks.
    // Regular clients (miners included) need to verify subinode rank for future block votes only.
    if (!fSubiNode && nBlockHeight < nValidationHeight) return true;

    int nRank = mnodeman.GetSubinodeRank(vinSubinode, nBlockHeight - 100, nMinRequiredProtocol, false);

    if (nRank == -1) {
        //LogPrint("mnpayments", "CSubinodePaymentVote::IsValid -- Can't calculate rank for subinode %s\n",
                 //vinSubinode.prevout.ToStringShort());
        return false;
    }

    if (nRank > MNPAYMENTS_SIGNATURES_TOTAL) {
        // It's common to have subinodes mistakenly think they are in the top 10
        // We don't want to print all of these messages in normal mode, debug mode should print though
        strError = strprintf("Subinode is not in the top %d (%d)", MNPAYMENTS_SIGNATURES_TOTAL, nRank);
        // Only ban for new mnw which is out of bounds, for old mnw MN list itself might be way too much off
        if (nRank > MNPAYMENTS_SIGNATURES_TOTAL * 2 && nBlockHeight > nValidationHeight) {
            strError = strprintf("Subinode is not in the top %d (%d)", MNPAYMENTS_SIGNATURES_TOTAL * 2, nRank);
            //LogPrint("CSubinodePaymentVote::IsValid -- Error: %s\n", strError);
            Misbehaving(pnode->GetId(), 20);
        }
        // Still invalid however
        return false;
    }

    return true;
}

bool CSubinodePayments::ProcessBlock(int nBlockHeight) {

    // DETERMINE IF WE SHOULD BE VOTING FOR THE NEXT PAYEE

    if (fLiteMode || !fSubiNode) {
        return false;
    }

    // We have little chances to pick the right winner if winners list is out of sync
    // but we have no choice, so we'll try. However it doesn't make sense to even try to do so
    // if we have not enough data about subinodes.
    if (!subinodeSync.IsSubinodeListSynced()) {
        return false;
    }

    int nRank = mnodeman.GetSubinodeRank(activeSubinode.vin, nBlockHeight - 100, GetMinSubinodePaymentsProto(), false);

    if (nRank == -1) {
        LogPrintf("mnpayments CSubinodePayments::ProcessBlock -- Unknown Subinode\n");
        return false;
    }

    if (nRank > MNPAYMENTS_SIGNATURES_TOTAL) {
        LogPrintf("mnpayments CSubinodePayments::ProcessBlock -- Subinode not in the top %d (%d)\n", MNPAYMENTS_SIGNATURES_TOTAL, nRank);
        return false;
    }

    // LOCATE THE NEXT SUBINODE WHICH SHOULD BE PAID

    //LogPrintf("CSubinodePayments::ProcessBlock -- Start: nBlockHeight=%d, subinode=%s\n", nBlockHeight, activeSubinode.vin.prevout.ToStringShort());

    // pay to the oldest MN that still had no payment but its input is old enough and it was active long enough
    int nCount = 0;
    CSubinode *pmn = mnodeman.GetNextSubinodeInQueueForPayment(nBlockHeight, true, nCount);

    if (pmn == NULL) {
        LogPrintf("CSubinodePayments::ProcessBlock -- ERROR: Failed to find subinode to pay\n");
        return false;
    }

    //LogPrintf("CSubinodePayments::ProcessBlock -- Subinode found by GetNextSubinodeInQueueForPayment(): %s\n", pmn->vin.prevout.ToStringShort());


    CScript payee = GetScriptForDestination(pmn->pubKeyCollateralAddress.GetID());

    CSubinodePaymentVote voteNew(activeSubinode.vin, nBlockHeight, payee);

    CTxDestination address1;
    ExtractDestination(payee, address1);
    CBitcoinAddress address2(address1);

    // SIGN MESSAGE TO NETWORK WITH OUR SUBINODE KEYS

    //LogPrintf("ProcessBlock -- vote: address=%s, nBlockHeight=%d, nHeight=%d, prevout=%s\n", address2.ToString(), voteNew.nBlockHeight, pCurrentBlockIndex->nHeight, voteNew.vinSubinode.prevout.ToStringShort());

    if (voteNew.Sign()) {
        if (AddPaymentVote(voteNew)) {
            voteNew.Relay();
            return true;
        }
    }

    return false;
}

void CSubinodePaymentVote::Relay() {
    // do not relay until synced
    if (!subinodeSync.IsWinnersListSynced()) {
        //LogPrint("CSubinodePaymentVote::Relay - subinodeSync.IsWinnersListSynced() not sync\n");
        return;
    }
    CInv inv(MSG_SUBINODE_PAYMENT_VOTE, GetHash());
    g_connman->RelayInv(inv);
}

bool CSubinodePaymentVote::CheckSignature(const CPubKey &pubKeySubinode, int nValidationHeight, int &nDos) {
    // do not ban by default
    nDos = 0;

    std::string strMessage = vinSubinode.prevout.ToStringShort() +
                             boost::lexical_cast<std::string>(nBlockHeight) +
                             ScriptToAsmStr(payee);

    std::string strError = "";
    if (!darkSendSigner.VerifyMessage(pubKeySubinode, vchSig, strMessage, strError)) {
        // Only ban for future block vote when we are already synced.
        // Otherwise it could be the case when MN which signed this vote is using another key now
        // and we have no idea about the old one.
        if (subinodeSync.IsSubinodeListSynced() && nBlockHeight > nValidationHeight) {
            nDos = 20;
        }
        return error("CSubinodePaymentVote::CheckSignature -- Got bad Subinode payment signature, subinode=%s, error: %s", vinSubinode.prevout.ToStringShort().c_str(), strError);
    }

    return true;
}

std::string CSubinodePaymentVote::ToString() const {
    std::ostringstream info;

    info << vinSubinode.prevout.ToStringShort() <<
         ", " << nBlockHeight <<
         ", " << ScriptToAsmStr(payee) <<
         ", " << (int) vchSig.size();

    return info.str();
}

// Send only votes for future blocks, node should request every other missing payment block individually
void CSubinodePayments::Sync(CNode *pnode) {
    LOCK(cs_mapSubinodeBlocks);

    if (!pCurrentBlockIndex) return;

    int nInvCount = 0;

    for (int h = pCurrentBlockIndex->nHeight; h < pCurrentBlockIndex->nHeight + 20; h++) {
        if (mapSubinodeBlocks.count(h)) {
            BOOST_FOREACH(CSubinodePayee & payee, mapSubinodeBlocks[h].vecPayees)
            {
                std::vector <uint256> vecVoteHashes = payee.GetVoteHashes();
                BOOST_FOREACH(uint256 & hash, vecVoteHashes)
                {
                    if (!HasVerifiedPaymentVote(hash)) continue;
                    pnode->PushInventory(CInv(MSG_SUBINODE_PAYMENT_VOTE, hash));
                    nInvCount++;
                }
            }
        }
    }

    //LogPrint("CSubinodePayments::Sync -- Sent %d votes to peer %d\n", nInvCount, pnode->GetId());
    const CNetMsgMaker msgMaker(pnode->GetSendVersion());
    g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::SYNCSTATUSCOUNT, SUBINODE_SYNC_MNW, nInvCount));
}

// Request low data/unknown payment blocks in batches directly from some node instead of/after preliminary Sync.
void CSubinodePayments::RequestLowDataPaymentBlocks(CNode *pnode) {
    if (!pCurrentBlockIndex) return;

    LOCK2(cs_main, cs_mapSubinodeBlocks);

    std::vector <CInv> vToFetch;
    int nLimit = GetStorageLimit();

    const CBlockIndex *pindex = pCurrentBlockIndex;

    while (pCurrentBlockIndex->nHeight - pindex->nHeight < nLimit) {
        if (!mapSubinodeBlocks.count(pindex->nHeight)) {
            // We have no idea about this block height, let's ask
            vToFetch.push_back(CInv(MSG_SUBINODE_PAYMENT_BLOCK, pindex->GetBlockHash()));
            // We should not violate GETDATA rules
            if (vToFetch.size() == MAX_INV_SZ) {
                //LogPrint("CSubinodePayments::SyncLowDataPaymentBlocks -- asking peer %d for %d blocks\n", pnode->GetId(), MAX_INV_SZ);
                const CNetMsgMaker msgMaker(pnode->GetSendVersion());
                g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::GETDATA, vToFetch));
                // Start filling new batch
                vToFetch.clear();
            }
        }
        if (!pindex->pprev) break;
        pindex = pindex->pprev;
    }

    std::map<int, CSubinodeBlockPayees>::iterator it = mapSubinodeBlocks.begin();

    while (it != mapSubinodeBlocks.end()) {
        int nTotalVotes = 0;
        bool fFound = false;
        BOOST_FOREACH(CSubinodePayee & payee, it->second.vecPayees)
        {
            if (payee.GetVoteCount() >= MNPAYMENTS_SIGNATURES_REQUIRED) {
                fFound = true;
                break;
            }
            nTotalVotes += payee.GetVoteCount();
        }
        // A clear winner (MNPAYMENTS_SIGNATURES_REQUIRED+ votes) was found
        // or no clear winner was found but there are at least avg number of votes
        if (fFound || nTotalVotes >= (MNPAYMENTS_SIGNATURES_TOTAL + MNPAYMENTS_SIGNATURES_REQUIRED) / 2) {
            // so just move to the next block
            ++it;
            continue;
        }
        // DEBUG
//        DBG (
//            // Let's see why this failed
//            BOOST_FOREACH(CSubinodePayee& payee, it->second.vecPayees) {
//                CTxDestination address1;
//                ExtractDestination(payee.GetPayee(), address1);
//                CBitcoinAddress address2(address1);
//                printf("payee %s votes %d\n", address2.ToString().c_str(), payee.GetVoteCount());
//            }
//            printf("block %d votes total %d\n", it->first, nTotalVotes);
//        )
        // END DEBUG
        // Low data block found, let's try to sync it
        uint256 hash;
        if (GetBlockHash(hash, it->first)) {
            vToFetch.push_back(CInv(MSG_SUBINODE_PAYMENT_BLOCK, hash));
        }
        // We should not violate GETDATA rules
        if (vToFetch.size() == MAX_INV_SZ) {
            //LogPrint("CSubinodePayments::SyncLowDataPaymentBlocks -- asking peer %d for %d payment blocks\n", pnode->GetId(), MAX_INV_SZ);
            // Start filling new batch
            const CNetMsgMaker msgMaker(pnode->GetSendVersion());
            g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::GETDATA, vToFetch));
            vToFetch.clear();
        }
        ++it;
    }
    // Ask for the rest of it
    if (!vToFetch.empty()) {
        //LogPrint("CSubinodePayments::SyncLowDataPaymentBlocks -- asking peer %d for %d payment blocks\n", pnode->GetId(), vToFetch.size());
        const CNetMsgMaker msgMaker(pnode->GetSendVersion());
        g_connman->PushMessage(pnode, msgMaker.Make(NetMsgType::GETDATA, vToFetch));
    }
}

std::string CSubinodePayments::ToString() const {
    std::ostringstream info;

    info << "Votes: " << (int) mapSubinodePaymentVotes.size() <<
         ", Blocks: " << (int) mapSubinodeBlocks.size();

    return info.str();
}

bool CSubinodePayments::IsEnoughData() {
    float nAverageVotes = (MNPAYMENTS_SIGNATURES_TOTAL + MNPAYMENTS_SIGNATURES_REQUIRED) / 2;
    int nStorageLimit = GetStorageLimit();
    return GetBlockCount() > nStorageLimit && GetVoteCount() > nStorageLimit * nAverageVotes;
}

int CSubinodePayments::GetStorageLimit() {
    return std::max(int(mnodeman.size() * nStorageCoeff), nMinBlocksToStore);
}

void CSubinodePayments::UpdatedBlockTip(const CBlockIndex *pindex) {
    pCurrentBlockIndex = pindex;
    //LogPrint("mnpayments", "CSubinodePayments::UpdatedBlockTip -- pCurrentBlockIndex->nHeight=%d\n", pCurrentBlockIndex->nHeight);
    
    ProcessBlock(pindex->nHeight + 5);
}
