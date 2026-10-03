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
      date +"db \"%Y-%m-%d\"" > rdate.inc &&
      z88dk-z80asm -b -d -l -m $defines -O"$OUT/obj_$name" -o=jio_$name.bin p1_main.asm p3_paging.asm drv_jio.asm "$kernel" &&
      z88dk-appmake +glue -b "$OUT/obj_$name/jio_$name" --filler 0xFF --clean > /dev/null &&
      z88dk-appmake +rom -b "$OUT/obj_$name/jio_${name}__.bin" -o "$OUT/jio_$name.rom" -s 32768 --org 0 > /dev/null
      rc=$?; rm -f rdate.inc; exit $rc ) || { echo "Build of $name ROM failed"; exit 1; }
    awk '/^__P0_KERNEL_size/ { printf "  %s ROM: kernel %d bytes (limit 16384)\n", n, strtonum("0x" substr($3,2)) }' n="$name" "$OUT/obj_$name/jio_$name.map"
}

# Addresses of the intercepted routines, from the map file
make_bridge() { # map output
    local a
    a() { grep -E "^$1 " "$2" | sed -E 's/.*\$([0-9A-F]+).*/0x\1/'; }
    sed -e "s/@RFS_TX@/$(a RFS_TX "$1")/g; s/@J_RX1@/$(a J_RX1 "$1")/g; s/@DRIVES_Retry@/$(a DRIVES_Retry "$1")/g; s/@DRIVES_Exit@/$(a DRIVES_Exit "$1")/g; s/@vJIOTransmit@/$(a vJIOTransmit "$1")/g; s/@bJIOReceive@/$(a bJIOReceive "$1")/g" \
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
    # JIO.COM (clients/JIO_NFS), built in a copy (its make script writes next to the sources)
    rm -rf "$OUT/nfs"
    mkdir -p "$OUT/nfs/clients"
    cp -r "$SRC/../../common" "$OUT/nfs/"
    cp -r "$SRC/../JIO_NFS" "$OUT/nfs/clients/"
    rm -rf "$OUT/nfs/clients/JIO_NFS/0_Builds" "$OUT/nfs/0_Builds"
    ( cd "$OUT/nfs/clients/JIO_NFS" && bash 0_Build.sh > build.log 2>&1 && cp 0_Builds/JIO.COM "$OUT/base/" ) || { echo "Build of JIO.COM failed"; exit 1; }
    # serial routines: in the resident stub (driver in a mapper segment) or in the resident driver
    NFS_MAP="$OUT/nfs/0_Builds/obj/JIO_NFS/stub.map"
    [ -f "$NFS_MAP" ] || NFS_MAP="$OUT/nfs/0_Builds/obj/JIO_NFS/driver.sym"
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
    TOOL_TX=$(tool_addr vJIOTransmit) TOOL_RX=$(tool_addr bJIOReceive) \
    NFS_TX=${NFS:+$(nfs_offset vJIOTransmit)} NFS_RX=${NFS:+$(nfs_offset bJIOReceive)} \
    JIO_DRIVES=$njio JIO_HANDSHAKE=$handshake SCREEN_TIMES="$times" timeout 180 openmsx -machine "$machine" $disk $slots \
        -script bridge.tcl -script "$script" > openmsx.log 2>&1
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
test_ramdisk() { # name rom map machine slots description [floppy size]
    wanted "$1" || return
    local autoexec="$RAMDISK$RAMDISK_END"
    [ -n "${7:-}" ] && autoexec="$RAMDISK$RAMDISK_FLOPPY$RAMDISK_END"
    FIRST_TIME=50 SECOND_TIME=20 REBOOT_AUTOEXEC='RAMDISK > A:RAM2.TXT\r\nDIR H:\r\n' \
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
test_jiotime() { # name rom map machine slots description [mock date]
    wanted "$1" || return
    [ -n "${REAL_SERVER:-}" ] && return     # date of the mock server (or no answer)
    local date=${7:-"2031-10-25 13:45:30"}
    MOCK_DATE="$date" run_scenario "$1" "$2" "$3" "$4" "$5" 'JIOTIME\r\n' "$TEST/tcl/screens.tcl" "" "" 1 "15"
    local d="$OUT/$1"
    begin_checks "$1"
    check "date and time requested" "$(grep -q '^DATE TIME' "$d/server.log" && echo ok)"
    if [ "$date" = none ]; then
        check "no answer reported" "$(has_text "$d/screen_15.txt" 'No answer from the JIO server' && echo ok)"
    else
        check "MSX date and time set (read back)" "$(has_text "$d/screen_15.txt" 'Date and time set:' && has_text "$d/screen_15.txt" "${date%?}" && echo ok)"
    fi
    end_checks "$1" "$6"
}

# JIO.COM (clients/JIO_NFS) on the original MSX-DOS 2 (cartridge, no JIO ROM): boot from the floppy A:, the
# directory of the server is drive D: (JIO +D). JIO.COM ends with a warm restart (MSXDOS2.SYS and COMMAND2.COM
# reloaded, the batch file is not continued): the commands are in NFSTEST.BAT, typed at the prompt.
# Not tested: redirection to the JIO drive (_DUP of a file handle of the server is not supported by JIO.COM).
NFS_CMDS='D:\r\nDIR /W\r\nTYPE HELLO.TXT\r\nA:FCBREAD\r\nA:P2TEST\r\nCOPY A:COMMAND2.COM X.COM\r\nMD SUB\r\nCD SUB\r\nCOPY \\HELLO.TXT\r\nCD \\\r\nCOPY HELLO.TXT R1.TXT\r\nREN R1.TXT R2.TXT\r\nMOVE R2.TXT SUB\r\nATTRIB +R SUB\\R2.TXT\r\nDIR /W\r\n'
test_nfs() { # name machine "slots" description
    wanted "$1" || return
    rm -rf "$OUT/floppy_nfs"; mkdir -p "$OUT/floppy_nfs"
    cp -p "$OUT"/base/MSXDOS2.SYS "$OUT"/base/COMMAND2.COM "$OUT"/base/JIO.COM "$OUT"/base/FCBREAD.COM "$OUT"/base/P2TEST.COM "$OUT/floppy_nfs/"
    printf 'JIO +D\r\n' > "$OUT/floppy_nfs/AUTOEXEC.BAT"
    printf "$NFS_CMDS" > "$OUT/floppy_nfs/NFSTEST.BAT"
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
    end_checks "$1" "$4"
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

R="$OUT/jio_dos2.rom"; RM="$OUT/obj_dos2/jio_dos2.map"

echo "MSX-DOS 2 ROM (JIO drive A: + floppy B:):"
test_dos_hybrid      dos2_vg8235      "$R" "$RM" Philips_VG_8235   "-carta $R" 360 "VG-8235 (360 KB drive), both drives"
test_dos_hybrid      dos2_nms8255     "$R" "$RM" Philips_NMS_8255  "-carta $R" 720 "NMS 8255 (720 KB drive), both drives"
test_dos_hybrid      dos2_turbor      "$R" "$RM" Panasonic_FS-A1ST "-carta $R" 720 "turbo R, both drives"
test_noserver        dos2_noserver    "$R" "$RM" Philips_VG_8235   360 "VG-8235, no server: boots from the floppy"
test_basic           dos2_basic_jio   "$R" "$RM" Philips_VG_8235   "-carta $R" A "VG-8235, Disk BASIC on the JIO drive" 360
test_basic           dos2_basic_flop  "$R" "$RM" Philips_VG_8235   "-carta $R" B "VG-8235, Disk BASIC on the floppy" 360
test_renmove         dos2_renmove     "$R" "$RM" Philips_VG_8235   "-carta $R" "VG-8235, REN, MOVE, ATTRIB on JIO drive A: and floppy B:" 360
test_longnames       dos2_longnames   "$R" "$RM" Philips_VG_8235   "-carta $R" "VG-8235, long host names and 8.3 aliases"
test_ramdisk         dos2_ramdisk     "$R" "$RM" Philips_VG_8235   "-carta $R" "VG-8235, RAMDISK (H: on the server), MSX reset" 360
test_takeover_hybrid dos2_takeover    "$R" "$RM" Philips_NMS_8255  720 "NMS 8255, takes over from a MSX-DOS 2 cartridge in slot 1"
test_readonly        dos2_readonly    "$R" "$RM" Philips_VG_8235   "-carta $R" "VG-8235, read only server, floppy B: writable" 360
test_bootloader_hybrid dos2_bootsector "$R" "$RM" Philips_VG_8235 "-carta $R" "VG-8235, disk image mode: self-booting image (boot loader of the boot sector)"
test_image_hybrid    dos2_image       "$R" "$RM" Philips_VG_8235   "-carta $R" 360 "VG-8235, server in disk image mode: image A: (sectors) + floppy B:"

echo "Clients:"
test_jiotime   jiotime      "$R" "$RM" Philips_VG_8235   "-carta $R"   "VG-8235, JIOTIME.COM sets the date and time"
test_jiotime   jiotime_tr   "$R" "$RM" Panasonic_FS-A1ST "-carta $R"   "turbo R, JIOTIME.COM sets the date and time (Z80 mode)"
test_jiotime   jiotime_none "$R" "$RM" Philips_VG_8235   "-carta $R"   "VG-8235, JIOTIME.COM without answer of the server" none
test_nfs       nfs_nms8255  Philips_NMS_8255  "-ext msxdos2"           "NMS 8255, JIO.COM (JIO_NFS) on the original MSX-DOS 2, drive D:"
test_nfs       nfs_turbor   Panasonic_FS-A1ST ""                       "turbo R, JIO.COM (JIO_NFS) on the internal MSX-DOS 2, drive D:"

echo
echo "Passed: $PASSED, failed: $FAILED${FAILED_LIST:+ ($FAILED_LIST )}"
[ "$FAILED" = 0 ]
