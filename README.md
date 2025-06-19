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
- [Acknowledgements](#acknowledgements)

## Prerequisites

Before you begin, ensure you have the following:

* **Assembled N64 Cart Reader Device**: Based on the sanni Cart Reader with an Arduino MEGA 2560 PRO.
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

For those building the device from scratch, you will need to order a custom PCB.

1.  Download the KiCAD PCB file: [`main_pcb.kicad_pcb`](https://github.com/jgazeley/n64cartreader/tree/main/pcb/main_pcb.kicad_pcb).
2.  Follow the instructions on the [sanni Cart Reader wiki](https://github.com/sanni/cartreader/wiki/How-to-order-a-PCB) for details on how to order a PCB from the file.
3.  Assemble the components according to the schematic and PCB layout.

## Acknowledgements

* A big thank you to **sanni** for creating and maintaining the original [Cart Reader](https://github.com/sanni/cartreader) project, which this is based on.