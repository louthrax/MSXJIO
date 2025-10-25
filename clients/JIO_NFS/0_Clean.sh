#!/usr/bin/env bash

set -ex

cd "$(dirname "$0")"

rm -f ./Tmp/* driver.c.asm main.c.asm
