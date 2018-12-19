Subi Core
=============

Setup
---------------------
Subi Core is the original SUBI client and it builds the backbone of the network. It downloads and, by default, stores the entire history of SUBI transactions (which is currently more than 100 GBs); depending on the speed of your computer and network connection, the synchronization process can take anywhere from a few hours to a day or more.

To download Subi Core, visit [subicore.org](https://subicore.org/en/releases/).

Running
---------------------
The following are some helpful notes on how to run SUBI on your native platform.

### Usubi

Unpack the files into a directory and run:

- `bin/subi-qt` (GUI) or
- `bin/subid` (headless)

### Windows

Unpack the files into a directory, and then run subi-qt.exe.

### OS X

Drag SUBI-Core to your applications folder, and then run SUBI-Core.

### Need Help?

* See the documentation at the [SUBI Wiki](https://en.subi.it/wiki/Main_Page)
for help and more information.
* Ask for help on [#subi](http://webchat.freenode.net?channels=subi) on Freenode. If you don't have an IRC client use [webchat here](http://webchat.freenode.net?channels=subi).
* Ask for help on the [SUBITalk](https://subitalk.org/) forums, in the [Technical Support board](https://subitalk.org/index.php?board=4.0).

Building
---------------------
The following are developer notes on how to build SUBI on your native platform. They are not complete guides, but include notes on the necessary libraries, compile flags, etc.

- [Dependencies](dependencies.md)
- [OS X Build Notes](build-osx.md)
- [Usubi Build Notes](build-usubi.md)
- [Windows Build Notes](build-windows.md)
- [OpenBSD Build Notes](build-openbsd.md)
- [Gitian Building Guide](gitian-building.md)

Development
---------------------
The SUBI repo's [root README](/README.md) contains relevant information on the development process and automated testing.

- [Developer Notes](developer-notes.md)
- [Release Notes](release-notes.md)
- [Release Process](release-process.md)
- [Source Code Documentation (External Link)](https://dev.visucore.com/subi/doxygen/)
- [Translation Process](translation_process.md)
- [Translation Strings Policy](translation_strings_policy.md)
- [Travis CI](travis-ci.md)
- [Unauthenticated REST Interface](REST-interface.md)
- [Shared Libraries](shared-libraries.md)
- [BIPS](bips.md)
- [Dnsseed Policy](dnsseed-policy.md)
- [Benchmarking](benchmarking.md)

### Resources
* Discuss on the [SUBITalk](https://subitalk.org/) forums, in the [Development & Technical Discussion board](https://subitalk.org/index.php?board=6.0).
* Discuss project-specific development on #subi-core-dev on Freenode. If you don't have an IRC client use [webchat here](http://webchat.freenode.net/?channels=subi-core-dev).
* Discuss general SUBI development on #subi-dev on Freenode. If you don't have an IRC client use [webchat here](http://webchat.freenode.net/?channels=subi-dev).

### Miscellaneous
- [Assets Attribution](assets-attribution.md)
- [Files](files.md)
- [Fuzz-testing](fuzzing.md)
- [Reduce Traffic](reduce-traffic.md)
- [Tor Support](tor.md)
- [Init Scripts (systemd/upstart/openrc)](init.md)
- [ZMQ](zmq.md)

License
---------------------
Distributed under the [MIT software license](/COPYING).
This product includes software developed by the OpenSSL Project for use in the [OpenSSL Toolkit](https://www.openssl.org/). This product includes
cryptographic software written by Eric Young ([eay@cryptsoft.com](mailto:eay@cryptsoft.com)), and UPnP software written by Thomas Bernard.
