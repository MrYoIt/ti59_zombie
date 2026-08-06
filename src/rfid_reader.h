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
 * rfid_reader.h — Lettore schede NFC (MFRC522 + NTAG213) tipo "schede magnetiche"
 *
 * Riproduce il flusso del lettore magnetico TI-59 originale:
 *   READ  : inserisci la scheda nel taglio a destra -> il microswitch
 *           PIN_CARD_SENSE si chiude -> il firmware accende il modulo
 *           RFID, legge lo slot dal tag (pagina RFD_TAG_SLOT_PAGE) o
 *           dalla mappa UID, carica la scheda virtuale su SPIFFS nel
 *           CPU, poi espelle la scheda dal lato opposto (sinistra)
 *           azionando il motore PIN_CARD_MOTOR per RFD_EJECT_MS.
 *   WRITE : si arma la scrittura (tasto WRITE fisico o /api/rfid/arm
 *           dal web) -> alla successiva inserzione il programma corrente
 *           viene salvato in uno slot, lo slot viene scritto nel tag e
 *           in /rfid_map.json, poi la scheda viene espulsa.
 *
 * Il modulo resta SPENTO a riposo (PIN_RFID_PWR): si consuma corrente
 * solo per la durata della lettura/scrittura.
 *
 * Ereditarietà: senza modulo collegato (auto-detect al boot), il
 * vecchio flusso a microswitch (load ultimo slot scritto) resta attivo.
 */
#include <stdint.h>
#include <stdbool.h>
#include "tms1500.h"
#include "cardemu.h"

// Lock/unlock opzionali attorno alle mutazioni della CPU (passati dal
// chiamante, es. taskKeyboard, così il modulo non dipende dal mutex
// globale dell'applicazione). Possono essere NULL se il chiamante
// garantisce la mutua esclusione.
typedef void (*rfid_lock_fn)(void);
typedef void (*rfid_unlock_fn)(void);

// Inizializza pin, carica la mappa UID e auto-detecta il modulo.
void rfid_reader_init(void);

// true se al boot è stato rilevato un MFRC522 rispondente.
bool rfid_reader_enabled(void);

// Gestisce l'inserimento scheda (da chiamare sul fronte di salita di
// PIN_CARD_SENSE). Ritorna true se il lettore ha preso in carico
// l'evento (modulo presente), false se non c'è RFID (il chiamante può
// usare il vecchio flusso). Bloccante: accensione + scan (~1.5s max)
// + scrittura + espulsione motore.
bool rfid_reader_handle_insert(CardEmuState *card, TMS1500_State *cpu,
                               rfid_lock_fn lock, rfid_unlock_fn unlock);

// Arma la scrittura: alla prossima inserzione il programma corrente
// verrà salvato. slot = -1 => prossimo slot libero.
void rfid_reader_arm_write(int slot);

// true se una scrittura è armata e in attesa di scheda.
bool rfid_reader_write_armed(void);

// Durata espulsione motore (ms), regolabile via web e persistita in NVS.
void     rfid_reader_set_eject_ms(uint16_t ms);
uint16_t rfid_reader_get_eject_ms(void);

// Mappa UID -> slot persistita in /rfid_map.json (fallback quando lo
// slot non è (ancora) scritto nel tag).
int  rfid_uid_lookup(const uint8_t *uid, uint8_t len);
bool rfid_map_set_uid(const uint8_t *uid, uint8_t len, int slot);
void rfid_map_list(char *out, int max_len);          // JSON array
int  rfid_map_count(void);

// Diagnostica: accende il modulo, prova a leggere un tag e ritorna il suo
// UID hex (14 char, senza terminatore) + slot letto/trovato. Usato da
// /api/rfid/read per la configurazione tramite web.
bool rfid_reader_probe(uint8_t *uid_out /* 7 byte */, uint8_t *uid_len,
                       int *slot_read);