# Manufacturing Test Procedure

This procedure outlines the steps to verify a newly assembled N64 Pico Cart Reader V2 unit.

## Prerequisites
- A freshly soldered V2 board with a Raspberry Pi Pico installed.
- USB data cable compatible with the Pico installed on the board (typically Micro-USB for a standard Raspberry Pi Pico).
- Host computer with the V2 Python CLI tools installed.
- A known-good retail N64 cartridge (preferably one with SRAM, e.g., *1080 Snowboarding* or *Ocarina of Time*).
- An N64 Controller with a known-good Controller Pak (MPK).

## Step 1: Initial Firmware Flash
1. Hold the `BOOTSEL` button on the Pico while plugging in the USB cable to enter mass storage mode.
2. Flash the latest stable `pico_headless_demo.uf2` firmware.
3. Verify the device reboots and enumerates as a USB CDC serial device.

Typical serial port names:
- **Windows:** `COM5`
- **macOS:** `/dev/tty.usbmodemXXXX`
- **Linux:** `/dev/ttyACM0`

Run the commands below from the repository root. Replace `<PORT>` with the detected port. On Windows, use `python` or `py` instead of `python3` if needed.

## Step 2: Identification & Basic IO
1. Insert the known-good retail N64 cartridge.
2. Run the cart ID rescan:
   ```bash
   python3 host-tools/python/pico_pak_n64_magic_cli.py --port <PORT> n64-cart-id --rescan
   ```
3. **Pass Criteria:** The tool must successfully identify the cartridge header and report the correct ROM size and save type without I/O errors.

## Step 3: Save Media Test
1. Export the existing save:
   ```bash
   python3 host-tools/python/pico_pak_n64_magic_cli.py --port <PORT> n64-export --out mfg_test_save.bin
   # Or using PowerShell:
   # pwsh host-tools/powershell/Export-Save.ps1 -Port <PORT> -Out mfg_test_save.bin -Rescan
   ```
2. Verify the export completes successfully.
3. If a golden reference is available, run `host-tools/python/n64_cart_verify.py` for a write/readback verification cycle. Do not write test data to a customer or collector cartridge unless the original save has been backed up.

## Step 4: Controller Port Test
1. Remove the cartridge to avoid Joybus contention (if testing with an EEPROM cart).
2. Plug in the N64 Controller with the MPK.
3. Run the controller probe:
   ```bash
   python3 host-tools/python/pico_pak_n64_magic_cli.py --port <PORT> n64-controller-probe
   ```
4. **Pass Criteria:** The controller must be detected and report the presence of the memory pak.

## Step 5: Web Serial UI Check (Optional)
1. Open the Web Serial UI in a compatible browser.
2. Connect to the Pico.
3. Verify that the UI correctly populates the cartridge information.

If all steps pass, the hardware assembly is validated for core I/O functionality.
