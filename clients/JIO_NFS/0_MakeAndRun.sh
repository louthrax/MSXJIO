#!/usr/bin/env bash

set -ex

cd "$(dirname "$0")"

./0_Make.sh

killall openmsx || true
openmsx -machine Philips_NMS_8255 -script openMSX_Run.tcl
