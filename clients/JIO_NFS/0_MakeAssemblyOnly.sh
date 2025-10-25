#!/usr/bin/env bash

set -ex

cd "$(dirname "$0")/Tmp"

rm -f *

zcc --allseg CODE --no-crt -nostdlib +z80 --sdcccall1 -mz80 -a ../driver.c 
zcc --allseg CODE --no-crt -nostdlib +z80 --sdcccall1 -mz80 -a ../main.c 
