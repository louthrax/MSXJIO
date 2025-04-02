#!/usr/bin/env bash

cd "$(dirname "$0")"
#openmsx -machine Philips_NMS_8245 -carta ./rs_dos2.rom -command "debug set_watchpoint write_io 0x2D"
openmsx -machine Panasonic_FS-A1ST -carta ./rs_dos2.rom -command "debug set_watchpoint write_io 0x2D"
