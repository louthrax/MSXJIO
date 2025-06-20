# MSXJIO

**MSXJIO** is a project that allows serving a hard or floppy disk-image (and other things to come),
 from a host computer (or smartphone) to your MSX, through high-speed **115200 bauds** communication on joystick port 2.

 All you need is a cheap USB or Bluetooth communication chip, and a 16KB or 32KB ROM (for the MSX-DOS clients).

**JIO** stands for **J**oystick **I**nput **O**utput.

<table  align="center">
  <tr>
    <td align="center">
      <img src="./readme_resources/JIO_Server.png" width="300"/>
    </td>
    <td align="center">
      <img src="./readme_resources/JIO_MSX-DOS_2.jpg" width="500"/>
    </td>
  </tr>
</table>

The system is divided into two parts:

- **JIOServer**: the server application, which runs on **Linux**, **Windows**, **macOS**, or **Android**.

- **MSX Clients**:
  - **JIO MSX-DOS 1**  
  A modified version of MSX-DOS 1 reading sectors from a served hard or floppy disk image

  - **JIO MSX-DOS 2**  
  A modified and compact version of MSX-DOS 2 reading sectors from a served hard or floppy disk image

  - **JSYNC** (not released yet)  
  An MSX-DOS 2 tool to synchronize files an directories between MSX and host (a bit like rsync).

  - **JRTC** (not released yet)  
  An MSX-DOS 2 tool to synchronize MSX RTC time and date with host.

Details about b3rendsh's MSX-DOS clients can be found here: https://github.com/b3rendsh/msxdos2s

There's also a **JSM** (JIO Serial Monitor) helper tool that you can use to configure your Bluetooth communication chip.

## Downloads

