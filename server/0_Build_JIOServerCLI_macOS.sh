#!/usr/bin/env bash
set -euo pipefail

# JIOServerCLI (command line server): zip archive, built on the macOS VM by tools/Build_JIOServerCLI_macOS.zsh.
# Usage: 0_Build_JIOServerCLI_macOS.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# The macOS VM mounts nothing by default: mount the host "/" over NFS, cd into the project and run the
# build script there. launch.shnt --run executes this command over ssh (10.0.2.2 = the host, via slirp).
MAC_VM_LAUNCHER=${BUILD_MAC_VM_LAUNCHER:-/vms/macOSSequoia_Qt/launch.shnt}
RUN_CMD="mkdir -p ~/Host && sudo -n mount -t nfs -o vers=3,nolocks,locallocks,noresvport 10.0.2.2:/ ~/Host && cd ~/Host$SCRIPT_DIR && ./tools/Build_JIOServerCLI_macOS.zsh"
"$MAC_VM_LAUNCHER" --run="$RUN_CMD" > >(awk '{ print strftime("[%H:%M:%S]"), $0; fflush() }') 2>&1
