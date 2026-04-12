# Web Serial UI

The N64 Pico Cart Reader V2 includes a browser-based interface that communicates with the device over USB using the Web Serial API. No drivers or software installation required.

## Requirements

- **Browser:** Google Chrome or Microsoft Edge (desktop). Firefox and Safari do not support Web Serial.
- **Connection:** The Pico must be flashed with the headless firmware and connected via USB.
- **No other serial connections:** Close any terminal emulator or serial monitor that might have the port open.

## Opening the UI

Open `host-tools/web/index.html` directly in your browser. It runs entirely client-side — no server or internet connection needed.

## Connecting

1. Click **Connect**.
2. A browser dialog lists available serial ports. Select the Pico (usually the only CDC device listed).
3. The status indicator turns green and the firmware version is displayed.

## Loading the Cart Catalog

The UI can automatically identify cartridges using the bundled ROM catalog:

1. Open the Web UI; it loads the bundled catalog from `host-tools/web/n64_catalog.js`.
2. To override it later, drop the source catalog at `host-tools/n64.txt` onto the catalog drop zone.
3. Once loaded, the catalog persists for the session.

With the catalog loaded, rescanning a cartridge reports the game title, region, save type, and ROM size.

## GamePak Operations

After connecting and inserting a cartridge:

- **Rescan** — re-detects the cartridge and updates game info.
- **Export Save** — reads the cartridge save (SRAM, EEPROM, or FlashRAM) and downloads it as a binary file.
- **Import Save** — writes a save file back to the cartridge. Select a file from your computer when prompted.
- **Dump ROM** — reads the full ROM and downloads it as a `.z64` file.
- **ROM Header** — displays the first 64 bytes of the ROM header.

## GameShark Operations

With a GameShark Pro cartridge inserted:

- **GameShark Info** — reads the GameShark header and flash ID.
- **Export GameShark** — dumps the full GameShark flash contents.
- **Import GameShark** — writes a GameShark image back to the cartridge.

## Controller Operations

With an N64 controller plugged into the reader's controller port:

- **Controller Probe** — detects the controller and reports whether a Controller Pak (MPK) is present.
- **Export MPK** — reads the Controller Pak contents and downloads them.
- **Import MPK** — writes a Controller Pak image back.

Note: EEPROM cartridges and controllers share the same Joybus data line. Unplug one to use the other without bus contention.

## Troubleshooting

**"Web Serial API not available":**
You need Chrome or Edge. Other browsers do not support this API.

**Port not listed in the browser dialog:**
Make sure no other application has the serial port open. On Linux, you may need to add your user to the `dialout` group:
```bash
sudo usermod -aG dialout $USER
```
Log out and back in for the change to take effect.

**Connection drops during large transfers:**
Check your USB cable. Some cables are charge-only and do not support data. Try a different cable or a shorter one.
