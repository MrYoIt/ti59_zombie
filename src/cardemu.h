/*
 * TI-59 Zombie — emulatore TI-59 su ESP32-S3 (TMS1500) — TI-59 emulator on the ESP32-S3 (TMS1500)
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
 * cardemu.h — Emulazione lettore schede magnetiche TI-59 — Emulation of the TI-59 magnetic card reader
 *
 * Le schede originali memorizzano 480 passi programma (lato A) + — Original cards store 480 program steps (side A) +
 * 480 passi (lato B) su 4 tracce magnetiche a 2.3 IPS. — 480 steps (side B) on 4 magnetic tracks at 2.3 IPS.
 *
 * Nell'emulazione: ogni scheda è un file JSON in SPIFFS — In the emulation: each card is a JSON file in SPIFFS
 * Formato: {"name":"...","prog_a":"...","prog_b":"...","regs":"..."} — Format: {"name":"...","prog_a":"...","prog_b":"...","regs":"..."}
 *
 * Il microswitch card sense (GPIO15) simula l'inserimento. — The card-sense microswitch (GPIO15) simulates insertion.
 * Tenere premuto e selezionare la scheda via WiFi, — Hold it down and select the card via WiFi,
 * oppure usare un selettore fisico rotativo (opzionale). — or use a physical rotary selector (optional).
 */
#include <stdint.h>
#include <stdbool.h>
#include "tms1500.h"
#define CARD_SLOT_COUNT   50
#define CARD_NAME_LEN     24
#define CARD_PROG_BYTES   480   // passi per lato (A o B): 480+480 = 960 passi totali, — steps per side (A or B): 480+480 = 960 total steps,
                                 // 1 byte/passo (non più nibble impacchettati) — 1 byte/step (no more packed nibbles)
// Spazio totale registri dati per scheda: 100 registri × REG_WIDTH — Total data register space per card: 100 registers × REG_WIDTH
// nibble ciascuno (REG_WIDTH=18: segno+esponente+13 cifre mantissa, — nibbles each (REG_WIDTH=18: sign+exponent+13 mantissa digits,
// v. tms1500.h). Prima era 100*8, che troncava mantissa/esponente. — see tms1500.h). Previously it was 100*8, which truncated mantissa/exponent.
#define CARD_REGS_BYTES   (100 * REG_WIDTH)
typedef struct {
    char    name[CARD_NAME_LEN];
    uint8_t prog_a[CARD_PROG_BYTES];  // lato A (passi 000-479) — side A (steps 000-479)
    uint8_t prog_b[CARD_PROG_BYTES];  // lato B (passi 480-959) — side B (steps 480-959)
    uint8_t regs[CARD_REGS_BYTES];    // 100 registri × REG_WIDTH nibble ciascuno — 100 registers × REG_WIDTH nibbles each
    uint16_t prog_len_a;
    uint16_t prog_len_b;
    bool     valid;
} CardSlot;
typedef struct {
    CardSlot slots[CARD_SLOT_COUNT];
    int8_t   active_slot;      // -1 = nessuna scheda inserita — -1 = no card inserted
    bool     sense;            // stato corrente microswitch — current microswitch state
    bool     sense_rising;     // true per un ciclo dopo fronte di salita — true for one cycle after a rising edge
    uint8_t  last_written_slot;// ultimo slot scritto (fisico o web) — last written slot (physical or web)
    uint8_t  num_slots;        // slot popolati — populated slots
} CardEmuState;
void cardemu_init(CardEmuState *card);
bool cardemu_read(CardEmuState *card, TMS1500_State *cpu, uint8_t slot);
bool cardemu_write(CardEmuState *card, TMS1500_State *cpu, uint8_t slot,
                   const char *name);
bool cardemu_delete(CardEmuState *card, uint8_t slot);
void cardemu_list(CardEmuState *card, char *out_json, int max_len);
bool cardemu_sense(CardEmuState *card);
// Carica il programma e i registri di uno slot nella CPU, — Loads a slot's program and registers into the CPU,
// pronto per l'esecuzione (usa tms1500_load_prog internamente). — ready for execution (uses tms1500_load_prog internally).
bool cardemu_load_to_cpu(CardEmuState *card, TMS1500_State *cpu, uint8_t slot);
bool cardemu_load_from_json(CardEmuState *card, const char *json, uint8_t slot);
int  cardemu_save_to_json(CardEmuState *card, uint8_t slot, char *out, int max);
bool cardemu_import_batch(CardEmuState *card, const char *path);
// Scrive su SPIFFS il contenuto attuale (in RAM) di uno slot, così — Writes a slot's current (in-RAM) content to SPIFFS, as-is —
// com'è — usata sia da cardemu_write() sia da chi importa una scheda — used both by cardemu_write() and by whoever imports a card
// da un file di testo esterno (v. cardemu_import_text) senza dover — from an external text file (see cardemu_import_text) without having to
// duplicare la logica di serializzazione JSON + scrittura file. — duplicate the JSON serialization + file write logic.
bool cardemu_persist_slot(CardEmuState *card, uint8_t slot);
// Carica una scheda da testo (JSON, lo stesso formato scaricato con — Loads a card from text (JSON, the same format downloaded with
// GET /api/card/file) in uno slot e lo persiste subito su SPIFFS — GET /api/card/file) into a slot and immediately persists it to SPIFFS —
// per lo scambio schede con un PC come semplice file di testo. — for exchanging cards with a PC as a simple text file.
bool cardemu_import_text(CardEmuState *card, const char *text, uint8_t slot);
// Salva/carica lo stato persistente della CPU (programma, etichette, — Saves/loads the CPU persistent state (program, labels,
// registri, ultimo slot scritto) su SPIFFS — "memoria solid state" — registers, last written slot) on SPIFFS — the TI-59's "solid state memory":
// della TI-59: tutto sopravvive al riavvio. — everything survives a reboot.
void cardemu_save_persistent(CardEmuState *card, TMS1500_State *cpu);
void cardemu_load_persistent(CardEmuState *card, TMS1500_State *cpu);
