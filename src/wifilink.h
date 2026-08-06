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
 * In caso contrario, vedi <https://www.gnu.org/licenses/>. — If not, see <https://www.gnu.org/licenses/>.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once
/*
 * wifilink.h — Web server HTTP per sync programmi TI-59 via WiFi
 *
 * Endpoints:
 *   GET  /            → IDE web (HTML/JS single page)
 *   GET  /api/status  → stato CPU JSON
 *   GET  /api/cards   → lista schede JSON
 *   GET  /api/card?slot=N → scarica scheda N
 *   POST /api/card?slot=N&name=X → carica scheda N
 *   POST /api/keypress?row=R&col=C → invia tasto
 *   GET  /api/display → stato display corrente JSON
 *   POST /api/prog    → carica programma (body hex)
 *   GET  /api/prog    → scarica programma corrente (hex)
 *   POST /api/reset   → reset CPU
 */
#include <stdint.h>
#include "tms1500.h"
#include "cardemu.h"
#include "keyboard.h"

void wifi_server_loop(TMS1500_State *cpu, CardEmuState *card, KeyboardState *kbd);

// ─── Istanza globali (definite in wifilink.cpp) — Global instances (defined in wifilink.cpp)
#include <WebServer.h>
extern WebServer server;
extern TMS1500_State *g_cpu;
extern CardEmuState  *g_card;
extern KeyboardState *g_kbd;
