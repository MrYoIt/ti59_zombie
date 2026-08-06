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
 * rfid_reader.h — Lettore schede NFC (MFRC522 + NTAG213) tipo "schede magnetiche" — NFC card reader (MFRC522 + NTAG213) mimicking "magnetic cards"
 *
 * Riproduce il flusso del lettore magnetico TI-59 originale: — Reproduces the flow of the original TI-59 magnetic reader:
 *   READ  : inserisci la scheda nel taglio a destra -> il microswitch — READ: insert the card into the right slot -> the microswitch
 *           PIN_CARD_SENSE si chiude -> il firmware accende il modulo — PIN_CARD_SENSE closes -> the firmware powers on the
 *           RFID, legge lo slot dal tag (pagina RFD_TAG_SLOT_PAGE) o — RFID module, reads the slot from the tag (page RFD_TAG_SLOT_PAGE) or
 *           dalla mappa UID, carica la scheda virtuale su SPIFFS nel — from the UID map, loads the virtual card from SPIFFS into the
 *           CPU, poi espelle la scheda dal lato opposto (sinistra) — CPU, then ejects the card from the opposite side (left)
 *           azionando il motore PIN_CARD_MOTOR per RFD_EJECT_MS. — driving the motor PIN_CARD_MOTOR for RFD_EJECT_MS.
 *   WRITE : si arma la scrittura (tasto WRITE fisico o /api/rfid/arm — WRITE: writing is armed (physical WRITE key or /api/rfid/arm
 *           dal web) -> alla successiva inserzione il programma corrente — from the web) -> on the next insertion the current program
 *           viene salvato in uno slot, lo slot viene scritto nel tag e — is saved into a slot, the slot is written to the tag and
 *           in /rfid_map.json, poi la scheda viene espulsa. — to /rfid_map.json, then the card is ejected.
 *
 * Il modulo resta SPENTO a riposo (PIN_RFID_PWR): si consuma corrente — The module stays OFF at rest (PIN_RFID_PWR): current is drawn
 * solo per la durata della lettura/scrittura. — only for the duration of the read/write.
 *
 * Ereditarietà: senza modulo collegato (auto-detect al boot), il — Fallback: without a module attached (auto-detect at boot), the
 * vecchio flusso a microswitch (load ultimo slot scritto) resta attivo. — old microswitch flow (load last written slot) stays active.
 */
#include <stdint.h>
#include <stdbool.h>
#include "tms1500.h"
#include "cardemu.h"

// Lock/unlock opzionali attorno alle mutazioni della CPU (passati dal — Optional lock/unlock around CPU mutations (passed by the
// chiamante, es. taskKeyboard, così il modulo non dipende dal mutex — caller, e.g. taskKeyboard, so the module does not depend on the
// globale dell'applicazione). Possono essere NULL se il chiamante — application's global mutex). May be NULL if the caller
// garantisce la mutua esclusione. — guarantees mutual exclusion.
typedef void (*rfid_lock_fn)(void);
typedef void (*rfid_unlock_fn)(void);

// Inizializza pin, carica la mappa UID e auto-detecta il modulo. — Initializes pins, loads the UID map and auto-detects the module.
void rfid_reader_init(void);

// true se al boot è stato rilevato un MFRC522 rispondente. — true if a responding MFRC522 was detected at boot.
bool rfid_reader_enabled(void);

// Gestisce l'inserimento scheda (da chiamare sul fronte di salita di — Handles card insertion (call on the rising edge of
// PIN_CARD_SENSE). Ritorna true se il lettore ha preso in carico — PIN_CARD_SENSE). Returns true if the reader took over
// l'evento (modulo presente), false se non c'è RFID (il chiamante può — the event (module present), false if there is no RFID (the caller may
// usare il vecchio flusso). Bloccante: accensione + scan (~1.5s max) — use the old flow). Blocking: power-up + scan (~1.5s max)
// + scrittura + espulsione motore. — + write + motor ejection.
bool rfid_reader_handle_insert(CardEmuState *card, TMS1500_State *cpu,
                               rfid_lock_fn lock, rfid_unlock_fn unlock);

// Arma la scrittura: alla prossima inserzione il programma corrente — Arms the write: on the next insertion the current program
// verrà salvato. slot = -1 => prossimo slot libero. — will be saved. slot = -1 => next free slot.
void rfid_reader_arm_write(int slot);

// true se una scrittura è armata e in attesa di scheda. — true if a write is armed and waiting for a card.
bool rfid_reader_write_armed(void);

// Durata espulsione motore (ms), regolabile via web e persistita in NVS. — Motor eject duration (ms), web-adjustable and persisted in NVS.
void     rfid_reader_set_eject_ms(uint16_t ms);
uint16_t rfid_reader_get_eject_ms(void);

// Mappa UID -> slot persistita in /rfid_map.json (fallback quando lo — UID -> slot map persisted in /rfid_map.json (fallback when the
// slot non è (ancora) scritto nel tag). — slot is not (yet) written in the tag).
int  rfid_uid_lookup(const uint8_t *uid, uint8_t len);
bool rfid_map_set_uid(const uint8_t *uid, uint8_t len, int slot);
void rfid_map_list(char *out, int max_len);          // Array JSON — JSON array
int  rfid_map_count(void);

// Diagnostica: accende il modulo, prova a leggere un tag e ritorna il suo — Diagnostics: powers the module, tries to read a tag and returns its
// UID hex (14 char, senza terminatore) + slot letto/trovato. Usato da — hex UID (14 chars, no terminator) + slot read/found. Used by
// /api/rfid/read per la configurazione tramite web. — /api/rfid/read for web-based configuration.
bool rfid_reader_probe(uint8_t *uid_out /* 7 byte — 7 bytes */, uint8_t *uid_len,
                       int *slot_read);