# Known Limitations

While the N64 Pico Cart Reader V2 provides stable support for retail N64 cartridges, there are several known limitations and experimental features.

## Reproduction Cartridge Support
Reproduction cartridges vary widely in flash memory, CPLD/mapper logic, and save translation hardware. Retail N64 cartridges remain the primary stable release target.
- **ROM Dumping:** Some reproduction cartridges may require extra initialization and are not guaranteed to behave exactly like retail mask ROM boards.
- **Saves:** Reproduction cartridges that translate N64 Joybus save commands into non-retail save hardware may not be supported unless that board design has been explicitly validated.

## Hot-Swap Detection
Cartridges should be rescanned after insertion or swapping. If an EEPROM cartridge reports no save after a hot swap, reseat the cartridge and run rescan again before attempting save operations.

## Joybus Contention
Cartridge EEPROM and the optional controller port share the Joybus data line on this board.
- **Issue:** Probing an EEPROM cartridge and a controller simultaneously can produce `IO_ERR` or bitwise collisions.
- **Workaround:** Unplug the controller to test the cartridge save, or unplug the cartridge to test the controller.
