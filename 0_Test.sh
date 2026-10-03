#!/usr/bin/env bash
set -euo pipefail

# Emulator tests (openMSX) of the MSX-DOS 2 ROM and of the clients, see clients/JIO_MSX-DOS/test/README.md.
# Usage: 0_Test.sh [--real-server <JIOServerCLI>] [scenario...]
#   --real-server   the real server (command line version) instead of the mock server

cd "$(dirname "$0")"

if [ "${1:-}" = --real-server ]; then
    [ $# -ge 2 ] || { echo "Usage: $0 [--real-server <JIOServerCLI>] [scenario...]" >&2; exit 2; }
    export REAL_SERVER="$(realpath "$2")"
    shift 2
fi

exec ./clients/JIO_MSX-DOS/test/0_RunTests.sh "$@"
