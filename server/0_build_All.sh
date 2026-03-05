#!/bin/bash

set -ex

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

cd "$SCRIPT_DIR"
./0_build_Linux_Static.sh

cd "$SCRIPT_DIR"
./0_build_Linux_AppImage.sh

cd "$SCRIPT_DIR"
./0_build_Android.sh

cd "$SCRIPT_DIR"
./0_build_macOS.sh
sudo killall qemu-system-x86_64 || true

cd "$SCRIPT_DIR"
./0_build_Windows.sh
sudo killall qemu-system-x86_64 || true
