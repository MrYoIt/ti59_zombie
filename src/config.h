/*
 * TI-59 Zombie — emulatore TI-59 su ESP32-S3 (TMS1500) — TI-59 emulator on ESP32-S3 (TMS1500)
 * Copyright (C) 2026 Maurizio Petruccioli (MrYo)
 *
 * Questo programma è software libero: puoi ridistribuirlo e/o modificarlo — This program is free software: you can redistribute it and/or modify
 * nei termini della GNU General Public License pubblicata dalla — it under the terms of the GNU General Public License as published by
 * Free Software Foundation, versione 3 della Licenza, o (a tua scelta) — the Free Software Foundation, either version 3 of the License, or
 * qualsiasi versione successiva — (at your option) any later version.
 *
 * Questo programma è distribuito nella speranza che sia utile — This program is distributed in the hope that it will be useful,
 * ma SENZA ALCUNA GARANZIA; senza nemmeno la garanzia implicita di — but WITHOUT ANY WARRANTY; without even the implied warranty of
 * COMMERCIABILITÀ o IDONEITÀ A UNO SCOPO PARTICOLARE. Vedi la — MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License per maggiori dettagli — GNU General Public License for more details.
 *
 * Dovresti aver ricevuto una copia della GNU General Public License — You should have received a copy of the GNU General Public License
 * unitamente a questo programma. In caso contrario, vedi — along with this program.  If not, see <https://www.gnu.org/licenses/>. — testo licenza GNU GPL — GNU GPL license text
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
/*
 * config.h — TI-59 Zombie · Pinout e costanti globali — Pinout and global constants
 * Basato su schema elettrico Tavola 3 (Service Manual) — Based on the wiring diagram, Plate 3 (Service Manual)
 */

// ─── GPIO tastiera — keyboard GPIO ─────────────────────────
// Righe (D-lines) — OUTPUT, attivate LOW durante scan — Rows (D-lines) — OUTPUT, driven LOW during scan
#define PIN_D1   1
#define PIN_D2   2
#define PIN_D3   3
#define PIN_D4   4
#define PIN_D5   5
#define PIN_D6   6
#define PIN_D7   7
#define PIN_D8   8
#define PIN_D9   9

// Colonne (K-lines) — INPUT PULL-UP, LOW = tasto premuto — Columns (K-lines) — INPUT PULL-UP, LOW = key pressed
#define PIN_KD   10   // colonna 1 — column 1
#define PIN_KP   11   // colonna 2 (via CR5) — column 2 (via CR5)
#define PIN_KQ   12   // colonna 3 — column 3
#define PIN_KS   13   // colonna 4 — column 4
#define PIN_KT   14   // colonna 5 — column 5

// ─── GPIO display I2C (HT16K33, 16x8) — display I2C GPIO (HT16K33, 16x8) ─────
// Il display è passato da 2× MAX7219 (SPI) a un singolo HT16K33 (I2C): — The display moved from 2× MAX7219 (SPI) to a single HT16K33 (I2C):
// un solo chip 16 colonne × 8 righe copre i 12 digit (8 segmenti l'uno). — one chip of 16 columns × 8 rows covers the 12 digits (8 segments each).
// ATTENZIONE — modulo ESP32-S3-DevKitC-1 N8R8 (8MB PSRAM Octal), vedi — WARNING — ESP32-S3-DevKitC-1 N8R8 board (8MB Octal PSRAM), see
// datasheet WROOM-1 e guida DevKitC-1 (Espressif): — WROOM-1 datasheet and DevKitC-1 guide (Espressif):
//   - GPIO19/20 sono USB D-/D+ (USB-Serial-JTAG, la console): NON usarli — GPIO19/20 are USB D-/D+ (USB-Serial-JTAG, the console): do NOT use them
//     come GPIO, "scolleghi" la USB e perdi console/flashing. — as GPIO, you "unplug" USB and lose console/flashing.
//   - GPIO22-34 NON sono esposti sulla DevKitC-1 (bus interno di flash e — GPIO22-34 are NOT exposed on the DevKitC-1 (internal flash and
//     PSRAM): scriverci blocca la flash -> WDT reset in loop al boot. — PSRAM bus): writing to them blocks flash -> WDT reset loop at boot.
//   - GPIO35-37: PSRAM Octal (non usabili). GPIO38: LED RGB (v1.1). — GPIO35-37: Octal PSRAM (unusable). GPIO38: RGB LED (v1.1).
// Quindi SDA/SCL stanno su GPIO43/44 (piedini U0TXD/U0RXD di J3, liberi: — So SDA/SCL go on GPIO43/44 (J3 U0TXD/U0RXD pins, free:
// la console è su USB, non su UART0). I2C richiede pull-up 4.7k su — the console is on USB, not UART0). I2C requires 4.7k pull-ups on
// entrambi i fili (o modulo con pull-up integrati). — both wires (or a module with built-in pull-ups).
#define PIN_I2C_SDA     43
#define PIN_I2C_SCL     44
#define HT16K33_I2C_ADDR 0x70   // indirizzo I2C del driver (AD0/AD1 a massa) — I2C address of the driver (AD0/AD1 to ground)

