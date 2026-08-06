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
#include "display.h"
#include "tms1500.h"
#include "config.h"
#include "settings.h"
#include <Arduino.h>
#include <Wire.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// ─── Tabella caratteri 7-segmenti — 7-segment character table ────────────────────────
static const uint8_t DIGIT_SEG[10] = {
    0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110,
    0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111,
};

#define SEG_MINUS  0b1000000
#define SEG_E_ERR  0b1111001
#define SEG_r      0b1010000
#define SEG_SPACE  0b0000000
//#define SEG_DP     0b10000000
#define SEG_CALC   (SEG_A|SEG_D|SEG_E|SEG_F)
#define SEG_MINUS_SIGN SEG_MINUS

// ─── I2C helper HT16K33 — HT16K33 I2C helper ──────────────────────────────────
// Comando singolo (register) — nessun dato successivo. — Single (register) command — no data follows.
static void ht16k33_cmd(uint8_t reg) {
    Wire.beginTransmission(HT16K33_I2C_ADDR);
    Wire.write(reg);
    Wire.endTransmission();
}

// Scrive tutta la RAM display (16 colonne × 8 righe = 16 byte). — Writes the whole display RAM (16 columns × 8 rows = 16 bytes).
// Il primo byte dopo l'address è il registro di partenza (colonna 0); — The first byte after the address is the starting register (column 0);
// l'HT16K33 autoincrementa il puntatore a ogni byte successivo. — the HT16K33 auto-increments the pointer on each subsequent byte.
static void ht16k33_write_ram(const uint8_t *ram) {
    Wire.beginTransmission(HT16K33_I2C_ADDR);
    Wire.write(HT16K33_RAM_BASE);
    for (int i = 0; i < 16; i++) Wire.write(ram[i]);
    Wire.endTransmission();
}

// ─── Inizializzazione — Init ────────────────────────────────────────────────
void display_init(DisplayState *disp) {
    memset(disp, 0, sizeof(DisplayState));
    disp->brightness = 7;

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    // ─── Trap hardware — Hardware trap ──────────────────────────────────────
    // Se il driver non risponde (nessun HT16K33 cablato), si salta — If the driver does not respond (no HT16K33 wired), it skips
    // tutta l'inizializzazione e il refresh: il resto del firmware — all initialization and refresh: the rest of the firmware
    // (web, SPIFFS, emulatore) funziona senza display. — (web, SPIFFS, emulator) works without a display.
    Wire.beginTransmission(HT16K33_I2C_ADDR);
    disp->present = (Wire.endTransmission() == 0);
    if (!disp->present) {
        Serial.println("[DISP] HT16K33 non rilevato su I2C 0x70 — display disattivato");
        return;
    }

    ht16k33_cmd(HT16K33_OSC_ON);       // osc. interno ON — internal oscillator ON
    ht16k33_cmd(HT16K33_DISP_ON);      // display acceso, blink spento — display ON, blink OFF
    ht16k33_cmd(HT16K33_BRIGHT(disp->brightness));

    uint8_t ram[16];
    memset(ram, SEG_SPACE, 16);
    ht16k33_write_ram(ram);

    display_test(disp);
    delay(300);
    ht16k33_write_ram(ram);            // spegni tutto dopo il test — turn everything off after the test
}

// ─── Test display (accende tutti i segmenti 300ms) — Display test (lights all segments for 300ms) ───────
void display_test(DisplayState *disp) {
    if (!disp->present) return;
    uint8_t ram[16];
    memset(ram, 0xFF, 16);
    ht16k33_write_ram(ram);
    delay(200);
    memset(ram, SEG_SPACE, 16);
    ht16k33_write_ram(ram);
}

// ─── Luminosità — Brightness ──────────────────────────────────────────
void display_set_brightness(DisplayState *disp, uint8_t level) {
    disp->brightness = level & 0x0F;
    if (!disp->present) return;
    ht16k33_cmd(HT16K33_BRIGHT(disp->brightness));
}

