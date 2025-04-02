#!/usr/bin/python3

# Command to install dependencies (should be the same on Windows, macOS and Linux):
# pip3 install pyserial colorama

# For "managed" Python environments (Ubuntu 24.04 for example):
# sudo apt install python3-serial python3-colorama

import random
import time
import subprocess
import os
import argparse
import signal
import colorama
import serial.tools.list_ports
import serial

#######################################################################################################################

control_c_pressed    = False
com_port             = "auto"
timeout              = 63
readChecksum         = False
writeChecksum        = False

test_data_corruption = False

RESET     = "\033[0m"
GREEN_BG  = "\033[42m"
YELLOW_BG = "\033[43m"
RED_BG    = "\033[41m\a"

#######################################################################################################################

def get_loop_device(image_file):
    image_file = os.path.abspath(image_file)
    
    try:
        result = subprocess.run(["losetup", "--list"], capture_output=True, text=True)
    except:
        return None

    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) > 1 and os.path.abspath(fields[-3]) == image_file:
            return fields[0]
    return None

#######################################################################################################################

def flush_loop_partitions(image_file):
    loop_device = get_loop_device(image_file)
    if not loop_device:
        return

    base_name = os.path.basename(loop_device)  # Extract loop device name (e.g., loop15)
    
    # Find all partitions dynamically
    partitions = [
        f"/dev/{entry}" for entry in os.listdir("/sys/class/block/")
        if entry.startswith(base_name) and entry != base_name
    ]

    # Avoid sudo problems... to be understood...
    subprocess.run(["sudo", "bash", "-c", "exit"])

    # Flush the main loop device
    subprocess.run(["sudo", "-n", "blockdev", "--flushbufs", loop_device])

    # Flush all detected partitions
    for partition in partitions:
        subprocess.run(["sudo", "-n", "blockdev", "--flushbufs", partition])

#######################################################################################################################

def format_size(size_bytes):
    units = ["B", "KiB", "MiB", "GiB", "TiB"]
    for unit in units:
        if size_bytes < 1024:
            return f"{size_bytes:.1f}{unit}"
        size_bytes /= 1024
    return f"{size_bytes:.1f}PiB"

#######################################################################################################################

def get_server_info(image_file):
    global readChecksum, writeChecksum, timeout

    try:
        file_path = os.path.abspath(image_file)
        file_size = os.path.getsize(image_file)
        mod_time = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(os.path.getmtime(image_file)))
        
        Checksum_info = f"{'R' if readChecksum else ''}{'W' if writeChecksum else ''}" or "-"

        size_text = format_size(file_size)
        info_text = f"File  : {file_path}\r\nSize  : {size_text}\r\nDate  : {mod_time}\r\nChecks: {Checksum_info}\r\nTimeout: {timeout}\r\n"

    except Exception as e:
        print(f"Error generating server info: {e}")
        info_text = "** Error retrieving server info\r\n"

    info_data = info_text.encode('ascii')[:510]
    info_data = bytes([(readChecksum << 0) | (writeChecksum << 1) | (timeout << 2)]) + info_data.ljust(511, b'\0')
    return info_data

#######################################################################################################################

def calculate_Checksum(data):
    Checksum = 0
    for byte in data:
        Checksum += byte
        if Checksum > 0xFF:
            Checksum = (Checksum & 0xFF) + 1
    return Checksum & 0xFF

#######################################################################################################################

def respond(ser, message):
    Checksum = calculate_Checksum(message)

    if test_data_corruption and random.randint(0, 3) == 0:
        Checksum ^= 0xFF

    full_message = bytes([0xFF] * 3) + bytes([0xF0]) + message

    full_message += bytes([Checksum])
    ser.write(full_message)

#######################################################################################################################

def find_JIO_serial_port(timeout=1):
    available_ports = [port.device for port in serial.tools.list_ports.comports()]
    
    if not available_ports:
        print("No serial ports found.")
        exit(1)

    for port in available_ports:
        try:
            with serial.Serial(port, timeout=timeout, baudrate=115200, bytesize=serial.EIGHTBITS, 
                               parity=serial.PARITY_NONE, stopbits=serial.STOPBITS_ONE) as ser:
                data = ser.read(1)
                
                if data:
                    ser.reset_input_buffer()
                    ser.reset_output_buffer()
                    ser.close()
                    return port
        except (serial.SerialException, OSError):
            pass
    
    return None

#######################################################################################################################

def timeout_range(value):
    ivalue = int(value)
    if ivalue < 0 or ivalue > 63:
        raise argparse.ArgumentTypeError("Timeout must be between 0 and 63")
    return ivalue

