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
 * cardemu.h — Emulazione lettore schede magnetiche TI-59
 *
 * Le schede originali memorizzano 480 passi programma (lato A) +
 * 480 passi (lato B) su 4 tracce magnetiche a 2.3 IPS.
 *
 * Nell'emulazione: ogni scheda è un file JSON in SPIFFS
 * Formato: {"name":"...","prog_a":"...","prog_b":"...","regs":"..."}
 *
 * Il microswitch card sense (GPIO15) simula l'inserimento.
 * Tenere premuto e selezionare la scheda via WiFi,
 * oppure usare un selettore fisico rotativo (opzionale).
 */
#include <stdint.h>
#include <stdbool.h>
#include "tms1500.h"
#define CARD_SLOT_COUNT   50
#define CARD_NAME_LEN     24
#define CARD_PROG_BYTES   480   // passi per lato (A o B): 480+480 = 960 passi totali,
                                 // 1 byte/passo (non più nibble impacchettati)
// Spazio totale registri dati per scheda: 100 registri × REG_WIDTH
// nibble ciascuno (REG_WIDTH=18: segno+esponente+13 cifre mantissa,
// v. tms1500.h). Prima era 100*8, che troncava mantissa/esponente.
#define CARD_REGS_BYTES   (100 * REG_WIDTH)
typedef struct {
    char    name[CARD_NAME_LEN];
    uint8_t prog_a[CARD_PROG_BYTES];  // lato A (passi 000-479)
    uint8_t prog_b[CARD_PROG_BYTES];  // lato B (passi 480-959)
    uint8_t regs[CARD_REGS_BYTES];    // 100 registri × REG_WIDTH nibble ciascuno
    uint16_t prog_len_a;
    uint16_t prog_len_b;
    bool     valid;
} CardSlot;
typedef struct {
    CardSlot slots[CARD_SLOT_COUNT];
    int8_t   active_slot;      // -1 = nessuna scheda inserita
    bool     sense;            // stato corrente microswitch
    bool     sense_rising;     // true per un ciclo dopo fronte di salita
    uint8_t  last_written_slot;// ultimo slot scritto (fisico o web)
    uint8_t  num_slots;        // slot popolati
} CardEmuState;
void cardemu_init(CardEmuState *card);
bool cardemu_read(CardEmuState *card, TMS1500_State *cpu, uint8_t slot);
bool cardemu_write(CardEmuState *card, TMS1500_State *cpu, uint8_t slot,
                   const char *name);
bool cardemu_delete(CardEmuState *card, uint8_t slot);
void cardemu_list(CardEmuState *card, char *out_json, int max_len);
bool cardemu_sense(CardEmuState *card);
// Carica il programma e i registri di uno slot nella CPU,
// pronto per l'esecuzione (usa tms1500_load_prog internamente).
bool cardemu_load_to_cpu(CardEmuState *card, TMS1500_State *cpu, uint8_t slot);
bool cardemu_load_from_json(CardEmuState *card, const char *json, uint8_t slot);
int  cardemu_save_to_json(CardEmuState *card, uint8_t slot, char *out, int max);
bool cardemu_import_batch(CardEmuState *card, const char *path);
// Scrive su SPIFFS il contenuto attuale (in RAM) di uno slot, così
// com'è — usata sia da cardemu_write() sia da chi importa una scheda
// da un file di testo esterno (v. cardemu_import_text) senza dover
// duplicare la logica di serializzazione JSON + scrittura file.
bool cardemu_persist_slot(CardEmuState *card, uint8_t slot);
// Carica una scheda da testo (JSON, lo stesso formato scaricato con
// GET /api/card/file) in uno slot e lo persiste subito su SPIFFS —
// per lo scambio schede con un PC come semplice file di testo.
bool cardemu_import_text(CardEmuState *card, const char *text, uint8_t slot);
// Salva/carica lo stato persistente della CPU (programma, etichette,
// registri, ultimo slot scritto) su SPIFFS — "memoria solid state"
// della TI-59: tutto sopravvive al riavvio.
void cardemu_save_persistent(CardEmuState *card, TMS1500_State *cpu);
void cardemu_load_persistent(CardEmuState *card, TMS1500_State *cpu);