// ─── Converti BCD TI-59 → array segmenti 12 digit — Convert TI-59 BCD to a 12-digit segment array ───────
void display_update_from_cpu(DisplayState *disp, TMS1500_State *cpu) {
    if (!cpu->disp_dirty) return;
    // NON resettare disp_dirty qui — lascia che display_refresh lo faccia — Do NOT reset disp_dirty here — let display_refresh do it
    // dopo aver inviato i dati all'HT16K33 — after sending the data to the HT16K33

    const int8_t *n = cpu->disp_buf;

    if (cpu->flags.error) {
        memset(disp->segments, SEG_SPACE, 12);
        disp->segments[11] = SEG_E_ERR;
        disp->segments[10] = SEG_r;
        disp->segments[9]  = SEG_r;
        disp->dirty = true;
        return;
    }

    memset(disp->segments, SEG_SPACE, 12);

    // Esponente (D12, D11) — Exponent (D12, D11)
    int exp_sign = (n[1] < 0) ? -1 : 1;
    int exp_val  = n[2] * 10 + n[3];

    if (exp_val != 0) {
        disp->segments[11] = (exp_sign < 0) ? SEG_MINUS : SEG_SPACE;
        if (n[2] > 0)
            disp->segments[11] |= DIGIT_SEG[n[2]];
        disp->segments[10] = DIGIT_SEG[n[3]];
    }

    // Segno mantissa — Mantissa sign
    if (n[0] < 0) {
        disp->segments[9] = SEG_MINUS;
    }

    // Mantissa (D8..D1 = nibble n[4]..n[11]) — Mantissa (D8..D1 = nibbles n[4]..n[11])
    int dp_pos = 7;
    for (int d = 0; d < 8; d++) {
        int nibble_idx = 4 + (7 - d);
        if (nibble_idx > 14) nibble_idx = 14;
        int8_t nib = n[nibble_idx];
        if (nib < 0) nib = 0;
        if (nib > 9) nib = 9;
        uint8_t seg = DIGIT_SEG[nib];
        if (d == dp_pos) seg |= SEG_DP;
        disp->segments[d] = seg;
    }

    // Modalità calcolo — Calculation mode
    if (disp->calc_mode) {
        disp->segments[11] = SEG_CALC;
    }

    // Trailing DP (indicatore operazione in sospeso) — Trailing DP (pending operation indicator)
    if (tms1500_get_trailing_dp()) {
        for (int d = 7; d >= 0; d--) {
            if (disp->segments[d] != SEG_SPACE) {
                disp->segments[d] |= SEG_DP;
                break;
            }
        }
    }

    disp->dirty = true;
}

// ─── Refresh fisico HT16K33 — Physical HT16K33 refresh ──────────────────────────────
// Colonna 0 = D12 (esponente, sinistra), colonna 11 = D1 (destra). — Column 0 = D12 (exponent, left), column 11 = D1 (right).
// Il buffer firmware usa segments[0]=D1 ... segments[11]=D12, quindi — The firmware buffer uses segments[0]=D1 ... segments[11]=D12, so
// si rispecchia l'ordine: RAM[col] = segments[11 - col]. — the order is mirrored: RAM[col] = segments[11 - col].
void display_refresh(DisplayState *disp) {
    if (!disp->dirty) return;
    disp->dirty = false;
    if (!disp->present) return;   // nessun hardware: niente scritture I2C — no hardware: no I2C writes

    uint8_t ram[16];
    memset(ram, SEG_SPACE, 16);
    for (int c = 0; c < 12; c++) {
        ram[c] = disp->segments[11 - c];
    }
    ht16k33_write_ram(ram);
}

