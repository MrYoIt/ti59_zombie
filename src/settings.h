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
/* settings.h — Impostazioni runtime persistenti (SPIFFS) */
#include <stdint.h>

typedef struct SystemSettings {
    uint32_t calc_min_show_ms;
    uint32_t calc_blink_ms;
    uint8_t  display_brightness;
    bool     wifi_ap_mode;
    char     wifi_ssid[32];
    char     wifi_pass[64];
} SystemSettings;

#define SETTINGS_DEFAULT_CALC_MIN_SHOW_MS  500
#define SETTINGS_DEFAULT_CALC_BLINK_MS     250
#define SETTINGS_DEFAULT_BRIGHTNESS        7
