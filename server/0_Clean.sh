#!/usr/bin/env bash
set -euo pipefail

# Packages (0_Builds), generated Android and macOS icons. The build folders of the packages are in
# /mnt/DataLinux/Tmp (deleted by each build script).

cd "$(dirname "$0")"

rm -rf ./0_Builds ./android/res ./JIOServer.icns
