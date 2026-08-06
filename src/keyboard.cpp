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
#include "keyboard.h"
#include "config.h"
#include <Arduino.h>
#include <string.h>

static const uint8_t ROW_PINS[KBD_ROWS] = {
    PIN_D1, PIN_D2, PIN_D3, PIN_D4, PIN_D5,
    PIN_D6, PIN_D7, PIN_D8, PIN_D9
};
static const uint8_t COL_PINS[KBD_COLS] = {
    PIN_KD, PIN_KP, PIN_KQ, PIN_KS, PIN_KT
};

void keyboard_init(KeyboardState *kbd) {
    memset(kbd, 0, sizeof(KeyboardState));
    kbd->key_queue_head = 0;
    kbd->key_queue_tail = 0;
    for (int r = 0; r < KBD_ROWS; r++) {
        pinMode(ROW_PINS[r], OUTPUT);
        digitalWrite(ROW_PINS[r], HIGH);
    }
    for (int c = 0; c < KBD_COLS; c++) {
        pinMode(COL_PINS[c], INPUT_PULLUP);
    }
}

void keyboard_enqueue(KeyboardState *kbd, uint8_t row, uint8_t col) {
    uint8_t keycode = (row << 4) | (col & 0x0F);
    uint8_t next = (kbd->key_queue_head + 1) % KBD_QUEUE_SIZE;
    if (next != kbd->key_queue_tail) {
        kbd->key_queue[kbd->key_queue_head] = keycode;
        kbd->key_queue_head = next;
    }
}

bool keyboard_dequeue(KeyboardState *kbd, uint8_t *row, uint8_t *col) {
    if (kbd->key_queue_head == kbd->key_queue_tail) return false;
    uint8_t keycode = kbd->key_queue[kbd->key_queue_tail];
    *row = (keycode >> 4) & 0x0F;
    *col = keycode & 0x0F;
    kbd->key_queue_tail = (kbd->key_queue_tail + 1) % KBD_QUEUE_SIZE;
    return true;
}

void keyboard_scan(KeyboardState *kbd) {
    for (int r = 0; r < KBD_ROWS; r++) {
        digitalWrite(ROW_PINS[r], LOW);
        delayMicroseconds(10);
        for (int c = 0; c < KBD_COLS; c++) {
            bool pressed = (digitalRead(COL_PINS[c]) == LOW);
            if (pressed) {
                if (kbd->debounce[r][c] < KBD_DEBOUNCE_MS)
                    kbd->debounce[r][c]++;
                if (kbd->debounce[r][c] == KBD_DEBOUNCE_MS &&
                    kbd->matrix[r][c] == 0) {
                    kbd->matrix[r][c] = 1;
                    keyboard_enqueue(kbd, r, c);
                }
            } else {
                kbd->debounce[r][c] = 0;
                kbd->matrix[r][c]   = 0;
            }
        }
        digitalWrite(ROW_PINS[r], HIGH);
    }

    // Processa queue: se c'è un tasto in attesa, esponilo — Process the queue: if a key is pending, expose it
    if (!kbd->key_ready && kbd->key_queue_head != kbd->key_queue_tail) {
        uint8_t row, col;
        if (keyboard_dequeue(kbd, &row, &col)) {
            kbd->last_row = row;
            kbd->last_col = col;
            kbd->key_ready = true;
        }
    }
}
