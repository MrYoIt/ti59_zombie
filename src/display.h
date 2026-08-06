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
 * display.h — Driver HT16K33 (16x8, I2C) per display a 12 digit del TI-59 — HT16K33 (16x8, I2C) driver for the TI-59 12-digit display
 * I2C: SDA=19 SCL=20 (indirizzo 0x70, pull-up 4.7k sui due fili) — I2C: SDA=19 SCL=20 (address 0x70, 4.7k pull-ups on both wires)
 *
 * Mappatura hardware (16 colonne × 8 righe del driver): — Hardware mapping (driver's 16 columns × 8 rows):
 *   colonna 0 = D12 (esponente, digit più a sinistra) — column 0 = D12 (exponent, leftmost digit)
 *   colonna 11 = D1  (cifra meno significativa della mantissa) — column 11 = D1 (least significant mantissa digit)
 *   colonne 12-15 = libere (riservate a futuri annunciatori: 2nd, DEG...) — columns 12-15 = free (reserved for future annunciators: 2nd, DEG...)
 *   riga 0..7 = segmenti A..G + DP (bit0=A ... bit6=G, bit7=DP) — row 0..7 = segments A..G + DP (bit0=A ... bit6=G, bit7=DP)
 *
 * Le due colonne libere NON vengono toccate: il display VFD originale ha — The two free columns are NOT touched: the original VFD display also has
 * anche annunciatori (DEG/RAD/GRAD, 2nd, INV, USER...) che un domani si — annunciators (DEG/RAD/GRAD, 2nd, INV, USER...) that could one day be
 * possono pilotare nelle colonne 12-15 aggiungendo un map in questo file. — driven in columns 12-15 by adding a map in this file.
 */
#include <stdint.h>
#include <stdbool.h>
#include "tms1500.h"

#ifndef SETTINGS_H
struct SystemSettings;
typedef struct SystemSettings SystemSettings;
#endif

// Comandi HT16K33 (register/command) — HT16K33 commands (register/command)
#define HT16K33_OSC_ON       0x21   // system setup: oscillatore acceso — system setup: oscillator on
#define HT16K33_DISP_ON      0x81   // display setup: display acceso, blink spento — display setup: display on, blink off
#define HT16K33_DISP_OFF     0x80   // display setup: display spento — display setup: display off
#define HT16K33_BRIGHT(x)   (0xE0 | ((x) & 0x0F))   // attenuazione 0-15 — dimming 0-15
#define HT16K33_RAM_BASE     0x00   // primo byte RAM display (colonna 0) — first display RAM byte (column 0)

// Segmenti 7-seg (bitmask, bit0=A ... bit6=G, bit7=DP) — 7-seg segments (bitmask, bit0=A ... bit6=G, bit7=DP)
#define SEG_A  0x01
#define SEG_B  0x02
#define SEG_C  0x04
#define SEG_D  0x08
#define SEG_E  0x10
#define SEG_F  0x20
#define SEG_G  0x40
#define SEG_DP 0x80

typedef struct _DisplayState {
    uint8_t  segments[12];   // pattern segmenti per digit D1..D12 — segment patterns for digits D1..D12
    bool     dirty;
    uint8_t  brightness;     // 0-15
    bool     calc_mode;      // mostra simbolo "⌐" su digit 12 — shows the "⌐" symbol on digit 12
    bool     present;        // HT16K33 rilevato su I2C (trap hardware) — HT16K33 detected on I2C (hardware trap)
} DisplayState;

void display_init(DisplayState *disp);
void display_refresh(DisplayState *disp);
void display_update_from_cpu(DisplayState *disp, TMS1500_State *cpu);
void display_calc_indicator_tick(DisplayState *disp, TMS1500_State *cpu, uint32_t now_ms);
void display_test(DisplayState *disp);
void display_set_brightness(DisplayState *disp, uint8_t level);
void display_show_string(DisplayState *disp, const char *s);
void display_set_settings(const SystemSettings *settings);
