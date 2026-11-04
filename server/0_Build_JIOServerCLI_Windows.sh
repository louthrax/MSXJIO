#!/usr/bin/env bash
set -euo pipefail

# JIOServerCLI (command line server): zip archive, built on the Windows VM by tools\Build_JIOServerCLI_Windows.bat.
# Usage: 0_Build_JIOServerCLI_Windows.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# The Windows VM mounts nothing by default: map the host Samba share on S:, cd into the project and
# call the .bat there. launch.shnt --run executes this command over ssh (10.0.2.2 = the host, via slirp).
WIN_VM_LAUNCHER=${BUILD_WINDOWS_VM_LAUNCHER:-/vms/Windows10_Qt/launch.shnt}
WIN_DIR="S:${SCRIPT_DIR//\//\\}"
RUN_CMD="net use S: \\\\10.0.2.2\\Host /persistent:no >nul && cd /D \"$WIN_DIR\" && call tools\\Build_JIOServerCLI_Windows.bat"
"$WIN_VM_LAUNCHER" --run="$RUN_CMD" > >(awk '{ print strftime("[%H:%M:%S]"), $0; fflush() }') 2>&1
