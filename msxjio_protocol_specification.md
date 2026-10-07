# MSXJIO protocol specification

This document describes the protocol used by the JIO server.  
Clients must adhere to this protocol to communicate correctly.  
No rights can be derived from this publication.  

**Protocol version:** 1.0 draft

## Overview

- Communication is unidirectional: the client sends commands, and the server responds.
- All messages start with a 3-byte signature.
- Commands can include an optional CRC.
- Responses depend on the command and are typically data blocks or acknowledgments.
- Data is transferred over a serial interface.
- The server serves either a disk image (commands `0x10` to `0x13`) or host directories as drives A: to H: (command `0x16`), depending on the mode selected on the server. The commands of the other mode get no data: in directories mode, COMMAND DRIVE INFO reports 0 partitions and the sectors cannot be read or written; in disk image mode, no drive is served by COMMAND BDOS (_LOGIN answers `0x00`).

## Packet structure

**Command packet:**

| Field     | Size        | Description                          |
|:----------|:------------|:-------------------------------------|
| Signature | 3 bytes     | Always 'JIO' (`0x4A 0x49 0x4F`)      |
| Flags     | 1 byte      | Bit 0: enable CRC checking (`0x01`)  |
| Command   | 1 byte      | Command identifier                   |
| Payload   | variable    | Command-specific data                |
| CRC       | 2 bytes     | Present only if CRC flag is set      |

**Response packet:**

| Field     | Size        | Description                       |
|:----------|:------------|:-----------------------------------
| Sync      | 2 bytes     | Synchronization bytes `0xFF 0xF0` |
| Response  | variable    | Server response data              |
| CRC       | 2 bytes     | Present only if CRC flag is set   |

**CRC details:**

- CRC-16 used over all bytes starting after the signature up to the end of the payload.
- Not transmitted unless the CRC flag is set.
- Polynomial used: CRC-16-CCITT Xmodem

## Command list

#### 0x10 — COMMAND DRIVE READ

**Description:** Read block(s) of 512 bytes raw data

**Payload:** 
| Field     | Size        | Description                              |
|:----------|:------------|:-----------------------------------------|
| Partition | 1 byte      | Partition number                         |
| Sector    | 3 bytes     | Starting sector number                   |
| Count     | 1 byte      | Number of 512-bytes sectors to read      |
| Address   | 2 bytes     | Client destination memory address info   |

**Response:**  
Count * 512 bytes of raw data  
  
  
#### 0x11 — COMMAND DRIVE WRITE

**Description:** Write block(s) of 512 bytes raw data

**Payload:**
| Field     | Size        | Description                            |
|:----------|:------------|:---------------------------------------|
| Partition | 1 byte      | Partition number                       |
| Sector    | 3 bytes     | Starting sector number                 |
| Count     | 1 byte      | Number of 512-bytes sectors to write   |
| Address   | 2 bytes     | Client source memory address info      |
| Data      | Count * 512 | Raw data to write                      |

**Response:**  
`0x11 0x11`: CRC mismatch (if TX CRC is enabled)  
`0x22 0x22`: Success  
`0x33 0x33`: Disk is write protected
  
  
#### 0x12 — COMMAND DRIVE INFO

**Description:** Request server metadata. Sent by the JIO ROMs at boot, and by JIO.COM at install (flags 0: no CRC,
512-byte answer), in both serve modes.  
**Payload:** none  
**Response:** 512 bytes (+ CRC if requested)
| Field     | Size        | Description                                            |
|:----------|:------------|:-------------------------------------------------------|
| Flags     | 1 byte      | Error detection flags                                  |
| Drives    | 1 byte      | Number of partitions (drives) on disk                  |
| Bootdrv   | 1 byte      | The partition number that is marked active (default 0) |
| Info      | variable    | Disk and flag information string (ends with 0x00)      |

**Error detection flags**  
| Bit | Flag           |
|----:|:---------------|
|   0 | RX CRC         |
|   1 | TX CRC         |
|   2 | TIMEOUT        |
|   3 | RETRY          |
|   4 | SLOW TX        |
|   5 | TX BLOCKS      |
|   6 | RESERVED       |
|   7 | RESERVED       |

