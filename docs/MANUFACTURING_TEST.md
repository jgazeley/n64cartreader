# Manufacturing Test Procedure

This procedure outlines the steps to verify a newly assembled N64 Pico Cart Reader V2 unit.

## Prerequisites
- A freshly soldered V2 board with a Raspberry Pi Pico installed.
- USB-C cable.
- Host PC with the V2 Python CLI tools installed.
- A known-good retail N64 cartridge (preferably one with SRAM, e.g., *1080 Snowboarding* or *Ocarina of Time*).
- An N64 Controller with a known-good Controller Pak (MPK).

## Step 1: Initial Firmware Flash
1. Hold the `BOOTSEL` button on the Pico while plugging in the USB cable to enter mass storage mode.
2. Flash the latest stable `pico_headless_demo.uf2` firmware.
3. Verify the device reboots and enumerates as a USB CDC serial device (e.g., `/dev/ttyACMx` on Linux).

## Step 2: Identification & Basic IO
1. Insert the known-good retail N64 cartridge.
2. Run the cart ID rescan:
   ```bash
   python3 tools/pico_pak_n64_magic_cli.py --port /dev/ttyACMx n64-cart-id --rescan
   ```
3. **Pass Criteria:** The tool must successfully identify the cartridge header and report the correct ROM size and save type without I/O errors.

## Step 3: Save Media Round-Trip Test
1. Export the existing save:
   ```bash
   python3 tools/pico_pak_n64_magic_cli.py --port /dev/ttyACMx n64-export --out /tmp/mfg_test_save.bin
   ```
2. Verify the export completes successfully.
3. Use the verify tool (if a golden reference is available) or perform a write/readback cycle using `n64_cart_verify.py` to ensure the data lines to the save media are fully functional.

## Step 4: Controller Port Test
1. Remove the cartridge to avoid Joybus contention (if testing with an EEPROM cart).
2. Plug in the N64 Controller with the MPK.
3. Run the controller probe:
   ```bash
   python3 tools/pico_pak_n64_magic_cli.py --port /dev/ttyACMx n64-controller-probe
   ```
4. **Pass Criteria:** The controller must be detected and report the presence of the memory pak.

## Step 5: Web Serial UI Check (Optional)
1. Open the Web Serial UI in a compatible browser.
2. Connect to the Pico.
3. Verify that the UI correctly populates the cartridge information.

If all steps pass, the hardware assembly is validated for core I/O functionality.
