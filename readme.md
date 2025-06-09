# MSXJIO: Serve a Hard-Disk Image to Your MSX

<p align="center">
  <img src="./readme_resources/snapshot.png" alt="Snapshot" width="300"/>
</p>

[![Watch the video]](https://www.youtube.com/watch?v=n8Oy6to-5Io)


## Overview

**MSXJIO** is a project that allows serving a hard or floppy disk-image from a host computer or smartphone to your MSX.

**JIO** stands for **J**oystick **I**nput **O**utput, as communication is done through the MSX joystick port 2.

The system is divided into two parts:

- **JIOServer**: the server application, which runs on **Linux, Windows, macOS, or Android**.
- **MSX Client**: currently, the only available client is `JIO-MSXDOS2`. Additional clients could be developed in the future for features like syncing the MSX real-time clock or mirroring directories with the host.

JIOServer can communicate with the MSX through:

- **USB serial port**
- **Bluetooth adapter**

## Requirements

- On the MSX side, at least **128KB RAM**
- A **JIO cable** to connect the MSX joystick port to a USB or Bluetooth serial adapter

---

## Features

*To be listed.*

---

## Usage Instructions

1. Create an MSX-DOS 2 cartridge (or flash `JIO-MSXDOS2` to a MegaFlashROM or Carnivore2).
2. Connect your MSX to your PC using a USB serial cable or Bluetooth adapter.
3. Launch **JIOServer** and select the disk image to serve.
4. Click the **Connect** button.
5. Boot your MSX.
6. You should see `"init"` appear in the server log and the LED blink.
7. The MSX should now access the image.

---

## Releases

- **Android**: [APK Download]  
  → [Instructions for installing APKs manually](#)

- **Windows**: [ZIP Package]  
  → Extract and run `JIOServer.exe`

- **macOS**: [DMG Package]

- **Linux**: [Static Binary]  
  → Compatible with most distributions.  
     Tested on: *to be listed*

---

## How to Create the 32KB JIO-MSXDOS2 Cartridge

*Instructions to be added.*

---

## Building the Adapter Cable

- Currently using a 3.3V adapter (works, but may be unstable on MSX1).
- A switchable 3.3V/5V adapter has been ordered for testing.
- Looking for a 5V model with a crystal for better signal stability.

⚠️ **Do NOT use standard RS-232 adapters**—they may output +12V/-12V, which can **damage your MSX**.

### Wiring:

- MSX Joystick Port 2, Pin 1 → Adapter **TX**  
- MSX Joystick Port 2, Pin 6 → Adapter **RX**  
- MSX Joystick Port 2, Pin 9 → Adapter **GND**  
- MSX Joystick Port 2, Pin 5 → Adapter **VCC**

---

## Bluetooth Configuration for MSXJIO

1. Default password: `1234`
2. Plug the **HC-05** into the **Bluetooth** socket.
3. Plug the **FTDI FT232RL** into the **USB Cable** socket.
4. Hold the **AT** button on the HC-05.
5. While holding, plug the USB into your PC.
6. The HC-05 LED should blink **slowly** (~2s).
7. Launch the **DSD TECH Wireless Tools** utility.
8. Set UART port to e.g. `COM9`, Baud Rate to `38400`, and click **Open`.
9. You should now be able to send AT commands.
10. Configure for this project by sending:  
    `AT+UART=115200,1,0`

---

## Known Issues


## Bug report
You can submit tickets on GitHub, or post messages on this thread:



## Links

https://github.com/b3rendsh/msxdos2s


---

## History

This project began with **NYYRIKKI**’s brilliant 115200 bps MSX serial communication routine:  
👉 https://www.msx.org/forum/msx-talk/development/software-rs-232-115200bps-on-msx

Shortly after, he released a working **MSX-DOS 1** version serving disk images with **drive sound emulation**:  
👉 https://www.youtube.com/watch?v=OHs5a-gZtuc

At the same time, I was aware of **b3rendsh**’s MSX-DOS 2 project:  
👉 https://github.com/b3rendsh/msxdos2s  
It was a natural fit to integrate NYYRIKKI’s fast communication routines to support full disk image serving.

NYYRIKKI’s original server code was functional but command-line only. I contacted b3rendsh, and he kindly joined the project. After several months of collaborative coding and debugging, we finally reached a stable and usable version.

---

## Credits

