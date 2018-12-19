<hr />
<div align="center">
    <img src="images/subi_full.png" alt="Logo" width='300px' height='auto'/>
</div>
<hr />

[![Build Status](https://travis-ci.org/SubiPlatform/subi.svg?branch=master)](https://travis-ci.org/SubiPlatform/SubiCore) <a href="https://discord.gg/7FNBwGB"><img src="https://discordapp.com/api/guilds/523723976799682571/embed.png" alt="Discord server" /><a href="https://twitter.com/intent/follow?screen_name=SubiPlatform"><img src="https://img.shields.io/twitter/follow/SubiPlatform.svg?style=social&logo=twitter" alt="follow on Twitter"></a>

[Website](https://subi.io) — [SubiDesktop](https://github.com/SubiPlatform/SubiDesktop) — [Block Explorer](https://explorer.subi.io/) — [Discord](https://discord.gg/7FNBwGB) — [Forum](https://bitcointalk.org) — [Telegram](https://t.me) — [Twitter](https://twitter.com/SubiPlatform)

Features
=============

* Initial PoW Distribution
* Hybrid SubiNode System
* Hybrid PoS/SubiNode (After Initial Distribution)
* Segwit
* Smart Contracts
* Shade Technology

Subi Specifications
=============

| Specification | Value |
|:-----------|:-----------|
| Algorithm | `Neoscrypt` |
| Developer Fund | `5%` |
| Initial PoW Blocks | `75,000` |
| PoS Start Block | `75,000` |
| Block Size | `4MB` |
| Block Time | `60s` |
| PoW Reward | `45 SUBI` |
| PoS Reward | `5% reducing each year` |
| Stake Time | `24 hours` | 
| SubiNode Requirement | `10,000 SUBI` |
| SubiNode Reward | `30% PoW/PoS` |
| Port | `5335` |
| RPC Port | `5336` |
| SubiNode Port | `5335` |

Development Process
-------------------

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Tags](https://github.com/SubiPlatform/SubiCore/tags) are created
regularly to indicate new official, stable release versions of Subi.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md).


Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test on short notice. Please be patient and help out by testing
other people's pull requests, and remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write [unit tests](src/test/README.md) for new code, and to
submit new unit tests for old code. Unit tests can be compiled and run
(assuming they weren't disabled in configure) with: `make check`. Further details on running
and extending unit tests can be found in [/src/test/README.md](/src/test/README.md).

There are also [regression and integration tests](/qa) of the RPC interface, written
in Python, that are run automatically on the build server.
These tests can be run (if the [test dependencies](/qa) are installed) with: `qa/pull-tester/rpc-tests.py`

### Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the
code. This is especially important for large or high-risk changes. It is useful
to add a test plan to the pull request description if testing the changes is
not straightforward.

### Issue

We try to prompt our users for the basic information We always need for new issues.
Please fill out the issue template below and submit it to our discord channel.
[ISSUE_TEMPLATE](doc/template/ISSUE_TEMPLATE_example.md)
