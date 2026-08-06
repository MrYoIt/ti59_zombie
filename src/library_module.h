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
 * library_module.h — Moduli libreria "Solid State Software" — "Solid State Software" library modules
 * =================================================================
 * Sulla TI-59 reale, un modulo libreria innestato nello slot esterno — On the real TI-59, a library module plugged into the external slot
 * contiene fino a 5000 "passi" (stessa codifica a 2 cifre usata per i — holds up to 5000 "steps" (same 2-digit encoding used for
 * programmi utente) organizzati in un certo numero di programmi — user programs) organized into a number of numbered
 * numerati. Il tasto "Pgm" (2nd LRN) seguito da un numero a 2 cifre — programs. The "Pgm" key (2nd LRN) followed by a 2-digit number
 * seleziona quel programma — l'esecuzione avviene DIRETTAMENTE sulla — selects that program — execution happens DIRECTLY on the
 * ROM del modulo (nessuna copia in memoria utente), perché i — module ROM (no copy into user memory), because the
 * programmi della Master Library richiamano spesso subroutine — Master Library programs often call shared subroutines
 * condivise in altre zone della stessa ROM. — elsewhere in the same ROM.
 *
 * Un solo modulo alla volta può essere "innestato" (esattamente come — Only one module at a time can be "plugged in" (exactly as
 * sull'hardware reale, dove è fisicamente uno slot unico). La — on the real hardware, where it is physically a single slot). The
 * selezione del modulo attivo avviene via web (vedi wifilink.cpp, — active module is selected via the web (see wifilink.cpp,
 * /api/modules) dato che qui non c'è uno slot fisico. — /api/modules) since here there is no physical slot.
 *
 * Per aggiungere un nuovo modulo in futuro: — To add a new module in the future:
 *   1. Genera un nuovo rom_XXX.cpp con la stessa struttura di — 1. Generate a new rom_XXX.cpp with the same structure as
 *      rom_ml1.cpp (array byte + indice programmi + LibraryModule). — rom_ml1.cpp (byte array + program index + LibraryModule).
 *   2. Registralo in library_module.cpp (LIBRARY_REGISTRY[]). — 2. Register it in library_module.cpp (LIBRARY_REGISTRY[]).
 * Nessun'altra modifica architetturale è necessaria. — No other architectural change is needed.
 */

#include <stdint.h>
#include <stdbool.h>
#include "tms1500.h"

typedef struct {
    uint8_t     num;     // numero programma (quello digitato dopo Op 09) — program number (the one typed after Op 09)
    uint16_t    addr;    // indirizzo di partenza nel ROM del modulo — start address in the module ROM
    uint16_t    len;     // lunghezza in byte/passi — length in bytes/steps
    const char *title;   // titolo leggibile (es. "ML-01 MASTER LIBRARY DIAGNOSTIC") — human-readable title (e.g. "ML-01 MASTER LIBRARY DIAGNOSTIC")
} LibraryProgram;

typedef struct {
    const char           *id;             // identificatore breve (es. "ml1") — short identifier (e.g. "ml1")
    const char           *name;           // nome leggibile (es. "Master Library -1-") — readable name (e.g. "Master Library -1-")
    const uint8_t         *rom;           // dati ROM grezzi (stessa codifica di cpu->prog[]) — raw ROM data (same encoding as cpu->prog[])
    uint16_t               rom_size;
    const LibraryProgram  *programs;
    uint8_t                program_count;
} LibraryModule;

// ─── Registro moduli — Module registry ────────────────────────────────────────
// Numero di moduli compilati/registrati (v. library_module.cpp). — Number of compiled/registered modules (see library_module.cpp).
int library_module_count(void);
// Restituisce il modulo all'indice i (0..library_module_count()-1), — Returns the module at index i (0..library_module_count()-1),
// o NULL se fuori range. — or NULL if out of range.
const LibraryModule* library_module_at(int index);
// Cerca un modulo per id (es. "ml1"); NULL se non trovato. — Looks up a module by id (e.g. "ml1"); NULL if not found.
const LibraryModule* library_module_find(const char *id);

// ─── Modulo attivo (uno solo alla volta, come lo slot reale) — Active module (only one at a time, like the real slot) ──
bool library_set_active(const char *id);   // false se id non esiste — false if the id does not exist
const LibraryModule* library_get_active(void);  // NULL se nessuno selezionato — NULL if none selected

// Hook chiamato ogni volta che il modulo attivo cambia (con l'id — Hook called every time the active module changes (with the
// selezionato, o stringa vuota se nessuno). Implementazione di — selected id, or an empty string if none). The default
// default no-op; un livello esterno con storage persistente (es. — implementation is a no-op; an outer layer with persistent storage (e.g.
// wifilink.cpp/NVS) può sovrascriverla per ricordare la scelta tra — wifilink.cpp/NVS) can override it to remember the choice between
// un riavvio e l'altro — stesso pattern già usato per il timing. — one reboot and the next — the same pattern already used for timing.
void library_on_module_changed(const char *id);

// ─── Richiamo programma (tasto Pgm + 2 cifre) — Program call (Pgm key + 2 digits) ──────────────
// Cerca "page" (0-99) tra i programmi del modulo attivo. Se trovato, — Looks up "page" (0-99) among the active module's programs. If found,
// restituisce il suo indirizzo/lunghezza/titolo e true — SENZA — returns its address/length/title and true — WITHOUT
// copiare alcun byte: sull'hardware reale (e qui) il modulo resta — copying any byte: on the real hardware (and here) the module stays
// indirizzabile in blocco, ed i programmi possono richiamare — block-addressable, and programs can call
// subroutine condivise altrove nella stessa ROM. È compito del — shared subroutines elsewhere in the same ROM. It is up to the
// chiamante (tms1500.cpp) posizionare cpu->prog_pc sull'indirizzo — caller (tms1500.cpp) to set cpu->prog_pc to the returned
// restituito e attivare la modalità di esecuzione da modulo. — address and enable module execution mode.
bool library_find_program(uint8_t page, uint16_t *out_addr,
                           uint16_t *out_len, const char **out_title);