RETRY ("Auto retry" of the server) only applies to the disk image (`COMMAND_DRIVE_*`): for the served directories, no
request is sent again for now (see `COMMAND_BDOS`). JIO.COM returns a "Not ready" error when the server does not answer
within about 5 s.

TX BLOCKS is set by the server on a Bluetooth link: JIO.COM then sends its large writes (`_WRITE`) as several requests
of 8192 bytes at most, each one answered before the next one is sent (a long continuous transmission can be lost by
the Bluetooth serial module, there is no flow control on the MSX side).
  
  
#### 0x13 — COMMAND DRIVE DISK CHANGED

**Description:** Report if the disk image was changed since the command drive info or since the previous command drive disk changed. Applies to whole disk file not a partition or data content within the disk image.  
**Payload:** none  
**Response:**  
`0x44 0x44` : Changed  
`0x55 0x55` : Not changed
  
  
#### 0x16 — COMMAND BDOS

**Description:** Remote file system. MSX-DOS 2 file functions are executed by the server on the directories it serves as drives A: to H: (JIO MSX-DOS 2 ROM, no FAT or sectors on the MSX side).  
CRC is not used and no CRC follows the payload. The flags byte is the sequence number of the request (`0x00`: not
numbered), incremented by the client for each new request (skipping `0x00`). The server keeps the last 16 numbered
requests with their answers: a request sent again with a kept number and the same bytes is not executed again, its
answers are sent again (a write is not done twice). The requests are forgotten at the reset (function `0x1D`, sent by
the clients at boot / install). The clients (MSX-DOS 2 ROM, JIO.COM) do not send requests again for now: they wait for
the answers.  
The server answers with zero, one or more response packets (sync bytes `0xFF ... 0xF0` followed by the data). Empty packets are not sent.

**Payload:**
| Field     | Size        | Description                              |
|:----------|:------------|:-----------------------------------------|
| Function  | 1 byte      | MSX-DOS function number (see below)      |
| Data      | variable    | Function parameters                      |

**Parameter types:**
- *byte*, *word* (2 bytes), *dword* (4 bytes): little endian
- *string*: ASCIIZ string, including the terminating `0x00`
- *path*: either a *string* (MSX-DOS path, may start with a drive), or a *FIB* (first byte `0xFF`)
- *FIB*: File Info Block, 50 bytes:

| Offset | Size | Description                                                    |
|-------:|-----:|:---------------------------------------------------------------|
|      0 |    1 | Always `0xFF`                                                  |
|      1 |   13 | File name (ASCIIZ)                                             |
|     14 |    1 | Attributes                                                     |
|     15 |    2 | Time of last modification                                      |
|     17 |    2 | Date of last modification                                      |
|     19 |    2 | Start cluster (always 0)                                       |
|     21 |    4 | File size                                                      |
|     25 |    1 | Drive (1 = A:)                                                 |
|     26 |    6 | Reserved for the client (bit 7 of byte 30 set = device), byte 26 = search attributes |
|     32 |    4 | Server find entry id                                           |
|     36 |   13 | Search mask                                                    |
|     49 |    1 | Error code (MSX-DOS 2 error, `0x00` = no error)                |

