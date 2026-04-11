# Known Limitations

While the N64 Pico Cart Reader V2 provides stable support for retail N64 cartridges, there are several known limitations and experimental features.

## Experimental Repro Cartridge Support (Beta)
Repro (reproduction) cartridge support exists in development builds and should be treated strictly as **beta**. Retail N64 carts remain the stable release target.
- **ROM Dumping:** Generic full ROM dumping from repro cartridges is currently unstable and may yield inconsistent hashes.
- **EEPROM Save Bridge:** The SM64 MX29 repro save EEPROM bridge remains unresolved and is not supported in the stable release.

## Hardware Quirk: EEPROM Latency
After a physical cartridge swap, the initial detection may incorrectly return `type=none` for EEPROM saves.
- **Workaround:** Requires 3-5 retries or an explicit rescan command (`n64-rescan`) to allow the bus to settle.

## Joybus Contention
4K EEPROM cartridges (e.g., GoldenEye) and N64 Controllers share the same data line on the Joybus.
- **Issue:** Probing both simultaneously without hardware isolation will result in `IO_ERR` or bitwise collisions.
- **Workaround:** Unplug the controller to test the cartridge save, or unplug the cartridge to test the controller.

## OOT Repro (USA) Board
- **Issue:** The known OOT Repro (USA) board design is unstable, yielding high-entropy random noise on raw SRAM reads and non-repeatable ROM hashes.
- **Note:** Do not use this specific reproduction board for regression testing.
