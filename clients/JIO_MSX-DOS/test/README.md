# JIO MSX-DOS 2 emulator tests

Automatic tests of the JIO MSX-DOS 2 ROMs in openMSX, without an MSX and without a JIO server.

```
./0_RunTests.sh              # both ROMs
./0_RunTests.sh jio          # JIO only ROM (p0_kernel.asm, as built by 0_Make_DOS2.sh)
./0_RunTests.sh hybrid       # hybrid ROM (p0_hybrid.asm, as built by 0_Make_DOS2_Hybrid.sh)
./0_RunTests.sh all hyb_vg8235 jio_basic   # only some scenarios
```

Every scenario prints `PASS` or `FAIL` (with the failed checks). The script exits with 0 when all
scenarios passed. A full run takes about 6 minutes.

Needs `openmsx` (with the system ROMs of the machines used), `z88dk` and `python3`.
`MSXDOS2.SYS` and `COMMAND2.COM` are taken from `../../JIO_NFS/MSX-DOS2/`.

## How it works

- The ROMs are assembled into `out/` (without the IAR compiler step: `drv_jio_c.asm` is used as is).
- `tcl/bridge.tcl.in` is turned into `bridge.tcl` with the addresses of the ROM map file. In openMSX it
  intercepts the two serial routines of the kernel (`RFS_TX`, `J_RX1`) and forwards the bytes to
  `mockserver.py` over TCP. The "waiting for server" handshake of the ROM is skipped; `JIO_DRIVES`
  sets the number of JIO drives it reports (hybrid ROM, 0 simulates [ESC]).
- `mockserver.py` implements the `COMMAND_BDOS` file functions of the protocol
  (see `msxjio_protocol_specification.md`) on a host directory, served as drive A:. It follows the
  behavior of the C++ server, it is not the C++ server: server changes must still be tested with the
  real server. `_RAMD` creates the RAM disk H: in a temporary directory, like the C++ server.
- `AUTOEXEC.BAT` of drive A: runs the DOS commands of the scenario, screens are dumped with
  `get_screen`. Floppy images are created and read back with the openMSX disk manipulator.
- `fcbtest/fcbtest.asm` tests the MSX-DOS 1 FCB functions (open, sequential read and write, block
  read, search, file size, rename, delete). The expected result line is `RENAME/DEL/DEL: 00 00 FF`.

Because the serial routines are replaced, the tests do not check the transmission timings: those are
only tested on real hardware.

## Scenarios

| Scenario | ROM | Machine | Tests |
|---|---|---|---|
| `jio_nms8255`, `jio_vg8235`, `jio_turbor` | JIO only | NMS 8255, VG-8235, FS-A1ST | boot, FIB handling (`fibtest/`: open with the FIB after the last `_FNEXT`, `_FNEW` on an existing directory), COPY of a 23 KB file (byte compare), MD/CD, redirection, FCB functions |
| `jio_takeover` | JIO only | NMS 8255 + MSX-DOS 2 cartridge in slot 1 | same, the JIO ROM takes over |
| `jio_basic` | JIO only | VG-8235 | Disk BASIC: OPEN, PRINT#, LINE INPUT#, SAVE, KILL, LOAD, FILES |
| `hyb_vg8235`, `hyb_nms8255`, `hyb_turbor` | hybrid | VG-8235 (360 KB), NMS 8255, FS-A1ST (720 KB) | JIO drive A: + floppy B:: copies both ways (byte compare), MD/CD on the floppy, DIR of the floppy redirected to the JIO drive, FCB functions on both drives, DEL |
| `hyb_noserver` | hybrid | VG-8235 | no server: the floppy is A:, MSX-DOS 2 boots from it, FCB functions, `RAMDISK` answers "Not enough memory" |
| `hyb_basic_jio`, `hyb_basic_flop` | hybrid | VG-8235 | Disk BASIC on the JIO drive / on the floppy |
| `jio_ramdisk`, `hyb_ramdisk` | JIO only, hybrid | VG-8235 (hybrid: 360 KB) | `RAMDISK 32K` (H: on the server): copies both ways (byte compare), MD, FCB functions on H:, free space, disk full, H: -> floppy (hybrid), `RAMDISK 0 /D`, RAM disk destroyed by a MSX reset (`tcl/reboot.tcl`) |
| `jio_longnames`, `hyb_longnames` | JIO only, hybrid | VG-8235 | long host names: MD/CD with a long name, 8.3 aliases (`BOMBAM~1`, `LONGFI~1.TEX`) in DIR, CD, TYPE, COPY into an aliased directory |
| `jio_renmove`, `hyb_renmove` | JIO only, hybrid | VG-8235 (hybrid: 360 KB) | REN, MOVE into a directory, ATTRIB +R / -R, DEL of a read only file refused, copy back (`_RENAME`, `_MOVE`, `_ATTR`); on the JIO drive A: and, with the hybrid ROM, on the floppy B: |
| `hyb_takeover` | hybrid | NMS 8255 + MSX-DOS 2 cartridge in slot 1 | the hybrid ROM takes over, copy to the floppy |

## Results

`out/<scenario>/` contains the screens (`screen_*.txt`), the mock server log (`server.log`), the
served directory (`drive/`), the floppy image and its files (`floppy.dsk`, `floppy_out/`) and the
openMSX output. `out/` is not versioned.