All server (Android, Linux, Windows and macOS) and client software can be downloaded [here](https://github.com/louthrax/MSXJIO/releases).

## Supported MSX models

As the signal decoding is done in a software way on MSX, the client MSX Z80 frequency needs to be as
close as possible from the "standard" one (3 579 545 Hz):  

- Models confirmed to be working:  

    | Model           | Frequency     |
    |----------------|----------------| 
    | VG-8235        | 3 554 367 Hz   |
    | HB-G900AP      | 3 578 959 Hz   |
    | VG-8020        | 3 579 200 Hz   |
    | Toshiba HX-10  | 3 579 367 Hz   |
    | Palcom PX-V60  | 3 579 431 Hz   |
    | VG-8010        | 3 579 545 Hz   |
    | HB-F700F       | 3 579 599 Hz   |
    | NMS 8255       | 3 579 617 Hz   |
    | turboR         | 3 579 617 Hz   |

- Models tested as non-working:

    | Model          | Frequency     |
    |----------------|---------------|
    |National CF-3000|3 579 405 Hz   |
    |Casio PV-7      |3 579 431 Hz   |

    Weirdly, the frequency of these models is close from the standard one, but there might be other
(electronical) factors here...

## Hardware

JIOServer can communicate with the MSX through:

- **USB**, using a **USB to TTL UART adapter**

  <p align="center">
    <img src="./readme_resources/USB_to_TTL-UART_Converter.jpg" width="200"/>
  </p>

  Models confirmed to be working are:
  
    - **FTDI USB UART IC FT232RL**

- **Bluetooth**, using a **Bluetooth Serial Transceiver module**  
    <p align="center">
      <img src="./readme_resources/Bluetooth_Serial_Transceiver_Module.jpg" width="200"/>
    </p>  
    Models confirmed to be working are:

  - **HC-05**: Easy configuration for MSXJIO with JSM.BAS tool.
  - **HC-06**: Works but not recommended (requires an extra USB to TTL UART adapter for configuration).

⚠️ **Do NOT use standard RS-232 adapters**—they may output +12V/-12V, which can **damage your MSX**.

For both USB and Bluetooth, prefer the 5V versions, as the standard voltage on the MSX joystick port is 5V. Some models have jumpers to switch between different voltages (5V or 3.3V).

## Building the adapter cable

- MSX Joystick Port 2, Pin **1** → Adapter **TX**  
- MSX Joystick Port 2, Pin **6** → Adapter **RX**  
- MSX Joystick Port 2, Pin **9** → Adapter **GND**  
- MSX Joystick Port 2, Pin **5** → Adapter **VCC**, ⚠️ only required for Bluetooth

<p align="center">
    <img src="./readme_resources/MSX_joystick_port.png" width="500"/>
</p>  

## Usage instructions for the MSX-DOS clients

1. Create an MSX-DOS 2 cartridge (or flash `JIO-MSXDOS2` to a MegaFlashROM or Carnivore2).
1. Connect your MSX to your PC using a USB serial cable or Bluetooth adapter.
1. Launch **JIOServer** and select the disk image to serve.
1. Select USB or Bluetooth mode using the <img src="./server/icons/Bluetooth.svg" width="20"/> or  <img src="./server/icons/USB.svg" width="20"/> button
1. Select the communication device to use (ttyUSB0 or DSD TECH HC-05 for example)
1. Click the <img src="./server/icons/disconnected.svg" width="20"/> button.
1. Boot your MSX.
1. You should see an <span style="color:green">Info✓ </span> appear in the server log and the LED blink.
1. The MSX should now access the image.

## How to create the 32KB JIO-MSXDOS2 cartridge

*Instructions to be added.*

## Bluetooth configuration for MSXJIO

It is very likely that the Bluetooth Serial Transceiver module you just bought is not configured to match the required MSXJIO settings:

|          |               |
|----------|---------------|
|Baud rate | **115200 bits/s** |
|Stop bit  | **1 bit**         |
|Parity    | **None**          |

For the **HC-05** chip, you can download the [JSM tool](https://github.com/louthrax/MSXJIO/releases/download/v1.0/JIO_38400_bauds_serial_monitor_1_0.zip) provided by JIOMSX.

- Plug your HC-05 module in MSX joystick port 2
- Power on your MSX while keeping the AT switch pressed. The HC-05 led should be blinking in a stable and slowly (2s) way.
- Run JSM.BAS from MSX-BASIC
- Enter this command:  
  **AT+UART=115200,0,0**
- You can also change the name of your device with the command:  
  **AT+NAME=<name_here>**
  <p align="center">
      <img src="./readme_resources/Configure_BT_with_JSM.jpg" width="500"/>
  </p>
- A list of the available AT commands for the HC-05 is available [here](https://github.com/louthrax/MSXJIO/blob/main/tools/JSM/HC-03_05_AT_command_set.pdf).

Configuration of the **HC-06** is trickier and requires an extra USB to TTL UART adapter.  
Procedure is described [here](https://github.com/b3rendsh/msxdos2s/tree/main/jio/bluetooth).


## Fun things to try with JIOMSX

- JIOMSX can serve openMSX hard disk images: code some stuff on openMSX, and immediately test them on a real MSX machine by just serving
the same disk image. No need to swap any SD card !
- If you own an MSX with a built-in factory ROM (like  MSX Designer for the NMS 8220), simply reflash that ROM with JIO MSX-DOS 1 (16KB) or 2 (32KB) to save an external slot.
- Buy several Bluetooth adapters, and name them according to the MSX machine they are plugged in (JIO_VG_8238, JIO_turboR). To debug something on a specific
MSX machine, power it on, select the matching Bluetooth adapter on the server, and connect to test.
- A same disk image can be served to several MSX machines : start playing SD Snatcher on one MSX machine, and continue playing it on another one.
- Use your favorite disk system (floppy, sunrise, other) together with the MSX JIO interface.
You can use a mix of local drives and remote JIO server drives.
- Select a disk image with CP/M Plus for MSX2 and boot with the JIO MSX-DOS 1 ROM:
https://www.msx.org/downloads/bootable-hdd-image-of-cpm-31-for-beer-ide-interface
- Load an experimental romless DOS from tape on your MSX1/64K RAM and you're gone in sixty seconds:
https://github.com/b3rendsh/cxdos

Post your "fun things to try" experiences and suggestions on this [MRC thread](https://www.msx.org/forum/msx-talk/development/msx-jio)

## History

All started with **NYYRIKKI**’s breakthrough **115200 bps** MSX serial communication routine, posted on January 3rd 2025 on msx.org:  
https://www.msx.org/forum/msx-talk/development/software-rs-232-115200bps-on-msx

Shortly after, he released a working **MSX-DOS 1** version serving disk images with **drive sound emulation**!  
https://www.youtube.com/watch?v=OHs5a-gZtuc  
Thats was crazy !

At the same time, I was aware of **b3rendsh**’s MSX-DOS 2 project:  
https://github.com/b3rendsh/msxdos2s  
... which was compact (32KB only, and no need for mapper), and designed to easily integrate any hardware.

I told myself that combining NYYRIKKI’s 115200 bauds routine together with b3rendsh modular MSX-DOS could lead
to a very cheap, versatile and not so slow MSX-DOS 2 hard-disk server !

I quickly contacted b3rendsh, and we started working together on that project.

After several months of collaborative coding and debugging, we hopefuly reached a stable and usable first version !

## Server details

macOS, Windows and Linux versions of the server have tooltips for each UI componenents, which should be self-explanatory.

For Android (that provides not tooltips), here's a quick explanation view:
<p align="center">
    <img src="./readme_resources/JIO_Server_with_tooltips.png" width="900"/>
</p>  

## Known issues

Casio PV-7 and National CF3000 are showing these kind of corruptions on reception:
<p align="center">
    <img src="./readme_resources/RxIssues.png" width="900"/>
</p>
It works a bit better if adding a pull down resistor between MSX RX and GND, but there are still errors.

## Bug report

You can submit tickets on GitHub directly [here](https://github.com/louthrax/MSXJIO/issues), or post messages on this [MRC thread](https://www.msx.org/forum/msx-talk/development/msx-jio)

## Credits

- Enhanced MSX DOS 2 and MSX DOS 1 versions, ideas, debugging, testing, help on JIOServer: **b3rendsh**  
(https://github.com/b3rendsh/msxdos2s)

- 115200 bauds MSX communication routine and originial Python server: **NYYRIKKI**  
(https://msx.fi/nyyrikki/software.html)

- Original 38400 bauds communication routine used by JIO Serial Monitor tool: **Tiny Yarou**  
(https://www.tiny-yarou.com/)

## License

This project is licensed under the terms of the **Attribution-NonCommercial-ShareAlike 4.0 International** license.

The license applies to the JIO protocol, JIO server and JIO specific code in the MSX clients and tools in this repository.
For material that contains substantial parts of work by others, the origins are mentioned in the source code and the copyright is respected when applicable.
