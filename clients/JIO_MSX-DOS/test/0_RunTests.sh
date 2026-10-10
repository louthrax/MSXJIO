#!/usr/bin/env bash
#
# Emulator tests of the JIO MSX-DOS 2 ROM and of the JIO clients (no MSX and no JIO server needed).
#
# openMSX runs the ROM, tcl/bridge.tcl intercepts the serial routines of the
# kernel and forwards the bytes to mockserver.py, which serves a host directory
# as drive A:. See README.md.
#
# Usage: ./0_RunTests.sh [scenario...]       (all the scenarios by default)
# The MSX-DOS 2 ROM is built as by 0_Build.sh (JIO drives and local drives).
# Results: out/<scenario>/ (screens, mock server log, JIO drive, floppy contents)
#
# REAL_SERVER=<path of JIOServerCLI> (environment): the real server (server/JIOServerCLI.pro) instead of mockserver.py, on a
# pseudo terminal (realbridge.py between it and the bridge). The checks of the log of the mock server and the
# JIOTIME scenarios (date of the mock server) are skipped.

set -u

cd "$(dirname "$(realpath "$0")")"
TEST=$(pwd)
SRC=$(realpath ..)
OUT=$TEST/out
MSXDOS2_FILES=$(realpath ../../JIO_NFS/MSX-DOS2)
MOCK_PORT=${MOCK_PORT:-9876}
export MOCK_PORT

# first argument of the former versions (JIO only and hybrid ROMs): ignored
case "${1:-}" in all|jio|hybrid) shift ;; esac
ONLY="$*"

PASSED=0
FAILED=0
FAILED_LIST=""

mkdir -p "$OUT"

# ------------------------------------------------------------------------------
# Build
# ------------------------------------------------------------------------------
build_rom() { # name kernel defines
    local name=$1 kernel=$2 defines=$3
    rm -rf "$OUT/obj_$name"
    mkdir -p "$OUT/obj_$name"
    ( cd "$SRC" &&
      date +"db \"%Y-%m-%d\"" > "$OUT/obj_$name/rdate.inc" &&
      z88dk-z80asm -b -d -l -m $defines -I"$OUT/obj_$name" -O"$OUT/obj_$name" -o=jio_$name.bin p1_main.asm p3_paging.asm drv_jio.asm "$kernel" &&
      z88dk-appmake +glue -b "$OUT/obj_$name/jio_$name" --filler 0xFF --clean > /dev/null &&
      z88dk-appmake +rom -b "$OUT/obj_$name/jio_${name}__.bin" -o "$OUT/jio_$name.rom" -s 32768 --org 0 > /dev/null
    ) || { echo "Build of $name ROM failed"; exit 1; }
    awk '/^__P0_KERNEL_size/ { printf "  %s ROM: kernel %d bytes (limit 16384)\n", n, strtonum("0x" substr($3,2)) }' n="$name" "$OUT/obj_$name/jio_$name.map"
}

# Addresses of the intercepted routines, from the map file
make_bridge() { # map output
    local a
    a() { grep -E "^$1 " "$2" | sed -E 's/.*\$([0-9A-F]+).*/0x\1/'; }
    sed -e "s/@J_TXSEG@/$(a J_TXSEG "$1")/g; s/@J_RX1@/$(a J_RX1 "$1")/g; s/@DRIVES_Retry@/$(a DRIVES_Retry "$1")/g; s/@DRIVES_Exit@/$(a DRIVES_Exit "$1")/g; s/@vJIOTransmit@/$(a vJIOTransmit "$1")/g; s/@bJIOReceive@/$(a bJIOReceive "$1")/g; s/@JioDetect@/$(a JioDetect "$1")/g" \
        "$TEST/tcl/bridge.tcl.in" > "$2"
}

# Address of a routine of JIOTIME.COM (map file)
tool_addr() { grep -E "^$1 " "$TOOL_MAP" | sed -E 's/.*\$([0-9A-F]+).*/0x\1/'; }
# Offset of a routine in the resident stub or driver of JIO.COM (map or symbol file)
nfs_offset() { grep -E "^$1 " "$NFS_MAP" | sed -E 's/.*\$([0-9A-F]+).*/0x\1/'; }

