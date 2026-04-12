# N64 Pico-Pak: Connectivity Source of Truth

**Reference Code:** `/srv/agent_share/n64/n64-fw/include/n64/pins.h`

| Function | Pin (pins.h) | Target MCU (RP2040) | N64 Slot Pin | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **ADBUS 0-15** | `GPIO0-15` | GPIO0 - GPIO15 | AD0 - AD15 | 16-bit Multiplexed Bus |
| **SYSTEM_RESET** | `GPIO16` | GPIO16 | RESET | Active-High for Bus |
| **WR_STROBE** | `GPIO17` | GPIO17 | /WRITE | Active-Low |
| **RD_STROBE** | `GPIO18` | GPIO18 | /READ | Active-Low |
| **ALE_H** | `GPIO19` | GPIO19 | ALE_H | Address Latch High |
| **ALE_L** | `GPIO20` | GPIO20 | ALE_L | Address Latch Low |
| **EEPROM DATA** | `GPIO21` | GPIO21 | EE_DAT | Shared Joybus data line for cartridge EEPROM/RTC and controller |
| **EEPROM CLK** | `GPIO22` | GPIO22 | EE_CLK | Clock for cartridge EEPROM transfers |

## Power Requirements
- **VCC:** +3.3V (RP2040 and N64 Logic)
- **VBUS:** +5V (Optional, for USB or specific N64 lines)
- **GND:** Shared Ground Plane
