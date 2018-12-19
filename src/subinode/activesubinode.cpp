// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2018-2019 The Subi Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activesubinode.h"
#include "subinode.h"
#include "subinode-sync.h"
#include "subinodeman.h"
#include "protocol.h"
#include "boost/foreach.hpp"

// Keep track of the active Subinode
CActiveSubinode activeSubinode;

void CActiveSubinode::ManageState() {
    //LogPrint("subinode", "CActiveSubinode::ManageState -- Start\n");
    if (!fSubiNode) {
        //LogPrint("subinode", "CActiveSubinode::ManageState -- Not a subinode, returning\n");
        return;
    }

    if (Params().NetworkIDString() != CBaseChainParams::REGTEST && !subinodeSync.IsBlockchainSynced()) {
        nState = ACTIVE_SUBINODE_SYNC_IN_PROCESS;
        //LogPrint("CActiveSubinode::ManageState -- %s: %s\n", GetStateString(), GetStatus());
        return;
    }

    if (nState == ACTIVE_SUBINODE_SYNC_IN_PROCESS) {
        nState = ACTIVE_SUBINODE_INITIAL;
    }

    //LogPrint("subinode", "CActiveSubinode::ManageState -- status = %s, type = %s, pinger enabled = %d\n",
             //GetStatus(), GetTypeString(), fPingerEnabled);

    if (eType == SUBINODE_UNKNOWN) {
        ManageStateInitial();
    }

    if (eType == SUBINODE_REMOTE) {
        ManageStateRemote();
    } else if (eType == SUBINODE_LOCAL) {
        // Try Remote Start first so the started local subinode can be restarted without recreate subinode broadcast.
        ManageStateRemote();
        if (nState != ACTIVE_SUBINODE_STARTED)
            ManageStateLocal();
    }

    SendSubinodePing();
}

std::string CActiveSubinode::GetStateString() const {
    switch (nState) {
        case ACTIVE_SUBINODE_INITIAL:
            return "INITIAL";
        case ACTIVE_SUBINODE_SYNC_IN_PROCESS:
            return "SYNC_IN_PROCESS";
        case ACTIVE_SUBINODE_INPUT_TOO_NEW:
            return "INPUT_TOO_NEW";
        case ACTIVE_SUBINODE_NOT_CAPABLE:
            return "NOT_CAPABLE";
        case ACTIVE_SUBINODE_STARTED:
            return "STARTED";
        default:
            return "UNKNOWN";
    }
}

std::string CActiveSubinode::GetStatus() const {
    switch (nState) {
        case ACTIVE_SUBINODE_INITIAL:
            return "Node just started, not yet activated";
        case ACTIVE_SUBINODE_SYNC_IN_PROCESS:
            return "Sync in progress. Must wait until sync is complete to start Subinode";
        case ACTIVE_SUBINODE_INPUT_TOO_NEW:
            return strprintf("Subinode input must have at least %d confirmations",
                             Params().GetConsensus().nSubinodeMinimumConfirmations);
        case ACTIVE_SUBINODE_NOT_CAPABLE:
            return "Not capable subinode: " + strNotCapableReason;
        case ACTIVE_SUBINODE_STARTED:
            return "Subinode successfully started";
        default:
            return "Unknown";
    }
}

std::string CActiveSubinode::GetTypeString() const {
    std::string strType;
    switch (eType) {
        case SUBINODE_UNKNOWN:
            strType = "UNKNOWN";
            break;
        case SUBINODE_REMOTE:
            strType = "REMOTE";
            break;
        case SUBINODE_LOCAL:
            strType = "LOCAL";
            break;
        default:
            strType = "UNKNOWN";
            break;
    }
    return strType;
}

bool CActiveSubinode::SendSubinodePing() {
    if (!fPingerEnabled) {
        //LogPrint("subinode",
                 //"CActiveSubinode::SendSubinodePing -- %s: subinode ping service is disabled, skipping...\n",
                 //GetStateString());
        return false;
    }

    if (!mnodeman.Has(vin)) {
        strNotCapableReason = "Subinode not in subinode list";
        nState = ACTIVE_SUBINODE_NOT_CAPABLE;
        //LogPrint("CActiveSubinode::SendSubinodePing -- %s: %s\n", GetStateString(), strNotCapableReason);
        return false;
    }

    CSubinodePing mnp(vin);
    if (!mnp.Sign(keySubinode, pubKeySubinode)) {
        //LogPrint("CActiveSubinode::SendSubinodePing -- ERROR: Couldn't sign Subinode Ping\n");
        return false;
    }

    // Update lastPing for our subinode in Subinode list
    if (mnodeman.IsSubinodePingedWithin(vin, SUBINODE_MIN_MNP_SECONDS, mnp.sigTime)) {
        //LogPrint("CActiveSubinode::SendSubinodePing -- Too early to send Subinode Ping\n");
        return false;
    }

    mnodeman.SetSubinodeLastPing(vin, mnp);

    //LogPrint("CActiveSubinode::SendSubinodePing -- Relaying ping, collateral=%s\n", vin.ToString());
    mnp.Relay();

    return true;
}

