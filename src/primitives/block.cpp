// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2019 The Subi Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/block.h>

#include <hash.h>
#include <tinyformat.h>
#include <utilstrencodings.h>
#include <crypto/common.h>
#include <crypto/neoscrypt.h>

#define TIME_MASK 0xffffff80

uint256 CBlockHeader::GetHash() const
{
    return SerializeHash(*this);
}

uint256 CBlockHeader::GetPoWHash(int nHeight) const
{

  uint256 thash;
  unsigned int profile = 0x0;

  if (Params().NetworkIDString() == "testnet")
  {
    if (nHeight >= 5) {
      int32_t nTimeX16r = nTime&TIME_MASK;
      uint256 hashTime = Hash(BEGIN(nTimeX16r), END(nTimeX16r));
      thash = HashX16R(BEGIN(nVersion), END(nNonce), hashTime);
    } else {
      neoscrypt((unsigned char *) &nVersion, (unsigned char *) &thash, profile);
    }
    return thash;
  }
  else
  {
    if (nHeight >= 60000) {
      int32_t nTimeX16r = nTime&TIME_MASK;
      uint256 hashTime = Hash(BEGIN(nTimeX16r), END(nTimeX16r));
      thash = HashX16R(BEGIN(nVersion), END(nNonce), hashTime);
    } else {
      neoscrypt((unsigned char *) &nVersion, (unsigned char *) &thash, profile);
    }
    return thash;
  }
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}

void CBlock::ZerocoinClean() const {
    zerocoinTxInfo = NULL;
}

