#!/bin/bash

set -eo pipefail

# if not yet done, cross-connect stdin/stdout of this script with xmpp-bridge
if [ -z "$HAS_XMPP_BRIDGE" ]; then
    source ./.env
    export HAS_XMPP_BRIDGE=1
    pipexec -- [ A "$0" "$@" ] [ B ./build/xmpp-bridge ] "{A:1>B:0}" "{B:1>A:0}"
    exit 0
fi

echo "What's your name?"
read ANSWER
echo "Hi $ANSWER!"
