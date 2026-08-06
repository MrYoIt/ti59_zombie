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
 * library_module.h — Moduli libreria "Solid State Software"
 * =================================================================
 * Sulla TI-59 reale, un modulo libreria innestato nello slot esterno
 * contiene fino a 5000 "passi" (stessa codifica a 2 cifre usata per i
 * programmi utente) organizzati in un certo numero di programmi
 * numerati. Il tasto "Pgm" (2nd LRN) seguito da un numero a 2 cifre
 * seleziona quel programma — l'esecuzione avviene DIRETTAMENTE sulla
 * ROM del modulo (nessuna copia in memoria utente), perché i
 * programmi della Master Library richiamano spesso subroutine
 * condivise in altre zone della stessa ROM.
 *
 * Un solo modulo alla volta può essere "innestato" (esattamente come
 * sull'hardware reale, dove è fisicamente uno slot unico). La
 * selezione del modulo attivo avviene via web (vedi wifilink.cpp,
 * /api/modules) dato che qui non c'è uno slot fisico.
 *
 * Per aggiungere un nuovo modulo in futuro:
 *   1. Genera un nuovo rom_XXX.cpp con la stessa struttura di
 *      rom_ml1.cpp (array byte + indice programmi + LibraryModule).
 *   2. Registralo in library_module.cpp (LIBRARY_REGISTRY[]).
 * Nessun'altra modifica architetturale è necessaria.
 */

#include <stdint.h>
#include <stdbool.h>
#include "tms1500.h"

typedef struct {
    uint8_t     num;     // numero programma (quello digitato dopo Op 09)
    uint16_t    addr;    // indirizzo di partenza nel ROM del modulo
    uint16_t    len;     // lunghezza in byte/passi
    const char *title;   // titolo leggibile (es. "ML-01 MASTER LIBRARY DIAGNOSTIC")
} LibraryProgram;

typedef struct {
    const char           *id;             // identificatore breve (es. "ml1")
    const char           *name;           // nome leggibile (es. "Master Library -1-")
    const uint8_t         *rom;           // dati ROM grezzi (stessa codifica di cpu->prog[])
    uint16_t               rom_size;
    const LibraryProgram  *programs;
    uint8_t                program_count;
} LibraryModule;

// ─── Registro moduli ────────────────────────────────────────
// Numero di moduli compilati/registrati (v. library_module.cpp).
int library_module_count(void);
// Restituisce il modulo all'indice i (0..library_module_count()-1),
// o NULL se fuori range.
const LibraryModule* library_module_at(int index);
// Cerca un modulo per id (es. "ml1"); NULL se non trovato.
const LibraryModule* library_module_find(const char *id);

// ─── Modulo attivo (uno solo alla volta, come lo slot reale) ──
bool library_set_active(const char *id);   // false se id non esiste
const LibraryModule* library_get_active(void);  // NULL se nessuno selezionato

// Hook chiamato ogni volta che il modulo attivo cambia (con l'id
// selezionato, o stringa vuota se nessuno). Implementazione di
// default no-op; un livello esterno con storage persistente (es.
// wifilink.cpp/NVS) può sovrascriverla per ricordare la scelta tra
// un riavvio e l'altro — stesso pattern già usato per il timing.
void library_on_module_changed(const char *id);

// ─── Richiamo programma (tasto Pgm + 2 cifre) ──────────────
// Cerca "page" (0-99) tra i programmi del modulo attivo. Se trovato,
// restituisce il suo indirizzo/lunghezza/titolo e true — SENZA
// copiare alcun byte: sull'hardware reale (e qui) il modulo resta
// indirizzabile in blocco, ed i programmi possono richiamare
// subroutine condivise altrove nella stessa ROM. È compito del
// chiamante (tms1500.cpp) posizionare cpu->prog_pc sull'indirizzo
// restituito e attivare la modalità di esecuzione da modulo.
bool library_find_program(uint8_t page, uint16_t *out_addr,
                           uint16_t *out_len, const char **out_title);