// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2018-2019 The Subi Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <consensus/merkle.h>

#include <tinyformat.h>
#include <util.h>
#include <utilstrencodings.h>
#include <assert.h>
#include "arith_uint256.h"
#include <utilmoneystr.h>

#include <chainparamsseeds.h>

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << nBits << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "Violent Delights Lead to Violent Ends: Rebuilding After the Crypto Crash 12/14/2018";
    const CScript genesisOutputScript = CScript();
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    consensus.vDeployments[d].nStartTime = nStartTime;
    consensus.vDeployments[d].nTimeout = nTimeout;
}

CAmount GetInitialRewards(int nHeight, const Consensus::Params& consensusParams)
{
    int halvings = nHeight / consensusParams.nSubsidyHalvingInterval;

    if (halvings >= 64)
        return 0;

    CAmount nSubsidy = 45 * COIN;
    nSubsidy >>= halvings;

    if(nHeight == 1)
        nSubsidy = 200000 * COIN;

    if(nSubsidy < (1 * COIN))
        nSubsidy = 1*COIN;

    return nSubsidy;
}

int64_t CChainParams::GetCoinYearReward(int64_t nTime) const
{
    static const int64_t nSecondsInYear = 365 * 24 * 60 * 60;

    if (strNetworkID == "main")
    {
        return nCoinYearReward;
    }
    else if (strNetworkID != "regtest")
    {
        int64_t nYearsSinceGenesis = (nTime - genesis.nTime) / nSecondsInYear;

        if (nYearsSinceGenesis >= 0 && nYearsSinceGenesis < 3)
            return (5 - nYearsSinceGenesis) * CENT;
    }

    return nCoinYearReward;
}

int64_t CChainParams::GetProofOfStakeReward(const CBlockIndex *pindexPrev, int64_t nFees, bool allowInitial) const
{
    int64_t nSubsidy;

    if(!pindexPrev->IsProofOfStake()){
        CAmount nTotal = pindexPrev->nHeight * GetInitialRewards(pindexPrev->nHeight, Params().GetConsensus()) + GetInitialRewards(1, Params().GetConsensus());
        nSubsidy = (nTotal / COIN) * (5 * CENT) / (365 * 24 * (60 * 60 / nTargetSpacing));

    }else{
        nSubsidy = (pindexPrev->nMoneySupply / COIN) * GetCoinYearReward(pindexPrev->nTime) / (365 * 24 * (60 * 60 / nTargetSpacing));
    }

    if(allowInitial && pindexPrev->IsProofOfStake()){
        nSubsidy = (pindexPrev->nMoneySupply / COIN) * (5 * CENT) / (365 * 24 * (60 * 60 / nTargetSpacing));
    }

    return nSubsidy + nFees;
}

