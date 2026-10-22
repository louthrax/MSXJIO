#!/usr/bin/env bash
set -euo pipefail

# Graphical server, macOS disk image (on the macOS VM)

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

# The macOS VM mounts nothing by default: mount the host "/" over NFS, cd into the project and run the
# build script there. launch.shnt --run executes this command over ssh (10.0.2.2 = the host, via slirp).
MAC_VM_LAUNCHER=${BUILD_MAC_VM_LAUNCHER:-/mnt/DataLinux/VirtualBox_VMs/macOSSequoia_Qt/launch.shnt}
RUN_CMD="mkdir -p ~/Host && sudo -n mount -t nfs -o vers=3,nolocks,locallocks,noresvport 10.0.2.2:/ ~/Host && cd ~/Host$SCRIPT_DIR && ./tools/Build_macOS.zsh"
"$MAC_VM_LAUNCHER" --ssh-port="${BUILD_MAC_SSH_PORT:-2223}" --run="$RUN_CMD" \
    > >(awk '{ print strftime("[%H:%M:%S]"), $0; fflush() }') 2>&1
