#!/usr/bin/env bash
set -euo pipefail

# Outputs and intermediate files of all the projects (0_Clean.sh of each), and the 0_Builds folder of the repository

cd "$(dirname "$0")"

./clients/JIO_MSX-DOS/0_Clean.sh
./clients/JIO_NFS/0_Clean.sh
./clients/JIO_TIME/0_Clean.sh
./clients/JIO_ROM/0_Clean.sh
./clients/JIO_CAS/0_Clean.sh
./tools/JSM/0_Clean.sh
./server/0_Clean.sh
rm -rf ./0_Builds