#######################################################################################################################

def signal_handler(sig, frame):
    global control_c_pressed
    control_c_pressed = True
    signal.signal(signal.SIGINT, signal.SIG_DFL)
    print("Press Ctrl+C again to quit...")

#######################################################################################################################

def main():
    global readChecksum, writeChecksum, timeout, control_c_pressed, com_port

    colorama.init(autoreset=True)

    parser = argparse.ArgumentParser(description="Serial communication with image file handling.")
    parser.add_argument("image_file", help="Path to the image file")
    parser.add_argument("--com_port",       default=com_port,      help="Serial port device (e.g., /dev/ttyUSB0) or auto for automatic detection")
    parser.add_argument("--read-checksum",  default=readChecksum,  help="Enable checksum verification for reads", action="store_true")
    parser.add_argument("--write-checksum", default=writeChecksum, help="Enable checksum verification for writes", action="store_true")
    parser.add_argument("--timeout",        default=timeout,       help="Timeout (0-63)", type=timeout_range)
    args = parser.parse_args()

    image_file    = args.image_file

    com_port      = args.com_port
    readChecksum  = args.read_checksum
    writeChecksum = args.write_checksum
    timeout       = args.timeout

    if test_data_corruption:
        readChecksum = True
        writeChecksum = True

    if (com_port == "auto"):
        print("Checking for JIO COM ports...",end='')
        com_port = None
        while com_port == None:
            com_port = find_JIO_serial_port()
            print(".",end='')
        print("")
        print(f"JIO found on port {com_port}")

    try:
        with serial.Serial(
            port=com_port,
            baudrate=115200,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE
        ) as ser, open(image_file, "r+b") as f:

            ser.reset_input_buffer()
            ser.reset_output_buffer()
 
            signal.signal(signal.SIGINT, signal_handler)

            while not control_c_pressed:
                try:
                    cmd = ser.read(1)
                except IndexError:
                    print(f"{RED_BG}Error: Received empty command.{RESET}")
                    continue
                
                if cmd in [b'R', b'W', b'w']:
                    try:
                        sector_0, sector_1, sector_2 = ser.read(3)
                        sector_len = ser.read(1)[0]
                        address_0, address_1 = ser.read(2)
                    except IndexError:
                        print(f"{RED_BG}Error: Incomplete sector data received.{RESET}")
                        continue
                    
                    address = address_0 + (address_1 << 8)
                    sector = sector_0 + (sector_1 << 8) + (sector_2 << 16)
                    offset = sector * 512
                    total_len = sector_len * 512

                    f.seek(offset)

                    if cmd == b'R':
                        if sector == 0xFFFFFF and address == 0xC000:
                            print("Sending server information:")
                            info_data = get_server_info(image_file)
                            print(info_data[1:].decode('ascii').rstrip('\0'))
                            respond(ser, info_data)
                        else:
                            print(f"{GREEN_BG}Read {sector_len:>2} sector(s) at {sector:>10,} to 0x{address:X}{RESET}")
                            data = f.read(total_len)
                            respond(ser, data)

                    elif cmd in [b'W', b'w']:
                        data = ser.read(total_len)
                        calculated_Checksum = calculate_Checksum(data)

                        if test_data_corruption and random.randint(0, 3) == 0:
                            calculated_Checksum ^= 0xFF

                        print(f"{YELLOW_BG}Write {sector_len:>2} sector(s) at {sector:>10,} from 0x{address:X}{RESET}", end='')

                        Checksum_ok = True

                        if cmd == b'w':
                            received_Checksum = ser.read(1)[0]
                            Checksum_ok = (calculated_Checksum == received_Checksum)
                            respond(ser, bytes([0x00 if Checksum_ok else 0xFF]))

                            if not Checksum_ok:
                                print(f" {RED_BG}** Write Checksum error!{RESET}")
                            else:
                                print(" Checksum OK")
                        else:
                            print()

                        if Checksum_ok:
                            f.seek(offset)
                            f.write(data)
                            f.flush()
                            flush_loop_partitions(image_file)
                elif cmd == b'E':
                    print(f"{RED_BG}** Read Checksum error!{RESET}")
                else:
                    print(f"{RED_BG}** Unknown command!{RESET}")
                    ser.reset_input_buffer() 
                    ser.reset_output_buffer() 

    except serial.SerialException as e:
        print(f"Serial port error: {e}")

    except IOError as e:
        print(f"File error: {e}")

    ser.close()
    f.close()

#######################################################################################################################

if __name__ == "__main__":
    main()
