#!/usr/bin/env bash
set -euo pipefail

# ROMs, intermediate and generated files

cd "$(dirname "$0")"

rm -rf ./0_Builds ./0_Temp
rm -f drv_jio.r01 drv_jio_c.as rdate.inc
