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
 * wifilink_modules.h — Endpoint WiFi per moduli CROM TI-59
 */
#include <stdint.h>

/**
 * Registra le route HTTP per il sistema moduli CROM.
 * Chiamare da setup_routes() in wifilink.c PRIMA di server.begin()
 * (stesso punto in cui vengono registrate tutte le altre route, es.
 * /api/status, /api/card, ecc.).
 *
 * Endpoints aggiunti:
 *   GET  /api/modules              Lista moduli disponibili
 *   GET  /api/module?id=N          Lista programmi del modulo N
 *   POST /api/module/run?id=N&prog=P  Esegue programma P
 *   GET  /api/module/active        Modulo/programma attivi
 *   POST /api/module/select?id=N   Seleziona modulo attivo
 */
void register_module_routes(void);