// ─── GPIO microSD (bus SPI condiviso: SCK=18 / MOSI=17) — microSD GPIO (shared SPI bus: SCK=18 / MOSI=17) ─
// L'HT16K33 è I2C (non usa SPI); sul bus restano solo SD + (opzionale) — The HT16K33 is I2C (no SPI); only the SD + (optional)
// RFID MFRC522. Ogni periferica ha il proprio CS dedicato. MOSI/SCK — RFID MFRC522 remain on the bus. Each peripheral has its own dedicated CS. MOSI/SCK
// restano definiti per documentare il bus condiviso (SPIFFS non li usa). — stay defined to document the shared bus (SPIFFS does not use them).
#define PIN_SPI_MOSI  17
#define PIN_SPI_CLK   18
#define PIN_SPI_MISO  39
#define PIN_SD_CS     40

// ─── GPIO RFID MFRC522 (opzionale, NFC NTAG213 = schede magnetiche) — RFID MFRC522 GPIO (optional, NFC NTAG213 = magnetic cards) ─
// Bus SPI condiviso con la microSD (SCK=18 / MOSI=17 / MISO=39), CS e — SPI bus shared with the microSD (SCK=18 / MOSI=17 / MISO=39), dedicated
// RST dedicati. L'antenna del modulo (25x35mm) va incollata all'interno — CS and RST. The module antenna (25x35mm) must be glued inside
// dello chassis, rivolta verso l'esterno (distanza di lettura NTAG213 — the chassis, facing outward (NTAG213 read range
// ~2-4 cm), lato opposto all'ingresso scheda. — ~2-4 cm), on the side opposite the card slot.
//
// ALIMENTAZIONE A RIPOSO: il modulo resta SPENTO (consumo ~zero); — POWER AT REST: the module stays OFF (~zero consumption);
// PIN_RFID_PWR pilota un MOSFET (N-channel, low-side) o un modulo — PIN_RFID_PWR drives a MOSFET (N-channel, low-side) or a
// relè/MOSFET che taglia il 3.3V del lettore: HIGH = alimentato. Il — relay/MOSFET module that cuts the reader's 3.3V: HIGH = powered. The
// firmware lo accende solo quando il microswitch di inserimento — firmware powers it only when the insertion microswitch
// (PIN_CARD_SENSE, destra) si chiude, e lo rispegne a lettura finita. — (PIN_CARD_SENSE, right) closes, and turns it off when the read ends.
//
// ESPULSIONE: PIN_CARD_MOTOR pilota un piccolo motore DC (transistor — EJECTION: PIN_CARD_MOTOR drives a small DC motor (NPN/MOSFET
// NPN/MOSFET + diodo flyback) che espelle la scheda dal lato opposto — transistor + flyback diode) that ejects the card from the opposite
// (sinistra) per ~RFD_EJECT_MS a fine operazione. — side (left) for ~RFD_EJECT_MS at the end of the operation.
//
// FLUSSO (come il lettore originale): — FLOW (like the original reader):
//   - READ: inserisci la scheda a destra -> si legge lo slot dal tag — READ: insert the card on the right -> the slot is read from the tag
//     (pagina 4) o, se assente/invalido, dalla mappa UID -> il — (page 4) or, if missing/invalid, from the UID map -> the
//     programma viene caricato -> il motore espelle la scheda a sinistra. — program is loaded -> the motor ejects the card on the left.
//   - WRITE: premi il tasto WRITE della TI-59 (o /api/rfid/arm dal — WRITE: press the TI-59 WRITE key (or /api/rfid/arm from the
//     web) -> la TI-59 "aspetta" la scheda -> inseriscila: salva il — web) -> the TI-59 "waits" for the card -> insert it: saves the
//     programma corrente in uno slot, scrive lo slot nel tag e in — current program into a slot, writes the slot to the tag and
//     /rfid_map.json -> espelle. — to /rfid_map.json -> ejects.
//
// RICHIEDE la libreria Arduino "MFRC522" (Library Manager, autori — REQUIRES the Arduino "MFRC522" library (Library Manager, GitHubCommunity
// GitHubCommunity) per lo strato ISO14443A; lettura/scrittura delle — authors) for the ISO14443A layer; read/write of the
// pagine NTAG213 fatta qui in raw. Senza libreria o senza modulo — NTAG213 pages done here in raw. Without the library or without a
// collegato (auto-detect al boot), il vecchio flusso a microswitch — module attached (auto-detect at boot), the old microswitch flow
// resta attivo. — stays active.
//
// PIN NOTE: GPIO26/27 erano qui prima e causavano un WDT reset in loop — PIN NOTE: GPIO26/27 were here before and caused a WDT reset loop
// al boot: sulla DevKitC-1 N8R8 i GPIO22-34 sono il bus interno di — at boot: on the DevKitC-1 N8R8 GPIO22-34 are the internal
// flash/PSRAM (non esposti) e scriverci sopra blocca la flash. Usare — flash/PSRAM bus (not exposed) and writing to them blocks flash. Use
// SOLO i pin esposti: 0-21 e 35-48 (35-37 riservati PSRAM, 38 = LED RGB — ONLY the exposed pins: 0-21 and 35-48 (35-37 reserved PSRAM, 38 = RGB LED
// su rev v1.1, 39/40 JTAG->GPIO ok, 41/42 = MTDI/MTMS liberi, 43/44 = — on rev v1.1, 39/40 JTAG->GPIO ok, 41/42 = MTDI/MTMS free, 43/44 =
// U0TXD/U0RXD liberi perché la console è su USB, 45/46 strapping ok). — U0TXD/U0RXD free because the console is on USB, 45/46 strapping ok).
#define PIN_RFID_CS     21
#define PIN_RFID_RST    45   // v1.1: GPIO38 = LED RGB -> RST su GPIO45 — v1.1: GPIO38 = RGB LED -> RST on GPIO45
#define PIN_RFID_PWR    41   // MOSFET alimentazione modulo (HIGH = ON) — module power MOSFET (HIGH = ON)
#define PIN_CARD_MOTOR  42   // motore espulsione scheda (HIGH = ON) — card eject motor (HIGH = ON)
#define RFD_TAG_SLOT_PAGE   4    // pagina NTAG213 con lo slot (3 cifre ASCII) — NTAG213 page holding the slot (3 ASCII digits)
#define RFD_EJECT_MS        500  // durata espulsione motore — motor eject duration
#define RFD_PWR_SETTLE_MS   100  // attesa dopo accensione modulo — settling delay after powering the module
#define RFD_SCAN_ATTEMPTS   12   // tentativi di rilevamento tag (~1.5s max) — tag detection attempts (~1.5s max)
#define RFD_MAP_PATH        "/rfid_map.json"
#define RFD_MAP_MAX         50   // associazioni UID->slot in RAM — UID->slot mappings in RAM

