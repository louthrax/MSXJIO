#!/usr/bin/env bash

set -ex

cd "$(dirname "$0")"

rm -rf ./0_Builds driver.c.asm main.c.asm
