# V1 Legacy: Arduino N64 Cart Reader

The original N64 Cart Reader (V1) was built on an Arduino Mega 2560 with an SD card for local storage and a serial terminal for control.

## V1 Hardware

- **MCU:** ATmega2560 (Arduino Mega 2560 or Mega 2560 Pro)
- **Storage:** SD card slot for saving ROMs and save files
- **Interface:** USB serial terminal (PuTTY, screen, or similar)
- **Logic levels:** 5V MCU with 3.3V level shifting to the N64 cartridge bus
- **Additional features:** RTC module, status LED, buzzer

The V1 board was a shield that plugged onto the Arduino Mega. It required desoldering the Arduino's pin headers for a flush fit — a significant barrier for less experienced builders.

## V1 Software

The V1 firmware was derived from the [Sanni Cart Reader](https://github.com/sanni/cartreader) project and ran as an interactive serial menu. Users navigated a text-based menu over a terminal emulator to:

- Read ROM data to SD card
- Read/write save data (SRAM, EEPROM, FlashRAM)
- Test controller and Controller Pak

Host-side operation required a serial terminal application:
- **Windows:** PuTTY
- **macOS/Linux:** `screen /dev/ttyXXXX 9600`
- **Android:** Serial USB Terminal app with USB-OTG adapter

## Why V2

V2 replaced the ATmega2560 with an RP2040 (Raspberry Pi Pico) to address V1's limitations:

| | V1 (ATmega2560) | V2 (RP2040) |
|---|---|---|
| Logic level | 5V (needs level shifting) | 3.3V native |
| Storage | SD card on the device | USB streaming to host |
| Interface | Serial terminal menu | Web Serial GUI + Python/PowerShell CLI |
| Speed | ~100 KB/s ROM dump | ~1 MB/s ROM dump |
| BOM cost | Higher (Mega + shield + SD + RTC + level shifters) | Lower (Pico + PCB + passives) |
| Build difficulty | Header desoldering required | Standard through-hole/SMD soldering |

## Where to Find V1 Code

- The V1 branch history is preserved at `legacy/v1-arduino`.
- The upstream Sanni Cart Reader project is at [github.com/sanni/cartreader](https://github.com/sanni/cartreader).
