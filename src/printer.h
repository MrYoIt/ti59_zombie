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
 * printer.h — Emulazione logica della stampante PC-100A (TI-59) — PC-100A printer logic emulation (TI-59)
 * =================================================================
 * Riproduce il buffer di stampa alfanumerico reale così come — Reproduces the real alphanumeric print buffer as
 * documentato nella guida hardware di Hynek Sladký (2014): — documented in Hynek Sladký's (2014) hardware guide:
 *
 *   - Buffer di 20 caratteri, indirizzato DA DESTRA VERSO SINISTRA — 20-character buffer, addressed RIGHT TO LEFT
 *   - Diviso in 4 "gruppi" da 5 caratteri (Op 01=outside-left, — Split into 4 "groups" of 5 characters (Op 01=outside-left,
 *     02=inside-left, 03=inside-right, 04=outside-right) — 02=inside-left, 03=inside-right, 04=outside-right)
 *   - Op 00 azzera il buffer — Op 00 clears the buffer
 *   - Op 05 stampa la riga così composta — Op 05 prints the composed line
 *   - Op 06 stampa il registro di visualizzazione + il gruppo 4 — Op 06 prints the display register + group 4
 *   - Op 07 stampa un asterisco nella colonna indicata dal display — Op 07 prints an asterisk in the column given by the display
 *     (usato per tracciare curve punto per punto) — (used to plot curves point by point)
 *   - Op 08 lista le etichette del programma corrente — Op 08 lists the labels of the current program
 *
 * Questo modulo NON sa nulla di Bluetooth/BLE: espone solo — This module knows NOTHING about Bluetooth/BLE: it only exposes
 * printer_output_line(), un hook debole che il backend di trasporto — printer_output_line(), a weak hook that the transport backend
 * (vedi ble_thermal_printer.h) deve implementare per inviare le — (see ble_thermal_printer.h) must implement to send the
 * righe di testo risultanti a una stampante fisica. In questo modo — resulting text lines to a physical printer. This way
 * la logica di formattazione resta testabile e riusabile anche con — the formatting logic stays testable and reusable with
 * backend diversi (seriale, file di log, ecc.). — different backends (serial, log file, etc.).
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PRINTER_LINE_WIDTH   20   // caratteri per riga (reale: 20) — characters per line (real: 20)
#define PRINTER_GROUP_WIDTH   5   // caratteri per gruppo (reale: 5) — characters per group (real: 5)

typedef struct {
    char buf[PRINTER_LINE_WIDTH]; // buffer alfanumerico corrente — current alphanumeric buffer
    bool connected;                // true se un backend fisico è collegato — true if a physical backend is attached
} PrinterState;

// ─── Ciclo di vita — Lifecycle ─────────────────────────────
void printer_init(PrinterState *p);

// ─── Op 00-08 (chiamate da exec_op in tms1500.cpp) — Op 00-08 (called from exec_op in tms1500.cpp) ─────
// value: il contenuto del registro di visualizzazione (REG_A) al — value: the display register contents (REG_A) at
// momento della chiamata, già convertito in stringa dal chiamante — call time, already converted to a string by the caller
// tramite tms1500_get_display_string(), così questo modulo non deve — via tms1500_get_display_string(), so this module does not need
// conoscere il formato BCD interno del core CPU. — to know the internal BCD format of the CPU core.
void printer_op00_init(PrinterState *p);
void printer_op01_group1(PrinterState *p, const char *text5);
void printer_op02_group2(PrinterState *p, const char *text5);
void printer_op03_group3(PrinterState *p, const char *text5);
void printer_op04_group4(PrinterState *p, const char *text5);
void printer_op05_print_line(PrinterState *p);
void printer_op06_print_display(PrinterState *p, const char *display_str);
void printer_op07_curve(PrinterState *p, int column);
void printer_op08_list_labels(PrinterState *p, const char *labels_csv);

// PRT / ADV / LST (tasti dedicati, non Op nn) — PRT / ADV / LST (dedicated keys, not Op nn)
void printer_prt_register(PrinterState *p, const char *display_str);
void printer_advance(PrinterState *p);

// true se il backend fisico è pronto (usato per Op 40, "stampante — true if the physical backend is ready (used for Op 40, "printer
// collegata" — su hardware reale letto dal pin KP.D0) — "connected" — on real hardware read from pin KP.D0)
bool printer_is_connected(const PrinterState *p);

// ─── Tabella caratteri alfanumerici (Op 01-04) — alphanumeric character table (Op 01-04) ──────────
// Tabella completa a 64 simboli (Table VII del brevetto US4153937). — Full 64-symbol table (Table VII of US patent 4153937).
// Vedi PRINTER_CHARMAP in printer.cpp per la mappa completa. — See PRINTER_CHARMAP in printer.cpp for the full map.
char printer_charcode_to_ascii(uint8_t code2digit);

#ifdef __cplusplus
}
#endif
