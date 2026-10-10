# JIO MSX-DOS 2 emulator tests

Automatic tests of the JIO MSX-DOS 2 ROM (JIO drives and local drives, as built by `0_Build.sh`) and of
the JIO clients (JIOTIME.COM, JIO.COM) in openMSX, without an MSX and without a JIO server.

```
./0_RunTests.sh                            # all the scenarios
./0_RunTests.sh dos2_vg8235 dos2_basic_jio # only some scenarios
```

From the root of the repository: `./0_Test.sh [--real-server <JIOServerCLI>] [scenario...]`.

Every scenario prints `PASS` or `FAIL` (with the failed checks). The script exits with 0 when all
scenarios passed. A full run takes about 6 minutes.

Needs `openmsx` (with the system ROMs of the machines used), `z88dk` and `python3`.
`MSXDOS2.SYS` and `COMMAND2.COM` are taken from `../../JIO_NFS/MSX-DOS2/`.

## How it works

- The ROM is assembled into `out/` (without the IAR compiler step: `drv_jio_c.asm` is used as is).
- `tcl/bridge.tcl.in` is turned into `bridge.tcl` with the addresses of the ROM map file. In openMSX it
  intercepts the serial routines of the kernel (`J_TXSEG`, `J_RX1`) and of the driver (`vJIOTransmit`,
  `bJIOReceive`) and forwards the bytes to `mockserver.py` over TCP. The "waiting for server"
  handshake of the ROM is skipped (the RESET request of the driver is still sent); `JIO_DRIVES` sets the
  number of JIO drives it reports (0 simulates [ESC]), `JIO_AUTORETRY` the "Auto retry" flag of the server.
  With `JIO_HANDSHAKE` (disk image scenario), the handshake is not skipped.
- `mockserver.py` implements the `COMMAND_BDOS` file functions of the protocol
  (see `msxjio_protocol_specification.md`) on a host directory, served as drive A:. It follows the
  behavior of the C++ server, it is not the C++ server: server changes must still be tested with the
  real server. `_RAMD` creates the RAM disk H: in a temporary directory, like the C++ server.
  With `MOCK_IMAGE`, it serves a disk image instead (`COMMAND_DRIVE_*` commands, no drive served by
  `COMMAND_BDOS`), like the C++ server in disk image mode.
- `AUTOEXEC.BAT` of drive A: runs the DOS commands of the scenario, screens are dumped with
  `get_screen`. Floppy images are created and read back with the openMSX disk manipulator.
- `fcbtest/fcbtest.asm` tests the MSX-DOS 1 FCB functions (open, sequential read and write, block
  read, search, file size, rename, delete). The expected result line is `RENAME/DEL/DEL: 00 00 FF`.

Because the serial routines are replaced, the tests do not check the transmission timings: those are
only tested on real hardware.

## Real server

```
REAL_SERVER=<build>/JIOServerCLI ./0_RunTests.sh
```

runs the scenarios with the real server (`server/JIOServerCLI.pro`, command line version of the C++ server) instead of
`mockserver.py`: the server opens a pseudo terminal as its serial port, `realbridge.py` passes the bytes
between it and the bridge (one answer of the server = one `FFh..F0h` packet). The checks of the log of the
mock server (`mock_check`) and the JIOTIME scenarios (date of the mock server, no answer) are skipped.

## Scenarios