void CActiveSubinode::ManageStateInitial() {
    //LogPrint("subinode", "CActiveSubinode::ManageStateInitial -- status = %s, type = %s, pinger enabled = %d\n",
             //GetStatus(), GetTypeString(), fPingerEnabled);

    // Check that our local network configuration is correct
    if (!fListen) {
        // listen option is probably overwritten by smth else, no good
        nState = ACTIVE_SUBINODE_NOT_CAPABLE;
        strNotCapableReason = "Subinode must accept connections from outside. Make sure listen configuration option is not overwritten by some another parameter.";
        //LogPrint("CActiveSubinode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    bool fFoundLocal = false;
    {
        LOCK(g_connman->cs_vNodes);

        // First try to find whatever local address is specified by externalip option
        fFoundLocal = GetLocal(service) && CSubinode::IsValidNetAddr(service);
        if (!fFoundLocal) {
            // nothing and no live connections, can't do anything for now
            if (g_connman->vNodes.empty()) {
                nState = ACTIVE_SUBINODE_NOT_CAPABLE;
                strNotCapableReason = "Can't detect valid external address. Will retry when there are some connections available.";
                //LogPrint("CActiveSubinode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
                return;
            }
            // We have some peers, let's try to find our local address from one of them
            BOOST_FOREACH(CNode * pnode, g_connman->vNodes)
            {
                if (pnode->fSuccessfullyConnected && pnode->addr.IsIPv4()) {
                    fFoundLocal = GetLocal(service, &pnode->addr) && CSubinode::IsValidNetAddr(service);
                    if (fFoundLocal) break;
                }
            }
        }
    }

    if (!fFoundLocal) {
        nState = ACTIVE_SUBINODE_NOT_CAPABLE;
        strNotCapableReason = "Can't detect valid external address. Please consider using the externalip configuration option if problem persists. Make sure to use IPv4 address only.";
        //LogPrint("CActiveSubinode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if (service.GetPort() != mainnetDefaultPort) {
            nState = ACTIVE_SUBINODE_NOT_CAPABLE;
            strNotCapableReason = strprintf("Invalid port: %u - only %d is supported on mainnet.", service.GetPort(),
                                            mainnetDefaultPort);
            //LogPrint("CActiveSubinode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
    } else if (service.GetPort() == mainnetDefaultPort) {
        nState = ACTIVE_SUBINODE_NOT_CAPABLE;
        strNotCapableReason = strprintf("Invalid port: %u - %d is only supported on mainnet.", service.GetPort(),
                                        mainnetDefaultPort);
        //LogPrint("CActiveSubinode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    //LogPrint("CActiveSubinode::ManageStateInitial -- Checking inbound connection to '%s'\n", service.ToString());
    //TODO
    if (!g_connman->ConnectNode(CAddress(service, NODE_NETWORK), NULL, false, true)) {
        nState = ACTIVE_SUBINODE_NOT_CAPABLE;
        strNotCapableReason = "Could not connect to " + service.ToString();
        //LogPrint("CActiveSubinode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    // Default to REMOTE
    eType = SUBINODE_REMOTE;

    // Check if wallet funds are available
    if (!vpwallets.front()) {
        //LogPrint("CActiveSubinode::ManageStateInitial -- %s: Wallet not available\n", GetStateString());
        return;
    }

    if (vpwallets.front()->IsLocked()) {
        //LogPrint("CActiveSubinode::ManageStateInitial -- %s: Wallet is locked\n", GetStateString());
        return;
    }

    if (vpwallets.front()->GetBalance() < SUBINODE_COIN_REQUIRED * COIN) {
        //LogPrint("CActiveSubinode::ManageStateInitial -- %s: Wallet balance is < 10000 SUBI\n", GetStateString());
        return;
    }

    // Choose coins to use
    CPubKey pubKeyCollateral;
    CKey keyCollateral;

    // If collateral is found switch to LOCAL mode

    if (vpwallets.front()->GetSubinodeVinAndKeys(vin, pubKeyCollateral, keyCollateral)) {
        eType = SUBINODE_LOCAL;
    }

    //LogPrint("subinode", "CActiveSubinode::ManageStateInitial -- End status = %s, type = %s, pinger enabled = %d\n",
            // GetStatus(), GetTypeString(), fPingerEnabled);
}

void CActiveSubinode::ManageStateRemote() {
    //LogPrint("subinode",
             "CActiveSubinode::ManageStateRemote -- Start status = %s, type = %s, pinger enabled = %d, pubKeySubinode.GetID() = %s\n",
             //GetStatus(), fPingerEnabled, GetTypeString(), pubKeySubinode.GetID().ToString());

    mnodeman.CheckSubinode(pubKeySubinode);
    subinode_info_t infoMn = mnodeman.GetSubinodeInfo(pubKeySubinode);
    if (infoMn.fInfoValid) {
        if (infoMn.nProtocolVersion != PROTOCOL_VERSION) {
            nState = ACTIVE_SUBINODE_NOT_CAPABLE;
            strNotCapableReason = "Invalid protocol version";
            //LogPrint("CActiveSubinode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if (service != infoMn.addr) {
            nState = ACTIVE_SUBINODE_NOT_CAPABLE;
            // //LogPrint("service: %s\n", service.ToString());
            // //LogPrint("infoMn.addr: %s\n", infoMn.addr.ToString());
            strNotCapableReason = "Broadcasted IP doesn't match our external address. Make sure you issued a new broadcast if IP of this subinode changed recently.";
            //LogPrint("CActiveSubinode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if (!CSubinode::IsValidStateForAutoStart(infoMn.nActiveState)) {
            nState = ACTIVE_SUBINODE_NOT_CAPABLE;
            strNotCapableReason = strprintf("Subinode in %s state", CSubinode::StateToString(infoMn.nActiveState));
            //LogPrint("CActiveSubinode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if (nState != ACTIVE_SUBINODE_STARTED) {
            //LogPrint("CActiveSubinode::ManageStateRemote -- STARTED!\n");
            vin = infoMn.vin;
            service = infoMn.addr;
            fPingerEnabled = true;
            nState = ACTIVE_SUBINODE_STARTED;
        }
    } else {
        nState = ACTIVE_SUBINODE_NOT_CAPABLE;
        strNotCapableReason = "Subinode not in subinode list";
        //LogPrint("CActiveSubinode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
    }
}

void CActiveSubinode::ManageStateLocal() {
    //LogPrint("subinode", "CActiveSubinode::ManageStateLocal -- status = %s, type = %s, pinger enabled = %d\n",
             //GetStatus(), GetTypeString(), fPingerEnabled);
    if (nState == ACTIVE_SUBINODE_STARTED) {
        return;
    }

    // Choose coins to use
    CPubKey pubKeyCollateral;
    CKey keyCollateral;

    if (vpwallets.front()->GetSubinodeVinAndKeys(vin, pubKeyCollateral, keyCollateral)) {
        int nInputAge = GetInputAge(vin);
        if (nInputAge < Params().GetConsensus().nSubinodeMinimumConfirmations) {
            nState = ACTIVE_SUBINODE_INPUT_TOO_NEW;
            strNotCapableReason = strprintf(_("%s - %d confirmations"), GetStatus(), nInputAge);
            //LogPrint("CActiveSubinode::ManageStateLocal -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }

        {
            LOCK(vpwallets.front()->cs_wallet);
            vpwallets.front()->LockCoin(vin.prevout);
        }

        CSubinodeBroadcast mnb;
        std::string strError;
        if (!CSubinodeBroadcast::Create(vin, service, keyCollateral, pubKeyCollateral, keySubinode,
                                     pubKeySubinode, strError, mnb)) {
            nState = ACTIVE_SUBINODE_NOT_CAPABLE;
            strNotCapableReason = "Error creating mastenode broadcast: " + strError;
            //LogPrint("CActiveSubinode::ManageStateLocal -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }

        fPingerEnabled = true;
        nState = ACTIVE_SUBINODE_STARTED;

        //update to subinode list
        //LogPrint("CActiveSubinode::ManageStateLocal -- Update Subinode List\n");
        mnodeman.UpdateSubinodeList(mnb);
        mnodeman.NotifySubinodeUpdates();

        //send to all peers
        //LogPrint("CActiveSubinode::ManageStateLocal -- Relay broadcast, vin=%s\n", vin.ToString());
        mnb.RelaySubiNode();
    }
}
