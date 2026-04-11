# Getting Started

## What You Need

- **N64 Pico Cart Reader V2 board** (assembled, with Raspberry Pi Pico soldered)
- **USB cable** (Micro-USB for Pico, or USB-C if using a Pico variant with USB-C)
- **A host computer** (Windows, macOS, or Linux) or Android device with USB-OTG
- **One or more retail N64 cartridges**

Optional:
- **N64 Controller** with Controller Pak (for MPK backup)
- **GameShark Pro** cartridge (for GameShark backup/restore)

## Step 1: Flash the Firmware

If your Pico is not already flashed with the V2 firmware:

1. Hold the **BOOTSEL** button on the Pico while plugging in the USB cable.
2. The Pico appears as a USB mass storage drive.
3. Drag `pico_headless_demo.uf2` onto the drive. The Pico reboots automatically.

To build the firmware from source, see the [Building the Firmware](#building-the-firmware) section below.

After flashing, the Pico enumerates as a USB CDC serial device:
- **Linux:** `/dev/ttyACM0` (or similar)
- **macOS:** `/dev/tty.usbmodemXXXX`
- **Windows:** `COMx` (visible in Device Manager under Ports)

## Step 2: Insert a Cartridge

Push the N64 cartridge firmly into the slot until it seats. A loose connection is the most common cause of read errors.

If the cartridge contacts are dirty, clean them with isopropyl alcohol and a cotton swab before inserting.

## Step 3: Connect and Use

### Option A: Web Serial UI (recommended for most users)

1. Open `host-tools/web/index.html` in **Chrome** or **Edge**.
2. Click **Connect** and select the Pico's serial port.
3. The UI detects the cartridge and shows game info, save type, and available operations.

No software installation required. Works on any OS with a supported browser.

### Option B: Python CLI (recommended for power users and testing)

```bash
cd host-tools/python

# Check firmware version
python3 pico_pak_n64_magic_cli.py --port /dev/ttyACM0 fw-version

# Identify the cartridge
python3 pico_pak_n64_magic_cli.py --port /dev/ttyACM0 n64-cart-id --rescan

# Export save data
python3 pico_pak_n64_magic_cli.py --port /dev/ttyACM0 n64-export --out save.bin --sha256

# Import save data with verification
python3 pico_pak_n64_magic_cli.py --port /dev/ttyACM0 n64-import --in save.bin --verify

# Dump the full ROM
python3 pico_pak_n64_magic_cli.py --port /dev/ttyACM0 n64-rom-dump --out game.z64
```

Requirements: Python 3.8+ and pyserial. Install with:
```bash
pip install -r requirements.txt
```

### Option C: Android

Use a USB-OTG adapter and a serial terminal app such as [Serial USB Terminal](https://play.google.com/store/apps/details?id=de.kai_morich.serial_usb_terminal). The device will be detected automatically on connection.

## Building the Firmware

Requires the [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) and CMake.

```bash
cd firmware/rp2040
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -S . -B build
cmake --build build -j4 --target pico_headless_demo
```

The output is `build/pico_headless_demo.uf2`. Flash it by dragging onto the Pico in BOOTSEL mode, or with picotool:

```bash
picotool load -f build/pico_headless_demo.uf2 -x
```

## Updating Firmware

To update an already-flashed Pico without pressing BOOTSEL:

```bash
picotool load -f build/pico_headless_demo.uf2 -x
```

picotool will automatically reboot the Pico into BOOTSEL mode, flash, and restart.

## Identifying Your Hardware

If you have multiple Pico units connected, use the serial number to identify them:

```bash
ls -l /dev/serial/by-id/
```

This shows stable symlinks that encode the Pico's unique serial number, regardless of which `/dev/ttyACM` index the OS assigned.

## Next Steps

- [Web Serial UI guide](WEB_SERIAL_UI.md)
- [Hardware overview and pin map](HARDWARE.md)
- [Manufacturing test procedure](MANUFACTURING_TEST.md)
- [Known limitations](KNOWN_LIMITATIONS.md)