| Scenario | ROM | Machine | Tests |
|---|---|---|---|
| `dos2_vg8235`, `dos2_nms8255`, `dos2_turbor` | MSX-DOS 2 | VG-8235 (360 KB), NMS 8255, FS-A1ST (720 KB) | JIO drive A: + floppy B:: boot, FIB handling (`fibtest/`), copies both ways (byte compare), MD/CD on the floppy, DIR of the floppy redirected to the JIO drive, FCB functions on both drives, DEL |
| `dos2_noserver` | MSX-DOS 2 | VG-8235 | no server: the floppy is A:, MSX-DOS 2 boots from it, FCB functions, `RAMDISK` answers "Not enough memory" |
| `dos2_basic_jio`, `dos2_basic_flop` | MSX-DOS 2 | VG-8235 | Disk BASIC on the JIO drive / on the floppy: OPEN, PRINT#, LINE INPUT#, SAVE, KILL, LOAD, FILES |
| `dos2_ramdisk` | MSX-DOS 2 | VG-8235 (360 KB) | `RAMDISK 32K` (H: on the server): copies both ways (byte compare), MD, FCB functions on H:, free space, disk full, H: -> floppy, `RAMDISK 0 /D`, RAM disk destroyed by a MSX reset (`tcl/reboot.tcl`) |
| `dos2_longnames` | MSX-DOS 2 | VG-8235 | long host names: MD/CD with a long name, 8.3 aliases (`BOMBAM~1`, `LONGFI~1.TEX`) in DIR, CD, TYPE, COPY into an aliased directory |
| `dos2_renmove` | MSX-DOS 2 | VG-8235 (360 KB) | REN, MOVE into a directory, ATTRIB +R / -R, DEL of a read only file refused, copy back (`_RENAME`, `_MOVE`, `_ATTR`); on the JIO drive A: and on the floppy B: |
| `dos2_takeover` | MSX-DOS 2 | NMS 8255 + MSX-DOS 2 cartridge in slot 1 | the JIO ROM takes over, copy to the floppy |
| `sri_vg8235` | MSX-DOS 2 | VG-8235 + 1 MB mapper | SofaRunIt (`SRI.COM`, not in this repository: `SOFARUNIT` environment) launches `GAME.DSK` (MSX-DOS 1 boot disk made with the disk manipulator) from the JIO drive A:, its `AUTOEXEC.BAT` runs a program |
| `dos2_readonly` | MSX-DOS 2 | VG-8235 (360 KB) | read only server (`MOCK_READONLY`): COPY, MD, DEL, REN, ATTRIB on A: refused with "Write protected disk", host files unchanged, TYPE works, RAM disk H: and floppy B: writable |
| `dos2_bootsector` | MSX-DOS 2 | VG-8235 | server in disk image mode, self-booting image (`IMAGE_SETUP`): the boot loader of the boot sector (C01EH) is started at boot, as with a game disk, MSX-DOS 2 is not started |
| `dos2_image` | MSX-DOS 2 | VG-8235 (360 KB) | server in disk image mode (`MOCK_IMAGE`, 720 KB image made from the drive files): real handshake (`COMMAND_DRIVE_INFO`, `_LOGIN` = 0), MSX-DOS 2 boots from the image A: (sectors with CRC, local FAT12 drive), copies image <-> floppy B:, MD/CD, redirection, FCB functions, no RAM disk; image read back in `image_out/` |
| `dos2_cart` | MSX-DOS 2 | VG-8235 (360 KB) | JIO cartridge found at boot (`JIO_LINE=30`: the bridge fakes the line found, port 30H), same commands as `dos2_vg8235`. The serial routines are intercepted: the I/O register of the cartridge is not emulated |
| `dos2_safe` | MSX-DOS 2 | VG-8235 (360 KB) | safe ROM (`jio_dos2_safe.rom`, joystick ports only), same commands as `dos2_vg8235` |
| `dos2_joy1` | MSX-DOS 2 | VG-8235 (360 KB) | joystick port 1 found at boot (`JIO_LINE=FE`): kernel patched at boot, same commands as `dos2_vg8235` (serial routines intercepted) |
| `jiotime_joy1`, `nfs_joy1` | MSX-DOS 2 | VG-8235, NMS 8255 | `JIOTIME J1`, `JIO J1 +D`: joystick port 1 option (shown by `JIO S`) |
| `jiotime_cart` | MSX-DOS 2 | VG-8235 | `JIOTIME C30`: JIO cartridge option (port shown, date set; I/O register not emulated) |
| `jiotime`, `jiotime_tr`, `jiotime_none` | MSX-DOS 2 | VG-8235, FS-A1ST | `clients/JIO_TIME/JIOTIME.COM` (`COMMAND_DATE_TIME`, `MOCK_DATE`): date and time of the MSX set and read back, Z80 mode on turbo R, "No answer" without answer of the server. The RTC of openMSX 20.0 changes some months (July read back as May, also when written directly to the chip): October is used |
| `nfs_cart` | JIO.COM | NMS 8255 + MSX-DOS 2 cartridge | as `nfs_nms8255` with the JIO cartridge option (`JIO C30 +D`), `JIO S` shows port 30H (I/O register not emulated) |
| `nfs_nms8255`, `nfs_turbor` | JIO.COM (`clients/JIO_NFS`) | NMS 8255 + MSX-DOS 2 cartridge, FS-A1ST (internal MSX-DOS 2) | no JIO ROM: `JIO +D` from the floppy A:, then (after the warm restart of JIO.COM) `NFSTEST.BAT` typed at the prompt (`tcl/typecmd.tcl`): DIR, TYPE, COPY to D: (byte compare, date set with `_HFTIME`), MD/CD, REN, MOVE, ATTRIB, FCB open / block read / close (`fcbread/`), parameters and buffers in page 2 (`p2test/`: path, FIB, `_READ` buffer, FCB and DTA at 9000H and above, hidden by the driver which is mapped in page 2). The bridge intercepts the serial routines of the resident driver (or stub) when JIO.COM installs its hook. Not tested: redirection to D: (`_DUP` not supported by JIO.COM) |

