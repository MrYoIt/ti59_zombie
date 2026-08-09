# Setup Arduino IDE — TI-59 Zombie su ESP32-S3 DevKit N16R8

**Configurazione completa dell'Arduino IDE per compilare e caricare l'emulatore TI-59 Zombie sulla scheda ESP32-S3 DevKit N16R8 (16 MB flash, 8 MB PSRAM Octal).**

Testato con: Arduino IDE 2.x · core esp32 di Espressif · arduino-cli 1.5.1 / core esp32 3.3.11 (via CLI, stessi settaggi).

---

## 1. Installare il core ESP32

1. Apri Arduino IDE → **File → Preferenze** (macOS: *Arduino IDE → Settings*).
2. In **Additional boards manager URLs** aggiungi:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. **Tools → Board → Boards Manager** → cerca `esp32` → installa **esp32 by Espressif Systems** (ultima versione stabile).

> Dopo l'installazione del core riavvia l'IDE perché lo schema delle schede venga caricato.

## 2. Selezione scheda

**Tools → Board → esp32 → ESP32S3 Dev Module**

## 3. Settaggi della scheda (fondamentali!)

Questi valori sono **obbligatori** per il binario attuale (usa `partitions.csv` del progetto e la partition `app0` da 3 MB):

| Impostazione | Valore | Nota |
|---|---|---|
| Board | **ESP32S3 Dev Module** | anche su scheda DevKitC-1 |
| Upload Speed / USB CDC / JTAG Speed | **921600 / hwcdc** | via USB-Serial-JTAG |
| USB Mode | **Hardware CDC and JTAG** | console su USB |
| CPU Frequency | **240 MHz (WiFi)** | default |
| Flash Mode | **QIO 80MHz** | default della scheda |
| Flash Size | **16M** | N16R8 = 16 MB flash (obbligatorio, non 4M!) |
| Partition Scheme | **Custom** | legge `partitions.csv` nella cartella dello sketch |
| PSRAM | **OPI PSRAM** | N16R8 = 8 MB Octal PSRAM |
| Core Debug Level | Info | suggerito |
| Programmer | — | non serve |

> **FlashSize = 16M** e **PartitionScheme = Custom** sono essenziali: lo sketch supera il limite della partition di default (1.25 MB). Con `partitions.csv` l'app entra in `app0` (3 MB) e SPIFFS ha 1.5 MB.

## 4. Librerie da installare (Library Manager)

Le librerie **terze parti** richieste (i nomi esatti nel Library Manager):

| Libreria | Autore | Versione | Serve per |
|---|---|---|---|
| **MFRC522** | GithubCommunity | 1.4.x | Lettore RFID NFC (opzionale, modulo MFRC522) |
| **NimBLE-Arduino** | h2zero | 1.4.x / 2.x | Stampante termica BLE (opzionale, PC-100A) |

Tutte le altre librerie usate dal progetto **sono incluse nel core ESP32** (niente da installare):

`WiFi` · `WebServer` · `SPIFFS` · `DNSServer` · `Preferences` · `Wire` (I2C per HT16K33) · `SHA1Builder` · `FS` · `SPI` · `Arduino` · `freertos` (semphr.h, incluso in core)

> Le due librerie sono **opzionali**: lo sketch rileva al boot se il relativo modulo è presente e, senza libreria installata, il flusso RFID/BLE resta semplicemente inattivo. Se NON usi RFID o stampante, puoi anche non installarle.

## 5. Caricamento del firmware

1. Apri `ti59_zombie.ino` (dalla cartella del progetto, i `src/` accanto sono nella stessa cartella → rilevati automaticamente).
2. Seleziona la scheda **ESP32S3 Dev Module** coi settaggi della tabella.
3. **Seleziona la Porta**: col devkit collegato in USB (via USB-Serial-JTAG) scegli la porta seriale che appare.
4. **Sketch → Carica / Verifica** (Ctrl+R / Ctrl+U). La prima compilazione può durare qualche minuto.

> Se la porta non appare: usa un cavo dati USB di qualità, e su DevKitC-1 la console resta su USB-Serial-JTAG, quindi **non** toccare GPIO19/20 (li usa la USB).

## 6. Upload SPIFFS (cartella `data/`)

Il file system SPIFFS (template overlay, `overlays.txt`, `overlay_pos.json`) va caricato **dopo** il firmware:

- **Arduino IDE 2.x**: installa il **plugin ESP32 Sketch Data Upload** (es. `me-no-dev/arduino-esp32fs-plugin`), oppure:
- **ESP32FS / esptool**: `esptool --chip esp32s3 write_flash 0x310000 my.spiffs.bin`
  - offset = origine della partition `spiffs` in `partitions.csv` (0x310000)
  - size = 0x180000 (1.5 MB)
- In alternativa carica via web: pagina **`/manage`** → sezione file SPIFFS del device.

## 7. Primo avvio (WiFi)

Al primo boot la scheda apre un **captive portal WiFi** sulla pagina `/setup` per inserire le credenziali; vengono salvate in `/wifi.json` su SPIFFS (non nel codice). Porta base del web server: **80** (`WIFI_PORT` in `src/config.h`).

## Verifica rapida (checklist)

- [ ] Core `esp32` installato (Boards Manager)
- [ ] Scheda = **ESP32S3 Dev Module**
- [ ] Flash Size = **16M** (8M se DevKitC-1 N8R8)
- [ ] Partition Scheme = **Custom**
- [ ] MFRC522 installata (solo se usi RFID)
- [ ] NimBLE-Arduino installata (solo se usi la stampante BLE)
- [ ] Compilazione riuscita, app dentro 3 MB
- [ ] `data/` caricato su SPIFFS (1.5 MB)

---

## Differenze tra N16R8 e N8R8

| Scheda | Flash | PSRAM | Flash Size in IDE | Note |
|---|---|---|---|---|
| DevKitC-1 **N8R8** | 8 MB | 8 MB Octal | **8MB** | il progetto con `partitions.csv` resta valido |
| DevKitC-1 **N16R8** | 16 MB | 8 MB Octal | **16MB** | **scheda reale del progetto: usare 16M + Custom** |

La partition `app0` (3 MB) e `spiffs` (1.5 MB) entrano sia su 8 MB che su 16 MB; con **16M** resta spazio libero per future espansioni.

---

**Collegamenti**: vedi `documentazione.txt` (solo italiano), `README_IT.md`, `ISTRUZIONI_IT.txt`, `docs/Collegamenti_TI59_Zombie_IT.html` (schema collegamenti).
