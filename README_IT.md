# TI-59 Zombie

**Emulatore della calcolatrice Texas Instruments TI-59 su ESP32-S3**

Riproduzione fedele della calcolatrice programmabile TI-59 (1977) basata sull'emulazione del microprocessore TMC0501, con interfaccia web integrata che funge anche da IDE. Firmware Arduino per ESP32-S3 (DevKitC-1 N16R8, 16 MB flash, 8 MB PSRAM Octal).

---

## Caratteristiche

- **Emulazione completa del core TMC0501**: DSCOM, BROM, RAM, stack subroutine a 8 livelli, registri dati BCD, flag utente, indicatori INV/2nd.
- **Pacing a tempo reale**: velocità identica al TI-59 originale (~7.100 istruzioni/s) con modalità "Old/New" regolabile dal web.
- **14 moduli libreria "Solid State Software"** con numerazione ufficiale (vedi sotto).
- **Schede magnetiche emulate** come file JSON in SPIFFS (lato A/B da 480 passi, registri), con microswitch card-sense.
- **Lettore NFC opzionale** (MFRC522 + tag NTAG213) che riproduce il flusso READ/WRITE del lettore originale, con espulsione motore.
- **Stampante termica BLE** (backend NimBLE) per l'emulazione della stampante PC-100A.
- **Display a 12 digit** pilotato da HT16K33 (16x8, I2C).
- **Web IDE integrato**: calcolatrice virtuale, gestione programmi/schede/moduli, editor overlay con anteprima, pannello posizioni SVG.
- **i18n italiano/inglese** selezionabile dalle pagine web.

## Hardware

| Componente | Note |
|---|---|
| ESP32-S3-DevKitC-1 N16R8 | 16 MB flash, 8 MB Octal PSRAM |
| Display HT16K33 16x8 | I2C, 12 digit, SDA=43 SCL=44, addr 0x70 |
| Tastiera 9x5 | matrice 14 pin: righe GPIO1-9, colonne GPIO10-14 |
| Schede magnetiche | microswitch GPIO15 (card-sense), LED GPIO16 |
| RFID MFRC522 (opzionale) | SPI condiviso, CS=21, RST=45, alimentazione MOSFET GPIO41 |
| Motore espulsione (opzionale) | GPIO42 |
| Stampante BLE (opzionale) | Bluetooth LE (NimBLE) |
| microSD | CS=40, bus SPI condiviso SCK=18 / MOSI=17 / MISO=39 |
| Alimentazione | LiPo 3.7V + modulo di ricarica + interruttore SPST |

**Attenzione GPIO**: sulla DevKitC-1 i pin 22-34 sono il bus interno di flash/PSRAM (non esposti); scriverci blocca la flash → WDT reset in loop al boot. Usare solo i pin esposti (0-21, 35-48).

## Architettura software

- `ti59_zombie.ino` — firmware, task FreeRTOS e inizializzazione.
- `src/tms1500.cpp/h` — emulatore CPU TMC0501 (esecuzione programmi, decodifica, registri).
- `src/library_module.cpp/h` — registro dei moduli libreria.
- `src/rom_XX.cpp/h` — ROM dei moduli (generate da `rom_import_validator.py`).
- `src/keyboard.cpp/h` — scan della matrice tastiera.
- `src/display.cpp/h` — driver HT16K33.
- `src/cardemu.cpp/h` — emulazione schede magnetiche su SPIFFS.
- `src/rfid_reader.cpp/h` — lettore NFC (MFRC522 + NTAG213).
- `src/printer.cpp/h`, `src/ble_thermal_printer.cpp/h` — stampante PC-100A e backend BLE.
- `src/wifilink*.cpp/h` — web server HTTP + IDE web embedded.
- `src/config.h` — pinout e costanti.
- `data/` — contenuti SPIFFS: template SVG degli overlay, `overlays.txt`, `overlay_pos.json`.

La CPU viene eseguita in un task a priorità dedicata con mutex (`g_cpuMutex`) per proteggere l'accesso concorrente da tastiera, WiFi e salvataggio periodico.

## Moduli libreria

| id | N. ufficiale | Modulo |
|---|---|---|
| ml1 | -1- | Master Library |
| st | -2- | Applied Statistics |
| re | -3- | Real Estate |
| sv | -4- | Surveying |
| na | -5- | Marine Navigation |
| av | -6- | Aviation |
| ll | -7- | Leisure Library |
| sa | -8- | Securities Analysis |
| ee | -9- | Electrical Engineering |
| fm | -10- | Farming |
| mu | -11- | Music |
| ph | -12- | Photography |
| rp | -13- | RPN |
| se | -14- | Structural Engineering |

Non ancora implementato: **-15- Math Utilities**.

## Pagine web

| URL | Pagina |
|---|---|
| `/` | IDE calcolatrice |
| `/manage` | Pannello di controllo |
| `/wolf` | Pannello regolatori (solo con god mode) |
| `/overlays` | Editor overlay ROM/schede |

### API principali

`/api/status`, `/api/regs`, `/api/modules`, `/api/modules/listing`, `/api/overlays`, `/api/card_positions`, `/api/card`, `/api/card/file`, `/api/program_card`, `/api/keypress`, `/api/prog`, `/api/reset`, `/api/timing`, `/api/eject`, `/api/rfid/*`, `/api/wifi/*`, `/api/fs`.

## Compilazione e caricamento

1. Apri `ti59_zombie.ino` in Arduino IDE (1.8.x o 2.x).
2. Installa il core **esp32** di Espressif (Board Manager) e seleziona la scheda **ESP32S3 Dev Module** (16 MB flash / 8 MB OPI PSRAM, via USB-Serial-JTAG).
3. Librerie richieste: **MFRC522** (solo RFID) e **NimBLE-Arduino** (solo stampante BLE). Le altre (WiFi, WebServer, SPIFFS, DNSServer, Preferences, Wire, SHA1Builder) sono incluse nel core.
4. Flash del firmware.
5. Carica la cartella `data/` sul file system SPIFFS (strumento "ESP32 Sketch Data Upload" o `esptool` sulla partizione SPIFFS).

Vedi il file `SETUP_ARDUINO_IDE_IT.md` per i settaggi completi della scheda (Flash Size 16M, Partition Scheme Custom, ecc.).

Al primo avvio la scheda apre un **captive portal WiFi** (pagina `/setup`) per inserire le credenziali di rete, salvate nel file `/wifi.json` su SPIFFS (caricabile con la cartella `data/`).

## Sicurezza

- Le credenziali WiFi vivono nel file **`/wifi.json`** su SPIFFS (niente hardcoded, niente NVS), scaricabile/modificabile/ricaricabile dal portale `/setup` (`GET`/`POST /api/wifi/file`). Nota: il file contiene le password in chiaro — appartiene al proprietario del device.
- Il pannello `/wolf` (god mode) si attiva solo se su SPIFFS esiste `/god_mode.txt`.

## Licenza

**GPL-3.0-or-later** — vedi `LICENSE`.

---

*Progetto personale. Copyright (C) 2026 Maurizio Petruccioli (MrYo).*