// ─── GPIO misc — misc GPIO ─────────────────────────────────
#define PIN_CARD_SENSE  15   // microswitch card reader (INPUT PULL-UP) — card reader microswitch (INPUT PULL-UP)
#define PIN_LED_STATUS  16   // LED stato (OUTPUT) — status LED (OUTPUT)

// ─── Alimentazione (LiPo 3.7V) — power supply (LiPo 3.7V) ──
// LiPo 3.7V → modulo di ricarica (solo poli B+/B- usati in entrata) → — LiPo 3.7V → charging module (only B+/B- terminals used at input) →
// interruttore fisico di accensione/spegnimento (SPST in serie sul — physical on/off switch (SPST in series on the
// positivo) → pin 5V del DevKitC-1 (il regolatore onboard 3.3V accetta — positive rail) → DevKitC-1 5V pin (the onboard 3.3V regulator accepts
// 4.2-3.0V in ingresso e fornisce il 3.3V a ESP32 + display + RFID). — 4.2-3.0V input and provides 3.3V to ESP32 + display + RFID).
// Il modulo di carica non alimenta il circuito quando è sotto carica: — The charging module does not power the circuit while charging:
// il diodo interno impedisce il passaggio, quindi senza batteria il — the internal diode blocks the path, so without a battery the
// device non si accende nemmeno con il caricatore collegato. — device won't turn on even with the charger connected.