**Functions:**
| Function | Name      | Parameters                                                   | Response packets                                   |
|:---------|:----------|:-------------------------------------------------------------|:---------------------------------------------------|
| `0x0E`   | _SELDSK   | drive (byte, 0 = A:)                                         | number of drives (byte)                            |
| `0x18`   | _LOGIN    | none                                                         | drives served (byte, bit 0 = A:)                   |
| `0x1B`   | _ALLOC    | drive (byte, 0 = current, 1 = A:)                            | sectors per cluster (byte, 0 = invalid drive), total clusters (word), free clusters (word) |
| `0x1D`   | RESET     | none                                                         | none (all files closed, current directories reset) |
| `0x40`   | _FFIRST   | path, [file name (string) if path is a FIB], attributes (byte) | FIB                                              |
| `0x41`   | _FNEXT    | FIB                                                          | FIB                                                |
| `0x42`   | _FNEW     | path, [file name (string) if path is a FIB], attributes (byte), template file name (13 bytes) | FIB               |
| `0x43`   | _OPEN     | path, open mode (byte)                                       | error (byte), file handle (byte)                   |
| `0x44`   | _CREATE   | path, open mode (byte), attributes (byte)                    | error (byte), file handle (byte, `0xFF` for a sub-directory) |
| `0x45`   | _CLOSE    | file handle (byte)                                           | error (byte)                                       |
| `0x48`   | _READ     | file handle (byte), size (word)                              | error (byte), size read (word) ; data (if size read > 0) |
| `0x49`   | _WRITE    | file handle (byte), size (word), data                        | error (byte), size written (word)                  |
| `0x4A`   | _SEEK     | file handle (byte), method (byte), offset (dword)            | error (byte), new file pointer (dword)             |
| `0x4D`   | _DELETE   | path                                                         | error (byte)                                       |
| `0x4E`   | _RENAME   | path, new name (string)                                      | error (byte)                                       |
| `0x4F`   | _MOVE     | path, new path (string)                                      | error (byte)                                       |
| `0x50`   | _ATTR     | path, set (byte, 0 = get), attributes (byte)                 | error (byte), attributes (byte)                    |
| `0x51`   | _FTIME    | path, set (byte, 0 = get), time (word), date (word)          | error (byte), time (word), date (word)             |
| `0x52`   | _HDELETE  | file handle (byte)                                           | error (byte)                                       |
| `0x53`   | _HRENAME  | file handle (byte), new name (string)                        | error (byte)                                       |
| `0x54`   | _HMOVE    | file handle (byte), new path (string)                        | error (byte)                                       |
| `0x55`   | _HATTR    | file handle (byte), set (byte, 0 = get), attributes (byte)   | error (byte), attributes (byte)                    |
| `0x56`   | _HFTIME   | file handle (byte), set (byte, 0 = get), time (word), date (word) | error (byte), time (word), date (word)        |
| `0x59`   | _GETCD    | drive (byte, 0 = current, 1 = A:)                            | size (byte, including the `0x00`) ; path (string)  |
| `0x5A`   | _CHDIR    | path                                                         | error (byte)                                       |
| `0x5E`   | _WPATH    | none                                                         | error (byte), offset of last item (byte), size (byte, including the `0x00`) ; whole path of last entry found (string) |
| `0x68`   | _RAMD     | size (byte: `0x00` = destroy, `0x01`-`0xFE` = create with this number of 16 KB segments, `0xFF` = get size) | error (byte), RAM disk size (byte, segments, 0 = no RAM disk), drives served (byte, bit 0 = A:) |
| `0xE0`   | JIO_GET_LONG_NAME | sub-function (byte), size of the buffer of the program (word); sub-function 1: FIB; sub-function 3: drive (byte, 0 = current, 1 = A:) | error (byte), size (word, including the `0x00`, 0 if error) ; long name (string) |

Notes:
- File handles are allocated by the server (`0x80` to `0xFF`). The file pointer is kept by the server.
- Paths without a drive use the drive selected by _SELDSK. Relative paths use the current directory of the drive (_CHDIR).
- Device names (CON, AUX, PRN, LST, NUL) and the FCB functions (MSX-DOS 1) are handled by the client, using the functions above.
- A _READ or _WRITE never crosses a 16 KB page boundary of the client memory.
- When the server is read only, the functions that would modify a served directory (_FNEW, _CREATE, _WRITE, _DELETE, _RENAME, _MOVE, _ATTR and _FTIME with set, and the handle versions) answer `0xF8` (.WPROT, write protected disk). Files are opened read only on the host. The RAM disk H: stays writable.

##### JIO_GET_LONG_NAME (BDOS function `0xE0`, JIO extension)

MSX-DOS only knows 8.3 names: the server shows long host names as aliases (`LongFileName.text` → `LONGFI~1.TEX`).
This function gives the long host names to the MSX programs that want to show them. It is not a function of MSX-DOS 2
or Nextor: the JIO MSX-DOS 2 ROM and JIO.COM handle it. Elsewhere it returns `0xDC` (.IBDOS): the program then uses
the 8.3 names. Long names are only returned: to open a file, use its 8.3 name (a long name can be given too, within
the 63 characters of an MSX-DOS path).

Call (`CALL 0005H` from an MSX-DOS program):

