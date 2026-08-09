# Arduino IDE Setup — TI-59 Zombie on ESP32-S3 DevKit N16R8

**Complete Arduino IDE configuration for compiling and flashing the TI-59 Zombie emulator on the ESP32-S3 DevKit N16R8 board (16 MB flash, 8 MB Octal PSRAM).**

Tested with: Arduino IDE 2.x · Espressif esp32 core · arduino-cli 1.5.1 / core esp32 3.3.11 (via CLI, same settings).

---

## 1. Install the ESP32 core

1. Open Arduino IDE → **File → Preferences** (macOS: *Arduino IDE → Settings*).
2. In **Additional boards manager URLs** add:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. **Tools → Board → Boards Manager** → search `esp32` → install **esp32 by Espressif Systems** (latest stable).

> Restart the IDE after installing the core so the board definitions load.

## 2. Board selection

**Tools → Board → esp32 → ESP32S3 Dev Module**

## 3. Board settings (essential!)

These values are **mandatory** for the current binary (uses the project's `partitions.csv` and the 3 MB `app0` partition):

| Setting | Value | Notes |
|---|---|---|
| Board | **ESP32S3 Dev Module** | also on DevKitC-1 boards |
| Upload Speed / USB CDC / JTAG Speed | **921600 / hwcdc** | via USB-Serial-JTAG |
| USB Mode | **Hardware CDC and JTAG** | console over USB |
| CPU Frequency | **240 MHz (WiFi)** | default |
| Flash Mode | **QIO 80MHz** | board default |
| Flash Size | **16M** | N16R8 = 16 MB flash (required, not 4M!) |
| Partition Scheme | **Custom** | reads `partitions.csv` from the sketch folder |
| PSRAM | **OPI PSRAM** | N16R8 = 8 MB Octal PSRAM |
| Core Debug Level | Info | suggested |
| Programmer | — | not needed |

> **FlashSize = 16M** and **PartitionScheme = Custom** are essential: the sketch exceeds the default partition limit (1.25 MB). With `partitions.csv` the app fits in `app0` (3 MB) and SPIFFS gets 1.5 MB.

## 4. Libraries to install (Library Manager)

The required **third-party** libraries (exact names in the Library Manager):

| Library | Author | Version | Used for |
|---|---|---|---|
| **MFRC522** | GithubCommunity | 1.4.x | NFC RFID reader (optional, MFRC522 module) |
| **NimBLE-Arduino** | h2zero | 1.4.x / 2.x | BLE thermal printer (optional, PC-100A) |

All other libraries used by the project **ship with the ESP32 core** (nothing to install):

`WiFi` · `WebServer` · `SPIFFS` · `DNSServer` · `Preferences` · `Wire` (I2C for HT16K33) · `SHA1Builder` · `FS` · `SPI` · `Arduino` · `freertos` (semphr.h, included in core)

> Both libraries are **optional**: the sketch detects at boot whether the related module is present and, without the library installed, the RFID/BLE flow simply stays inactive. If you don't use RFID or a printer, you can skip them.

## 5. Flashing the firmware

1. Open `ti59_zombie.ino` (the `src/` files live in the same folder → picked up automatically).
2. Select the **ESP32S3 Dev Module** board with the settings above.
3. **Select the Port**: with the devkit plugged via USB (USB-Serial-JTAG) pick the serial port that appears.
4. **Sketch → Upload / Verify** (Ctrl+R / Ctrl+U). The first build may take a few minutes.

> If the port doesn't show up: use a good USB data cable; on the DevKitC-1 the console stays on USB-Serial-JTAG, so do **not** touch GPIO19/20 (used by USB).

## 6. SPIFFS upload (`data/` folder)

The SPIFFS filesystem (overlay templates, `overlays.txt`, `overlay_pos.json`) must be uploaded **after** the firmware:

- **Arduino IDE 2.x**: install the **ESP32 Sketch Data Upload** plugin (e.g. `me-no-dev/arduino-esp32fs-plugin`), or:
- **ESP32FS / esptool**: `esptool --chip esp32s3 write_flash 0x310000 my.spiffs.bin`
  - offset = spiffs partition origin in `partitions.csv` (0x310000)
  - size = 0x180000 (1.5 MB)
- Alternatively upload via web: **`/manage`** page → SPIFFS file section.

## 7. First boot (WiFi)

On first boot the board opens a **WiFi captive portal** on the `/setup` page to enter credentials; they are saved in `/wifi.json` on SPIFFS (not in code). Web server base port: **80** (`WIFI_PORT` in `src/config.h`).

## Quick verification (checklist)

- [ ] `esp32` core installed (Boards Manager)
- [ ] Board = **ESP32S3 Dev Module**
- [ ] Flash Size = **16M** (8M on DevKitC-1 N8R8)
- [ ] Partition Scheme = **Custom**
- [ ] MFRC522 installed (only if you use RFID)
- [ ] NimBLE-Arduino installed (only if you use the BLE printer)
- [ ] Build succeeded, app within 3 MB
- [ ] `data/` uploaded to SPIFFS (1.5 MB)

---

## N16R8 vs N8R8

| Board | Flash | PSRAM | IDE Flash Size | Notes |
|---|---|---|---|---|
| DevKitC-1 **N8R8** | 8 MB | 8 MB Octal | **8MB** | the project with `partitions.csv` stays valid |
| DevKitC-1 **N16R8** | 16 MB | 8 MB Octal | **16MB** | **the project's real board: use 16M + Custom** |

The `app0` (3 MB) and `spiffs` (1.5 MB) partitions fit on both 8 MB and 16 MB; with **16M** there's room left for future expansions.

---

**Links**: see `documentation.txt` (English only), `README_EN.md`, `INSTRUCTIONS_EN.txt`, `docs/Connections_TI59_Zombie_EN.html` (wiring scheme).
