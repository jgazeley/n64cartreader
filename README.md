# N64 Pico Cart Reader V2

A simple, open-source N64 cartridge reader powered by a Raspberry Pi Pico. Plug it in via USB to dump ROMs and back up saves directly to your computer using a web browser or command line.

![Device in Action](https://user-images.githubusercontent.com/89006649/187055008-d4ed1e56-0636-4c86-967c-e2c1d843efed.jpg)

## Features

- **Full ROM dumping** for retail N64 cartridges (up to 32 MB)
- **Save read/write/restore** for all retail save types:
  - SRAM (e.g. 1080 Snowboarding, Ocarina of Time)
  - EEPROM 4K/16K (e.g. GoldenEye, Super Mario 64)
  - FlashRAM (e.g. Majora's Mask, Pokemon Stadium)
  *(For a full list of N64 games and their save types, see [Micro-64's Game Save List](http://micro-64.com/database/gamesave.shtml))*
- **Controller Pak (MPK)** export/import
- **GameShark** cartridge export/import/restore
- **Web Serial GUI** — runs in Chrome/Edge, no install required
- **Python CLI** for all other users
- **3.3V-native design** — no level shifting needed, lower BOM cost
- **Automatic cart identification** — detects game, ROM size, and save type from a built-in catalog

## Getting Started

See [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) for full setup instructions.

### Quick Start

1. Flash the `pico_headless_demo.uf2` firmware to your Pico (hold BOOTSEL while plugging in USB, then drag the file onto the drive).
2. Insert an N64 cartridge.
3. Open the [Web Serial UI](host-tools/web/index.html) in Chrome or Edge, or use a CLI:

**Windows (PowerShell):**
```powershell
./host-tools/powershell/N64CartReader.ps1 -Port COM5
```

**Windows/macOS/Linux (Python):**
```bash
# example for windows
python3 host-tools/python/pico_pak_n64_magic_cli.py --port /dev/ttyCOM5 n64-cart-id --rescan
# example for linux
python3 host-tools/python/pico_pak_n64_magic_cli.py --port /dev/ttyACM0 n64-cart-id --rescan
```

## Project Layout

```
firmware/
  rp2040/          C-based firmware for the Raspberry Pi Pico (timing-critical bus logic)
host-tools/
  powershell/      Native CLI for Windows (no setup needed); also supports macOS/Linux
  python/          Cross-platform CLI for validation, diagnostics, and batch scripts
  web/             Easiest, driverless browser-based interface using Web Serial
hardware/
  board/           V2 hardware project (KiCad PCB, schematics, and BOM)
enclosure/
  board/           3D-printable protective case (FreeCAD and STL files)
docs/              Technical guides, hardware documentation, and troubleshooting
LICENSES/          Third-party license texts for shared components
```

## Web Serial UI

The browser-based interface is the easiest way to use the reader. It works in Chrome and Edge via the Web Serial API. No drivers or software installation required.

Open `host-tools/web/index.html` directly in your browser, click Connect, and the UI will detect your cartridge and present options for ROM dumping, save export/import, and GameShark operations.

See [docs/WEB_SERIAL_UI.md](docs/WEB_SERIAL_UI.md) for details.

## Python CLI

The Python CLI provides full access to all device capabilities and is used for bench testing, validation, and scripted workflows.

```bash
cd host-tools/python

# Identify the cartridge
python3 pico_pak_n64_magic_cli.py --port /dev/ttyACM0 n64-cart-id --rescan

# Export a save
python3 pico_pak_n64_magic_cli.py --port /dev/ttyACM0 n64-export --out my_save.bin --sha256

# Import a save with verification
python3 pico_pak_n64_magic_cli.py --port /dev/ttyACM0 n64-import --in my_save.bin --verify

# Dump the full ROM
python3 pico_pak_n64_magic_cli.py --port /dev/ttyACM0 n64-rom-dump --out my_rom.z64

# Run the validation matrix
python3 n64_validation_matrix.py --port /dev/ttyACM0
```

## PowerShell CLI

A native alternative for Windows users (no installation required). Works with the built-in PowerShell 5.1 or PowerShell 7+ (`pwsh`) on any platform. See [Getting Started](docs/GETTING_STARTED.md) for Windows execution policy setup.

```powershell
# Interactive menu (use COMx on Windows, /dev/ttyACM0 on Linux)
./host-tools/powershell/N64CartReader.ps1 -Port COM5

# Export a save
./host-tools/powershell/Export-Save.ps1 -Port COM5 -Out my_save.bin

# Import a save with verification
./host-tools/powershell/Import-Save.ps1 -Port COM5 -In my_save.bin -Verify

# Dump the full ROM
./host-tools/powershell/Dump-Rom.ps1 -Port COM5 -Out my_rom.z64
```

## Building the Firmware

Requires the [Pico SDK](https://github.com/raspberrypi/pico-sdk) and CMake.

```bash
cd firmware/rp2040
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -S . -B build
cmake --build build -j4 --target pico_headless_demo
```

Flash with picotool or drag `build/pico_headless_demo.uf2` onto the Pico in BOOTSEL mode.

## Hardware

The V2 board is a single PCB with an integrated N64 cartridge slot and Raspberry Pi Pico footprint. The design is in KiCad under `hardware/board/kicad/`.

Power is supplied through USB. The RP2040 runs at 3.3V, which matches N64 cartridge logic levels directly.

Pin mapping and board documentation are in [docs/HARDWARE.md](docs/HARDWARE.md).

## Troubleshooting

**Cartridge not detected or garbled header:**
Remove the cartridge, clean the edge connector contacts with isopropyl alcohol and a cotton swab, and reseat firmly.

**EEPROM save shows `type=none` after cart swap:**
Run `n64-cart-id --rescan` or retry 3-5 times. The Joybus bus needs a moment to settle after a physical swap.

**Controller probe returns `IO_ERR`:**
EEPROM cartridges and controllers share the same Joybus data line. Unplug one to test the other.

**Web Serial won't connect:**
Web Serial requires Chrome or Edge. Firefox and Safari are not supported. Make sure no other application (terminal, serial monitor) has the port open.

## V1 Legacy

The original N64 Cart Reader was built on an Arduino Mega 2560 with an SD card for storage and a serial terminal interface. That design is preserved in the `legacy/v1-arduino` branch.

See [docs/V1_LEGACY.md](docs/V1_LEGACY.md) for details on the original hardware.

## Acknowledgements

N64 Pico Cart Reader V2 was designed and implemented by Jayson Gazeley as an RP2040-based successor to the original Arduino N64 Cart Reader.

The project builds on prior open-source preservation work, especially:

- **[Sanni Cart Reader](https://github.com/sanni/cartreader)** (GPL-3.0) — foundational N64 cartridge interfacing logic
- **[drmdmp64_mass / DreamDumper64](https://github.com/nopjne/drmdmp64_mass)** by nopjne (BSD-2-Clause) — Joybus PIO and EEPROM reference implementation
- **skaman** — multi-console cart-reader modules
- **hkz & themanbehindthecurtain** — FlashRAM command documentation
- **Andrew Brown & Peter Den Hartog** — N64 controller protocol
- **libdragon** — N64 controller checksums

See [docs/CREDITS_AND_PRIOR_ART.md](docs/CREDITS_AND_PRIOR_ART.md) for full attribution and [docs/LICENSING.md](docs/LICENSING.md) for license details.

## References

- [Joybus Protocol](https://n64brew.dev/wiki/Joybus_Protocol)
- [N64 Hardware Architecture](https://www.copetti.org/writings/consoles/nintendo-64/)