// ─── Mostra stringa ASCII — Show ASCII string ──────────────────────────────
void display_show_string(DisplayState *disp, const char *s) {
    static uint8_t seg_map[128] = {0};
    static bool initialized = false;
    if (!initialized) {
        seg_map[' ']  = SEG_SPACE;
        seg_map['-']  = SEG_MINUS;
        seg_map['.']  = SEG_DP;
        for (int i = 0; i <= 9; i++) seg_map['0'+i] = DIGIT_SEG[i];
        seg_map['A'] = seg_map['a'] = 0b1110111;
        seg_map['C'] = seg_map['c'] = 0b0111001;
        seg_map['E'] = seg_map['e'] = 0b1111001;
        seg_map['F'] = seg_map['f'] = 0b1110001;
        seg_map['H'] = seg_map['h'] = 0b1110110;
        seg_map['I'] = seg_map['i'] = 0b0110000;
        seg_map['L'] = seg_map['l'] = 0b0111000;
        seg_map['n']               = 0b1010100;
        seg_map['O'] = seg_map['o'] = 0b1011100;
        seg_map['P'] = seg_map['p'] = 0b1110011;
        seg_map['r']               = 0b1010000;
        seg_map['S'] = seg_map['s'] = 0b1101101;
        seg_map['t']               = 0b1111000;
        seg_map['U'] = seg_map['u'] = 0b0111110;
        seg_map['Y'] = seg_map['y'] = 0b1101110;
        initialized = true;
    }

    memset(disp->segments, SEG_SPACE, 12);
    int len = strlen(s);

    /* Trova la sottostringa significativa — Find the significant substring */
    int start = 0;
    while (start < len && s[start] == ' ') start++;
    int end = len - 1;
    while (end >= 0 && s[end] == ' ') end--;
    if (end < start) {
        disp->dirty = true;
        return;
    }

    /* Conta i caratteri visibili: il punto NON occupa una cifra a sé — Counts the visible characters: the dot does NOT occupy a digit of its own */
    int digits = 0;
    for (int i = start; i <= end; i++) {
        if (s[i] != '.') digits++;
    }

    /* Giustificazione a destra: l'ultima cifra finisce sempre in colonna 11 — Right justification: the last digit always ends up in column 11 */
    int pos = 11 - (digits - 1);
    if (pos < 0) pos = 0;

    for (int i = start; i <= end && pos < 12; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '.') {
            /* Il punto si attacca alla cifra PRECEDENTE nel display — The dot attaches to the PREVIOUS digit on the display */
            if (pos > 0) disp->segments[pos - 1] |= SEG_DP;
        } else {
            if (ch < 128) disp->segments[pos] = seg_map[ch];
            pos++;
        }
    }

    /* Punto operativo (trailing DP) sull'ultima cifra a destra — Operating point (trailing DP) on the last rightmost digit */
    if (tms1500_get_trailing_dp() && digits > 0) {
        disp->segments[11] |= SEG_DP;
    }

    disp->dirty = true;
}

// ═══════════════════════════════════════════════════════════
// FUNZIONI AGGIUNTE (display_calc_indicator_tick, settings) — ADDED FUNCTIONS (display_calc_indicator_tick, settings)
// ═══════════════════════════════════════════════════════════

static const SystemSettings *g_disp_settings = nullptr;

void display_set_settings(const SystemSettings *settings) {
    g_disp_settings = settings;
}

void display_calc_indicator_tick(DisplayState *disp, TMS1500_State *cpu, uint32_t now_ms) {
    if (!g_disp_settings) return;

    if (cpu->flags.idle) {
        if (disp->calc_mode) {
            disp->calc_mode = false;
            disp->dirty = true;
        }
        return;
    }

    uint32_t blink_period = g_disp_settings->calc_blink_ms;
    if (blink_period == 0) blink_period = 250;

    bool show = ((now_ms / blink_period) % 2) == 0;

    if (disp->calc_mode != show) {
        disp->calc_mode = show;
        disp->dirty = true;
    }
}
