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
/*
 * printer.cpp — Emulazione logica della stampante PC-100A
 * Vedi printer.h per la descrizione generale.
 */
#include "printer.h"
#include <string.h>
#include <stdio.h>

// ─── Hook di trasporto (debole) ────────────────────────────
// Il backend fisico (es. ble_thermal_printer.cpp) deve fornire la
// propria implementazione. Se nessun backend è collegato, questa
// versione di default non fa nulla (nessuna stampante = nessun
// output, coerente con l'hardware reale senza PC-100A agganciato).
__attribute__((weak)) void printer_output_line(const char *line) {
    (void)line;
}
__attribute__((weak)) bool printer_backend_is_connected(void) {
    return false;
}

// ─── Tabella caratteri (Table VII, brevetto US4153937) ─────
// Codepage completa della stampante PC-100A a 64 simboli.
typedef struct { uint8_t code; char ch; } CharMapEntry;
static const CharMapEntry PRINTER_CHARMAP[] = {
    { 0,  ' ' },  { 1,  '0' },  { 2,  '1' },  { 3,  '2' },
    { 4,  '3' },  { 5,  '4' },  { 6,  '5' },  { 7,  '6' },
    { 10, '7' },  { 11, '8' },  { 12, '9' },
    { 13, 'A' },  { 14, 'B' },  { 15, 'C' },  { 16, 'D' },
    { 17, 'E' },  { 20, '-' },  { 21, 'F' },  { 22, 'G' },
    { 23, 'H' },  { 24, 'I' },  { 25, 'J' },  { 26, 'K' },
    { 27, 'L' },  { 30, 'M' },  { 31, 'N' },  { 32, 'O' },
    { 33, 'P' },  { 34, 'Q' },  { 35, 'R' },  { 36, 'S' },
    { 37, 'T' },  { 40, '.' },  { 41, 'U' },  { 42, 'a' },
    { 43, 'b' },  { 44, 'c' },  { 45, 'd' },  { 46, 'e' },
    { 47, '+' },  { 50, 'x' },  { 51, '*' },  { 52, 'V' },  // √
    { 53, 'p' },  // π
    { 54, 'e' },  { 55, '(' },  { 56, ')' },  { 57, ',' },
    { 60, '^' },  // ↑
    { 61, '%' },  { 62, '>' },  { 63, '<' },  { 64, '=' },
    { 65, '/' },  { 66, '\'' }, { 67, 'X' },  // X̄
    { 70, 'x' },  // x₂
    { 71, '?' },  { 72, '/' },  // ÷
    { 73, '|' },  { 74, 'P' },  // ∏
    { 75, 'D' },  // Δ
    { 76, 'U' },  // ∪
    { 77, 'S' },  // Σ
};
#define PRINTER_CHARMAP_LEN (sizeof(PRINTER_CHARMAP)/sizeof(PRINTER_CHARMAP[0]))

char printer_charcode_to_ascii(uint8_t code2digit) {
    for (size_t i = 0; i < PRINTER_CHARMAP_LEN; i++) {
        if (PRINTER_CHARMAP[i].code == code2digit) return PRINTER_CHARMAP[i].ch;
    }
    return ' ';   // carattere non mappato -> spazio (comportamento sicuro)
}

void printer_init(PrinterState *p) {
    memset(p->buf, ' ', PRINTER_LINE_WIDTH);
    p->connected = false;
}

bool printer_is_connected(const PrinterState *p) {
    (void)p;
    return printer_backend_is_connected();
}

// ─── Op 00: inizializza / azzera il buffer alfanumerico ────
void printer_op00_init(PrinterState *p) {
    memset(p->buf, ' ', PRINTER_LINE_WIDTH);
}

static void fill_group(PrinterState *p, int group_index /*0-3*/, const char *text5) {
    int base = group_index * PRINTER_GROUP_WIDTH;
    for (int i = 0; i < PRINTER_GROUP_WIDTH; i++) {
        char c = text5 ? text5[i] : ' ';
        p->buf[base + i] = c ? c : ' ';
    }
}

// Op 01 = gruppo 1 (outside-left, colonne 0-4)
void printer_op01_group1(PrinterState *p, const char *text5) { fill_group(p, 0, text5); }
// Op 02 = gruppo 2 (inside-left, colonne 5-9)
void printer_op02_group2(PrinterState *p, const char *text5) { fill_group(p, 1, text5); }
// Op 03 = gruppo 3 (inside-right, colonne 10-14)
void printer_op03_group3(PrinterState *p, const char *text5) { fill_group(p, 2, text5); }
// Op 04 = gruppo 4 (outside-right, colonne 15-19)
void printer_op04_group4(PrinterState *p, const char *text5) { fill_group(p, 3, text5); }

// Op 05: stampa la riga così com'è stata composta con Op 01-04
void printer_op05_print_line(PrinterState *p) {
    char line[PRINTER_LINE_WIDTH + 1];
    memcpy(line, p->buf, PRINTER_LINE_WIDTH);
    line[PRINTER_LINE_WIDTH] = '\0';
    printer_output_line(line);
}

// Op 06: stampa il registro di visualizzazione + il gruppo 4
// (reale: "stampa il display seguito dagli ultimi 4 caratteri del
// gruppo 4"; qui usiamo l'intero gruppo 4 per semplicità)
void printer_op06_print_display(PrinterState *p, const char *display_str) {
    char line[64];
    snprintf(line, sizeof(line), "%-12s %.5s", display_str ? display_str : "", &p->buf[15]);
    printer_output_line(line);
}

// Op 07: stampa un asterisco nella colonna 0-19 indicata dal display
// (usato per tracciare grafici punto per punto, una riga alla volta)
void printer_op07_curve(PrinterState *p, int column) {
    char line[PRINTER_LINE_WIDTH + 1];
    memset(line, ' ', PRINTER_LINE_WIDTH);
    if (column < 0) column = 0;
    if (column >= PRINTER_LINE_WIDTH) column = PRINTER_LINE_WIDTH - 1;
    line[column] = '*';
    line[PRINTER_LINE_WIDTH] = '\0';
    printer_output_line(line);
}

// Op 08: lista le etichette usate dal programma
// NOTA: sulla TI-59 reale questa funzione usa la tabella "nomi
// funzione a 3 caratteri" (STO/RCL/SUM/GTO/LRN/SIN/COS/TAN/...) per
// rendere leggibili i mnemonici; qui il chiamante (tms1500.cpp) deve
// già fornire la stringa formattata (es. "A=012 B=045 C=103"), dato
// che questo modulo non ha visibilità sulla tabella etichette/programma.
void printer_op08_list_labels(PrinterState *p, const char *labels_csv) {
    (void)p;
    printer_output_line(labels_csv ? labels_csv : "");
}

// PRT (tasto dedicato, non Op nn): stampa il registro di visualizzazione
void printer_prt_register(PrinterState *p, const char *display_str) {
    (void)p;
    printer_output_line(display_str ? display_str : "");
}

// Adv (2nd .): avanza la carta di una riga (qui: riga vuota)
void printer_advance(PrinterState *p) {
    (void)p;
    printer_output_line("");
}
