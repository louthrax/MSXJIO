#!/usr/bin/env bash

cd "$(dirname "$0")"

cp empty_MSX-DOS2.dsk disk.dsk
mtools -c mcopy -i disk.dsk -D o -s disk/* ::
killall openmsx || true
openmsx -machine Philips_NMS_8245_2MB -ext msxdos2 -script openmsx.tcl -diska $(dirname "$0")/disk.dsk
