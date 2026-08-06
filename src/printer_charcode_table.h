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
// ═══════════════════════════════════════════════════════════════
// Tabella codepage stampante TI-58/59 (Table VII, brevetto US4153937)
// Fonte: TI Master Library Quick Reference Guide, pag. 5
// "Alphanumeric Print Codes" — griglia riga-colonna a 2 cifre.
//
// STATO DI CONFIDENZA:
//   - Righe 0-4 (codici 00-47): VERIFICATE. Confermate con i due
//     esempi espliciti nel testo del manuale ("A is code 13",
//     "+ is code 47"), entrambi coerenti con questa tabella.
//   - Righe 5-7 (codici 50-77): DA VERIFICARE A VISTA. La scansione
//     OCR di questi simboli speciali (Gamma, pi greco, radice, x
//     soprasegnato, frecce, ecc.) è ambigua — prima di committare in
//     produzione, confrontare visivamente con la scansione originale
//     pag. 5 del PDF "SSSM_ML_US.pdf". I valori qui sotto sono la
//     miglior ricostruzione possibile dal testo estratto, non una
//     trascrizione certa al 100% per queste due righe.
//
// Uso: code = riga*10 + colonna (es. 'A' = riga1,col3 = codice 13)
// ═══════════════════════════════════════════════════════════════

char printer_charcode_to_ascii(uint8_t code) {
    static const char table[8][8] = {
        /* riga 0 */ { ' ', '0', '1', '2', '3', '4', '5', '6' },
        /* riga 1 */ { '7', '8', '9', 'A', 'B', 'C', 'D', 'E' },
        /* riga 2 */ { '-', 'F', 'G', 'H', 'I', 'J', 'K', 'L' },
        /* riga 3 */ { 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T' },
        /* riga 4 */ { '.', 'U', 'V', 'W', 'X', 'Y', 'Z', '+' },
        // ── da qui in giù: verificare a vista sullo scan, vedi nota sopra ──
        /* riga 5 */ { 'x', '*', 'G', 'p', 'e', '(', ')', ',' },  // G=Gamma, p=pi, e=epsilon?
        /* riga 6 */ { '^', '%', '/', '/', '=', '"', 'x', '_' }, // simboli incerti (freccia, percento, ecc.)
        /* riga 7 */ { 'z', '?', '/', '!', 'P', '.', 'P', 'S' }, // z-bar, Pi, Sigma incerti
    };
    int row = code / 10;
    int col = code % 10;
    if (row < 0 || row > 7 || col < 0 || col > 7) return ' ';
    return table[row][col];
}
