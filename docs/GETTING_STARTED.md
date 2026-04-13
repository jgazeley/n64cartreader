# Getting Started

## What You Need

- **N64 Pico Cart Reader V2 board** (assembled, with Raspberry Pi Pico soldered)
- **USB cable** (Micro-USB for Pico, or USB-C if using a Pico variant with USB-C)
- **A host computer** (Windows, macOS, or Linux) or Android device with USB-OTG
- **One or more retail N64 cartridges**

Optional:
- **N64 Controller** with Controller Pak (for MPK backup/restore)
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

No software installation required. Works on any OS with a supported browser. The Web UI includes a bundled cartridge catalog for automatic identification; if you ever need to override it, drag `host-tools/n64.txt` onto the catalog drop zone.

### Option B: Python CLI (recommended for users without a Chromium-based browser)

Requires Python 3.10+ and pyserial. If on Windows, install Python from python.org and check "Add python.exe to PATH" during setup. Install dependencies with:

**macOS/Linux:**
```bash
python3 -m pip install -r host-tools/python/requirements.txt
```

**Windows:**
```powershell
python -m pip install -r host-tools\python\requirements.txt
# Or if that fails:
py -m pip install -r host-tools\python\requirements.txt
```

The Python CLI auto-detects the Pico when exactly one Pico serial device is connected. If multiple Pico devices are connected, or if auto-detect fails, specify the port manually with `--port`.

On Windows, open **Device Manager -> Ports (COM & LPT)** and look for the Pico/USB serial port, then use that COM name, for example `--port COM5`. In the interactive CLI, choose **U -> P** to list detected ports and set the port without restarting the program.

The examples below use `python3`; on Windows, use `python` or `py` instead if that is how Python is installed on your system.

```bash
cd host-tools/python

# Check firmware version (auto-detects if exactly one Pico is connected)
python3 pico_pak_n64_magic_cli.py fw-version

# Check firmware version with an explicit port, if needed
python3 pico_pak_n64_magic_cli.py --port <PORT> fw-version

# Identify the cartridge
python3 pico_pak_n64_magic_cli.py --port <PORT> n64-cart-id --rescan

# Export save data
python3 pico_pak_n64_magic_cli.py --port <PORT> n64-export --out save.bin --sha256

# Import save data with verification
python3 pico_pak_n64_magic_cli.py --port <PORT> n64-import --in save.bin --verify

# Dump the full ROM
python3 pico_pak_n64_magic_cli.py --port <PORT> n64-rom-dump --out game.z64
```

### Option C: PowerShell CLI (native alternative for Windows)

The PowerShell scripts have zero external dependencies -- no pip, no packages, just PowerShell and a USB cable. This makes them a great option on Windows where PowerShell is already installed.

**Windows (PowerShell 5.1 is built in):**

You may need to allow script execution the first time. Open PowerShell and run:
```powershell
Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned
```
You only need to do this once. Then run scripts directly:
```powershell
# Interactive menu (use COMx on Windows)
.\host-tools\powershell\N64CartReader.ps1 -Port COM5

# Or run individual operations:

# Export save data
.\host-tools\powershell\Export-Save.ps1 -Port COM5 -Out save.bin

# Import save data with verification
.\host-tools\powershell\Import-Save.ps1 -Port COM5 -In save.bin -Verify

# Dump the full ROM
.\host-tools\powershell\Dump-Rom.ps1 -Port COM5 -Out game.z64
```

**macOS/Linux (requires PowerShell 7+):**

Install PowerShell 7 from https://github.com/PowerShell/PowerShell#get-powershell, then use `pwsh`:
```bash
# Interactive menu (use /dev/tty... on Linux/macOS)
pwsh host-tools/powershell/N64CartReader.ps1 -Port /dev/ttyACM0

# Export save data
pwsh host-tools/powershell/Export-Save.ps1 -Port /dev/ttyACM0 -Out save.bin

# Import save data with verification
pwsh host-tools/powershell/Import-Save.ps1 -Port /dev/ttyACM0 -In save.bin -Verify

# Dump the full ROM
pwsh host-tools/powershell/Dump-Rom.ps1 -Port /dev/ttyACM0 -Out game.z64
```

All scripts accept `-SaveType` to override auto-detection (e.g. `-SaveType sram`, `flashram`, `eeprom4k`, `eeprom16k`).

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

For one reader, the Web UI port picker or Python CLI auto-detect is usually enough. If multiple Pico readers are connected, identify the right one by unplugging the other readers temporarily, or use the operating system's port list:

- **Windows:** Open **Device Manager -> Ports (COM & LPT)** and look for the Pico/USB serial device, such as `COM5`.
- **macOS:** Check **System Information -> USB**, or list serial devices with `ls /dev/tty.usbmodem*`.
- **Linux:** List serial devices with `ls /dev/ttyACM*`; for stable names that include the USB serial number, use `ls -l /dev/serial/by-id/`.

In the Python interactive CLI, choose **U -> P** to list detected ports and set the active port without restarting.

## Next Steps

- [Web Serial UI guide](WEB_SERIAL_UI.md)
- [Hardware overview and pin map](HARDWARE.md)
- [Manufacturing test procedure](MANUFACTURING_TEST.md)
- [Known limitations](KNOWN_LIMITATIONS.md)
