# MSX JIO

---


<p align="center">
  <img src="./readme_resources/snapshot.png" alt="Snapshot" width="400"/>
</p>

# Binary Communication Protocol Specification

This document describes the low-level binary protocol used by the server. Clients must adhere to this format to communicate correctly.

---

## Overview

- Communication is **unidirectional**: the client sends commands, and the server responds.
- All messages start with a 3-byte signature.
- Commands can include an optional CRC.
- Responses depend on the command and are typically data blocks or acknowledgments.

---

## Packet Structure

### Signature

Each packet **must begin** with the following 3-byte signature:

```
0x4A 0x49 0x4F   // ASCII: 'J' 'I' 'O'
```

This signature allows the server to synchronize on valid packets.

### Generic Packet Format

| Field     | Size        | Description                                  |
|-----------|-------------|----------------------------------------------|
| Signature | 3 bytes     | Always 'JIO' (0x4A 0x49 0x4F)                |
| Flags     | 1 byte      | Bit 0: enable CRC checking (0x01)           |
| Command   | 1 byte      | Command identifier                           |
| Payload   | variable    | Command-specific data                        |
| CRC       | 2 bytes     | Present **only** if CRC flag is set          |

CRC-16 is computed from the **Flags byte** up to the last byte of the payload.

---

## Command List

### `0x01` — INFO

- **Payload**: none
- **Description**: Request server metadata.
- **Response**: Variable-length text or binary blob with info.

---

### `0x02` — READ

- **Payload (7 bytes)**:
  ```
  [0–1] Address (ignored by server)
  [2–5] Sector number (uint32)
  [6]   Sector count (number of 512-byte sectors to read)
  ```
- **Response**: `count * 512` bytes of raw data

---

### `0x03` — WRITE

- **Payload**:
  ```
  [0–6]   Same header as READ
  [7–...] Raw data to write (count * 512 bytes)
  ```
- **Response (if CRC is enabled)**:
  - `0x22 0x22`: Success
  - `0x11 0x11`: CRC mismatch

---

### `0x10` — REPORT_BAD_RX_CRC

Client reports it received a packet with incorrect CRC.

- **Payload**: none
- **Response**: none

---

### `0x11` — REPORT_BAD_TX_CRC

Client reports the packet it sent was not acknowledged due to CRC failure.

- **Payload**: none
- **Response**: none

---

### `0x12` — REPORT_BAD_ACKNOWLEDGE

Client signals that the acknowledgment from a WRITE was incorrect.

- **Payload**: none
- **Response**: none

---

### `0x13` — REPORT_TIMEOUT

Client signals a timeout occurred while waiting for server response.

- **Payload**: none
- **Response**: none

---

## CRC Details

- CRC-16 used over all bytes starting **after the signature** up to the end of the payload.
- Not transmitted unless **CRC flag** is set.
- Polynomial used: _(to be filled in if known)_  
  (e.g., CRC-16-CCITT, CRC-16-IBM, etc.)

---

## Examples

### INFO Command (CRC enabled)

**Send:**
```
4A 49 4F 01 01 A1 B2
```

**Receive:**
```
56 65 72 73 69 6F 6E 20 31 2E 30   // "Version 1.0"
```

---

### READ Command (read 2 sectors at 0x00000200)

**Send:**
```
4A 49 4F
01          // CRC flag
02          // Command READ
00 10       // Address (ignored)
00 00 02 00 // Sector number
02          // Sector count
D3 7C       // CRC (example)
```

**Receive:**
```
<1024 bytes of raw data>
```

---

### WRITE Command (write 1 sector at 0x00000300)

**Send:**
```
4A 49 4F
01          // CRC flag
03          // Command WRITE
00 20       // Address
00 00 03 00 // Sector
01          // Sector count
<data: 512 bytes>
C5 E1       // CRC
```

**Receive (ACK):**
```
22 22       // if OK
```

---

## Notes

- Always wait for the full response after sending a command.
- In case of CRC failure, use the relevant REPORT_* command to notify the server.
- For robustness, the client may re-sync using the signature if desynchronized.

---