| Register | Input |
|---|---|
| C  | `0xE0` |
| A  | sub-function: 1 = name of the entry of a FIB (from _FFIRST, _FNEXT, _FNEW), 2 = whole path of the last entry found (as _WPATH), 3 = current directory of a drive (as _GETCD) |
| DE | sub-function 1: FIB. Sub-function 3: E = drive (0 = current, 1 = A:) |
| HL | buffer (ASCIIZ string returned) |
| B  | size of the buffer (1 to 255, 0 = 256) |

Output: A = error. `0x00` OK, `0xDB` (.IDRV) not a drive of the server (e.g. the floppy drive), `0xD8` (.PLONG) buffer
too small, `0xDC` (.IBDOS) invalid sub-function (or not handled: no JIO).

- The FIB and the buffer can be anywhere in the TPA (MSXDOS2.SYS does not copy the parameters of this function: the
  JIO ROM and JIO.COM access the memory of the program themselves).
- The names are in the MSX character set: ASCII, accented letters of the MSX international character set (codes
  `0x80`-`0xA8`), `?` for the other characters.
- Paths have no drive and no leading `\`, as _GETCD.
- The registers differ from _FNEXT (FIB in IX) because the JIO ROM reaches this function through an interslot call
  (CALLF), which changes IX and IY.
  
  
#### 0x17 — COMMAND DATE TIME

**Description:** Request the current date and time of the server (local time), used by the JIOTIME.COM client (`clients/JIO_TIME`) to set the MSX clock. Served in both modes (disk image or directories).  
**Payload:** none  
**Response:**
| Field     | Size        | Description                  |
|:----------|:------------|:-----------------------------|
| Year      | 2 bytes     | Year (e.g. 2026)             |
| Month     | 1 byte      | Month (1-12)                 |
| Day       | 1 byte      | Day of month (1-31)          |
| Hour      | 1 byte      | Hour (0-23)                  |
| Minute    | 1 byte      | Minute (0-59)                |
| Second    | 1 byte      | Second (0-59)                |
  
  
#### 0x18 — COMMAND LOG

**Description:** Text sent by the MSX, shown in the log of the server (debugging of the clients). Served in both modes.  
**Payload:**
| Field     | Size        | Description                  |
|:----------|:------------|:-----------------------------|
| Text      | variable    | ASCIIZ string (max 64 characters shown) |

**Response:** none
  
  
#### 0xNN — COMMAND DRIVE REPORT [NN]

**Description:** Report a disk i/o result to the server  
**Payload:** none  
**Response:** none

[NN]  
`0x00` : read/write ok (not used)  
`0x01` : drive write protected  
`0x03` : drive not ready / time-out  
`0x05` : CRC error (if RX CRC is enabled)  
`0x0B` : write fault

## Data transfer

### General setup

- The client and server are linked via a serial interface.
- The default transfer protocol is 115200 baud, no parity, 1 stop bit.
- The client always initiates a data transfer.
- The server is able to respond on each command immediately.
- Error detection and handling flags are set on the server only.
- Error handling logic is implemented on the client only.

### Command/response sequence

Example implementation of transmit command and receive response with error handling.

| Step    | Client | Server | Description                                                    |
|--------:|:------ |:-------|:---------------------------------------------------------------|
|       1 | X      |        | Transmit command                                               |
|       2 |        | X      | Receive command                                                |
|       3 |        | X      | If TX CRC flag set then validate CRC else goto 5               |
|       4 |        | X      | If CRC error then transmit response 'CRC mismatch': goto 6     |
|       5 |        | X      | Process command and transmit response                          |
|       6 | X      |        | Receive response until completed or timeout (if TIMEOUT flag)  |
|       7 | X      |        | If receive response timeout then goto 12                       |
|       8 | X      |        | If RX CRC flag set then validate CRC else goto 10              |
|       9 | X      |        | If CRC error then transmit report 'CRC error': goto 11         |
|      10 | X      |        | Process response and if error then transmit report             |
|      11 |        | X      | Optional: receive command report and display it in the log     |
|      12 | X      |        | If error and AUTORETRY flag set then goto 1                    |
|      13 | X      |        | Return to calling function with response result                |
