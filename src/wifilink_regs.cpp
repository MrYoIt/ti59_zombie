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
#include "wifilink_regs.h"
#include "wifilink.h"
#include "tms1500.h"
#include <Arduino.h>
#include <WebServer.h>

static void handle_regs_get() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    if (!g_cpu) {
        server.send(500, "application/json", "{\"error\":\"no cpu\"}");
        return;
    }
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"pc\":%u,\"sp\":%u,\"flags\":{\"error\":%s,\"run\":%s,\"lrn\":%s,\"inv\":%s}}",
        g_cpu->pc, g_cpu->sp,
        g_cpu->flags.error ? "true" : "false",
        g_cpu->flags.run   ? "true" : "false",
        g_cpu->flags.lrn   ? "true" : "false",
        g_cpu->flags.inv   ? "true" : "false"
    );
    server.send(200, "application/json", buf);
}

static void handle_regs_post() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", "{\"ok\":true,\"note\":\"stub\"}");
}

void register_regs_routes(void) {
    server.on("/api/regs", HTTP_GET,  handle_regs_get);
    server.on("/api/regs", HTTP_POST, handle_regs_post);
}
