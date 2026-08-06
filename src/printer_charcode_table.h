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
// ═══════════════════════════════════════════════════════════════
// Tabella codepage stampante TI-58/59 (Table VII, brevetto US4153937) — TI-58/59 printer codepage table (Table VII, US patent 4153937)
// Fonte: TI Master Library Quick Reference Guide, pag. 5 — Source: TI Master Library Quick Reference Guide, p. 5
// "Alphanumeric Print Codes" — griglia riga-colonna a 2 cifre. — "Alphanumeric Print Codes" — 2-digit row-column grid.
//
// STATO DI CONFIDENZA: — CONFIDENCE STATUS:
//   - Righe 0-4 (codici 00-47): VERIFICATE. Confermate con i due — Rows 0-4 (codes 00-47): VERIFIED. Confirmed with the two
//     esempi espliciti nel testo del manuale ("A is code 13", — explicit examples in the manual text ("A is code 13",
//     "+ is code 47"), entrambi coerenti con questa tabella. — "+ is code 47"), both consistent with this table.
//   - Righe 5-7 (codici 50-77): DA VERIFICARE A VISTA. La scansione — Rows 5-7 (codes 50-77): TO BE VERIFIED BY EYE. The scan
//     OCR di questi simboli speciali (Gamma, pi greco, radice, x — OCR of these special symbols (Gamma, pi, square root, x
//     soprasegnato, frecce, ecc.) è ambigua — prima di committare in — with overline, arrows, etc.) is ambiguous — before committing to
//     produzione, confrontare visivamente con la scansione originale — production, compare visually with the original scan
//     pag. 5 del PDF "SSSM_ML_US.pdf". I valori qui sotto sono la — p. 5 of the PDF "SSSM_ML_US.pdf". The values below are the
//     miglior ricostruzione possibile dal testo estratto, non una — best possible reconstruction from the extracted text, not a
//     trascrizione certa al 100% per queste due righe. — 100% certain transcription for these two rows.
//
// Uso: code = riga*10 + colonna (es. 'A' = riga1,col3 = codice 13) — Use: code = row*10 + column (e.g. 'A' = row1,col3 = code 13)
// ═══════════════════════════════════════════════════════════════

char printer_charcode_to_ascii(uint8_t code) {
    static const char table[8][8] = {
        /* riga 0 — row 0 */ { ' ', '0', '1', '2', '3', '4', '5', '6' },
        /* riga 1 — row 1 */ { '7', '8', '9', 'A', 'B', 'C', 'D', 'E' },
        /* riga 2 — row 2 */ { '-', 'F', 'G', 'H', 'I', 'J', 'K', 'L' },
        /* riga 3 — row 3 */ { 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T' },
        /* riga 4 — row 4 */ { '.', 'U', 'V', 'W', 'X', 'Y', 'Z', '+' },
        // ── da qui in giù: verificare a vista sullo scan, vedi nota sopra — from here down: verify by eye on the scan, see note above ──
        /* riga 5 — row 5 */ { 'x', '*', 'G', 'p', 'e', '(', ')', ',' },  // G=Gamma, p=pi, e=epsilon? — G=Gamma, p=pi, e=epsilon?
        /* riga 6 — row 6 */ { '^', '%', '/', '/', '=', '"', 'x', '_' }, // simboli incerti (freccia, percento, ecc.) — uncertain symbols (arrow, percent, etc.)
        /* riga 7 — row 7 */ { 'z', '?', '/', '!', 'P', '.', 'P', 'S' }, // z-bar, Pi, Sigma incerti — z-bar, Pi, Sigma uncertain
    };
    int row = code / 10;
    int col = code % 10;
    if (row < 0 || row > 7 || col < 0 || col > 7) return ' ';
    return table[row][col];
}
