#!/usr/bin/env bash
set -euo pipefail

# Serial lines of a JIO ROM (JioPorts of drv_jio.asm, INCLUDE "jio_ports.inc"), in the order they are tried at boot:
#   J1, J2    joystick port 1 or 2 (not probed, last of the list)
#   00..FD    I/O port of a JIO cartridge (hex), probed (detection routine of herraa1)
# Joystick port 2 is added if the list does not end with a joystick port.
# Usage: jio_ports.sh <line>... > jio_ports.inc         e.g. jio_ports.sh 00 20 30 J2

BYTES=()
LAST=""
for LINE in "$@"; do
    if [ "$LAST" = 0FEh ] || [ "$LAST" = 0FFh ]; then
        echo "jio_ports.sh: '$LINE' after a joystick port (never tried)" >&2
        exit 2
    fi
    case "${LINE^^}" in
        J1) LAST=0FEh ;;
        J2) LAST=0FFh ;;
        *)
            if [[ ! "$LINE" =~ ^[0-9A-Fa-f]{1,2}$ ]] || [ $((16#$LINE)) -ge 254 ]; then
                echo "jio_ports.sh: invalid serial line '$LINE' (J1, J2 or I/O port in hex, 00 to FD)" >&2
                exit 2
            fi
            LAST=$(printf '0%02Xh' $((16#$LINE)))
            ;;
    esac
    BYTES+=("$LAST")
done
[ "$LAST" = 0FEh ] || [ "$LAST" = 0FFh ] || BYTES+=(0FFh)
(IFS=,; echo "db ${BYTES[*]}")
