/*
 * TI-59 Zombie — emulatore TI-59 su ESP32-S3 (TMS1500)
 * Copyright (C) 2026 Maurizio Petruccioli (MrYo)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
/*
 * config.h — TI-59 Zombie · Pinout e costanti globali
 * Basato su schema elettrico Tavola 3 (Service Manual)
 */

// ─── GPIO tastiera ────────────────────────────────────────
// Righe (D-lines) — OUTPUT, attivate LOW durante scan
#define PIN_D1   1
#define PIN_D2   2
#define PIN_D3   3
#define PIN_D4   4
#define PIN_D5   5
#define PIN_D6   6
#define PIN_D7   7
#define PIN_D8   8
#define PIN_D9   9

// Colonne (K-lines) — INPUT PULL-UP, LOW = tasto premuto
#define PIN_KD   10   // colonna 1
#define PIN_KP   11   // colonna 2 (via CR5)
#define PIN_KQ   12   // colonna 3
#define PIN_KS   13   // colonna 4
#define PIN_KT   14   // colonna 5

// ─── GPIO display I2C (HT16K33, 16x8) ─────────────────────
// Il display è passato da 2× MAX7219 (SPI) a un singolo HT16K33 (I2C):
// un solo chip 16 colonne × 8 righe copre i 12 digit (8 segmenti l'uno).
// ATTENZIONE — modulo ESP32-S3-DevKitC-1 N8R8 (8MB PSRAM Octal), vedi
// datasheet WROOM-1 e guida DevKitC-1 (Espressif):
//   - GPIO19/20 sono USB D-/D+ (USB-Serial-JTAG, la console): NON usarli
//     come GPIO, "scolleghi" la USB e perdi console/flashing.
//   - GPIO22-34 NON sono esposti sulla DevKitC-1 (bus interno di flash e
//     PSRAM): scriverci blocca la flash -> WDT reset in loop al boot.
//   - GPIO35-37: PSRAM Octal (non usabili). GPIO38: LED RGB (v1.1).
// Quindi SDA/SCL stanno su GPIO43/44 (piedini U0TXD/U0RXD di J3, liberi:
// la console è su USB, non su UART0). I2C richiede pull-up 4.7k su
// entrambi i fili (o modulo con pull-up integrati).
#define PIN_I2C_SDA     43
#define PIN_I2C_SCL     44
#define HT16K33_I2C_ADDR 0x70   // indirizzo I2C del driver (AD0/AD1 a massa)

// ─── GPIO microSD (bus SPI condiviso: SCK=18 / MOSI=17) ──
// L'HT16K33 è I2C (non usa SPI); sul bus restano solo SD + (opzionale)
// RFID MFRC522. Ogni periferica ha il proprio CS dedicato. MOSI/SCK
// restano definiti per documentare il bus condiviso (SPIFFS non li usa).
#define PIN_SPI_MOSI  17
#define PIN_SPI_CLK   18
#define PIN_SPI_MISO  39
#define PIN_SD_CS     40

// ─── GPIO RFID MFRC522 (opzionale, NFC NTAG213 = schede magnetiche) ──
// Bus SPI condiviso con la microSD (SCK=18 / MOSI=17 / MISO=39), CS e
// RST dedicati. L'antenna del modulo (25x35mm) va incollata all'interno
// dello chassis, rivolta verso l'esterno (distanza di lettura NTAG213
// ~2-4 cm), lato opposto all'ingresso scheda.
//
// ALIMENTAZIONE A RIPOSO: il modulo resta SPENTO (consumo ~zero);
// PIN_RFID_PWR pilota un MOSFET (N-channel, low-side) o un modulo
// relè/MOSFET che taglia il 3.3V del lettore: HIGH = alimentato. Il
// firmware lo accende solo quando il microswitch di inserimento
// (PIN_CARD_SENSE, destra) si chiude, e lo rispegne a lettura finita.
//
// ESPULSIONE: PIN_CARD_MOTOR pilota un piccolo motore DC (transistor
// NPN/MOSFET + diodo flyback) che espelle la scheda dal lato opposto
// (sinistra) per ~RFD_EJECT_MS a fine operazione.
//
// FLUSSO (come il lettore originale):
//   - READ: inserisci la scheda a destra -> si legge lo slot dal tag
//     (pagina 4) o, se assente/invalido, dalla mappa UID -> il
//     programma viene caricato -> il motore espelle la scheda a sinistra.
//   - WRITE: premi il tasto WRITE della TI-59 (o /api/rfid/arm dal
//     web) -> la TI-59 "aspetta" la scheda -> inseriscila: salva il
//     programma corrente in uno slot, scrive lo slot nel tag e in
//     /rfid_map.json -> espelle.
//
// RICHIEDE la libreria Arduino "MFRC522" (Library Manager, autori
// GitHubCommunity) per lo strato ISO14443A; lettura/scrittura delle
// pagine NTAG213 fatta qui in raw. Senza libreria o senza modulo
// collegato (auto-detect al boot), il vecchio flusso a microswitch
// resta attivo.
//
// PIN NOTE: GPIO26/27 erano qui prima e causavano un WDT reset in loop
// al boot: sulla DevKitC-1 N8R8 i GPIO22-34 sono il bus interno di
// flash/PSRAM (non esposti) e scriverci sopra blocca la flash. Usare
// SOLO i pin esposti: 0-21 e 35-48 (35-37 riservati PSRAM, 38 = LED RGB
// su rev v1.1, 39/40 JTAG->GPIO ok, 41/42 = MTDI/MTMS liberi, 43/44 =
// U0TXD/U0RXD liberi perché la console è su USB, 45/46 strapping ok).
#define PIN_RFID_CS     21
#define PIN_RFID_RST    45   // v1.1: GPIO38 = LED RGB -> RST su GPIO45
#define PIN_RFID_PWR    41   // MOSFET alimentazione modulo (HIGH = ON)
#define PIN_CARD_MOTOR  42   // motore espulsione scheda (HIGH = ON)
#define RFD_TAG_SLOT_PAGE   4    // pagina NTAG213 con lo slot (3 cifre ASCII)
#define RFD_EJECT_MS        500  // durata espulsione motore
#define RFD_PWR_SETTLE_MS   100  // attesa dopo accensione modulo
#define RFD_SCAN_ATTEMPTS   12   // tentativi di rilevamento tag (~1.5s max)
#define RFD_MAP_PATH        "/rfid_map.json"
#define RFD_MAP_MAX         50   // associazioni UID->slot in RAM

