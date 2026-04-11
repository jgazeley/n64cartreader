# Licensing

The N64 Pico Cart Reader V2 project encompasses several different domains, each with its own licensing terms to respect prior art and open-source boundaries.

## Firmware and Software
The firmware and host software components are open source and subject to the following licenses:

- **GPL-3.0:** Portions of the firmware and software derived from the [Sanni Cart Reader / Open Source Cartridge Reader](https://github.com/sanni/cartreader) project are licensed under the GNU General Public License v3.0. See `LICENSES/GPL-3.0.txt`.
- **BSD-2-Clause:** The Joybus PIO and EEPROM implementation derived in part from [drmdmp64_mass by nopjne](https://github.com/nopjne/drmdmp64_mass) are covered by the BSD-2-Clause license. See `LICENSES/BSD-2-Clause.txt`.

Where applicable, source files contain specific attribution headers indicating their origin and licensing:

**For files derived from Sanni:**
```c
/*
 * Derived from the Open Source Cartridge Reader / sanni Cart Reader project.
 * Original project: https://github.com/sanni/cartreader
 * Original license: GPL-3.0
 *
 * Modified by Jayson Gazeley for N64 Pico Cart Reader V2.
 */
```

**For files derived from or based on drmdmp64_mass:**
```c
/*
 * Joybus/EEPROM PIO implementation derived in part from:
 *   drmdmp64_mass by nopjne
 *   https://github.com/nopjne/drmdmp64_mass
 *
 * Original source files identify core code as BSD-2-Clause.
 * Modified by Jayson Gazeley for N64 Pico Cart Reader V2.
 */
```

## Hardware and Enclosure
Hardware design files (KiCad schematics, PCB layouts, Gerber files) and mechanical enclosure models (FreeCAD, STL files) are licensed separately from the firmware and software. Check the specific `hardware/` and `enclosure/` directories for their respective licensing terms (e.g., CERN-OHL or Creative Commons).
