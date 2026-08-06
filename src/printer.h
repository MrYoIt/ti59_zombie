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
 * printer.h — Emulazione logica della stampante PC-100A (TI-59)
 * =================================================================
 * Riproduce il buffer di stampa alfanumerico reale così come
 * documentato nella guida hardware di Hynek Sladký (2014):
 *
 *   - Buffer di 20 caratteri, indirizzato DA DESTRA VERSO SINISTRA
 *   - Diviso in 4 "gruppi" da 5 caratteri (Op 01=outside-left,
 *     02=inside-left, 03=inside-right, 04=outside-right)
 *   - Op 00 azzera il buffer
 *   - Op 05 stampa la riga così composta
 *   - Op 06 stampa il registro di visualizzazione + il gruppo 4
 *   - Op 07 stampa un asterisco nella colonna indicata dal display
 *     (usato per tracciare curve punto per punto)
 *   - Op 08 lista le etichette del programma corrente
 *
 * Questo modulo NON sa nulla di Bluetooth/BLE: espone solo
 * printer_output_line(), un hook debole che il backend di trasporto
 * (vedi ble_thermal_printer.h) deve implementare per inviare le
 * righe di testo risultanti a una stampante fisica. In questo modo
 * la logica di formattazione resta testabile e riusabile anche con
 * backend diversi (seriale, file di log, ecc.).
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PRINTER_LINE_WIDTH   20   // caratteri per riga (reale: 20)
#define PRINTER_GROUP_WIDTH   5   // caratteri per gruppo (reale: 5)

typedef struct {
    char buf[PRINTER_LINE_WIDTH]; // buffer alfanumerico corrente
    bool connected;                // true se un backend fisico è collegato
} PrinterState;

// ─── Ciclo di vita ─────────────────────────────────────────
void printer_init(PrinterState *p);

// ─── Op 00-08 (chiamate da exec_op in tms1500.cpp) ─────────
// value: il contenuto del registro di visualizzazione (REG_A) al
// momento della chiamata, già convertito in stringa dal chiamante
// tramite tms1500_get_display_string(), così questo modulo non deve
// conoscere il formato BCD interno del core CPU.
void printer_op00_init(PrinterState *p);
void printer_op01_group1(PrinterState *p, const char *text5);
void printer_op02_group2(PrinterState *p, const char *text5);
void printer_op03_group3(PrinterState *p, const char *text5);
void printer_op04_group4(PrinterState *p, const char *text5);
void printer_op05_print_line(PrinterState *p);
void printer_op06_print_display(PrinterState *p, const char *display_str);
void printer_op07_curve(PrinterState *p, int column);
void printer_op08_list_labels(PrinterState *p, const char *labels_csv);

// PRT / ADV / LST (tasti dedicati, non Op nn)
void printer_prt_register(PrinterState *p, const char *display_str);
void printer_advance(PrinterState *p);

// true se il backend fisico è pronto (usato per Op 40, "stampante
// collegata" — su hardware reale letto dal pin KP.D0)
bool printer_is_connected(const PrinterState *p);

// ─── Tabella caratteri alfanumerici (Op 01-04) ─────────────
// Tabella completa a 64 simboli (Table VII del brevetto US4153937).
// Vedi PRINTER_CHARMAP in printer.cpp per la mappa completa.
char printer_charcode_to_ascii(uint8_t code2digit);

#ifdef __cplusplus
}
#endif
