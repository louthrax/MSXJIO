#!/usr/bin/env bash
set -euo pipefail

# Command line server (JIOServerCLI.pro), zip archive for Windows (on the Windows VM)

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

# The Windows VM mounts nothing by default: map the host Samba share on S:, cd into the project and
# call the .bat there. launch.shnt --run executes this command over ssh (10.0.2.2 = the host, via slirp).
WIN_VM_LAUNCHER=${BUILD_WINDOWS_VM_LAUNCHER:-/vms/Windows10_Qt/launch.shnt}
SMB_SHARE='\\10.0.2.2\Host'
WIN_DIR="S:${SCRIPT_DIR//\//\\}"
WIN_BAT="S:${SCRIPT_DIR//\//\\}\\tools\\Build_CLI_Windows.bat"
RUN_CMD="net use S: $SMB_SHARE /persistent:no >nul && cd /D \"$WIN_DIR\" && call \"$WIN_BAT\""
"$WIN_VM_LAUNCHER" --run="$RUN_CMD" \
    > >(awk '{ print strftime("[%H:%M:%S]"), $0; fflush() }') 2>&1
