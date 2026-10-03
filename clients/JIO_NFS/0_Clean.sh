#!/usr/bin/env bash
set -euo pipefail

# JIO.COM, disk images and intermediate files

cd "$(dirname "$0")"

rm -rf ./0_Builds ../../0_Builds/obj/JIO_NFS
rm -f driver.c.asm main.c.asm jumper.o stub.o