JIO-ROM.COM scenarios (`jiorom_*`): MSX-DOS 1 boots from the floppy (`MSXDOS.SYS` and `COMMAND.COM` of
`DOS1_FILES`, default `/mnt/DataLinux/Projects/MSX/sdcard/MSXDOS1`), its `AUTOEXEC.BAT` runs `JIO-ROM` (`clients/JIO_ROM`,
ROM built with `RAMROM`), which starts MSX-DOS 2 with the ROM in RAM (`JIOROM_BOOT` of `run_scenario`). The timed
actions of the scenario scripts are delayed by `TIME_SHIFT` seconds (`tcl/shift.tcl`) for this boot.

| Scenario | Machine | Tests |
|---|---|---|
| `jiorom_vg8235`, `jiorom_nms8255` | VG-8235 (360 KB), NMS 8255 (720 KB) | as `dos2_vg8235` (JIO drive A: + floppy B:), 112 KB of RAM for MSX-DOS 2 (top segment: ROM), `fdtest/`: segment written to port FDH by a program kept across the interrupts (signature search of the ROM) |
| `jiorom_1mb`, `jiorom_1mb_s2` | VG-8235 + 1 MB mapper (slot 1, slot 2) | same, ROM in the mapper of page 3 |
| `jiorom_sri` | VG-8235 + 1 MB mapper | as `sri_vg8235`: SofaRunIt calls the BDOS of MSX-DOS 2 (F37DH) with the BIOS in page 0 |
| `jiorom_basic_jio`, `jiorom_basic_flop` | VG-8235 | as `dos2_basic_jio`, `dos2_basic_flop` (hooks of Disk BASIC) |
| `jiorom_format` | NMS 8255 | as `dos2_format_nms` |
| `jiorom_ramdisk` | VG-8235 | as `dos2_ramdisk`: after the reset, MSX-DOS 1 starts JIO-ROM.COM again |

## Results

`out/<scenario>/` contains the screens (`screen_*.txt`), the mock server log (`server.log`), the
served directory (`drive/`), the floppy image and its files (`floppy.dsk`, `floppy_out/`) and the
openMSX output. `out/` is not versioned.
