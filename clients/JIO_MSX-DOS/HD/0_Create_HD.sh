#!/usr/bin/env bash

cd "$(dirname "$0")"

rm -f hd.dsk

openmsx -command "\
set renderer none;\
set power off;\
diskmanipulator create hd.dsk -nextor 32M 1000M 1000M; ext ide; hda hd.dsk;\
diskmanipulator import hda1 ./MSX-DOS_2;\
\
diskmanipulator mkdir  hda1 /BIN;\
diskmanipulator chdir  hda1 /BIN;\
diskmanipulator import hda1 ../../MSX/sdcard/BIN;\
\
diskmanipulator mkdir  hda1 /SOFARUN;\
diskmanipulator chdir  hda1 /SOFARUN;\
diskmanipulator import hda1 ../../MSX/sdcard/SOFARUN;\
\
diskmanipulator mkdir  hda1 /C;\
diskmanipulator chdir  hda1 /C;\
diskmanipulator import hda1 ../../MSX/sdcard/C;\
\
diskmanipulator mkdir  hda1 /DOCS;\
diskmanipulator chdir  hda1 /DOCS;\
diskmanipulator import hda1 ../../MSX/sdcard/DOCS;\
\
diskmanipulator mkdir  hda1 /HELP;\
diskmanipulator chdir  hda1 /HELP;\
diskmanipulator import hda1 ../../MSX/sdcard/HELP;\
\
diskmanipulator mkdir  hda1 /TESTS;\
diskmanipulator chdir  hda1 /TESTS;\
diskmanipulator import hda1 ../../MSX/sdcard/TESTS;\
\
diskmanipulator mkdir  hda1 /DEVICES;\
diskmanipulator chdir  hda1 /DEVICES;\
diskmanipulator import hda1 ../../MSX/sdcard/DEVICES;\
\
diskmanipulator mkdir  hda2 /IMAGES;\
diskmanipulator chdir  hda2 /IMAGES;\
diskmanipulator import hda2 ../../MSX/sdcard/IMAGES;\
\
exit\
"
