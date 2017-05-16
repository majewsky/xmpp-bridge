# xmpp-bridge

A simple program that connects to an XMPP account and communicates with a
specified peer. Text is read from stdin and sent to the peer, and any messages
received from the peer will be printed on stdout.

The intended usage is as a building-block for other programs (or shell scripts)
that want to communicate status information over XMPP, or wish to ask for
input over XMPP.

## Installation

[libstrophe](https://github.com/strophe/libstrophe) is required. Build with

```bash
make
make install
```

As a developer, say `make MODE=debug` instead.

## Usage

Set the environment variables `XMPPBRIDGE_JID`, `XMPPBRIDGE_PASSWORD` and `XMPPBRIDGE_PEER_JID` to the respective
values, then call `xmpp-bridge`.

For details (e.g. option arguments), have a look at the [manpage](./xmpp-bridge.1).
