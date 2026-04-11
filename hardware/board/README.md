# N64 Pico Cart Reader V2 Hardware

This directory contains the custom KiCad PCB design files and fabrication outputs for the N64 Pico Cart Reader V2.

## Directory Structure

- **`docs/`**: Hardware documentation, including the current `PINMAP.md` that outlines the routing between the Raspberry Pi Pico and the N64 cartridge slot.
- **`kicad/`**: The complete, editable KiCad project (`.kicad_pro`, `.kicad_sch`, `.kicad_pcb`).
  - **`lib/`**: Custom project symbols and footprints (e.g., the N64 cartridge slot footprint).
- **`fabrication/`**: Board manufacturing outputs.
  - **`jlcpcb/`**: A known-good `.zip` file containing the Gerbers and drill files formatted and ready for upload to JLCPCB.
  - **`gerbers/`**: The individual, extracted Gerber and drill files.
  - **`reports/`**: Optional DRC/ERC evidence from the released board. These are useful for review but are not required to order boards.

## Hardware Quirk: EEPROM Latency
When testing the hardware, note that after a physical cartridge swap, the initial Joybus detection may return `type=none` for EEPROM saves. This is a known hardware limitation. It typically requires 3-5 retries or an explicit software rescan to allow the bus to settle.

## Licensing
The hardware design files (schematics, PCB layouts, custom footprints) are released under the [Creative Commons Attribution 4.0 International (CC BY 4.0)](https://creativecommons.org/licenses/by/4.0/) license. This license was chosen to maintain continuity with the hardware licensing of the original V1 Arduino-based project from which this work conceptually descends.
