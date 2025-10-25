#!/usr/bin/env bash

set -ex

cd "$(dirname "$0")"

killall openmsx || true
openmsx -machine Philips_NMS_8255 openMSX_TestMSX-DOS2.tcl
