// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2018-2019 The Subi Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ACTIVESUBINODE_H
#define ACTIVESUBINODE_H

#include "net.h"
#include "key.h"
#include "wallet/wallet.h"

class CActiveSubinode;

static const int ACTIVE_SUBINODE_INITIAL          = 0; // initial state
static const int ACTIVE_SUBINODE_SYNC_IN_PROCESS  = 1;
static const int ACTIVE_SUBINODE_INPUT_TOO_NEW    = 2;
static const int ACTIVE_SUBINODE_NOT_CAPABLE      = 3;
static const int ACTIVE_SUBINODE_STARTED          = 4;

extern CActiveSubinode activeSubinode;

// Responsible for activating the Subinode and pinging the network
class CActiveSubinode
{
public:
    enum subinode_type_enum_t {
        SUBINODE_UNKNOWN = 0,
        SUBINODE_REMOTE  = 1,
        SUBINODE_LOCAL   = 2
    };

private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    subinode_type_enum_t eType;

    bool fPingerEnabled;

    /// Ping Subinode
    bool SendSubinodePing();

public:
    // Keys for the active Subinode
    CPubKey pubKeySubinode;
    CKey keySubinode;

    // Initialized while registering Subinode
    CTxIn vin;
    CService service;

    int nState; // should be one of ACTIVE_SUBINODE_XXXX
    std::string strNotCapableReason;

    CActiveSubinode()
        : eType(SUBINODE_UNKNOWN),
          fPingerEnabled(false),
          pubKeySubinode(),
          keySubinode(),
          vin(),
          service(),
          nState(ACTIVE_SUBINODE_INITIAL)
    {}

    /// Manage state of active Subinode
    void ManageState();

    std::string GetStateString() const;
    std::string GetStatus() const;
    std::string GetTypeString() const;

private:
    void ManageStateInitial();
    void ManageStateRemote();
    void ManageStateLocal();
};

#endif
