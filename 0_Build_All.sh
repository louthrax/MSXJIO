#!/usr/bin/env bash
set -euo pipefail

# All the projects: clients and tools (0_Build.sh of each), then the servers of all the platforms
# (server/0_Build_All.sh: Docker, Android SDK, Windows and macOS VMs).
# Usage: 0_Build_All.sh [clients]     clients: clients and tools only
# Outputs in the 0_Builds folder of each project, all copied to the 0_Builds folder of the repository.

cd "$(dirname "$0")"

PROJECTS=(clients/JIO_MSX-DOS clients/JIO_NFS clients/JIO_TIME clients/JIO_ROM clients/JIO_CAS tools/JSM)
for PROJECT in "${PROJECTS[@]}"; do
    "./$PROJECT/0_Build.sh"
done

if [ "${1:-}" != clients ]; then
    ./server/0_Build_All.sh
    PROJECTS+=(server)
fi

# all the outputs in 0_Builds (the macOS metadata files ._* left by the macOS build are not copied)
rm -rf ./0_Builds
mkdir -p ./0_Builds
for PROJECT in "${PROJECTS[@]}"; do
    find "$PROJECT/0_Builds" -maxdepth 1 -type f ! -name '._*' -exec cp -p {} ./0_Builds/ \;
done
echo "0_Builds: $(ls ./0_Builds | wc -l) files"
