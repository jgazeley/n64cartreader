# N64 ROM/Save Dumper

An open-source hardware and firmware solution for backing up N64 game cartridges over USB.  The reference implementation runs on a Raspberry Pi Pico (RP2040) with TinyUSB-CDC and supports:

* **Interactive CLI mode** for diagnostics and small reads
* **High-throughput streamer mode** that reads the full 8 MiB ROM in chunked bursts (up to \~1 MiB/s)
* **Built-in CRC checking** and a test-pattern mode for link validation

Host-side examples (Python and Win32 C) include live progress reporting and automatic integrity checks. Future work will add a USB Mass-Storage-Class interface, allowing the cartridge to appear as a standard drive for even simpler backups.



This project is a Pico‐centric rework of the original [sanni Cart Reader](https://github.com/sanni/cartreader).

![Device in Action](https://user-images.githubusercontent.com/89006649/187055008-d4ed1e56-0636-4c86-967c-e2c1d843efed.jpg)

## Table of Contents
- [Getting Started](#getting-started)
- [Building & Flashing Firmware](#building--flashing-firmware)
- [Host-Side Test Tools](#host-side-test-tools)
- [Operating the Device](#operating-the-device)
  - [Windows](#windows)
  - [macOS / Linux](#macos--linux)
  - [Android](#android)
- [Troubleshooting](#troubleshooting)
- [Hardware Overview](#hardware-overview)
- [Acknowledgements](#acknowledgements)

---

## Getting Started

> ⚙️ **WIP:** This project is under development. Current instructions target the Raspberry Pi Pico.

Follow these two steps before the first use.

### 1. What You'll Need
* N64 Cart Reader device (RP2040 + Shield or adapter)
* A Micro-USB cable
* A host computer (Windows, macOS, Linux) or Android device

### 2. Building & Flashing Firmware

1. **Clone & configure** in an out-of-source build directory:
   ```bash
   git clone https://github.com/you/pico-pak.git
   cd pico-pak
   mkdir build && cd build
   cmake .. \
     -G "NMake Makefiles" \             # or omit on Linux/macOS
     -DENABLE_CLI=OFF \                  # stream-only mode
     -DPICO_SDK_PATH="C:/path/to/pico-sdk"

## Operating the Device

The device is powered and controlled via a USB serial connection.

### Windows
The recommended tool is **[PuTTY](https://www.putty.org/)**.

1.  **Find COM Port**: Open **Device Manager**, expand **Ports (COM & LPT)**, and find the device (e.g., `COM3`).
2.  **Connect**: In PuTTY, select **Connection type: Serial**, enter your `COM#` in **Serial line**, and set **Speed** to `9600`. Click **Open**.

![PuTTY Configuration](https://user-images.githubusercontent.com/89006649/163255730-a5c36813-b4a7-441c-adde-c2492dd997ba.jpg)

### macOS / Linux
The recommended tool is the built-in `screen` command.

1.  **Find Device Name**: Connect the device and run `ls /dev/tty.*` in a terminal to find the device name (e.g., `/dev/tty.usbmodem14101` or `/dev/ttyACM0`).
2.  **Connect**: In the terminal, run `screen /dev/<your-device-name> 9600`.

### Android
The recommended app is **[Serial USB Terminal](https://play.google.com/store/apps/details?id=de.kai_morich.serial_usb_terminal)**, used with a USB-OTG adapter. The app will auto-detect the device upon connection.

![Device powered by Android](https://user-images.githubusercontent.com/89006649/171938872-d692c80f-fe8c-4ee9-9113-56fd095a9bde.png)

## Troubleshooting

#### Cartridge Read Errors
If the game title appears garbled or you get a `Gamepak Error`, the cartridge is not being read correctly. Like an original N64, a poor connection is the most common cause.

* **Solution**: Remove the cartridge and clean the contacts with a q-tip and isopropyl alcohol. Reseat the cartridge firmly in the slot and try again.

![Bad Read Example](https://user-images.githubusercontent.com/89006649/163607454-d823e7a7-48d4-445f-a9a2-1c160c8e53b5.jpg)
_Example of a bad read. The title should say 'TONYHAWKPROSKAT'._

#### "Clock generator not found..." Message
You may occasionally see this message. It is a known software quirk. Simply select the `Game Cartridge` option again and it will proceed normally.

## Hardware Overview

This project supports three configurations, but note that the **Single-Board Reader** (RP2040 version) is now the primary, actively maintained design—while the Shield + ATmega2560 setup remains as legacy hardware support.

---

## Hardware Overview

> **Note:** The image shown below is the ATmega2560-embed version of the Single-Board Reader—our new RP2040 PCB will look different and still needs to be finalized.  

---

### Single-Board Reader (RP2040)  
![Future RP2040 SBR PCB](hardware/board/kicad/board.png)  
**Definitive & Actively Developed**  
- **Status:** `Prototype (RP2040)`  
- **Description:** Fully integrated N64 slot + RP2040 microcontroller on one PCB.  
- **Planned Features:**  
  - High-speed USB-CDC streaming via TinyUSB  
  - Status LEDs and buzzer for feedback  
  - Configuration jumpers for USB modes  
  
> ⚠️ **Legacy Note:** The board shown above is the older ATmega2560-embed version. The RP2040 design requires a new PCB layout before it can be used.

---

### Shield (Legacy)  
![Shield PCB](hardware/shield/kicad/shield.png)  
**Status:** `Stable (ATmega2560 Legacy)`  
- **Description:** Arduino‐Mega-form-factor Shield with N64 slot, SD, and RTC.  
- **Key Features:**  
  - N64 cartridge interface  
  - SD card + real-time clock support  
- **Warning:**  
  - Requires desoldering headers on the Mega2560 board  
  - **Not recommended** unless you want to practice desoldering and rework.  

---

### Shield Adapter for Pico (Legacy)  
![Adapter PCB](hardware/adapter/kicad/adapter.png)  
**Status:** `Prototype`  
- **Description:** Adapter board that lets a Raspberry Pi Pico plug into the Shield’s Mega2560 footprint.  
- **Use Case:** Transitional hardware for Shield users to run the new Pico firmware.  


---

## Acknowledgements

This project stands on the shoulders of many open-source contributors and resources in the retro-console community:

- **sanni** – original [cartreader](https://github.com/sanni/cartreader) author  
- **nopjne** – [drmdmp64_mass](https://github.com/nopjne/drmdmp64_mass/) USB-MSC dumper for N64    
- **skaman** – multi-console cart-reader modules (SNES, Famicom, Atari, C64, etc.)  
- **hkz & themanbehindthecurtain** – flashram commands for N64  
- **Andrew Brown & Peter Den Hartog** – N64 controller protocol  
- **libdragon** – N64 controller checksums  

**Technical references**  
- Joybus protocol: https://n64brew.dev/wiki/Joybus_Protocol  
- N64 hardware deep dive: https://www.copetti.org/writings/consoles/nintendo-64/  
