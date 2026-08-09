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
#include "library_module.h"
#include <string.h>

// Dichiarato in rom_ml1.cpp e rom_XX.cpp. — Declared in rom_ml1.cpp and rom_XX.cpp.
extern const LibraryModule ML1_MODULE;   // Modulo -1- Master Library — Module -1- Master Library
extern const LibraryModule ST_MODULE;   // Modulo -2- Applied Statistics — Module -2- Applied Statistics
extern const LibraryModule RE_MODULE;   // Modulo -3- Real Estate — Module -3- Real Estate
extern const LibraryModule SV_MODULE;   // Modulo -4- Surveying — Module -4- Surveying
extern const LibraryModule NA_MODULE;   // Modulo -5- Marine Navigation — Module -5- Marine Navigation
extern const LibraryModule AV_MODULE;   // Modulo -6- Aviation — Module -6- Aviation
extern const LibraryModule LL_MODULE;   // Modulo -7- Leisure Library — Module -7- Leisure Library
extern const LibraryModule SA_MODULE;   // Modulo -8- Securities Analysis — Module -8- Securities Analysis
extern const LibraryModule EE_MODULE;   // Modulo -9- Electrical Engineering — Module -9- Electrical Engineering
extern const LibraryModule FM_MODULE;   // Modulo -10- Agriculture — Module -10- Agriculture
extern const LibraryModule MU_MODULE;   // Modulo -11- Math Utilities — Module -11- Math Utilities
extern const LibraryModule PH_MODULE;   // Modulo -12- Photography — Module -12- Photography
extern const LibraryModule RP_MODULE;   // Modulo -13- RPN — Module -13- RPN
extern const LibraryModule SE_MODULE;   // Modulo -14- Structural Engineering — Module -14- Structural Engineering

// ─── Registro moduli compilati (ordinati per numero) — Registry of compiled modules (sorted by number) ──────
static const LibraryModule* const LIBRARY_REGISTRY[] = {
    &ML1_MODULE,
    &ST_MODULE,
    &RE_MODULE,
    &SV_MODULE,
    &NA_MODULE,
    &AV_MODULE,
    &LL_MODULE,
    &SA_MODULE,
    &EE_MODULE,
    &FM_MODULE,
    &MU_MODULE,
    &PH_MODULE,
    &RP_MODULE,
    &SE_MODULE,
};
#define LIBRARY_REGISTRY_COUNT \
    (int)(sizeof(LIBRARY_REGISTRY) / sizeof(LIBRARY_REGISTRY[0]))

int library_module_count(void) {
    return LIBRARY_REGISTRY_COUNT;
}

const LibraryModule* library_module_at(int index) {
    if (index < 0 || index >= LIBRARY_REGISTRY_COUNT) return nullptr;
    return LIBRARY_REGISTRY[index];
}

const LibraryModule* library_module_find(const char *id) {
    if (!id) return nullptr;
    for (int i = 0; i < LIBRARY_REGISTRY_COUNT; i++) {
        if (strcmp(LIBRARY_REGISTRY[i]->id, id) == 0) return LIBRARY_REGISTRY[i];
    }
    return nullptr;
}

// ─── Modulo attivo — Active module ──────────────────────────────────────────
static const LibraryModule *g_active_module = nullptr;

__attribute__((weak)) void library_on_module_changed(const char *id) { (void)id; }

bool library_set_active(const char *id) {
    if (!id || id[0] == '\0') {
        if (g_active_module != nullptr) { g_active_module = nullptr; library_on_module_changed(""); }
        return true;   // "nessun modulo" è sempre una scelta valida — "no module" is always a valid choice
    }
    const LibraryModule *m = library_module_find(id);
    if (!m) return false;
    if (m != g_active_module) { g_active_module = m; library_on_module_changed(id); }
    return true;
}

const LibraryModule* library_get_active(void) {
    return g_active_module;
}

// ─── Richiamo programma: cerca indirizzo/lunghezza/titolo — Program call: looks up address/length/title ──
bool library_find_program(uint8_t page, uint16_t *out_addr,
                           uint16_t *out_len, const char **out_title) {
    if (!g_active_module) return false;

    for (int i = 0; i < g_active_module->program_count; i++) {
        const LibraryProgram *prog = &g_active_module->programs[i];
        if (prog->num != page) continue;
        if (prog->addr + prog->len > g_active_module->rom_size) return false;
        if (out_addr)  *out_addr  = prog->addr;
        if (out_len)   *out_len   = prog->len;
        if (out_title) *out_title = prog->title;
        return true;
    }
    return false;
}