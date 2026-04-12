# Hardware Overview

## V2 Single-Board Reader (RP2040)

The V2 board is a single PCB integrating an N64 cartridge slot and a Raspberry Pi Pico (RP2040). It replaces the original Arduino Mega 2560 + shield design with a simpler, lower-cost solution.

### Design Highlights

- **3.3V-native:** The RP2040 and N64 cartridges both operate at 3.3V. No level shifters required.
- **No SD card:** All data transfer happens over USB. Saves and ROMs are streamed directly to the host.
- **Low BOM:** The minimum build requires the PCB, a Pico, the N64 cartridge slot connector, and passive components.
- **USB-powered:** No external power supply needed.

### Pin Mapping

The RP2040 GPIO assignments are defined in `firmware/rp2040/include/n64/pins.h`.

| Function | GPIO | N64 Slot Pin | Notes |
|----------|------|-------------|-------|
| AD0-AD15 | GPIO 0-15 | AD0-AD15 | 16-bit multiplexed address/data bus |
| SYSTEM_RESET | GPIO 16 | RESET | Active-high for bus reset |
| /WRITE | GPIO 17 | /WR | Active-low write strobe |
| /READ | GPIO 18 | /RD | Active-low read strobe |
| ALE_H | GPIO 19 | ALE_H | Address latch enable (high byte) |
| ALE_L | GPIO 20 | ALE_L | Address latch enable (low byte) |
| EEPROM DATA | GPIO 21 | EE_DAT | Shared Joybus data line for cartridge EEPROM/RTC and controller |
| EEPROM CLK | GPIO 22 | EE_CLK | Clock for cartridge EEPROM transfers |

### Power

- **VCC (3.3V):** Supplied from the Pico's onboard regulator or an external 3.3V LDO (e.g. AP2112K-3.3) fed from USB 5V.
- **GND:** Shared ground between Pico, PCB, and cartridge slot.

If using an external LDO, ensure solid solder joints on all pins — a high-resistance connection on the regulator can cause voltage sag under load, resulting in save write failures while reads appear normal.

### KiCad Source

The schematic and PCB layout are in `hardware/board/kicad/`. Custom footprints for the N64 cartridge slot, controller port, and Pico module are in the project-local library under `hardware/board/kicad/lib/`.

### Controller Port

The optional controller port shares the Joybus data line (GPIO 21) with EEPROM cartridges. When both are connected, bus contention will occur. Use one at a time.

## Legacy Hardware

The original V1 design used an Arduino Mega 2560 with a shield PCB, SD card, and level shifters. See [V1_LEGACY.md](V1_LEGACY.md) for details. V1 hardware files are preserved in the `legacy/v1-arduino` branch.
