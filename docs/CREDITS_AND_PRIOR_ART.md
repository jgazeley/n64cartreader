# Credits and Prior Art

N64 Pico Cart Reader V2 was designed and implemented by Jayson Gazeley as an RP2040-based successor to the original Arduino N64 Cart Reader.

The project builds on prior open-source preservation work, and we extend our deep gratitude to the following projects and their authors:

## Sanni Cart Reader / Open Source Cartridge Reader
- **Project:** [https://github.com/sanni/cartreader](https://github.com/sanni/cartreader)
- **License:** GPL-3.0
- **Contribution:** The original foundational logic for N64 cartridge interfacing, save type identification, and overall architecture that inspired this V2 RP2040 port. Portions of the firmware and software in this project are derived from the Sanni Cart Reader.

## drmdmp64_mass / DreamDumper64
- **Author:** nopjne
- **Project:** [https://github.com/nopjne/drmdmp64_mass](https://github.com/nopjne/drmdmp64_mass)
- **License:** BSD-2-Clause
- **Contribution:** Crucial reference and derived implementations for the Joybus PIO, EEPROM handling, and baseline RP2040 N64 cartridge interface logic.

Thank you to the broader N64 preservation and homebrew community for documenting the hardware quirks that made this project possible.
