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
#include "wifilink_modules.h"
#include "wifilink.h"
#include <Arduino.h>
#include <WebServer.h>

static const char* MODULES_JSON = R"({"modules":[
  {"id":1,"name":"Master Library","programs":25},
  {"id":2,"name":"Statistics","programs":32},
  {"id":3,"name":"Real Estate","programs":25},
  {"id":5,"name":"Navigation","programs":22},
  {"id":7,"name":"Leisure","programs":20},
  {"id":10,"name":"Math Utilities","programs":31},
  {"id":11,"name":"Electrical Engineering","programs":28}
]})";

static void handle_modules_get() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", MODULES_JSON);
}

static void handle_module_get() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    if (!server.hasArg("id")) {
        server.send(400, "application/json", "{\"error\":\"missing id\"}");
        return;
    }
    server.send(200, "application/json", "{\"programs\":[],\"note\":\"stub\"}");
}

static void handle_module_run() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", "{\"ok\":true,\"note\":\"stub\"}");
}

static void handle_module_select() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", "{\"ok\":true,\"note\":\"stub\"}");
}

static void handle_module_active() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", "{\"active_module\":0,\"active_program\":0}");
}

void register_module_routes(void) {
    server.on("/api/modules",       HTTP_GET,  handle_modules_get);
    server.on("/api/module",        HTTP_GET,  handle_module_get);
    server.on("/api/module/run",    HTTP_POST, handle_module_run);
    server.on("/api/module/select", HTTP_POST, handle_module_select);
    server.on("/api/module/active", HTTP_GET,  handle_module_active);
}
