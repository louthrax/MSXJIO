#!/usr/bin/env bash

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

SSH_PORT=2223

cd "$SCRIPT_DIR"

/mnt/DataLinux/VirtualBox_VMs/macOSSequoia_Qt/launch.shnt --headless --ssh-port=$SSH_PORT > /dev/null 2>&1 &

until ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p $SSH_PORT laurent@127.0.0.1 cd "Host$SCRIPT_DIR"; do
    sleep 1
done

ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -p $SSH_PORT laurent@127.0.0.1 "
  cd \"Host$SCRIPT_DIR\"
  ./build_macOS.zsh
  result=$?
  (sleep 1 && sudo shutdown -h now) &
  exit $result
"
