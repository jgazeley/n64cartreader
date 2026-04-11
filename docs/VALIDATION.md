# Validation

The N64 Pico Cart Reader V2 firmware undergoes rigorous bench testing against known-good "golden" references to ensure data integrity for ROM dumping and save management.

## Validation Matrix
The current stable V2 firmware has been hardware-tested against the following matrix, successfully passing round-trip export/import/restore checks and regression tests:

- **1080 Snowboarding:** SRAM read/write/restore and full ROM dump.
- **GoldenEye 007:** EEPROM (4K) read/write/restore using `.eeprom` and `.ebay` references.
- **The Legend of Zelda: Ocarina of Time:** SRAM read/write/restore.
- **The Legend of Zelda: Majora's Mask:** FlashRAM read/write/restore.
- **Controller Pak / MPK:** Repeated export verified against a golden reference.
- **GameShark:** Export/import/restore verified against known stable images.
- **Regression Check:** FlashRAM-to-SRAM swap regression. The old corruption path does not reproduce after firmware fixes.

## Methodology
Validation is performed using the provided Python CLI tools (e.g., `tools/n64_validation_matrix.py` and `tools/n64_cart_verify.py`).
- Saves are exported and their SHA256 hashes are strictly compared against canonical golden references.
- Destructive tests (writes) are followed by readback verification and a final export hash match to guarantee stability.
