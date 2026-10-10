#!/usr/bin/env bash
set -euo pipefail

# All the projects: clients and tools (0_Build.sh of each), then the servers of all the platforms
# (server/0_Build_All.sh: Docker, Android SDK, Windows and macOS VMs).
# Usage: 0_Build_All.sh [clients]     clients: clients and tools only
# Outputs in the 0_Builds folder of each project.

cd "$(dirname "$0")"

./clients/JIO_MSX-DOS/0_Build.sh
./clients/JIO_NFS/0_Build.sh
./clients/JIO_TIME/0_Build.sh
./clients/JIO_ROM/0_Build.sh
./tools/JSM/0_Build.sh

if [ "${1:-}" != clients ]; then
    ./server/0_Build_All.sh
fi
