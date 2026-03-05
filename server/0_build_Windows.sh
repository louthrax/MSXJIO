#!/usr/bin/env bash

set -e

DRIVE="D"
SSH_PORT=2222

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

WIN_PATH="${DRIVE}:${SCRIPT_DIR//\//\\}"

echo "$WIN_PATH"

cd "$SCRIPT_DIR"

/mnt/DataLinux/VirtualBox_VMs/Windows10_Qt/launch.shnt --headless --ssh-port="$SSH_PORT" &

until ssh -p "$SSH_PORT" laurent@127.0.0.1 "net use ${DRIVE}: \\\\10.0.2.2\\Host" >/dev/null 2>&1; do
    sleep 1
done

ssh -p "$SSH_PORT" laurent@127.0.0.1 \
  "net use ${DRIVE}: \\\\10.0.2.2\\Host && cd /D $WIN_PATH && .\\tools\\build_Windows.bat"