// ─── Costanti CPU — CPU constants ──────────────────────────
// Clock originale: 227.5 kHz / 2 fasi / 16 stati = ~7.1 kHz cicli completi — Original clock: 227.5 kHz / 2 phases / 16 states = ~7.1 kHz full cycles
// In emulazione a task 1ms: eseguiamo ~228 cicli/ms — In 1ms-task emulation: we run ~228 cycles/ms
#define CPU_CYCLES_PER_TICK   228

// Istruzioni al secondo del TI-59 originale (~7.1 kHz, vedi sopra). È — Instructions per second of the original TI-59 (~7.1 kHz, see above). It is
// l'ancora del pacing a tempo reale e del controllo "Velocità Old": — the anchor for real-time pacing and the "Old Speed" control:
// 100% nel pannello impostazioni = stessa velocità del TI-59 reale. — 100% in the settings panel = same speed as the real TI-59.
#define TI59_INSTR_PER_SEC     7100

// ─── Memoria TI-59 — TI-59 memory ──────────────────────────
#define TI59_ROM_WORDS        5124   // DSCOM×2(2500+2500) + BROM(1024) parole 13bit — DSCOM×2(2500+2500) + BROM(1024) 13-bit words
#define TI59_RAM_REGS         100    // 100 registri dati utente (30 per 598×4 - overlap) — 100 user data registers (30 per 598×4 - overlap)
#define TI59_PROG_STEPS       960    // 480 passi per lato scheda ×2 (4 chip 598) — 480 steps per card side ×2 (4× 598 chips)
#define TI59_STACK_DEPTH      8      // stack di ritorno subroutine — subroutine return stack

// ─── Dimensioni display — display dimensions ────────────────
#define DISPLAY_DIGITS        12     // digit fisici (D1-D12) — physical digits (D1-D12)
#define DISPLAY_SEGS          8      // segmenti per digit (A-G + DP) — segments per digit (A-G + DP)

// ─── Indicatore di calcolo "[" (digit 12, vedi display.h) — calculation indicator "[" (digit 12, see display.h) ─
// Il timing (durata minima visibile + periodo di lampeggio) è ora — The timing (minimum visible duration + blink period) is now
// configurabile a runtime dal portale web e persistito su SPIFFS: — runtime-configurable from the web portal and persisted on SPIFFS:
// vedi settings.h (SystemSettings.calc_min_show_ms/calc_blink_ms) e — see settings.h (SystemSettings.calc_min_show_ms/calc_blink_ms) and
// i relativi SETTINGS_DEFAULT_*. Non più #define qui perché un — the related SETTINGS_DEFAULT_*. No longer #defined here because a
// valore fisso a compile-time non sarebbe modificabile dall'utente. — fixed compile-time value could not be changed by the user.

// ─── WiFi — WiFi ───────────────────────────────────────────
// Modificare con le proprie credenziali o usare WiFiManager — Edit with your own credentials or use WiFiManager
#define WIFI_SSID     "REPLACE_ME"
#define WIFI_PASS     "REPLACE_ME"
#define WIFI_PORT     80

// ─── Card emulation — card emulation ───────────────────────
// NOTA SICUREZZA: CARD_SLOT_COUNT e CARD_PROG_BYTES sono definiti — SECURITY NOTE: CARD_SLOT_COUNT and CARD_PROG_BYTES are defined
// UNA SOLA VOLTA in cardemu.h. Qui c'era una ridefinizione con valori — ONLY ONCE in cardemu.h. Here there used to be a redefinition with different
// diversi (240 invece di 480): il preprocessore "vince" con l'ultima — values (240 instead of 480): the preprocessor "wins" with the last
// define vista, quindi in wifilink.cpp (che include cardemu.h e POI — define seen, so in wifilink.cpp (which includes cardemu.h and THEN
// config.h) i buffer venivano dimensionati con 240 mentre i loop — config.h) the buffers were sized with 240 while the loops
// scrivevano fino a prog_len_a reale (fino a 480) → stack buffer — wrote up to the real prog_len_a (up to 480) → stack buffer
// overflow raggiungibile da remoto via GET /api/card?slot=N su una — overflow reachable remotely via GET /api/card?slot=N on a
// scheda con programma lungo. Non ridefinire queste costanti qui: — card with a long program. Do not redefine these constants here:
// usare sempre CARD_SLOT_COUNT / CARD_PROG_BYTES da cardemu.h. — always use CARD_SLOT_COUNT / CARD_PROG_BYTES from cardemu.h.