prepare_files() {
    rm -rf "$OUT/base" "$OUT/floppy_base"
    mkdir -p "$OUT/base" "$OUT/floppy_base"
    ( cd "$TEST/fcbtest" && z88dk-z80asm -b -o="$OUT/base/FCBTEST.COM" fcbtest.asm && rm -f "$OUT"/base/*.o fcbtest.o ) || { echo "Build of FCBTEST.COM failed"; exit 1; }
    ( cd "$TEST/fibtest" && z88dk-z80asm -b -o="$OUT/base/FIBTEST.COM" fibtest.asm && rm -f "$OUT"/base/*.o fibtest.o ) || { echo "Build of FIBTEST.COM failed"; exit 1; }
    rm -rf "$OUT/obj_jiotime"
    ( cd "$SRC/../JIO_TIME" && z88dk-z80asm -b -m -O"$OUT/obj_jiotime" -o=JIOTIME.COM jiotime.asm && cp "$OUT/obj_jiotime/JIOTIME.COM" "$OUT/base/" ) || { echo "Build of JIOTIME.COM failed"; exit 1; }
    TOOL_MAP="$OUT/obj_jiotime/JIOTIME.map"
    ( cd "$TEST/fcbread" && z88dk-z80asm -b -o="$OUT/base/FCBREAD.COM" fcbread.asm && rm -f "$OUT"/base/*.o fcbread.o ) || { echo "Build of FCBREAD.COM failed"; exit 1; }
    ( cd "$TEST/p2test" && z88dk-z80asm -b -o="$OUT/base/P2TEST.COM" p2test.asm && rm -f "$OUT"/base/*.o p2test.o ) || { echo "Build of P2TEST.COM failed"; exit 1; }
    ( cd "$TEST/fdtest" && z88dk-z80asm -b -o="$OUT/base/FDTEST.COM" fdtest.asm && rm -f "$OUT"/base/*.o fdtest.o ) || { echo "Build of FDTEST.COM failed"; exit 1; }
    # JIO.COM (clients/JIO_NFS), built in a copy (its make script writes next to the sources)
    rm -rf "$OUT/nfs"
    mkdir -p "$OUT/nfs/clients"
    cp -r "$SRC/../../common" "$OUT/nfs/"
    cp -r "$SRC/../JIO_NFS" "$OUT/nfs/clients/"
    rm -rf "$OUT/nfs/clients/JIO_NFS/0_Builds" "$OUT/nfs/clients/JIO_NFS/0_Temp"
    ( cd "$OUT/nfs/clients/JIO_NFS" && bash 0_Build.sh > build.log 2>&1 && cp 0_Builds/JIO.COM "$OUT/base/" ) || { echo "Build of JIO.COM failed"; exit 1; }
    # serial routines: in the resident stub (driver in a mapper segment) or in the resident driver
    NFS_MAP="$OUT/nfs/clients/JIO_NFS/0_Temp/stub.map"
    # serial routines of the installer (COMMAND_DRIVE_INFO at install), intercepted as the routines of JIOTIME.COM
    NFS_MAIN_MAP="$OUT/nfs/clients/JIO_NFS/0_Temp/main.map"
    [ -f "$NFS_MAP" ] || NFS_MAP="$OUT/nfs/clients/JIO_NFS/0_Temp/driver.sym"
    # JIO-ROM.COM (clients/JIO_ROM) and JIO-ROM.CAS (clients/JIO_CAS), with the ROM run in RAM (target dos2ram of
    # JIO_MSX-DOS), built in a copy: ROM map file for the bridge
    rm -rf "$OUT/jiorom"
    mkdir -p "$OUT/jiorom/clients/JIO_MSX-DOS"
    cp -r "$SRC/../../common" "$OUT/jiorom/"
    cp -r "$SRC/../JIO_ROM" "$SRC/../JIO_CAS" "$OUT/jiorom/clients/"
    cp "$SRC"/*.asm "$SRC"/*.inc "$SRC"/0_Build.sh "$OUT/jiorom/clients/JIO_MSX-DOS/"
    rm -rf "$OUT"/jiorom/clients/JIO_{ROM,CAS}/0_{Builds,Temp}
    ( cd "$OUT/jiorom/clients/JIO_ROM" && bash 0_Build.sh > build.log 2>&1 ) || { echo "Build of JIO-ROM.COM failed"; exit 1; }
    ( cd "$OUT/jiorom/clients/JIO_CAS" && bash 0_Build.sh > build.log 2>&1 ) || { echo "Build of JIO-ROM.CAS failed"; exit 1; }
    JIOROM="$OUT/jiorom/clients/JIO_ROM/0_Builds/JIO-ROM.COM"
    JIOCAS="$OUT/jiorom/clients/JIO_CAS/0_Builds/JIO-ROM.CAS"
    JIOROM_MAP="$OUT/jiorom/clients/JIO_MSX-DOS/0_Temp/dos2_ram/jio_dos2_ram.map"
    cp "$MSXDOS2_FILES/MSXDOS2.SYS" "$MSXDOS2_FILES/COMMAND2.COM" "$OUT/base/"
    printf 'Hello from the JIO server!\r\nSecond line.\r\n' > "$OUT/base/hello.txt"
    printf 'THIS FILE IS ON THE FLOPPY\r\n' > "$OUT/floppy_base/FLOPPY.TXT"
}

# ------------------------------------------------------------------------------
# Scenario
# ------------------------------------------------------------------------------
# run_scenario name rom map machine "slots" autoexec script [floppy size] [floppy files dir] [jio drives] [screen times]
# IMAGE_MODE (environment): the files of the JIO drive are put in a 720 KB disk image (jio.dsk) served by
# the mock server in disk image mode, the handshake of the driver is not skipped. Files read back: image_out/
# IMAGE_SETUP (environment): command run on jio.dsk after its creation
run_scenario() {
    local name=$1 rom=$2 map=$3 machine=$4 slots=$5 autoexec=$6 script=$7
    local fsize=${8:-} ffiles=${9:-} njio=${10:-1} times=${11:-"15 30 45 60"}
    local dir="$OUT/$name" disk="" mp rc

    rm -rf "$dir"
    mkdir -p "$dir/drive"
    cd "$dir"
    cp "$OUT"/base/* drive/
    [ -n "${SETUP:-}" ] && $SETUP drive
    printf "$autoexec" > drive/AUTOEXEC.BAT
    make_bridge "$map" bridge.tcl

    if [ -n "${JIOROM_BOOT:-}" ]; then
        # JIO-ROM.COM: MSX-DOS 1 boots from the floppy and starts the JIO ROM in RAM (files of the scenario added)
        rm -rf floppy_files; mkdir -p floppy_files
        [ -n "$ffiles" ] && cp -p "$ffiles"/* floppy_files/
        cp -p "$DOS1_FILES/MSXDOS.SYS" "$DOS1_FILES/COMMAND.COM" "$JIOROM" floppy_files/
        printf 'JIO-ROM\r\n' > floppy_files/AUTOEXEC.BAT
        ffiles="$dir/floppy_files"
        fsize=${fsize:-360}
    fi
    local shift=""
    [ -n "${TIME_SHIFT:-}" ] && shift="-script $TEST/tcl/shift.tcl"
    # EXTRA_SCRIPT (environment): other tcl script run before the scenario script
    [ -n "${EXTRA_SCRIPT:-}" ] && shift="$shift -script $EXTRA_SCRIPT"

    if [ -n "$fsize" ]; then
        FLOPPY_SIZE=$fsize FLOPPY_FILES=$ffiles timeout 30 openmsx -machine "$machine" -script "$TEST/tcl/mkdisk.tcl" > /dev/null 2>&1
        disk="-diska floppy.dsk"
    fi

    local image="" handshake=""
    if [ -n "${IMAGE_MODE:-}" ]; then
        DISK_FILE=jio.dsk FLOPPY_SIZE=720 FLOPPY_FILES="$dir/drive" timeout 30 openmsx -machine "$machine" -script "$TEST/tcl/mkdisk.tcl" > /dev/null 2>&1
        image="$dir/jio.dsk"
        handshake=1
        [ -n "${IMAGE_SETUP:-}" ] && $IMAGE_SETUP "$image"
    fi

    if [ -n "${REAL_SERVER:-}" ]; then
        # real server (JIOServerCLI) on a pseudo terminal, realbridge.py between it and the bridge
        rm -f pty.path
        python3 "$TEST/realbridge.py" "$MOCK_PORT" pty.path &
        mp=$!
        while [ ! -s pty.path ]; do sleep 0.1; done
        local serve=(-d "${JIO_DRIVE:-A}=$dir/drive")
        [ -n "$image" ] && serve=(-i "$image")
        [ -n "${READ_ONLY:-}" ] && serve+=(-r)
        "$REAL_SERVER" "${serve[@]}" --port "$(cat pty.path)" -q -l server.log &
        mp="$mp $!"
    else
        MOCK_IMAGE=$image MOCK_READONLY=${READ_ONLY:-} MOCK_DRIVE=${JIO_DRIVE:-A} python3 "$TEST/mockserver.py" "$dir/drive" "$MOCK_PORT" server.log &
        mp=$!
    fi
    sleep 1
    local tool_tx tool_rx
    if [ -n "${NFS:-}" ]; then
        tool_tx=$(grep -E "^vJIOTransmit " "$NFS_MAIN_MAP" | sed -E 's/.*\$([0-9A-F]+).*/0x\1/')
        tool_rx=$(grep -E "^bJIOReceive " "$NFS_MAIN_MAP" | sed -E 's/.*\$([0-9A-F]+).*/0x\1/')
    else
        tool_tx=$(tool_addr vJIOTransmit); tool_rx=$(tool_addr bJIOReceive)
    fi
    TOOL_TX=$tool_tx TOOL_RX=$tool_rx \
    NFS_TX=${NFS:+$(nfs_offset vJIOTransmit)} NFS_RX=${NFS:+$(nfs_offset bJIOReceive)} \
    JIO_DRIVES=$njio JIO_HANDSHAKE=$handshake SCREEN_TIMES="$times" timeout 180 openmsx -machine "$machine" $disk $slots \
        -script bridge.tcl $shift -script "$script" > openmsx.log 2>&1
    rc=$?
    kill $mp 2> /dev/null
    wait $mp 2> /dev/null

    if [ -n "$fsize" ]; then
        mkdir -p floppy_out
        timeout 30 openmsx -machine "$machine" -script "$TEST/tcl/rddisk.tcl" > /dev/null 2>&1
    fi
    if [ -n "$image" ]; then
        mkdir -p image_out
        DISK_FILE=jio.dsk DISK_OUT=image_out timeout 30 openmsx -machine "$machine" -script "$TEST/tcl/rddisk.tcl" > /dev/null 2>&1
    fi
    cat screen_*.txt > screens.txt 2> /dev/null
    echo "$rc" > exit_code
    cd "$TEST"
}

# ------------------------------------------------------------------------------
# Checks
# ------------------------------------------------------------------------------
ERRORS=""
check()      { [ "$2" ] || ERRORS="$ERRORS\n    - $1"; }
# check of the log of the mock server (format of mockserver.py): not done with the real server (REAL_SERVER)
mock_check() { [ -n "${REAL_SERVER:-}" ] || check "$@"; }
has_text()   { grep -qF -- "$2" "$1" 2> /dev/null; }
count_text() { grep -cF -- "$2" "$1" 2> /dev/null; }

begin_checks() { # name
    ERRORS=""
    local dir="$OUT/$1"
    check "openMSX exit code $(cat "$dir/exit_code")" "$([ "$(cat "$dir/exit_code")" = 0 ] && echo ok)"
    check "protocol errors in server.log" "$( ! grep -q '\*\*\*' "$dir/server.log" && echo ok)"
}

end_checks() { # name description
    if [ -z "$ERRORS" ]; then
        echo "  PASS  $1: $2"
        PASSED=$((PASSED + 1))
    else
        echo "  FAIL  $1: $2"
        echo -e "$ERRORS" | sed '/^$/d'
        FAILED=$((FAILED + 1))
        FAILED_LIST="$FAILED_LIST $1"
    fi
}

wanted() { [ -z "$ONLY" ] || [[ " $ONLY " == *" $1 "* ]]; }

FCB_OK='RENAME/DEL/DEL: 00 00 FF'
FIB_OK='00 D7 00 00 FIBTEST: Hello'
# FIBTEST on a FAT drive: result of the original MSX-DOS 2 kernel (floppy)
FIB_FAT='00 D7 00 C7 FIBTEST:'
FNEW_FAT='00 CC FF CA FNEW DIRX'

# Long host names: 8.3 aliases (XXXXXX~N.EXT) on the MSX
setup_longnames() { # drive directory
    mkdir -p "$1/Bombaman (2004)(TeamBomba)" "$1/BOMBAMAN_(2005)X"
    printf 'GAME FILE\r\n' > "$1/Bombaman (2004)(TeamBomba)/GAME.TXT"
    printf 'LONG NAME FILE\r\n' > "$1/LongFileName.text"
}
LONGNAMES='MD BOMBAMAN_(2004)(TEAMBOMBA)\r\nCD BOMBAMAN_(2004)(TEAMBOMBA)\r\nCD\r\nCD \\\r\nCD BOMBAM~1\r\nTYPE GAME.TXT\r\nCD \\\r\nTYPE LONGFI~1.TEX\r\nCOPY HELLO.TXT BOMBAM~2\r\nDIR /W\r\n'
test_longnames() { # name rom map machine slots description
    wanted "$1" || return
    SETUP=setup_longnames run_scenario "$1" "$2" "$3" "$4" "$5" "$LONGNAMES" "$TEST/tcl/screens.tcl" "" "" 1 "25"
    local d="$OUT/$1"
    begin_checks "$1"
    check "long directory created on the host" "$([ -d "$d/drive/BOMBAMAN_(2004)(TEAMBOMBA)" ] && echo ok)"
    check "CD with the long name, shown as alias" "$(has_text "$d/screens.txt" 'A:\BOMBAM~2' && echo ok)"
    check "CD and TYPE with aliases" "$(has_text "$d/screens.txt" 'GAME FILE' && has_text "$d/screens.txt" 'LONG NAME FILE' && echo ok)"
    check "COPY into an aliased directory" "$([ -f "$d/drive/BOMBAMAN_(2004)(TEAMBOMBA)/HELLO.TXT" ] && echo ok)"
    check "aliases listed" "$(grep -qi 'bombam~3' "$d/screens.txt" && has_text "$d/screens.txt" 'LONGFI~1.TEX' && echo ok)"
    end_checks "$1" "$6"
}

# SofaCopy (SC.COM, not in this repository: SOFACOPY environment, default the SD card folder of the author): it
# resets the archive attribute of the source files (_ATTR), then copies the files whose archive attribute is reset
# (the server keeps this attribute, not stored on the host)
SOFACOPY=${SOFACOPY:-/mnt/DataLinux/Projects/MSX/sdcard/SOFARUN/SC.COM}
setup_sofacopy() { # drive directory
    cp "$SOFACOPY" "$1/SC.COM"
}
test_sofacopy() { # name rom map machine slots description
    wanted "$1" || return
    if [ ! -f "$SOFACOPY" ]; then echo "  SKIP  $1: $SOFACOPY not found"; return; fi
    SETUP=setup_sofacopy run_scenario "$1" "$2" "$3" "$4" "$5" 'MD SUB\r\nCD SUB\r\nA:\\SC \\HELLO.TXT\r\nDIR\r\nCD \\\r\n' "$TEST/tcl/screens.tcl" "" "" 1 "40"
    local d="$OUT/$1"
    begin_checks "$1"
    check "SUB/HELLO.TXT copied by SofaCopy" "$(cmp -s "$d/drive/SUB/HELLO.TXT" "$OUT/base/hello.txt" && echo ok)"
    mock_check "archive attribute reset by SofaCopy" "$(grep -q 'ATTR .*hello.txt set=1 attr=00 -> 00' "$d/server.log" && echo ok)"
    end_checks "$1" "$6"
}

# REN, MOVE, ATTRIB (_RENAME, _MOVE, _ATTR): on drive A: (JIO), and on the floppy B:
renmove_cmds() { # drive (same escapes as the other command strings: run_scenario uses printf)
    echo -n "$1:"'\r\nCOPY A:HELLO.TXT R1.TXT\r\nREN R1.TXT R2.TXT\r\nMD SUB\r\nMOVE R2.TXT SUB\r\nATTRIB +R SUB\\R2.TXT\r\nDEL SUB\\R2.TXT\r\nATTRIB -R SUB\\R2.TXT\r\nCOPY SUB\\R2.TXT R3.TXT\r\nDIR /W\r\n'
}
check_renmove() { # directory of the drive files, drive
    local f="$1" r1 r2 r3 sub
    if [ "$2" = A ]; then r1=R1.TXT; r2=R2.TXT; r3=R3.TXT; sub=SUB; else r1=r1.txt; r2=r2.txt; r3=r3.txt; sub=sub; fi
    check "$2: REN of a file" "$([ ! -e "$f/$r1" ] && echo ok)"
    check "$2: MOVE into a directory" "$([ ! -e "$f/$r2" ] && [ -f "$f/$sub/$r2" ] && echo ok)"
    check "$2: file still there after DEL of the read only file, copied back (R3.TXT)" "$(cmp -s "$f/$r3" "$OUT/base/hello.txt" && echo ok)"
}
test_renmove() { # name rom map machine slots description [floppy size]
    wanted "$1" || return
    local cmds
    cmds="$(renmove_cmds A)"
    [ -n "${7:-}" ] && cmds="$cmds$(renmove_cmds B)"
    run_scenario "$1" "$2" "$3" "$4" "$5" "$cmds" "$TEST/tcl/screens.tcl" "${7:-}" "$OUT/floppy_base" 1 "10 20 30 40 50 60"
    local d="$OUT/$1"
    begin_checks "$1"
    check_renmove "$d/drive" A
    check "A: DEL of a read only file refused" "$(grep -qi 'read only' "$d/screens.txt" && echo ok)"
    check "A: read only attribute removed (host file writable)" "$([ -w "$d/drive/SUB/R2.TXT" ] && echo ok)"
    [ -n "${7:-}" ] && check_renmove "$d/floppy_out" B
    [ -n "${7:-}" ] && check "B: DEL of a read only file refused on both drives" "$([ "$(grep -ci 'read only' "$d/screens.txt")" -ge 2 ] && echo ok)"
    end_checks "$1" "$6"
}

# Disk BASIC on drive $6
test_basic() { # name rom map machine slots drive description [floppy size]
    wanted "$1" || return
    local size=${8:-}
    BASIC_DRIVE=$6 run_scenario "$1" "$2" "$3" "$4" "$5" 'BASIC\r\n' "$TEST/tcl/basic.tcl" "$size" "$OUT/floppy_base"
    local d="$OUT/$1" loc
    begin_checks "$1"
    check "program output (LINE ONE)" "$(has_text "$d/screen_run.txt" 'LINE ONE' && echo ok)"
    check "LOAD + LIST" "$(has_text "$d/screen_end.txt" '10 OPEN' && echo ok)"
    if [ "$6" = A ]; then loc="$d/drive"; prog=PROG.BAS; data=DATA.TXT; else loc="$d/floppy_out"; prog=prog.bas; data=data.txt; fi
    check "PROG.BAS saved on $6:" "$([ -f "$loc/$prog" ] && echo ok)"
    check "DATA.TXT deleted (KILL)" "$([ ! -f "$loc/$data" ] && echo ok)"
    end_checks "$1" "$7"
}


# FORMAT B: (_FORMAT, in the disk ROM page, called through the H_BDOS hook): floppy formatted, then listed
test_format() { # name rom map machine slots floppy size description [unused] [choice of the driver]
    wanted "$1" || return
    FORMAT_CHOICE=${9:-} run_scenario "$1" "$2" "$3" "$4" "$5" 'REM\r\n' "$TEST/tcl/format.tcl" "$6" "$OUT/floppy_base" 1 "23 27 40 130"
    local d="$OUT/$1"
    begin_checks "$1"
    check "no error" "$( ! grep -qi 'invalid\|error\|abort' "$d/screens.txt" && echo ok)"
    check "formatted B: holds only the file copied after FORMAT" "$(has_text "$d/screen_130.txt" 'HELLO' && has_text "$d/screen_130.txt" '1 file' && echo ok)"
    check "floppy read back: only HELLO.TXT" "$([ "$(ls -A "$d/floppy_out" 2> /dev/null)" = hello.txt ] && echo ok)"
    end_checks "$1" "$7"
}

# JIO drive A: + floppy B:
DOS_HYBRID='FIBTEST\r\nDIR A:/W\r\nDIR B:/W\r\nCOPY B:FLOPPY.TXT A:\r\nCOPY A:HELLO.TXT B:\r\nCOPY A:COMMAND2.COM B:X.COM\r\nCOPY B:X.COM A:Y.COM\r\nB:\r\nMD SUB\r\nCD SUB\r\nCOPY A:HELLO.TXT\r\nCD \\\r\nDIR > A:OUT.TXT\r\nA:FCBTEST\r\nA:\r\nFCBTEST\r\nDEL B:X.COM\r\nDIR B:/W\r\n'
test_dos_hybrid() { # name rom map machine slots floppy size description
    wanted "$1" || return
    run_scenario "$1" "$2" "$3" "$4" "$5" "$DOS_HYBRID" "$TEST/tcl/screens.tcl" "$6" "$OUT/floppy_base" 1
    local d="$OUT/$1"
    begin_checks "$1"
    check "FCB test result on both drives" "$([ "$(count_text "$d/screen_60.txt" "$FCB_OK")" -ge 2 ] && echo ok)"
    check "open with the FIB after the last _FNEXT" "$(has_text "$d/screens.txt" "$FIB_OK" && echo ok)"
    check "floppy -> JIO copy (FLOPPY.TXT)" "$([ -f "$d/drive/FLOPPY.TXT" ] && echo ok)"
    check "Y.COM (JIO -> floppy -> JIO) identical" "$(cmp -s "$d/drive/Y.COM" "$d/drive/COMMAND2.COM" && echo ok)"
    check "floppy SUB/HELLO.TXT" "$([ -f "$d/floppy_out/sub/hello.txt" ] && echo ok)"
    check "X.COM deleted from floppy" "$([ ! -f "$d/floppy_out/x.com" ] && echo ok)"
    check "floppy DIR redirected to JIO drive" "$(has_text "$d/drive/OUT.TXT" 'Directory of B:' && echo ok)"
    end_checks "$1" "$7"
}

# RAMDISK (_RAMD): H: served by the server, MSX reset (RESET destroys the RAM disk)
RAMDISK='RAMDISK\r\nRAMDISK 32K\r\nRAMDISK\r\nCOPY A:COMMAND2.COM H:\r\nCOPY A:HELLO.TXT H:\r\nMD H:SUB\r\nCOPY A:HELLO.TXT H:SUB\r\nCOPY H:COMMAND2.COM A:Z.COM\r\nDIR H: > A:RAM1.TXT\r\nH:\r\nA:FCBTEST\r\nA:\r\nCOPY A:COMMAND2.COM H:FULL.COM > A:FULL.TXT\r\n'
RAMDISK_FLOPPY='COPY H:SUB\\HELLO.TXT B:RAM.TXT\r\n'
RAMDISK_END='RAMDISK 0 /D\r\nDIR H: > A:RAM3.TXT\r\nRAMDISK 16K\r\nCOPY A:HELLO.TXT H:\r\n'
# RESET_TIME (environment): time of the screen dump after the MSX reset (default 20 s)
test_ramdisk() { # name rom map machine slots description [floppy size]
    wanted "$1" || return
    local autoexec="$RAMDISK$RAMDISK_END"
    [ -n "${7:-}" ] && autoexec="$RAMDISK$RAMDISK_FLOPPY$RAMDISK_END"
    FIRST_TIME=50 SECOND_TIME=${RESET_TIME:-20} REBOOT_AUTOEXEC='RAMDISK > A:RAM2.TXT\r\nDIR H:\r\n' \
        run_scenario "$1" "$2" "$3" "$4" "$5" "$autoexec" "$TEST/tcl/reboot.tcl" "${7:-}" "$OUT/floppy_base" 1
    local d="$OUT/$1"
    begin_checks "$1"
    check "RAMDISK 32K creates H:" "$(has_text "$d/screens.txt" 'RAM disk is 32K' && echo ok)"
    check "H: listed with its label" "$(has_text "$d/drive/RAM1.TXT" 'RAM DISK' && has_text "$d/drive/RAM1.TXT" 'COMMAND2' && has_text "$d/drive/RAM1.TXT" 'SUB' && echo ok)"
    check "free space of the RAM disk" "$(has_text "$d/drive/RAM1.TXT" '7K free' && echo ok)"
    check "Z.COM (JIO -> H: -> JIO) identical" "$(cmp -s "$d/drive/Z.COM" "$d/drive/COMMAND2.COM" && echo ok)"
    check "FCB functions on H:" "$(has_text "$d/screens.txt" "$FCB_OK" && echo ok)"
    check "disk full on H:" "$(grep -qi 'disk full' "$d/drive/FULL.TXT" "$d/screens.txt" 2> /dev/null && echo ok)"
    [ -n "${7:-}" ] && check "H: -> floppy copy" "$([ -f "$d/floppy_out/ram.txt" ] && echo ok)"
    check "RAMDISK 0 /D destroys H:" "$(! has_text "$d/drive/RAM3.TXT" 'COMMAND2' && echo ok)"
    check "RAM disk destroyed by the MSX reset" "$(has_text "$d/drive/RAM2.TXT" 'does not exist' && echo ok)"
    end_checks "$1" "$6"
}

# No server ([ESC] at boot): the floppy is A:, MSX-DOS 2 boots from it
test_noserver() { # name rom map machine floppy size description
    wanted "$1" || return
    rm -rf "$OUT/floppy_dos"; mkdir -p "$OUT/floppy_dos"
    cp "$OUT"/base/MSXDOS2.SYS "$OUT"/base/COMMAND2.COM "$OUT"/base/FCBTEST.COM "$OUT/floppy_dos/"
    cp "$OUT/base/hello.txt" "$OUT/floppy_dos/HELLO.TXT"
    printf 'VER\r\nFCBTEST\r\nRAMDISK 32K > RAMD.TXT\r\nDIR/W\r\n' > "$OUT/floppy_dos/AUTOEXEC.BAT"
    run_scenario "$1" "$2" "$3" "$4" "-carta $2" 'DIR\r\n' "$TEST/tcl/screens.tcl" "$5" "$OUT/floppy_dos" 0
    local d="$OUT/$1"
    begin_checks "$1"
    mock_check "no request to the server" "$([ ! -s "$d/server.log" ] && echo ok)"
    check "MSX-DOS 2 booted from the floppy" "$(has_text "$d/screen_60.txt" 'COMMAND2.COM version' && echo ok)"
    check "FCB test result" "$(has_text "$d/screen_60.txt" "$FCB_OK" && echo ok)"
    check "RAMDISK without server: not enough memory" "$(grep -qi 'not enough memory' "$d/floppy_out/ramd.txt" 2> /dev/null && echo ok)"
    end_checks "$1" "$6"
}

# Server in disk image mode: the JIO drive A: is a disk image (sectors, local FAT12 drive of the kernel)
DOS_IMAGE='VER\r\nFIBTEST\r\nDIR A:/W\r\nCOPY B:FLOPPY.TXT A:\r\nCOPY A:HELLO.TXT B:\r\nCOPY A:COMMAND2.COM X.COM\r\nMD SUB\r\nCD SUB\r\nCOPY \\HELLO.TXT\r\nCD \\\r\nDIR > OUT.TXT\r\nFCBTEST\r\nRAMDISK 32K > RAMD.TXT\r\nDIR /W\r\n'
test_image_hybrid() { # name rom map machine slots floppy size description
    wanted "$1" || return
    IMAGE_MODE=1 run_scenario "$1" "$2" "$3" "$4" "$5" "$DOS_IMAGE" "$TEST/tcl/screens.tcl" "$6" "$OUT/floppy_base" 1
    local d="$OUT/$1" i="$OUT/$1/image_out"
    begin_checks "$1"
    mock_check "handshake: drive info, no drive served by BDOS" "$(has_text "$d/server.log" 'INFO' && has_text "$d/server.log" 'LOGIN 00' && echo ok)"
    mock_check "only drive commands, RESET and LOGIN" "$( ! grep -qvE '^(RESET|LOGIN 00|INFO|DISK CHANGED|(READ|WRITE) P[0-9])' "$d/server.log" && echo ok)"
    mock_check "sectors written to the image" "$(grep -q '^WRITE P0' "$d/server.log" && echo ok)"
    check "FCB test result" "$(has_text "$d/screen_60.txt" "$FCB_OK" && echo ok)"
    check "FIBTEST as the original MSX-DOS 2 on a FAT drive" "$(has_text "$d/screens.txt" "$FIB_FAT" && has_text "$d/screens.txt" "$FNEW_FAT" && echo ok)"
    check "X.COM identical to COMMAND2.COM (image)" "$(cmp -s "$i/x.com" "$OUT/base/COMMAND2.COM" && echo ok)"
    check "floppy -> image copy (FLOPPY.TXT)" "$([ -f "$i/floppy.txt" ] && echo ok)"
    check "image -> floppy copy (HELLO.TXT)" "$(cmp -s "$d/floppy_out/hello.txt" "$OUT/base/hello.txt" && echo ok)"
    check "SUB/HELLO.TXT on the image" "$([ -f "$i/sub/hello.txt" ] && echo ok)"
    check "redirected DIR on the image (OUT.TXT)" "$(has_text "$i/out.txt" 'Directory of A:' && echo ok)"
    check "RAMDISK without directories served: not enough memory" "$(grep -qi 'not enough memory' "$i/ramd.txt" 2> /dev/null && echo ok)"
    end_checks "$1" "$7"
}

# "Read only" server: every modification of the JIO drive is refused (.WPROT), reading works, the RAM disk H:
# (and the floppy B:) stay writable
READONLY='COPY HELLO.TXT X.TXT\r\nMD SUB\r\nDEL HELLO.TXT\r\nREN HELLO.TXT Y.TXT\r\nATTRIB +R HELLO.TXT\r\nTYPE HELLO.TXT\r\nRAMDISK 32K\r\nCOPY HELLO.TXT H:\r\nDIR H:\r\n'
READONLY_FLOPPY='COPY HELLO.TXT B:\r\nDIR B:\r\n'
test_readonly() { # name rom map machine slots description [floppy size]
    wanted "$1" || return
    local cmds="$READONLY"
    [ -n "${7:-}" ] && cmds="$READONLY$READONLY_FLOPPY"
    READ_ONLY=1 run_scenario "$1" "$2" "$3" "$4" "$5" "$cmds" "$TEST/tcl/screens.tcl" "${7:-}" "$OUT/floppy_base" 1 "10 20 30 40 50 60"
    local d="$OUT/$1"
    begin_checks "$1"
    check "nothing created on the host (X.TXT, SUB)" "$([ ! -e "$d/drive/X.TXT" ] && [ ! -e "$d/drive/SUB" ] && echo ok)"
    check "HELLO.TXT not deleted nor renamed, still writable" "$([ -w "$d/drive/hello.txt" ] && [ ! -e "$d/drive/Y.TXT" ] && echo ok)"
    check "write protected errors shown" "$([ "$(grep -ci 'write protected' "$d/screen_60.txt")" -ge 4 ] && echo ok)"
    check "reading works (TYPE)" "$(has_text "$d/screen_60.txt" 'Hello from the JIO server!' && echo ok)"
    check "RAM disk H: writable" "$(grep -qE 'HELLO +TXT' "$d/screen_60.txt" && echo ok)"
    [ -n "${7:-}" ] && check "floppy B: writable" "$(cmp -s "$d/floppy_out/hello.txt" "$OUT/base/hello.txt" && echo ok)"
    end_checks "$1" "$6"
}

# Self-booting disk image (game disk): the boot loader of the boot sector (C01EH) is started at boot, it prints
# GAME STARTED with CHPUT and loops forever (MSX-DOS 2 is not started)
setup_bootloader() { # disk image
    python3 - "$1" <<'PY'
import sys
code = bytes([0x21, 0x2B, 0xC0,          # C01E: LD HL,C02B (message)
              0x7E,                      # C021: LD A,(HL)
              0xB7,                      #       OR A
              0x28, 0xFE,                #       JR Z,$ (end of message: loop forever)
              0xCD, 0xA2, 0x00,          #       CALL CHPUT
              0x23,                      #       INC HL
              0x18, 0xF6]) + b'GAME STARTED\0'  # JR C021
with open(sys.argv[1], 'r+b') as f:
    f.seek(0x1E)
    f.write(code)
PY
}
test_bootloader_hybrid() { # name rom map machine slots description
    wanted "$1" || return
    IMAGE_MODE=1 IMAGE_SETUP=setup_bootloader run_scenario "$1" "$2" "$3" "$4" "$5" 'VER\r\n' "$TEST/tcl/screens.tcl" "" "" 1 "20"
    local d="$OUT/$1"
    begin_checks "$1"
    check "boot loader started (GAME STARTED)" "$(has_text "$d/screen_20.txt" 'GAME STARTED' && echo ok)"
    mock_check "boot sector read (sector 0)" "$(grep -q '^READ P0 0 x 1' "$d/server.log" && echo ok)"
    check "MSX-DOS 2 not started" "$( ! has_text "$d/screen_20.txt" 'MSX-DOS' && echo ok)"
    end_checks "$1" "$6"
}

# JIOTIME.COM: date and time of the MSX set from the server (MOCK_DATE), or no answer.
# Note: the RTC (RP5C01) of openMSX 20.0 changes some months (July is read back as May, also when written directly
# to the chip), the default date uses a month it keeps.
test_jiotime() { # name rom map machine slots description [mock date] [parameters of JIOTIME]
    wanted "$1" || return
    [ -n "${REAL_SERVER:-}" ] && return     # date of the mock server (or no answer)
    local date=${7:-"2031-10-25 13:45:30"}
    MOCK_DATE="$date" run_scenario "$1" "$2" "$3" "$4" "$5" "JIOTIME${8:+ $8}\r\n" "$TEST/tcl/screens.tcl" "" "" 1 "15"
    local d="$OUT/$1"
    begin_checks "$1"
    check "date and time requested" "$(grep -q '^DATE TIME' "$d/server.log" && echo ok)"
    if [ "$date" = none ]; then
        check "no answer reported" "$(has_text "$d/screen_15.txt" 'No answer from the JIO server' && echo ok)"
    else
        check "MSX date and time set (read back)" "$(has_text "$d/screen_15.txt" 'Date and time set:' && has_text "$d/screen_15.txt" "${date%?}" && echo ok)"
    fi
    [ -z "${8:-}" ] && [ "$date" != none ] && check "automatic: serial line of the server shown" "$(has_text "$d/screen_15.txt" 'Joystick port 2' && echo ok)"
    [ "${8:-}" = C30 ] && check "JIO cartridge option (port 30H)" "$(has_text "$d/screen_15.txt" 'JIO cartridge, port 30H' && echo ok)"
    [ "${8:-}" = J1 ] && check "joystick port 1 option" "$(has_text "$d/screen_15.txt" 'Joystick port 1' && echo ok)"
    end_checks "$1" "$6"
}

# JIO.COM (clients/JIO_NFS) on the original MSX-DOS 2 (cartridge, no JIO ROM): boot from the floppy A:, the
# directory of the server is drive D: (JIO +D). JIO.COM ends with a warm restart (MSXDOS2.SYS and COMMAND2.COM
# reloaded, the batch file is not continued): the commands are in NFSTEST.BAT, typed at the prompt.
# Not tested: redirection to the JIO drive (_DUP of a file handle of the server is not supported by JIO.COM).
NFS_CMDS='D:\r\nDIR /W\r\nTYPE HELLO.TXT\r\nA:FCBREAD\r\nA:P2TEST\r\nCOPY A:COMMAND2.COM X.COM\r\nMD SUB\r\nCD SUB\r\nCOPY \\HELLO.TXT\r\nCD \\\r\nCOPY HELLO.TXT R1.TXT\r\nREN R1.TXT R2.TXT\r\nMOVE R2.TXT SUB\r\nATTRIB +R SUB\\R2.TXT\r\nDIR /W\r\n'
test_nfs() { # name machine "slots" description [options of JIO.COM] [serial line shown by JIO S]
    wanted "$1" || return
    rm -rf "$OUT/floppy_nfs"; mkdir -p "$OUT/floppy_nfs"
    cp -p "$OUT"/base/MSXDOS2.SYS "$OUT"/base/COMMAND2.COM "$OUT"/base/JIO.COM "$OUT"/base/FCBREAD.COM "$OUT"/base/P2TEST.COM "$OUT/floppy_nfs/"
    printf "JIO ${5:+$5 }+D\r\n" > "$OUT/floppy_nfs/AUTOEXEC.BAT"
    printf "$NFS_CMDS${5:+A:JIO S\r\n}" > "$OUT/floppy_nfs/NFSTEST.BAT"
    TYPE_TIME=25 TYPE_TEXT=$'NFSTEST\r' NFS=1 JIO_DRIVE=D run_scenario "$1" "$R" "$RM" "$2" "$3" 'REM\r\n' "$TEST/tcl/typecmd.tcl" 720 "$OUT/floppy_nfs" 0 "$(seq -s " " 20 2 80)"
    local d="$OUT/$1"
    begin_checks "$1"
    check "DIR of the JIO drive D:" "$(has_text "$d/screens.txt" 'COMMAND2.COM' && has_text "$d/screens.txt" 'FCBTEST .COM' && echo ok)"
    check "TYPE on D:" "$(has_text "$d/screens.txt" 'Hello from the JIO server!' && echo ok)"
    check "FCB open, block reads, close (FCBREAD)" "$(grep -A1 'FCBREAD: 00 00 0C 00 05 00 ' "$d/screens.txt" | tr -d ' \n' | grep -q 'Hellofromthe' && echo ok)"
    check "parameters and buffers in page 2 (P2TEST)" "$(grep -A2 'P2TEST: ' "$d/screens.txt" | tr -d ' \n' | grep -q 'P2TEST:00HELLO.TXT00Hello00000004from00\[\]' && echo ok)"
    check "X.COM identical to COMMAND2.COM" "$(cmp -s "$d/drive/X.COM" "$OUT/base/COMMAND2.COM" && echo ok)"
    check "date of the copy set (_HFTIME)" "$(t1=$(stat -c %Y "$d/drive/X.COM"); t2=$(stat -c %Y "$OUT/base/COMMAND2.COM"); [ $((t1 - t2)) -ge -2 ] && [ $((t1 - t2)) -le 2 ] && echo ok)"
    check "SUB/HELLO.TXT copied" "$([ -f "$d/drive/SUB/HELLO.TXT" ] && echo ok)"
    check "REN and MOVE (SUB/R2.TXT)" "$([ ! -e "$d/drive/R1.TXT" ] && [ ! -e "$d/drive/R2.TXT" ] && [ -f "$d/drive/SUB/R2.TXT" ] && echo ok)"
    check "ATTRIB +R (host file read only)" "$([ -f "$d/drive/SUB/R2.TXT" ] && [ ! -w "$d/drive/SUB/R2.TXT" ] && echo ok)"
    [ -n "${6:-}" ] && check "serial line of the installed driver (JIO S)" "$(has_text "$d/screens.txt" "$6" && echo ok)"
    [ -z "${5:-}" ] && check "automatic serial line shown at install" "$(has_text "$d/screens.txt" 'Serial line: joystick port 2' && echo ok)"
    end_checks "$1" "$4"
}

# JIO.COM on MSX-DOS 1 (no mapper support routines: driver segment found with the mapper I/O ports, stub below
# MSXDOS.SYS, BDOS jump and warm boot set to it): boot from the floppy A: (MSX-DOS 1 of the internal disk ROM),
# NFSTEST.BAT typed at the prompt once COMMAND.COM is reloaded
DOS1_FILES=${DOS1_FILES:-/mnt/DataLinux/Projects/MSX/sdcard/MSXDOS1}
setup_dos1() { # drive directory: RUNOK.COM, printing "RUN OK" (LD DE,0109H / LD C,9 / CALL 5 / RET)
    printf '\x11\x09\x01\x0e\x09\xcd\x05\x00\xc9RUN OK$' > "$1/RUNOK.COM"
}
test_nfs_dos1() { # name machine "slots" description
    wanted "$1" || return
    if [ ! -f "$DOS1_FILES/MSXDOS.SYS" ]; then echo "  SKIP  $1: $DOS1_FILES/MSXDOS.SYS not found"; return; fi
    rm -rf "$OUT/floppy_nfs"; mkdir -p "$OUT/floppy_nfs"
    cp -p "$DOS1_FILES/MSXDOS.SYS" "$DOS1_FILES/COMMAND.COM" "$OUT"/base/JIO.COM "$OUT/floppy_nfs/"
    printf 'JIO +D\r\n' > "$OUT/floppy_nfs/AUTOEXEC.BAT"
    printf 'JIO S\r\nD:\r\nDIR\r\nTYPE HELLO.TXT\r\nCOPY A:COMMAND.COM C.COM\r\nCOPY HELLO.TXT A:H2.TXT\r\nCOPY HELLO.TXT R1.TXT\r\nREN R1.TXT R2.TXT\r\nCOPY HELLO.TXT X.TXT\r\nDEL X.TXT\r\nRUNOK\r\nDIR\r\n' > "$OUT/floppy_nfs/NFSTEST.BAT"
    TYPE_TIME=25 TYPE_TEXT=$'NFSTEST\r' NFS=1 JIO_DRIVE=D SETUP=setup_dos1 run_scenario "$1" "$R" "$RM" "$2" "$3" 'REM\r\n' "$TEST/tcl/typecmd.tcl" 360 "$OUT/floppy_nfs" 0 "$(seq -s " " 20 2 80)"
    local d="$OUT/$1"
    begin_checks "$1"
    check "JIO.COM installed (JIO S)" "$(has_text "$d/screens.txt" 'Disks handled: D:' && echo ok)"
    check "DIR of the JIO drive D:" "$(has_text "$d/screens.txt" 'COMMAND2 COM' && echo ok)"
    check "TYPE on D: (whole text, nothing after)" "$(grep -m1 -A3 'TYPE HELLO.TXT' "$d/screens.txt" | tr -d ' \n' | grep -q '^D>TYPEHELLO.TXTHellofromtheJIOserver!Secondline.$' && echo ok)"
    check "COPY A: -> D: (byte compare)" "$(cmp -s "$d/drive/C.COM" "$DOS1_FILES/COMMAND.COM" && echo ok)"
    check "COPY D: -> A: (byte compare)" "$(cmp -s "$d/floppy_out/$(ls "$d/floppy_out" | grep -ix 'h2.txt')" "$OUT/base/hello.txt" && echo ok)"
    check "REN on D:" "$([ ! -e "$d/drive/R1.TXT" ] && cmp -s "$d/drive/R2.TXT" "$OUT/base/hello.txt" && echo ok)"
    check "DEL on D:" "$([ ! -e "$d/drive/X.TXT" ] && echo ok)"
    check "program run from D:" "$(has_text "$d/screens.txt" 'RUN OK' && echo ok)"
    end_checks "$1" "$4"
}

# JIO-ROM.COM on MSX-DOS 1: boot from the floppy (MSX-DOS 1 of the internal disk ROM), AUTOEXEC.BAT runs JIO-ROM, which
# starts MSX-DOS 2 with the JIO ROM in a segment of the memory mapper: JIO drive A: + floppy B:, same commands as the
# MSX-DOS 2 ROM (AUTOEXEC.BAT of the JIO drive)
test_jiorom() { # name machine "slots" description [floppy size] [primary RAM of MSX-DOS 2 (KB), default 112]
    wanted "$1" || return
    if [ ! -f "$DOS1_FILES/MSXDOS.SYS" ]; then echo "  SKIP  $1: $DOS1_FILES/MSXDOS.SYS not found"; return; fi
    JIOROM_BOOT=1 EXTRA_SCRIPT="$TEST/tcl/csrsw.tcl" run_scenario "$1" "$JIOROM" "$JIOROM_MAP" "$2" "$3" "VER\\r\\nFDTEST\\r\\n$DOS_HYBRID" "$TEST/tcl/screens.tcl" "${5:-360}" "$OUT/floppy_base" 1 "$(seq -s " " 14 2 30) 40 60 80"
    local d="$OUT/$1"
    begin_checks "$1"
    check "MSX-DOS 2 started (JIO ROM in RAM)" "$(has_text "$d/screens.txt" 'COMMAND2.COM version' && echo ok)"
    check "no cursor shown when printing (CSRSW cleared at boot: DIR as fast as with the ROM)" "$(grep -q 'CSRSW=00' "$d/csrsw.txt" 2> /dev/null && ! grep -q 'CSRSW=01' "$d/csrsw.txt" && echo ok)"
    check "segment written to port FDH kept across the interrupts (FDTEST, signature search)" "$(has_text "$d/screens.txt" 'FDTEST: OK' && echo ok)"
    check "top segment of the mapper used by the ROM (${6:-128 KB: 112 KB} for MSX-DOS 2)" "$(has_text "$d/screens.txt" "Primary RAM ${6:-112}KB" && echo ok)"
    check "FCB test result on both drives" "$([ "$(count_text "$d/screen_80.txt" "$FCB_OK")" -ge 2 ] && echo ok)"
    check "open with the FIB after the last _FNEXT" "$(has_text "$d/screens.txt" "$FIB_OK" && echo ok)"
    check "floppy -> JIO copy (FLOPPY.TXT)" "$([ -f "$d/drive/FLOPPY.TXT" ] && echo ok)"
    check "Y.COM (JIO -> floppy -> JIO) identical" "$(cmp -s "$d/drive/Y.COM" "$d/drive/COMMAND2.COM" && echo ok)"
    check "floppy SUB/HELLO.TXT" "$([ -f "$d/floppy_out/sub/hello.txt" ] && echo ok)"
    check "X.COM deleted from floppy" "$([ ! -f "$d/floppy_out/x.com" ] && echo ok)"
    check "floppy DIR redirected to JIO drive" "$(has_text "$d/drive/OUT.TXT" 'Directory of B:' && echo ok)"
    end_checks "$1" "$4"
}

# SofaRunIt (SRI.COM, not in this repository: SOFARUNIT environment, default the SD card folder of the author) launches
# GAME.DSK (MSX-DOS 1 boot disk made with the disk manipulator, AUTOEXEC.BAT runs RUNOK.COM) from the JIO drive A:
SOFARUNIT=${SOFARUNIT:-/mnt/DataLinux/Projects/MSX/sdcard/SOFARUN/SRI.COM}
setup_sri() { # drive directory
    cp "$SOFARUNIT" "$1/SRI.COM"
    rm -rf "$1/../game"; mkdir -p "$1/../game"
    cp -p "$DOS1_FILES/MSXDOS.SYS" "$DOS1_FILES/COMMAND.COM" "$1/../game/"
    printf '\x11\x09\x01\x0e\x09\xcd\x05\x00\xc9RUN OK$' > "$1/../game/RUNOK.COM"
    printf 'RUNOK\r\n' > "$1/../game/AUTOEXEC.BAT"
    ( cd "$1/.." && DISK_FILE=drive/GAME.DSK FLOPPY_SIZE=360 FLOPPY_FILES=game timeout 30 openmsx -machine Philips_VG_8235 -script "$TEST/tcl/mkdisk.tcl" > /dev/null 2>&1 )
}
test_sri() { # name rom map machine slots description
    wanted "$1" || return
    if [ ! -f "$SOFARUNIT" ]; then echo "  SKIP  $1: $SOFARUNIT not found"; return; fi
    if [ ! -f "$DOS1_FILES/MSXDOS.SYS" ]; then echo "  SKIP  $1: $DOS1_FILES/MSXDOS.SYS not found"; return; fi
    TYPE_TIME=18 TYPE_TEXT=$'\r' SETUP=setup_sri run_scenario "$1" "$2" "$3" "$4" "$5" 'SRI GAME.DSK\r\n' "$TEST/tcl/typecmd.tcl" "" "" 1 "15 20 25 30 40 60"
    local d="$OUT/$1"
    begin_checks "$1"
    check "MSX-DOS 1 of the disk image booted by SofaRunIt, program run" "$(has_text "$d/screens.txt" 'RUN OK' && echo ok)"
    end_checks "$1" "$6"
}

# JIO-ROM.CAS (clients/JIO_CAS, tape version of JIO-ROM.COM) in the cassette player, BLOAD"CAS:",R from MSX BASIC: SHIFT held at boot
# (disk ROM disabled), the ROM is read from the tape, MSX-DOS 2 starts with the JIO drive A:. Without SHIFT, MSX-DOS 1 of
# the disk ROM is there: message, nothing loaded.
test_jiocas() { # name machine "slots" description [no SHIFT at boot]
    wanted "$1" || return
    local shift=1 tt=10 first=20
    [ -n "${5:-}" ] && shift="" tt=25 first=40
    TAPE_TIME=$tt SHIFT_BOOT=$shift run_scenario "$1" "$JIOCAS" "$JIOROM_MAP" "$2" "-cassetteplayer $JIOCAS $3" \
        'VER\r\nFDTEST\r\nFCBTEST\r\n' "$TEST/tcl/tape.tcl" "" "" 1 "$first 200 240"
    local d="$OUT/$1"
    begin_checks "$1"
    if [ -n "$shift" ]; then
        check "loader started from the tape" "$(has_text "$d/screen_20.txt" 'Loading...' && echo ok)"
        check "MSX-DOS 2 started (JIO ROM in RAM)" "$(has_text "$d/screens.txt" 'COMMAND2.COM version' && echo ok)"
        check "FCB test result" "$(has_text "$d/screen_240.txt" "$FCB_OK" && echo ok)"
        check "segment written to port FDH kept across the interrupts (FDTEST)" "$(has_text "$d/screens.txt" 'FDTEST: OK' && echo ok)"
        check "function keys of BASIC not shown" "$( ! has_text "$d/screen_240.txt" 'color  auto' && echo ok)"
    else
        check "message: MSX-DOS already there" "$(has_text "$d/screen_40.txt" 'MSX-DOS is already there' && echo ok)"
        check "nothing loaded from the server" "$( ! grep -q OPEN "$d/server.log" && echo ok)"
    fi
    end_checks "$1" "$4"
}

# MSX-DOS 2 ROM on a MSX1 with a memory mapper (RAM expansion): JIO drive A:, FDTEST, FCB functions
test_msx1() { # name rom map machine slots description
    wanted "$1" || return
    run_scenario "$1" "$2" "$3" "$4" "$5" 'VER\r\nFDTEST\r\nFCBTEST\r\n' "$TEST/tcl/screens.tcl" "" "" 1 "20 40 60"
    local d="$OUT/$1"
    begin_checks "$1"
    check "MSX-DOS 2 started" "$(has_text "$d/screens.txt" 'COMMAND2.COM version' && echo ok)"
    check "FCB test result" "$(has_text "$d/screen_60.txt" "$FCB_OK" && echo ok)"
    check "FDTEST" "$(has_text "$d/screens.txt" 'FDTEST: OK' && echo ok)"
    end_checks "$1" "$6"
}

# MSX-DOS 2 ROM taking over from a MSX-DOS 2 cartridge in slot 1
test_takeover_hybrid() { # name rom map machine floppy size description
    wanted "$1" || return
    run_scenario "$1" "$2" "$3" "$4" "-ext msxdos2 -cartb $2" 'VER\r\nDIR B:/W\r\nCOPY A:HELLO.TXT B:\r\nTYPE B:HELLO.TXT\r\n' "$TEST/tcl/screens.tcl" "$5" "$OUT/floppy_base" 1
    local d="$OUT/$1"
    begin_checks "$1"
    check "floppy listed" "$(has_text "$d/screen_60.txt" 'FLOPPY  .TXT' && echo ok)"
    check "file copied to the floppy and typed" "$(has_text "$d/screen_60.txt" 'Hello from the JIO server!' && [ -f "$d/floppy_out/hello.txt" ] && echo ok)"
    end_checks "$1" "$6"
}

# ------------------------------------------------------------------------------
# Main
# ------------------------------------------------------------------------------
command -v openmsx > /dev/null || { echo "openmsx not found"; exit 1; }
command -v z88dk-z80asm > /dev/null || { echo "z88dk not found"; exit 1; }

echo "Build:"
prepare_files
build_rom dos2 p0_kernel.asm "-DJIO -DHYBRID"
build_rom dos2_safe p0_kernel.asm "-DJIO -DJIOSAFE -DHYBRID"

R="$OUT/jio_dos2.rom"; RM="$OUT/obj_dos2/jio_dos2.map"
RS="$OUT/jio_dos2_safe.rom"; RSM="$OUT/obj_dos2_safe/jio_dos2_safe.map"

echo "MSX-DOS 2 ROM (JIO drive A: + floppy B:):"
test_dos_hybrid      dos2_vg8235      "$R" "$RM" Philips_VG_8235   "-carta $R" 360 "VG-8235 (360 KB drive), both drives"
test_dos_hybrid      dos2_nms8255     "$R" "$RM" Philips_NMS_8255  "-carta $R" 720 "NMS 8255 (720 KB drive), both drives"
test_dos_hybrid      dos2_turbor      "$R" "$RM" Panasonic_FS-A1ST "-carta $R" 720 "turbo R, both drives"
test_noserver        dos2_noserver    "$R" "$RM" Philips_VG_8235   360 "VG-8235, no server: boots from the floppy"
test_basic           dos2_basic_jio   "$R" "$RM" Philips_VG_8235   "-carta $R" A "VG-8235, Disk BASIC on the JIO drive" 360
test_basic           dos2_basic_flop  "$R" "$RM" Philips_VG_8235   "-carta $R" B "VG-8235, Disk BASIC on the floppy" 360
test_renmove         dos2_renmove     "$R" "$RM" Philips_VG_8235   "-carta $R" "VG-8235, REN, MOVE, ATTRIB on JIO drive A: and floppy B:" 360
test_longnames       dos2_longnames   "$R" "$RM" Philips_VG_8235   "-carta $R" "VG-8235, long host names and 8.3 aliases"
test_sofacopy        dos2_sofacopy    "$R" "$RM" Philips_VG_8235   "-carta $R" "VG-8235, SofaCopy (SC.COM) from A: to A:\\SUB (archive attribute)"
test_ramdisk         dos2_ramdisk     "$R" "$RM" Philips_VG_8235   "-carta $R" "VG-8235, RAMDISK (H: on the server), MSX reset" 360
test_takeover_hybrid dos2_takeover    "$R" "$RM" Philips_NMS_8255  720 "NMS 8255, takes over from a MSX-DOS 2 cartridge in slot 1"
test_readonly        dos2_readonly    "$R" "$RM" Philips_VG_8235   "-carta $R" "VG-8235, read only server, floppy B: writable" 360
test_bootloader_hybrid dos2_bootsector "$R" "$RM" Philips_VG_8235 "-carta $R" "VG-8235, disk image mode: self-booting image (boot loader of the boot sector)"
test_image_hybrid    dos2_image       "$R" "$RM" Philips_VG_8235   "-carta $R" 360 "VG-8235, server in disk image mode: image A: (sectors) + floppy B:"
# JIO cartridge (I/O port 30H, JIO_LINE: faked by the bridge): the serial routines are intercepted, the port is not tested (not emulated)
JIO_LINE=30 test_dos_hybrid dos2_cart "$R" "$RM" Philips_VG_8235 "-carta $R" 360 "VG-8235, JIO cartridge found at boot (port 30H, not emulated), both drives"
test_dos_hybrid      dos2_safe        "$RS" "$RSM" Philips_VG_8235 "-carta $RS" 360 "VG-8235, safe ROM (joystick ports only), both drives"
JIO_LINE=FE test_dos_hybrid dos2_joy1 "$R" "$RM" Philips_VG_8235 "-carta $R" 360 "VG-8235, joystick port 1 found at boot (kernel patched, serial line intercepted), both drives"
test_format          dos2_format_vg   "$R" "$RM" Philips_VG_8235   "-carta $R" 360 "VG-8235, FORMAT B: (360 KB floppy), then COPY to B:" 354K
test_msx1            dos2_msx1        "$R" "$RM" Philips_VG_8020   "-carta $R -extb ram1mb" "VG-8020 (MSX1) + 1 MB mapper, JIO drive A:"
test_format          dos2_format_nms  "$R" "$RM" Philips_NMS_8255  "-carta $R" 720 "NMS 8255, FORMAT B: (720 KB floppy, double sided), then COPY to B:" 713K 2

echo "Clients:"
test_jiotime   jiotime      "$R" "$RM" Philips_VG_8235   "-carta $R"   "VG-8235, JIOTIME.COM sets the date and time"
test_jiotime   jiotime_tr   "$R" "$RM" Panasonic_FS-A1ST "-carta $R"   "turbo R, JIOTIME.COM sets the date and time (Z80 mode)"
test_jiotime   jiotime_none "$R" "$RM" Philips_VG_8235   "-carta $R"   "VG-8235, JIOTIME.COM without answer of the server" none
test_jiotime   jiotime_cart "$R" "$RM" Philips_VG_8235   "-carta $R"   "VG-8235, JIOTIME C30 (JIO cartridge option, port not emulated)" "" C30
test_jiotime   jiotime_joy1 "$R" "$RM" Philips_VG_8235   "-carta $R"   "VG-8235, JIOTIME J1 (joystick port 1, serial line intercepted)" "" J1
test_nfs       nfs_nms8255  Philips_NMS_8255  "-ext msxdos2"           "NMS 8255, JIO.COM (JIO_NFS) on the original MSX-DOS 2, drive D:"
test_nfs       nfs_turbor   Panasonic_FS-A1ST ""                       "turbo R, JIO.COM (JIO_NFS) on the internal MSX-DOS 2, drive D:"
test_nfs       nfs_cart     Philips_NMS_8255  "-ext msxdos2"           "NMS 8255, JIO.COM with the JIO cartridge option (JIO C30 +D, port not emulated)" C30 'JIO cartridge, port 30H'
test_nfs       nfs_joy1     Philips_NMS_8255  "-ext msxdos2"           "NMS 8255, JIO.COM on joystick port 1 (JIO J1 +D, serial line intercepted)" J1 'joystick port 1'
test_nfs_dos1  nfs_dos1     Philips_VG_8235   "-ext ram1mb"            "VG-8235 + 1 MB mapper, JIO.COM on MSX-DOS 1 (internal disk ROM)"

test_sri       sri_vg8235     "$R" "$RM" Philips_VG_8235  "-carta $R -extb ram1mb" "VG-8235 + 1 MB mapper, SofaRunIt launches a MSX-DOS 1 disk image from the JIO drive"

echo "JIO-ROM.COM, JIO-ROM.CAS (JIO MSX-DOS 2 ROM in RAM, MSX-DOS 1 computers, tape):"
test_jiorom    jiorom_vg8235  Philips_VG_8235  ""                      "VG-8235 (128 KB mapper), JIO-ROM.COM on MSX-DOS 1: JIO drive A: + floppy B:"
test_jiorom    jiorom_nms8255 Philips_NMS_8255 ""                      "NMS 8255 (128 KB mapper, 720 KB drive), JIO-ROM.COM on MSX-DOS 1: JIO drive A: + floppy B:" 720
test_jiorom    jiorom_1mb     Philips_VG_8235  "-ext ram1mb"           "VG-8235 + 1 MB mapper, JIO-ROM.COM: ROM in the mapper of page 3" 360 "${JIOROM_1MB_RAM:-1008}"
test_jiorom    jiorom_1mb_s2  Philips_VG_8235  "-extb ram1mb"          "VG-8235 + 1 MB mapper in slot 2, JIO-ROM.COM" 360 "${JIOROM_1MB_RAM:-1008}"
# scenarios of the MSX-DOS 2 ROM, with the ROM started by JIO-ROM.COM (MSX-DOS 1 boot: actions of the scripts delayed)
JIOROM_BOOT=1 TIME_SHIFT=15 test_basic  jiorom_basic_jio  "$JIOROM" "$JIOROM_MAP" Philips_VG_8235  "" A "VG-8235, JIO-ROM.COM, Disk BASIC on the JIO drive" 360
JIOROM_BOOT=1 TIME_SHIFT=15 test_basic  jiorom_basic_flop "$JIOROM" "$JIOROM_MAP" Philips_VG_8235  "" B "VG-8235, JIO-ROM.COM, Disk BASIC on the floppy" 360
JIOROM_BOOT=1 TIME_SHIFT=15 test_format jiorom_format     "$JIOROM" "$JIOROM_MAP" Philips_NMS_8255 "" 720 "NMS 8255, JIO-ROM.COM, FORMAT B: (720 KB floppy, double sided), then COPY to B:" 713K 2
JIOROM_BOOT=1 TIME_SHIFT=15 test_sri    jiorom_sri        "$JIOROM" "$JIOROM_MAP" Philips_VG_8235  "-extb ram1mb" "VG-8235 + 1 MB mapper, JIO-ROM.COM, SofaRunIt launches a MSX-DOS 1 disk image from the JIO drive"
test_jiocas    jiocas_vg8235  Philips_VG_8235  ""                      "VG-8235 booted with SHIFT (disk ROM disabled), JIO-ROM.CAS: BLOAD\"CAS:\",R"
test_jiocas    jiocas_msx1    Philips_VG_8020  "-ext ram1mb"           "VG-8020 (MSX1) + 1 MB mapper, JIO-ROM.CAS: BLOAD\"CAS:\",R"
test_jiocas    jiocas_dos     Philips_VG_8235  ""                      "VG-8235 with MSX-DOS 1, JIO-ROM.CAS: message, nothing loaded" noshift
JIOROM_BOOT=1 TIME_SHIFT=15 RESET_TIME=40 test_ramdisk jiorom_ramdisk   "$JIOROM" "$JIOROM_MAP" Philips_VG_8235  "" "VG-8235, JIO-ROM.COM, RAMDISK (H: on the server), MSX reset" 360

echo
echo "Passed: $PASSED, failed: $FAILED${FAILED_LIST:+ ($FAILED_LIST )}"
[ "$FAILED" = 0 ]