class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.nSubsidyHalvingInterval = 262080;
        consensus.BIP16Height = 0;
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256S("0x4c71cd3781718fac59ac635fd92b0c4938e9ffc1ca6ab3e4129c05b6135fd373");
        consensus.BIP65Height = 0;
        consensus.BIP66Height = 0;
        consensus.powLimit = uint256S("0x00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetSpacing = 60; 
        consensus.nPowTargetTimespan = consensus.nPowTargetSpacing;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1; 
        consensus.nMinerConfirmationWindow = 2;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1475020800; 
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1549756800; 

        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1462060800;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1549756800; 

        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1479168000;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1549756800; 

        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000005e064c0ee1b08");

        consensus.defaultAssumeValid = uint256S("0xfbf8b23c34bcd72b43364140f1e31ee805f46778caaba9d91c4c9d4cd09e72c4");

        consensus.nSubinodeMinimumConfirmations = 1;
        consensus.nSubinodePaymentsStartBlock = 1000; 
        consensus.nSubinodeInitialize = 999;
        consensus.nPosTimeActivation = 1548979200;
        consensus.nPosHeightActivate = 75000;
        nModifierInterval = 10 * 60;    
        nTargetSpacing = 60;
        nTargetTimespan = 24 * 60;

        consensus.nCoinMaturityReductionHeight = 999999;
        consensus.nStartShadeFeeDistribution = 75000;
        consensus.nShadeFeeDistributionCycle = 720;

        nMaxTipAge = 30 * 60 * 60;

        nPoolMaxTransactions = 3;
        nFulfilledRequestExpireTime = 60*60;
        strSporkPubKey = "04f968dad1e77969b7bb839d840da8dd5404582a7fbeee07d6a5ef6e1e02f0e722f05431ba3acfa0cd05268e2568890c1b149ec98b57bf971530a23861a75a58a0";
        strSubinodePaymentsPubKey = "04f968dad1e77969b7bb839d840da8dd5404582a7fbeee07d6a5ef6e1e02f0e722f05431ba3acfa0cd05268e2568890c1b149ec98b57bf971530a23861a75a58a0";

        pchMessageStart[0] = 0xa1;
        pchMessageStart[1] = 0xca;
        pchMessageStart[2] = 0xd4;
        pchMessageStart[3] = 0x5a;
        nDefaultPort = 5335;
        nBIP44ID = 0x800000cc;
        nPruneAfterHeight = 0;

        genesis = CreateGenesisBlock(1544832000, 1077149, 0x1e0ffff0, 1, 10 * COIN);

        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0xdf87e28509af333eb243782afcb5cf4a1f49364b355d4d3c7f78ecbb5c522d27"));
        assert(genesis.hashMerkleRoot == uint256S("0x9999ed4164bb6a643e373aa5dcfca3dd9a21b0c6741fcb99caa8b6cde5866165"));

        vSeeds.emplace_back("us1.subi.io");
        vSeeds.emplace_back("tokyo1.subi.io");
        vSeeds.emplace_back("uk1.subi.io");
		vSeeds.emplace_back("ru1.subi.io");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,75);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,63);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[PUBKEY_ADDRESS_256] = std::vector<unsigned char>(1,57);
        base58Prefixes[SCRIPT_ADDRESS_256] = {0x3d};
        base58Prefixes[STEALTH_ADDRESS]    = {0x4b}; 
        base58Prefixes[EXT_PUBLIC_KEY]     = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY]     = {0x04, 0x88, 0xAD, 0xE4};
        base58Prefixes[EXT_KEY_HASH]       = {0x4b};
        base58Prefixes[EXT_ACC_HASH]       = {0x17};
        base58Prefixes[EXT_PUBLIC_KEY_BTC] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY_BTC] = {0x04, 0x88, 0xAD, 0xE4};

        bech32Prefixes[PUBKEY_ADDRESS].assign       ("sh","sh"+2);
        bech32Prefixes[SCRIPT_ADDRESS].assign       ("sr","sr"+2);
        bech32Prefixes[PUBKEY_ADDRESS_256].assign   ("sl","sl"+2);
        bech32Prefixes[SCRIPT_ADDRESS_256].assign   ("sj","sj"+2);
        bech32Prefixes[SECRET_KEY].assign           ("sx","sx"+2);
        bech32Prefixes[EXT_PUBLIC_KEY].assign       ("sen","sen"+3);
        bech32Prefixes[EXT_SECRET_KEY].assign       ("sex","sex"+3);
        bech32Prefixes[STEALTH_ADDRESS].assign      ("sg","sg"+2);
        bech32Prefixes[EXT_KEY_HASH].assign         ("sek","sek"+3);
        bech32Prefixes[EXT_ACC_HASH].assign         ("sea","sea"+3);

        bech32_hrp = "subi";

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        checkpointData = {
            {
                { 0, uint256S("0xdf87e28509af333eb243782afcb5cf4a1f49364b355d4d3c7f78ecbb5c522d27")},
                { 10000, uint256S("0xaa641ebce6ace84550f195142b656b802bad35e37070161d11a977661526c342")},
				{ 40000, uint256S("0x111aaa8a8209871a73b484e52c4e9ad66fb989249302a8d831bf19cec7c449e5")},
            }
        };

        chainTxData = ChainTxData{
            1548050638,
            59844,
            0.02 
        };
    }
};

static CMainParams mainParams;

