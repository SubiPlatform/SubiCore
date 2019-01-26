# Copyright (c) 2018-2019 The Subi Core developers
# Copyright (c) 2013-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

DIST_SUBDIRS = secp256k1 univalue tor

AM_LDFLAGS = $(PTHREAD_CFLAGS) $(LIBTOOL_LDFLAGS) $(HARDENED_LDFLAGS)
AM_CXXFLAGS = $(HARDENED_CXXFLAGS) $(ERROR_CXXFLAGS)
AM_CPPFLAGS = $(HARDENED_CPPFLAGS)
EXTRA_LIBRARIES =

if EMBEDDED_UNIVALUE
LIBUNIVALUE = univalue/libunivalue.la

$(LIBUNIVALUE): $(wildcard univalue/lib/*) $(wildcard univalue/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)
else
LIBUNIVALUE = $(UNIVALUE_LIBS)
endif

TOR_LIBS = tor/src/or/libtor.a \
	tor/src/common/libor.a \
	tor/src/common/libor-ctime.a \
	tor/src/common/libor-crypto.a \
	tor/src/common/libor-event.a \
	tor/src/trunnel/libor-trunnel.a \
	tor/src/common/libcurve25519_donna.a \
	tor/src/ext/ed25519/donna/libed25519_donna.a \
	tor/src/ext/ed25519/ref10/libed25519_ref10.a \
	tor/src/ext/keccak-tiny/libkeccak-tiny.a

tor.timestamp: $(wildcard tor/src/or/*.c) $(wildcard tor/src/or/*.h) \
		$(wildcard tor/src/common/*.c) $(wildcard tor/src/common/*.h) \
		$(wildcard tor/src/trunnel/*.c) $(wildcard tor/src/trunnel/*.h) \
		$(wildcard tor/src/ext/*.c) $(wildcard tor/src/ext/*.h)
	@rm -f tor.timestamp.tmp
	@touch tor.timestamp.tmp
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C tor
	@mv -f tor.timestamp.tmp tor.timestamp

$(TOR_LIBS): tor.timestamp
	@if test -f $@; then :; else \
		rm -f tor.timestamp; \
		$(MAKE) $(AM_MAKEFLAGS) tor.timestamp; \
	fi


SUBI_INCLUDES=-I$(builddir) $(BDB_CPPFLAGS) $(BOOST_CPPFLAGS) $(LEVELDB_CPPFLAGS) $(CRYPTO_CFLAGS) $(SSL_CFLAGS)

SUBI_INCLUDES += -I$(srcdir)/secp256k1/include
SUBI_INCLUDES += $(UNIVALUE_CFLAGS)

LIBSUBI_SERVER=libsubi_server.a
LIBSUBI_COMMON=libsubi_common.a
LIBSUBI_CONSENSUS=libsubi_consensus.a
LIBSUBI_CLI=libsubi_cli.a
LIBSUBI_UTIL=libsubi_util.a
LIBSUBI_CRYPTO=crypto/libsubi_crypto.a
LIBSUBIQT=qt/libsubiqt.a
LIBSECP256K1=secp256k1/libsecp256k1.la

if ENABLE_ZMQ
LIBSUBI_ZMQ=libsubi_zmq.a
endif
if BUILD_SUBI_LIBS
LIBSUBICONSENSUS=libsubiconsensus.la
endif
if ENABLE_WALLET
LIBSUBI_WALLET=libsubi_wallet.a
endif
if ENABLE_USBDEVICE
LIBPARTICL_USBDEVICE=libsubi_usbdevice.a
endif

$(LIBSECP256K1): $(wildcard secp256k1/src/*) $(wildcard secp256k1/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)

# Make is not made aware of per-object dependencies to avoid limiting building parallelization
# But to build the less dependent modules first, we manually select their order here:
EXTRA_LIBRARIES += \
  $(LIBSUBI_CRYPTO) \
  $(LIBSUBI_UTIL) \
  $(LIBSUBI_COMMON) \
  $(LIBSUBI_CONSENSUS) \
  $(LIBSUBI_SERVER) \
  $(LIBSUBI_CLI) \
  $(LIBSUBI_WALLET) \
  $(LIBSUBI_USBDEVICE) \
  $(LIBSUBI_ZMQ)

lib_LTLIBRARIES = $(LIBSUBICONSENSUS)

bin_PROGRAMS =
noinst_PROGRAMS =
TESTS =
BENCHMARKS =

if BUILD_SUBID
  bin_PROGRAMS += subid
endif

if BUILD_SUBI_UTILS
  bin_PROGRAMS += subi-cli subi-tx
endif

.PHONY: FORCE check-symbols check-security
# subi core #
SUBI_CORE_H = \
  addrdb.h \
  addressindex.h \
  spentindex.h \
  addrman.h \
  base58.h \
  bech32.h \
  bloom.h \
  blockencodings.h \
  chain.h \
  chainparams.h \
  chainparamsbase.h \
  chainparamsseeds.h \
  checkpoints.h \
  checkqueue.h \
  clientversion.h \
  coins.h \
  compat.h \
  compat/byteswap.h \
  compat/endian.h \
  compat/sanity.h \
  compressor.h \
  consensus/consensus.h \
  consensus/tx_verify.h \
  core_io.h \
  core_memusage.h \
  pos/kernel.h \
  pos/miner.h \
  cuckoocache.h \
  fs.h \
  subinode/activesubinode.h \
  subinode/darksend.h \
  subinode/darksend-relay.h \
  subinode/subinode.h \
  subinode/subinode-payments.h \
  subinode/subinode-sync.h \
  subinode/subinodeman.h \
  subinode/subinodeconfig.h \
  subinode/instantx.h \
  subinode/netfulfilledman.h \
  httprpc.h \
  httpserver.h \
  indirectmap.h \
  init.h \
  key.h \
  keystore.h \
  dbwrapper.h \
  limitedmap.h \
  memusage.h \
  merkleblock.h \
  miner.h \
  net.h \
  net_processing.h \
  netaddress.h \
  netbase.h \
  netmessagemaker.h \
  noui.h \
  policy/feerate.h \
  policy/fees.h \
  policy/policy.h \
  policy/rbf.h \
  pow.h \
  protocol.h \
  random.h \
  reverse_iterator.h \
  reverselock.h \
  rpc/blockchain.h \
  rpc/client.h \
  rpc/mining.h \
  rpc/protocol.h \
  rpc/safemode.h \
  rpc/server.h \
  rpc/register.h \
  rpc/util.h \
  scheduler.h \
  script/sigcache.h \
  script/sign.h \
  script/standard.h \
  script/ismine.h \
  shade-address/commitmentkey.h \
  shade-address/lz4.h \
  shade-address/extkey.h \
  shade-address/keystore.h \
  shade-address/keyutil.h \
  shade-address/mnemonic.h \
  shade-address/stealth.h \
  shade-address/types.h \
  shade-address/xxhash.h \
  shade-address/wordlists/chinese_simplified.h \
  shade-address/wordlists/chinese_traditional.h \
  shade-address/wordlists/english.h \
  shade-address/wordlists/french.h \
  shade-address/wordlists/japanese.h \
  shade-address/wordlists/spanish.h \
  shade-address/wordlists/italian.h \
  shade-address/wordlists/korean.h \
  streams.h \
  support/allocators/secure.h \
  support/allocators/zeroafterfree.h \
  support/cleanse.h \
  support/events.h \
  support/lockedpool.h \
  sync.h \
  threadsafety.h \
  threadinterrupt.h \
  timedata.h \
  torcontrol.h \
  txdb.h \
  txmempool.h \
  ui_interface.h \
  undo.h \
  unilib/uninorms.h \
  unilib/utf8.h \
  util.h \
  utilmoneystr.h \
  utiltime.h \
  validation.h \
  validationinterface.h \
  versionbits.h \
  wallet/coincontrol.h \
  wallet/crypter.h \
  wallet/db.h \
  wallet/feebumper.h \
  wallet/fees.h \
  wallet/init.h \
  wallet/rpcwallet.h \
  wallet/wallet.h \
  wallet/walletdb.h \
  wallet/walletutil.h \
  warnings.h \
  zerocoin/zerocoin.h \
  zmq/zmqabstractnotifier.h \
  zmq/zmqconfig.h\
  zmq/zmqnotificationinterface.h \
  usbdevice/usbdevice.h \
  usbdevice/rpcusbdevice.h \
  zmq/zmqpublishnotifier.h


obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh "$(abs_top_builddir)/src/obj/build.h" \
	  "$(abs_top_srcdir)"
libsubi_util_a-clientversion.$(OBJEXT): obj/build.h

# server: shared between subid and subi-qt
libsubi_server_a_CPPFLAGS = $(AM_CPPFLAGS) $(SUBI_INCLUDES) $(MINIUPNPC_CPPFLAGS) $(EVENT_CFLAGS) $(EVENT_PTHREADS_CFLAGS)
libsubi_server_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libsubi_server_a_SOURCES = \
  addrdb.cpp \
  addressindex.cpp \
  addrman.cpp \
  bloom.cpp \
  blockencodings.cpp \
  chain.cpp \
  checkpoints.cpp \
  consensus/tx_verify.cpp \
  httprpc.cpp \
  httpserver.cpp \
  init.cpp \
  dbwrapper.cpp \
  subinode/rpcsubinode.cpp \
  subinode/netfulfilledman.cpp \
  merkleblock.cpp \
  miner.cpp \
  net.cpp \
  net_processing.cpp \
  noui.cpp \
  policy/fees.cpp \
  policy/policy.cpp \
  policy/rbf.cpp \
  pos/kernel.cpp \
  pow.cpp \
  rest.cpp \
  rpc/blockchain.cpp \
  rpc/mining.cpp \
  rpc/misc.cpp \
  rpc/net.cpp \
  rpc/rawtransaction.cpp \
  rpc/mnemonic.cpp \
  rpc/safemode.cpp \
  rpc/server.cpp \
  script/sigcache.cpp \
  script/ismine.cpp \
  timedata.cpp \
  torcontrol.cpp \
  txdb.cpp \
  txmempool.cpp \
  ui_interface.cpp \
  validation.cpp \
  validationinterface.cpp \
  versionbits.cpp \
  zerocoin/zerocoin.cpp \
  $(SUBI_CORE_H)

if ENABLE_ZMQ
libsubi_zmq_a_CPPFLAGS = $(SUBI_INCLUDES) $(ZMQ_CFLAGS)
libsubi_zmq_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libsubi_zmq_a_SOURCES = \
  zmq/zmqabstractnotifier.cpp \
  zmq/zmqnotificationinterface.cpp \
  zmq/zmqpublishnotifier.cpp
endif

if ENABLE_USBDEVICE
libsubi_usbdevice_a_CPPFLAGS = $(SUBI_INCLUDES) $(AM_CPPFLAGS)
libsubi_usbdevice_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libsubi_usbdevice_a_CFLAGS = -DHAVE_HIDAPI $(AM_CFLAGS)
libsubi_usbdevice_a_SOURCES = \
  usbdevice/ledger/btchipApdu.h \
  usbdevice/ledger/ledgerLayer.h \
  usbdevice/ledger/ledgerLayer.c \
  usbdevice/ledger/dongleCommHidHidapi.h \
  usbdevice/ledger/dongleCommHidHidapi.c \
  usbdevice/usbdevice.cpp \
  usbdevice/debugdevice.h \
  usbdevice/debugdevice.cpp \
  usbdevice/ledgerdevice.h \
  usbdevice/ledgerdevice.cpp \
  usbdevice/rpcusbdevice.cpp
endif


# wallet: shared between subid and subi-qt, but only linked
# when wallet enabled
libsubi_wallet_a_CPPFLAGS = $(AM_CPPFLAGS) $(SUBI_INCLUDES)
libsubi_wallet_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libsubi_wallet_a_SOURCES = \
  subinode/activesubinode.cpp \
  subinode/darksend.cpp \
  subinode/darksend-relay.cpp \
  subinode/subinode.cpp \
  subinode/subinode-payments.cpp \
  subinode/subinode-sync.cpp \
  subinode/subinodeman.cpp \
  subinode/subinodeconfig.cpp \
  subinode/instantx.cpp \
  shade-address/commitmentkey.cpp \
  pos/miner.cpp \
  wallet/crypter.cpp \
  wallet/db.cpp \
  wallet/feebumper.cpp \
  wallet/fees.cpp \
  wallet/init.cpp \
  wallet/rpcdump.cpp \
  wallet/rpcwallet.cpp \
  wallet/wallet.cpp \
  wallet/walletdb.cpp \
  wallet/walletutil.cpp \
  $(SUBI_CORE_H)

# crypto primitives library
crypto_libsubi_crypto_a_CFLAGS = $(AM_CFLAGS) $(PIC_FLAGS)
crypto_libsubi_crypto_a_CPPFLAGS = $(AM_CPPFLAGS)
crypto_libsubi_crypto_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
crypto_libsubi_crypto_a_SOURCES = \
  crypto/aes.cpp \
  crypto/aes.h \
  crypto/chacha20.h \
  crypto/chacha20.cpp \
  crypto/common.h \
  crypto/hmac_sha256.cpp \
  crypto/hmac_sha256.h \
  crypto/hmac_sha512.cpp \
  crypto/hmac_sha512.h \
  crypto/neoscrypt.c \
  crypto/neoscrypt.h \
  crypto/ripemd160.cpp \
  crypto/ripemd160.h \
  crypto/sha1.cpp \
  crypto/sha1.h \
  crypto/sha256.cpp \
  crypto/sha256.h \
  crypto/sha512.cpp \
  crypto/sha512.h \
  crypto/x16r/aes_helper.c \
  crypto/x16r/blake.c \
  crypto/x16r/bmw.c \
  crypto/x16r/cubehash.c \
  crypto/x16r/echo.c \
  crypto/x16r/groestl.c \
  crypto/x16r/jh.c \
  crypto/x16r/keccak.c \
  crypto/x16r/luffa.c \
  crypto/x16r/shavite.c \
  crypto/x16r/simd.c \
  crypto/x16r/skein.c \
  crypto/x16r/sph_hamsi.c \
  crypto/x16r/sph_hamsi_helper.c \
  crypto/x16r/sph_fugue.c \
  crypto/x16r/sph_shabal.c \
  crypto/x16r/sph_whirlpool.c \
  crypto/x16r/sph_sha2.c \
  crypto/x16r/sph_sha512.c \
  crypto/x16r/sph_blake.h \
  crypto/x16r/sph_bmw.h \
  crypto/x16r/sph_cubehash.h \
  crypto/x16r/sph_echo.h \
  crypto/x16r/sph_groestl.h \
  crypto/x16r/sph_jh.h \
  crypto/x16r/sph_keccak.h \
  crypto/x16r/sph_luffa.h \
  crypto/x16r/sph_shavite.h \
  crypto/x16r/sph_simd.h \
  crypto/x16r/sph_skein.h \
  crypto/x16r/sph_hamsi.h \
  crypto/x16r/sph_fugue.h \
  crypto/x16r/sph_shabal.h \
  crypto/x16r/sph_whirlpool.h \
  crypto/x16r/sph_sha2.h \
  crypto/x16r/sph_types.h

if USE_ASM
crypto_libsubi_crypto_a_SOURCES += crypto/sha256_sse4.cpp
endif

# consensus: shared between all executables that validate any consensus rules.
libsubi_consensus_a_CPPFLAGS = $(AM_CPPFLAGS) $(SUBI_INCLUDES)
libsubi_consensus_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libsubi_consensus_a_SOURCES = \
  amount.h \
  arith_uint256.cpp \
  arith_uint256.h \
  consensus/merkle.cpp \
  consensus/merkle.h \
  consensus/params.h \
  consensus/validation.h \
  subinode/flat-database.h \
  subinode/spork.h \
  hash.cpp \
  hash.h \
  prevector.h \
  primitives/block.cpp \
  primitives/block.h \
  primitives/transaction.cpp \
  primitives/transaction.h \
  pubkey.cpp \
  pubkey.h \
  script/subiconsensus.cpp \
  script/interpreter.cpp \
  script/interpreter.h \
  script/script.cpp \
  script/script.h \
  script/script_error.cpp \
  script/script_error.h \
  serialize.h \
  tinyformat.h \
  uint256.cpp \
  uint256.h \
  utilstrencodings.cpp \
  utilstrencodings.h \
  version.h

# common: shared between subid, and subi-qt and non-server tools
libsubi_common_a_CPPFLAGS = $(AM_CPPFLAGS) $(SUBI_INCLUDES)
libsubi_common_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libsubi_common_a_SOURCES = \
  base58.cpp \
  bech32.cpp \
  chainparams.cpp \
  coins.cpp \
  compressor.cpp \
  core_read.cpp \
  core_write.cpp \
  subinode/spork.cpp \
  key.cpp \
  keystore.cpp \
  netaddress.cpp \
  netbase.cpp \
  policy/feerate.cpp \
  protocol.cpp \
  scheduler.cpp \
  script/sign.cpp \
  script/standard.cpp \
  unilib/uninorms.cpp \
  unilib/utf8.cpp \
  shade-address/lz4.c \
  shade-address/extkey.cpp \
  shade-address/keystore.cpp \
  shade-address/keyutil.cpp \
  shade-address/mnemonic.cpp \
  shade-address/stealth.cpp \
  shade-address/xxhash.c \
  warnings.cpp \
  $(SUBI_CORE_H)

# util: shared between all executables.
# This library *must* be included to make sure that the glibc
# backward-compatibility objects and their sanity checks are linked.
libsubi_util_a_CPPFLAGS = $(AM_CPPFLAGS) $(SUBI_INCLUDES)
libsubi_util_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libsubi_util_a_SOURCES = \
  support/lockedpool.cpp \
  chainparamsbase.cpp \
  clientversion.cpp \
  compat/glibc_sanity.cpp \
  compat/glibcxx_sanity.cpp \
  compat/strnlen.cpp \
  libzerocoin/bignum.h \
  libzerocoin/Accumulator.h \
  libzerocoin/Accumulator.cpp \
  libzerocoin/AccumulatorProofOfKnowledge.h \
  libzerocoin/AccumulatorProofOfKnowledge.cpp \
  libzerocoin/Coin.h \
  libzerocoin/Coin.cpp \
  libzerocoin/CoinSpend.h \
  libzerocoin/CoinSpend.cpp \
  libzerocoin/Commitment.h \
  libzerocoin/Commitment.cpp \
  libzerocoin/ParallelTasks.h \
  libzerocoin/ParallelTasks.cpp \
  libzerocoin/ParamGeneration.h \
  libzerocoin/ParamGeneration.cpp \
  libzerocoin/Params.h \
  libzerocoin/Params.cpp \
  libzerocoin/SerialNumberSignatureOfKnowledge.h \
  libzerocoin/SerialNumberSignatureOfKnowledge.cpp \
  libzerocoin/SpendMetaData.h \
  libzerocoin/SpendMetaData.cpp \
  libzerocoin/Zerocoin.h \
  fs.cpp \
  random.cpp \
  rpc/protocol.cpp \
  rpc/util.cpp \
  support/cleanse.cpp \
  sync.cpp \
  threadinterrupt.cpp \
  util.cpp \
  utilmoneystr.cpp \
  utilstrencodings.cpp \
  utiltime.cpp \
  $(SUBI_CORE_H)


if GLIBC_BACK_COMPAT
libsubi_util_a_SOURCES += compat/glibc_compat.cpp
endif

# cli: shared between subi-cli and subi-qt
libsubi_cli_a_CPPFLAGS = $(AM_CPPFLAGS) $(SUBI_INCLUDES)
libsubi_cli_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libsubi_cli_a_SOURCES = \
  rpc/client.cpp \
  $(SUBI_CORE_H)

nodist_libsubi_util_a_SOURCES = $(srcdir)/obj/build.h
#

# subid binary #
subid_SOURCES = subid.cpp
subid_CPPFLAGS = $(AM_CPPFLAGS) $(SUBI_INCLUDES)
subid_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
subid_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
subid_SOURCES += subid-res.rc
endif

subid_LDADD = \
  $(LIBSUBI_SERVER) \
  $(LIBSUBI_COMMON) \
  $(LIBUNIVALUE) \
  $(LIBSUBI_UTIL) \
  $(LIBSUBI_WALLET) \
  $(LIBSUBI_ZMQ) \
  $(LIBSUBI_CONSENSUS) \
  $(LIBSUBI_CRYPTO) \
  $(LIBLEVELDB) \
  $(LIBLEVELDB_SSE42) \
  $(LIBMEMENV) \
  $(LIBSECP256K1) \
  $(LIBSUBI_USBDEVICE)

subid_LDADD += $(TOR_LIBS) $(BOOST_LIBS) $(BDB_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(MINIUPNPC_LIBS) $(EVENT_PTHREADS_LIBS) $(EVENT_LIBS) $(ZMQ_LIBS) $(USB_LIBS) -lz

# subi-cli binary #
subi_cli_SOURCES = subi-cli.cpp
subi_cli_CPPFLAGS = $(AM_CPPFLAGS) $(SUBI_INCLUDES) $(EVENT_CFLAGS)
subi_cli_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
subi_cli_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
subi_cli_SOURCES += subi-cli-res.rc
endif

subi_cli_LDADD = \
  $(LIBSUBI_CLI) \
  $(LIBUNIVALUE) \
  $(LIBSUBI_UTIL) \
  $(LIBSUBI_CRYPTO)

subi_cli_LDADD += $(BOOST_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(EVENT_LIBS)
#

# subi-tx binary #
subi_tx_SOURCES = subi-tx.cpp
subi_tx_CPPFLAGS = $(AM_CPPFLAGS) $(SUBI_INCLUDES)
subi_tx_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
subi_tx_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
subi_tx_SOURCES += subi-tx-res.rc
endif

subi_tx_LDADD = \
  $(LIBUNIVALUE) \
  $(LIBSUBI_COMMON) \
  $(LIBSUBI_UTIL) \
  $(LIBSUBI_CONSENSUS) \
  $(LIBSUBI_CRYPTO) \
  $(LIBSECP256K1)

subi_tx_LDADD += $(BOOST_LIBS) $(CRYPTO_LIBS)
#

# subiconsensus library #
if BUILD_SUBI_LIBS
include_HEADERS = script/subiconsensus.h
libsubiconsensus_la_SOURCES = $(crypto_libsubi_crypto_a_SOURCES) $(libsubi_consensus_a_SOURCES)

if GLIBC_BACK_COMPAT
  libsubiconsensus_la_SOURCES += compat/glibc_compat.cpp
endif

libsubiconsensus_la_LDFLAGS = $(AM_LDFLAGS) -no-undefined $(RELDFLAGS)
libsubiconsensus_la_LIBADD = $(LIBSECP256K1)
libsubiconsensus_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(builddir)/obj -I$(srcdir)/secp256k1/include -DBUILD_SUBI_INTERNAL
libsubiconsensus_la_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)

endif
#

CTAES_DIST =  crypto/ctaes/bench.c
CTAES_DIST += crypto/ctaes/ctaes.c
CTAES_DIST += crypto/ctaes/ctaes.h
CTAES_DIST += crypto/ctaes/README.md
CTAES_DIST += crypto/ctaes/test.c

CLEANFILES = $(EXTRA_LIBRARIES)

CLEANFILES += *.gcda *.gcno
CLEANFILES += compat/*.gcda compat/*.gcno
CLEANFILES += consensus/*.gcda consensus/*.gcno
CLEANFILES += crypto/*.gcda crypto/*.gcno
CLEANFILES += policy/*.gcda policy/*.gcno
CLEANFILES += primitives/*.gcda primitives/*.gcno
CLEANFILES += script/*.gcda script/*.gcno
CLEANFILES += support/*.gcda support/*.gcno
CLEANFILES += univalue/*.gcda univalue/*.gcno
CLEANFILES += wallet/*.gcda wallet/*.gcno
CLEANFILES += wallet/test/*.gcda wallet/test/*.gcno
CLEANFILES += zmq/*.gcda zmq/*.gcno
CLEANFILES += obj/build.h
CLEANFILES += tor.timestamp tor.timestamp.tmp

EXTRA_DIST = $(CTAES_DIST)


config/subi-config.h: config/stamp-h1
	@$(MAKE) -C $(top_builddir) $(subdir)/$(@)
config/stamp-h1: $(top_srcdir)/$(subdir)/config/subi-config.h.in $(top_builddir)/config.status
	$(AM_V_at)$(MAKE) -C $(top_builddir) $(subdir)/$(@)
$(top_srcdir)/$(subdir)/config/subi-config.h.in:  $(am__configure_deps)
	$(AM_V_at)$(MAKE) -C $(top_srcdir) $(subdir)/config/subi-config.h.in

clean-local:
	-$(MAKE) -C secp256k1 clean
	-$(MAKE) -C univalue clean
	-$(MAKE) -C tor clean
	-rm -f leveldb/*/*.gcda leveldb/*/*.gcno leveldb/helpers/memenv/*.gcda leveldb/helpers/memenv/*.gcno
	-rm -f config.h
	-rm -rf test/__pycache__

.rc.o:
	@test -f $(WINDRES)
	## FIXME: How to get the appropriate modulename_CPPFLAGS in here?
	$(AM_V_GEN) $(WINDRES) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS) -DWINDRES_PREPROC -i $< -o $@

check-symbols: $(bin_PROGRAMS)
if GLIBC_BACK_COMPAT
	@echo "Checking glibc back compat..."
	$(AM_V_at) READELF=$(READELF) CPPFILT=$(CPPFILT) $(top_srcdir)/contrib/devtools/symbol-check.py < $(bin_PROGRAMS)
endif

check-security: $(bin_PROGRAMS)
if HARDEN
	@echo "Checking binary security..."
	$(AM_V_at) READELF=$(READELF) OBJDUMP=$(OBJDUMP) $(top_srcdir)/contrib/devtools/security-check.py < $(bin_PROGRAMS)
endif

%.pb.cc %.pb.h: %.proto
	@test -f $(PROTOC)
	$(AM_V_GEN) $(PROTOC) --cpp_out=$(@D) --proto_path=$(<D) $<

if EMBEDDED_LEVELDB
include Makefile.leveldb.include
endif

if ENABLE_TESTS
include Makefile.test.include
endif

if ENABLE_BENCH
include Makefile.bench.include
endif

if ENABLE_QT
include Makefile.qt.include
endif

if ENABLE_QT_TESTS
include Makefile.qttest.include
endif