// ─── GPIO misc ────────────────────────────────────────────
#define PIN_CARD_SENSE  15   // microswitch card reader (INPUT PULL-UP)
#define PIN_LED_STATUS  16   // LED stato (OUTPUT)

// ─── Alimentazione (LiPo 3.7V) ────────────────────────────
// LiPo 3.7V → modulo di ricarica (solo poli B+/B- usati in entrata) →
// interruttore fisico di accensione/spegnimento (SPST in serie sul
// positivo) → pin 5V del DevKitC-1 (il regolatore onboard 3.3V accetta
// 4.2-3.0V in ingresso e fornisce il 3.3V a ESP32 + display + RFID).
// Il modulo di carica non alimenta il circuito quando è sotto carica:
// il diodo interno impedisce il passaggio, quindi senza batteria il
// device non si accende nemmeno con il caricatore collegato.

// ─── Costanti CPU ─────────────────────────────────────────
// Clock originale: 227.5 kHz / 2 fasi / 16 stati = ~7.1 kHz cicli completi
// In emulazione a task 1ms: eseguiamo ~228 cicli/ms
#define CPU_CYCLES_PER_TICK   228

// Istruzioni al secondo del TI-59 originale (~7.1 kHz, vedi sopra). È
// l'ancora del pacing a tempo reale e del controllo "Velocità Old":
// 100% nel pannello impostazioni = stessa velocità del TI-59 reale.
#define TI59_INSTR_PER_SEC     7100

// ─── Memoria TI-59 ────────────────────────────────────────
#define TI59_ROM_WORDS        5124   // DSCOM×2(2500+2500) + BROM(1024) parole 13bit
#define TI59_RAM_REGS         100    // 100 registri dati utente (30 per 598×4 - overlap)
#define TI59_PROG_STEPS       960    // 480 passi per lato scheda ×2 (4 chip 598)
#define TI59_STACK_DEPTH      8      // stack di ritorno subroutine

// ─── Dimensioni display ────────────────────────────────────
#define DISPLAY_DIGITS        12     // digit fisici (D1-D12)
#define DISPLAY_SEGS          8      // segmenti per digit (A-G + DP)

// ─── Indicatore di calcolo "[" (digit 12, vedi display.h) ──
// Il timing (durata minima visibile + periodo di lampeggio) è ora
// configurabile a runtime dal portale web e persistito su SPIFFS:
// vedi settings.h (SystemSettings.calc_min_show_ms/calc_blink_ms) e
// i relativi SETTINGS_DEFAULT_*. Non più #define qui perché un
// valore fisso a compile-time non sarebbe modificabile dall'utente.

// ─── WiFi ─────────────────────────────────────────────────
// Modificare con le proprie credenziali o usare WiFiManager
#define WIFI_SSID     "REPLACE_ME"
#define WIFI_PASS     "REPLACE_ME"
#define WIFI_PORT     80

// ─── Card emulation ───────────────────────────────────────
// NOTA SICUREZZA: CARD_SLOT_COUNT e CARD_PROG_BYTES sono definiti
// UNA SOLA VOLTA in cardemu.h. Qui c'era una ridefinizione con valori
// diversi (240 invece di 480): il preprocessore "vince" con l'ultima
// define vista, quindi in wifilink.cpp (che include cardemu.h e POI
// config.h) i buffer venivano dimensionati con 240 mentre i loop
// scrivevano fino a prog_len_a reale (fino a 480) → stack buffer
// overflow raggiungibile da remoto via GET /api/card?slot=N su una
// scheda con programma lungo. Non ridefinire queste costanti qui:
// usare sempre CARD_SLOT_COUNT / CARD_PROG_BYTES da cardemu.h.