class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nSubsidyHalvingInterval = 262080;
        consensus.BIP16Height = 0; 
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256S("0x");
        consensus.BIP65Height = 0; 
        consensus.BIP66Height = 0;
        consensus.powLimit = uint256S("0x00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 60;
        consensus.nPowTargetSpacing = 120;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1; 
        consensus.nMinerConfirmationWindow = 2; 

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1475020800;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1544865861;

        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1462060800;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1544865861;

        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1479168000; 
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1544865861; 

        consensus.nMinimumChainWork = uint256S("0x0");

        consensus.nSubinodeMinimumConfirmations = 1;
        consensus.nSubinodePaymentsStartBlock = 50;
        consensus.nSubinodeInitialize = 20;

        consensus.nPosTimeActivation = 9999999999; 
        consensus.nPosHeightActivate = 10;
        nModifierInterval = 10 * 60; 
        nTargetSpacing = 60;           
        nTargetTimespan = 24 * 60; 

        consensus.nCoinMaturityReductionHeight = 2;
        consensus.nStartShadeFeeDistribution = 1000;
        consensus.nShadeFeeDistributionCycle = 20;

        nMaxTipAge = 0x7fffffff;

        nPoolMaxTransactions = 3;
        nFulfilledRequestExpireTime = 5*60; 
        strSporkPubKey = "04f4db09ee1508f259b91c0a3e616bddb630a9b6e39c97806c70baad2052aa0e2ccd7273eaebecdd6bb9aea492dec4ef013cff85ce53c424558cff4add7111d9db";
        strSubinodePaymentsPubKey = "04f4db09ee1508f259b91c0a3e616bddb630a9b6e39c97806c70baad2052aa0e2ccd7273eaebecdd6bb9aea492dec4ef013cff85ce53c424558cff4add7111d9db";

        pchMessageStart[0] = 0xd9;
        pchMessageStart[1] = 0xa7;
        pchMessageStart[2] = 0x0e;
        pchMessageStart[3] = 0xb1;
        nDefaultPort = 15335;
        nBIP44ID = 0x80000001;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1544832001, 3024996, 0x1e0ffff0, 1, 10 * COIN);

        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x0d1bb7e419d77c66daf72d8a7f5ba28d43d23c0f9e3dd8101c340f48b7ee8734"));
        assert(genesis.hashMerkleRoot == uint256S("0x9999ed4164bb6a643e373aa5dcfca3dd9a21b0c6741fcb99caa8b6cde5866165"));

        vFixedSeeds.clear();
        vSeeds.clear();

        vSeeds.emplace_back("testnet.sub.io");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,1);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,3);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[PUBKEY_ADDRESS_256] = std::vector<unsigned char>(1,57);
        base58Prefixes[SCRIPT_ADDRESS_256] = {0x3d};
        base58Prefixes[STEALTH_ADDRESS]    = {0x0c}; // G
        base58Prefixes[EXT_PUBLIC_KEY]     = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY]     = {0x04, 0x88, 0xAD, 0xE4};
        base58Prefixes[EXT_KEY_HASH]       = {0x4b}; // X
        base58Prefixes[EXT_ACC_HASH]       = {0x17}; // A
        base58Prefixes[EXT_PUBLIC_KEY_BTC] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY_BTC] = {0x04, 0x88, 0xAD, 0xE4};

        bech32Prefixes[PUBKEY_ADDRESS].assign       ("sh","sh"+2);
        bech32Prefixes[SCRIPT_ADDRESS].assign       ("sr","sr"+2);
        bech32Prefixes[PUBKEY_ADDRESS_256].assign   ("sl","sl"+2);
        bech32Prefixes[SCRIPT_ADDRESS_256].assign   ("sj","sj"+2);
        bech32Prefixes[SECRET_KEY].assign           ("sx","sx"+2);
        bech32Prefixes[EXT_PUBLIC_KEY].assign       ("sen","sen"+3);
        bech32Prefixes[EXT_SECRET_KEY].assign       ("sex","sex"+3);
        bech32Prefixes[STEALTH_ADDRESS].assign      ("sg","sg"+2);
        bech32Prefixes[EXT_KEY_HASH].assign         ("sek","sek"+3);
        bech32Prefixes[EXT_ACC_HASH].assign         ("sea","sea"+3);

        bech32_hrp = "tsubi";

        vFixedSeeds.clear();

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;


        checkpointData = {
            {
                {0, uint256S("0x0d1bb7e419d77c66daf72d8a7f5ba28d43d23c0f9e3dd8101c340f48b7ee8734")},
            }
        };

        chainTxData = ChainTxData{
            1544832001,
            1,
            1
        };

    }
};

static CTestNetParams testNetParams;

