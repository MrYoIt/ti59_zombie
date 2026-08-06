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
// keyboard.h — driver tastiera a scansione — keyboard scan driver
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#define KBD_ROWS  9
#define KBD_COLS  5
#define KBD_DEBOUNCE_MS  12
#define KBD_QUEUE_SIZE   8

typedef struct _KeyboardState {
    uint8_t  matrix[KBD_ROWS][KBD_COLS];
    uint8_t  debounce[KBD_ROWS][KBD_COLS];
    volatile bool     key_ready;
    volatile uint8_t  last_row;
    volatile uint8_t  last_col;
    uint32_t last_time_ms;

    // Queue circolare thread-safe per keypress WiFi/keyboard — Thread-safe circular queue for WiFi/keyboard key presses
    volatile uint8_t key_queue[KBD_QUEUE_SIZE];
    volatile uint8_t key_queue_head;
    volatile uint8_t key_queue_tail;
} KeyboardState;

void keyboard_init(KeyboardState *kbd);
void keyboard_scan(KeyboardState *kbd);
bool keyboard_dequeue(KeyboardState *kbd, uint8_t *row, uint8_t *col);
void keyboard_enqueue(KeyboardState *kbd, uint8_t row, uint8_t col);
