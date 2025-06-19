# N64 ROM/Save Dumper

This repository contains the necessary information and files for building and operating a DIY N64 Cartridge Reader/Dumper. This device allows you to back up your N64 game ROMs and save files directly to a microSD card.

This project is a modification of the [sanni Cart Reader](https://github.com/sanni/cartreader).

![Device in Action](https://user-images.githubusercontent.com/89006649/187055008-d4ed1e56-0636-4c86-967c-e2c1d843efed.jpg)

## Table of Contents
- [Prerequisites](#prerequisites)
- [Initial Setup](#initial-setup)
  - [1. Flash the Arduino Firmware](#1-flash-the-arduino-firmware)
  - [2. Prepare the microSD Card](#2-prepare-the-microsd-card)
- [Operating the Device](#operating-the-device)
  - [Windows](#windows)
  - [macOS / Linux](#macos--linux)
  - [Android](#android)
- [Downloads](#downloads)
- [DIY Build Guide](#diy-build-guide)
  - [Required Parts List](#required-parts-list)
  - [Assembly Instructions](#assembly-instructions)
- [Troubleshooting and Known Issues](#troubleshooting-and-known-issues)
- [Acknowledgements](#acknowledgements)

## Prerequisites

Before you begin, ensure you have the following:

* **Assembled N64 Cart Reader Device**: Or the parts to build one.
* **microSD Card**: 4 GB or larger.
* **Computer or Android Device**: To control the dumper.
* **Micro-USB Cable**: To connect the device.
* **USB OTG Adapter**: Required for connecting to an Android device.

> **IMPORTANT**: The device will show an `SD Error` and will not function without a properly formatted microSD card inserted.

## Initial Setup

Follow these two steps to prepare your device for its first use.

### 1. Flash the Arduino Firmware

The core of the device is an Arduino MEGA 2560 PRO that needs to be flashed with the correct firmware.

1.  **Download the Arduino IDE**: Use the portable package from the [sanni Cart Reader wiki](https://github.com/sanni/cartreader/wiki/How-to-flash-the-Arduino), which includes all necessary code. This page also includes instructions for installing the required drivers.
2.  **Replace Code Files**: Before uploading, you must replace three files in the Arduino IDE's sketchbook directory. Copy the following files into `...\Arduino IDE Portable\portable\sketchbook` and overwrite the originals:
    * [`Cart_Reader.ino`](https://github.com/jgazeley/n64cartreader/blob/main/code/Cart_Reader.ino)
    * [`N64.ino`](https://github.com/jgazeley/n64cartreader/blob/main/code/N64.ino)
    * [`options.h`](https://github.com/jgazeley/n64cartreader/blob/main/code/options.h)
3.  **Upload to Arduino**:
    * Connect the N64 Cart Reader to your computer.
    * Open the Arduino IDE.
    * Go to `Tools` -> `Board` and select **Arduino Mega or Mega 2560**.
    * Go to `Tools` -> `Port` and select the COM port corresponding to your device.
    * Click the **Upload** button (the arrow icon).

### 2. Prepare the microSD Card

1.  **Format the Card**: Format your microSD card (4 GB or larger) to either **FAT32** or **exFAT**.
2.  **Copy Configuration File**: Download the `n64.txt` file below and place it in the root directory of your microSD card.
    * [n64.txt](https://raw.githubusercontent.com/sanni/cartreader/master/sd/n64.txt) (Right-click and select "Save Link As...")

## Operating the Device

The device is powered and controlled entirely via USB. You can use a computer or an Android device.

### Windows

The recommended tool for Windows is **PuTTY**.

1.  **Find the COM Port**:
    * Open **Device Manager**.
    * Expand the **Ports (COM & LPT)** section.
    * The cart reader will appear as `USB Serial Device (COM#)`. Note the COM number.
2.  **Configure PuTTY**:
    * Launch PuTTY.
    * Set **Connection Type**: `Serial`.
    * Set **Serial Line**: `COM#` (replace `#` with your number).
    * Set **Speed**: `9600`.
    * (Optional) You can save this configuration for quick access later. Enter a name under `Saved Sessions` and click `Save`.
3.  Click **Open** to connect.

![PuTTY Configuration](https://user-images.githubusercontent.com/89006649/163255730-a5c36813-b4a7-441c-adde-c2492dd997ba.jpg)

### macOS / Linux

For macOS and Linux, the recommended tool is `screen`.

1.  **Find the Device Name**:
    * Open a terminal **without** the cart reader connected and run:
        ```sh
        ls /dev/tty* > temp1
        ```
    * Connect the cart reader and wait a moment. Then run:
        ```sh
        ls /dev/tty* > temp2
        ```
    * Find the new device name by comparing the files:
        ```sh
        diff temp1 temp2
        ```
        The output (e.g., `/dev/ttyACM0` or `/dev/ttyUSB0`) is your device name.
    * Clean up the temporary files:
        ```sh
        rm temp1 temp2
        ```
2.  **Connect with `screen`**:
    * Use the device name from the previous step. You may need `sudo` privileges.
        ```sh
        sudo screen /dev/ttyACM0 9600
        ```
        (Replace `/dev/ttyACM0` with your device name).

![Connecting with screen on Linux](https://user-images.githubusercontent.com/89006649/163267268-a647b314-ba43-4a40-8c08-f821a90bd8b0.png)

### Android

You will need a USB OTG adapter to connect the cart reader to your phone or tablet.

1.  **Install the App**: Download [Serial USB Terminal](https://play.google.com/store/apps/details?id=de.kai_morich.serial_usb_terminal) from the Play Store.
2.  **Load Configuration**:
    * Download the [configuration file](https://raw.githubusercontent.com/jgazeley/n64cartreader/main/n64cartreader.txt).
    * In the Serial USB Terminal app, import the downloaded `.txt` file for a quick and easy setup.
3.  Connect the device via the USB OTG adapter and use the app to control it.

![Device powered by Android](https://user-images.githubusercontent.com/89006649/171938872-d692c80f-fe8c-4ee9-9113-56fd095a9bde.png)

## Downloads

| File / Link                                                                                                             | Description                                                                                             |
| ----------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------- |
| [Arduino IDE Portable Package](https://github.com/sanni/cartreader/wiki/How-to-flash-the-Arduino)                       | Contains the IDE and base code for flashing the Arduino.                                                |
| [PuTTY](https://www.putty.org/)                                                                                         | Recommended serial terminal application for Windows.                                                    |
| [Serial USB Terminal](https://play.google.com/store/apps/details?id=de.kai_morich.serial_usb_terminal&hl=en_US&gl=US)     | Recommended serial terminal application for Android.                                                    |
| [Android Config File](https://raw.githubusercontent.com/jgazeley/n64cartreader/main/n64cartreader.txt)                  | Configuration preset for the Serial USB Terminal app.                                                   |
| **Required Firmware Files** | **Overwrite original files in `.../sketchbook` directory before flashing.** |
| [`Cart_Reader.ino`](https://github.com/jgazeley/n64cartreader/blob/main/code/Cart_Reader.ino)                             | Main sketch file for the Arduino.                                                                       |
| [`N64.ino`](https://github.com/jgazeley/n64cartreader/blob/main/code/N64.ino)                                             | N64-specific code module.                                                                               |
| [`options.h`](https://github.com/jgazeley/n64cartreader/blob/main/code/options.h)                                         | Configuration options for the firmware.                                                                 |
| **Hardware / PCB** |                                                                                                         |
| [`main_pcb.kicad_pcb`](https://github.com/jgazeley/n64cartreader/tree/main/pcb/main_pcb.kicad_pcb)                         | KiCAD file for the main PCB. See [sanni's guide](https://github.com/sanni/cartreader/wiki/How-to-order-a-PCB) to order boards. |

## DIY Build Guide

This section is for those building the device from scratch.

### Required Parts List

This is the list of components needed to assemble the N64 Cart Reader.

* **Printed Circuit Board (PCB)**:
    * [PCB KiCAD File](https://github.com/jgazeley/n64cartreader/tree/main/pcb/main_pcb.kicad_pcb)
    * Details on ordering a custom PCB can be found [here](https://github.com/sanni/cartreader/wiki/How-to-order-a-PCB).
* **Microcontroller**: [MEGA 2560 PRO](https://www.amazon.com/dp/B07TGF9VMQ/?coliid=I2EKJHKCM3EWXH&colid=2D9YZCU5L0Q8Z&psc=1&ref_=lv_ov_lig_dp_it)
* **SD Card Reader**: [Micro SD Card Module](https://www.amazon.com/dp/B08CMLG4D6/?coliid=I3SOXN6UT9UUSA&colid=2D9YZCU5L0Q8Z&psc=1&ref_=lv_ov_lig_dp_it)
* **Clock Signal**: [48MHz Clock Generator](https://www.amazon.com/dp/B00SK8LNYG/?coliid=I2V2BTKHZ6KJ7U&colid=2D9YZCU5L0Q8Z&psc=1&ref_=lv_ov_lig_dp_it)
* **Cartridge Connector**: [N64 Cartridge Slot](https://www.ebay.com/itm/363620883667)
* **Wiring**: [N64 Controller Extension Cable](https://www.amazon.com/dp/B08TLXZ5B6/?coliid=I2G7QOP29RWDR&colid=2D9YZCU5L0Q8Z&psc=1&ref_=lv_ov_lig_dp_it) (used for its wiring and connector)
* **Resistor**: [1K ohm resistor](https://www.amazon.com/dp/B0185FJ6L0/?coliid=I1U7XC9HBRJS1M&colid=2D9YZCU5L0Q8Z&psc=1&ref_=lv_ov_lig_dp_it) (x1)
* **Storage**: 4GB+ SD card (formatted to FAT32 or exFAT)
* **Misc**: Zip tie (x1)

![Parts required for assembly](https://preview.redd.it/ciylewubf5t81.jpg?width=4032&format=pjpg&auto=webp&s=f52b22560592b94f0f0c511febdc4765fc3d2d59)

### Assembly Instructions

This guide will walk you through the process of soldering and assembling the N64 Cart Reader.
> _Note: This is not a comprehensive soldering or electronics tutorial. There are many excellent resources on YouTube and other sites if you are new to soldering._

#### Step 1: Solder Pin Headers
First, solder the pin headers to the MEGA 2560 PRO, the SD card module, and the Clock generator module. It is common practice to place the pin headers so they are facing down from the "top" of each module.

For the **MEGA 2560 PRO**, only solder the pins required for the PCB. Do not solder the 2x3 ICSP pin header. Solder only the pins highlighted in the red boxes below (the 2x16, 2x21, and 2x6 headers).

![MEGA 2560 PRO pin headers to solder](https://preview.redd.it/w5mz2jj8e5t81.png?width=505&format=png&auto=webp&s=e2e1ee9610861873a7c9a2bdc7639d27dfccd181)
_Solder only the pin headers in the red boxes. Do not solder the ICSP pins._

#### Step 2: Solder Components to the PCB
Solder the following components directly to the main circuit board:
* MEGA 2560 PRO
* SD Card Module
* Clock Generator
* N64 Cartridge Slot
* 1K Resistor
* N64 Controller Port wires

![N64 Cartridge Slot orientation](https://preview.redd.it/1pcxrmyos5t81.jpg?width=4032&format=pjpg&auto=webp&s=1a5734eec4357344d3cb2a9d16499cf1fae0a6aa)
_With this custom PCB, the cartridge slot should be inserted as pictured._

**Controller Port Wiring**

Cut the N64 controller extension cable, leaving enough length to work with. Before cutting, test the plug with a controller to ensure you are using the correct end. Solder the red, white, and black wires to the PCB as follows:

* **Red Wire** -> `3.3V`
* **White Wire** -> `D7`
* **Black Wire** -> `GND`

The **1K resistor** is a pull-up resistor that goes between the `3.3V` line and the `D7` data line.

![Controller Port Wiring Diagram](https://preview.redd.it/er70nb3fh5t81.png?width=1761&format=png&auto=webp&s=4e94741113ee7965d6a86781b564b26328ba7f6b)
_Red -> 3.3V; White -> D7 (Arduino data); Black -> GND_

#### Step 3 (Optional): Secure the Controller Port
For added stability, you can add hot glue to the solder joints of the controller cable wires and use a zip tie to secure the controller port to the PCB. The custom PCB includes holes specifically for this purpose.

> **Tip**: Make sure the "head" of the zip tie is on the opposite side of the cartridge slot. If it's on the same side, it may block a game cartridge from being inserted fully.

![Controller port secured with hot glue and zip tie](https://preview.redd.it/uxqutl21m5t81.jpg?width=6040&format=pjpg&auto=webp&s=a600b251aae2293782acbf4ed28628226f9d8eaf)
_Controller port soldered, glued, and zip-tied for stability._

#### Step 4 (Optional but Recommended): Verify Connections
Use a multimeter in continuity mode to check that all connections are solid and correct, referencing the schematic below. For example, check for continuity between pin #5 on the N64 cart slot (AD13) and pin A13 on the Arduino. This step can save you a lot of trouble later by catching bad solder joints or incorrect connections early.

#### Full Wiring Schematic (For Reference)
This schematic is provided for reference, especially if you are attempting a custom build without the PCB. If you are using the custom PCB, all of these connections are handled by the board itself.

![Full N64 Cart Reader Wiring Schematic](https://preview.redd.it/mzwpu3ugi5t81.png?width=1580&format=png&auto=webp&s=ca24d4ed7d4ba1901b0f6e29f71bac215804d437)
_VCC is 3.3 volts. The 50-pin N64 pinout is inside the rectangle, and the Arduino connections are on the sides._

#### Final Assembly Check
Once all components are soldered, your board should look similar to the image below.

![Fully assembled board](https://preview.redd.it/9ufgm38ds5t81.jpg?width=4032&format=pjpg&auto=webp&s=2e3d2e976ca8fd3faef4e8d7b975a4773e7084fb)
_Completed assembly._

## Troubleshooting and Known Issues

Here are some common issues and quirks you may encounter while using the device.

* **Clock Generator Not Found**: When you select `Game Cartridge` (option '0') from the main menu, you may see a `Clock generator not found...` message. This appears to be a bug. If you see this, simply ignore the message and select the option again; it should work on the second try.

* **Writing to a Controller Pak**: The `Write ControllerPak` option will automatically loop and retry the write/verify process until it succeeds. A write operation may sometimes appear to be "stuck," but it is best to let it run. The process is complete when you see the `Verifying.... OK` message.
    ![Controller Pak Write Verification](https://user-images.githubusercontent.com/89006649/163607342-d75ddb66-4a1a-47b2-88ca-d33c5f754307.png)

* **Cartridge Read Errors**: If you encounter garbled text for the game title, a `Gamepak Error`, or a ROM dump checksum that doesn't match a known good dump, the cartridge is not being read correctly.
    * **Solution**: Just like with an original N64 console, the cartridge contacts may need cleaning. Use a q-tip with isopropyl alcohol to gently clean the contacts. You may also need to reseat the cartridge in the slot to ensure a proper connection.
    * **Examples of Bad Reads**:

        ![Bad read of Tony Hawk's Pro Skater](https://user-images.githubusercontent.com/89006649/163607454-d823e7a7-48d4-445f-a9a2-1c160c8e53b5.jpg)
        _The title should read 'TONYHAWKPROSKAT'._

        ![Another bad read of Tony Hawk's Pro Skater](https://user-images.githubusercontent.com/89006649/163607464-08403f65-06f5-4dbb-9478-fdd95e7d8c5f.jpg)
        _No Gamepak Error, but the title text is still incorrect, indicating a bad connection._

        ![Bad read of The Legend of Zelda](https://user-images.githubusercontent.com/89006649/163607468-94402aef-8222-4e58-b3ff-09fa96d1769e.jpg)
        _The title should read 'THELEGENDOFZELD'._

* **More Information**: Additional known issues for the base hardware can be found on the sanni Cart Reader GitHub wiki. Note that since this is a custom N64-only build, not all issues listed there will be applicable.
    * [sanni/cartreader/wiki/Known-issues](https://github.com/sanni/cartreader/wiki/Known-issues)

## Acknowledgements

* A big thank you to **sanni** for creating and maintaining the original [Cart Reader](https://github.com/sanni/cartreader) project, which this is based on.