class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 262080;
        consensus.BIP16Height = 0; 
        consensus.BIP34Height = 1; 
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 0; 
        consensus.BIP66Height = 0; 
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; 
        consensus.nPowTargetSpacing = 1;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 1; 
        consensus.nMinerConfirmationWindow = 2;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;

        consensus.nSubinodePaymentsStartBlock = 720;
        consensus.nSubinodeInitialize = 600;

        consensus.nPosTimeActivation = 9999999999; 
        consensus.nPosHeightActivate = 500;
        nModifierInterval = 10 * 60;    
        nTargetSpacing = 60;           
        nTargetTimespan = 24 * 60;  

        nMaxTipAge = 30 * 60 * 60; 

        nPoolMaxTransactions = 3;
        nFulfilledRequestExpireTime = 60*60; 
        strSporkPubKey = "04f4db09ee1508f259b91c0a3e616bddb630a9b6e39c97806c70baad2052aa0e2ccd7273eaebecdd6bb9aea492dec4ef013cff85ce53c424558cff4add7111d9db";
        strSubinodePaymentsPubKey = "04f4db09ee1508f259b91c0a3e616bddb630a9b6e39c97806c70baad2052aa0e2ccd7273eaebecdd6bb9aea492dec4ef013cff85ce53c424558cff4add7111d9db";

        consensus.nMinimumChainWork = uint256S("0x00");
        consensus.defaultAssumeValid = uint256S("0x00");

        pchMessageStart[0] = 0xba;
        pchMessageStart[1] = 0xab;
        pchMessageStart[2] = 0xda;
        pchMessageStart[3] = 0xca;
        nDefaultPort = 25335;
        nBIP44ID = 0x80000001;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1544832002, 1203390, 0x1e0ffff0, 1, 10 * COIN);

        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0xff2828cbe24400a4df3e8bd72a87fd6e80a2039d4cb020247d1eed76df80182c"));
        assert(genesis.hashMerkleRoot == uint256S("0x9999ed4164bb6a643e373aa5dcfca3dd9a21b0c6741fcb99caa8b6cde5866165"));

        vFixedSeeds.clear(); 
        vSeeds.clear();     

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;

        checkpointData = {
            {
                {0, uint256S("0xff2828cbe24400a4df3e8bd72a87fd6e80a2039d4cb020247d1eed76df80182c")},
            }
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,3);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,53);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[PUBKEY_ADDRESS_256] = std::vector<unsigned char>(1,57);
        base58Prefixes[SCRIPT_ADDRESS_256] = {0x3d};
        base58Prefixes[STEALTH_ADDRESS]    = {0x0c}; // G
        base58Prefixes[EXT_PUBLIC_KEY]     = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY]     = {0x04, 0x88, 0xAD, 0xE4};
        base58Prefixes[EXT_KEY_HASH]       = {0x4b}; // X
        base58Prefixes[EXT_ACC_HASH]       = {0x17}; // A
        base58Prefixes[EXT_PUBLIC_KEY_BTC] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY_BTC] = {0x04, 0x88, 0xAD, 0xE4};

        bech32Prefixes[PUBKEY_ADDRESS].assign       ("sh","sh"+2);
        bech32Prefixes[SCRIPT_ADDRESS].assign       ("sr","sr"+2);
        bech32Prefixes[PUBKEY_ADDRESS_256].assign   ("sl","sl"+2);
        bech32Prefixes[SCRIPT_ADDRESS_256].assign   ("sj","sj"+2);
        bech32Prefixes[SECRET_KEY].assign           ("sx","sx"+2);
        bech32Prefixes[EXT_PUBLIC_KEY].assign       ("sen","sen"+3);
        bech32Prefixes[EXT_SECRET_KEY].assign       ("sex","sex"+3);
        bech32Prefixes[STEALTH_ADDRESS].assign      ("sg","sg"+2);
        bech32Prefixes[EXT_KEY_HASH].assign         ("sek","sek"+3);
        bech32Prefixes[EXT_ACC_HASH].assign         ("sea","sea"+3);

        bech32_hrp = "rsubi";
    }
};

static CRegTestParams regTestParams;

static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

CChainParams &Params(const std::string &chain) {
    if (chain == CBaseChainParams::MAIN)
        return mainParams;
    else if (chain == CBaseChainParams::TESTNET)
        return testNetParams;
    else if (chain == CBaseChainParams::REGTEST)
        return regTestParams;
    else
        throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

const CChainParams *pParams() {
    return globalChainParams.get();
};
std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if (chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if (chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams());
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    globalChainParams->UpdateVersionBitsParameters(d, nStartTime, nTimeout);
}

bool CChainParams::IsBech32Prefix(const std::vector<unsigned char> &vchPrefixIn) const
{
    for (auto &hrp : bech32Prefixes)
    {
        if (vchPrefixIn == hrp)
            return true;
    };

    return false;
};

bool CChainParams::IsBech32Prefix(const std::vector<unsigned char> &vchPrefixIn, CChainParams::Base58Type &rtype) const
{
    for (size_t k = 0; k < MAX_BASE58_TYPES; ++k)
    {
        auto &hrp = bech32Prefixes[k];
        if (vchPrefixIn == hrp)
        {
            rtype = static_cast<CChainParams::Base58Type>(k);
            return true;
        };
    };

    return false;
};

bool CChainParams::IsBech32Prefix(const char *ps, size_t slen, CChainParams::Base58Type &rtype) const
{
    for (size_t k = 0; k < MAX_BASE58_TYPES; ++k)
    {
        auto &hrp = bech32Prefixes[k];
        size_t hrplen = hrp.size();
        if (hrplen > 0
            && slen > hrplen
            && strncmp(ps, (const char*)&hrp[0], hrplen) == 0)
        {
            rtype = static_cast<CChainParams::Base58Type>(k);
            return true;
        };
    };

    return false;
};
