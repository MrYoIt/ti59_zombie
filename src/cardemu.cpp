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
#include "cardemu.h"
#include "config.h"
#include "tms1500.h"   // REG_WIDTH — necessario per non troncare i registri BCD (v. sotto)
#include <Arduino.h>
#include <SPIFFS.h>
#include <string.h>
#include <stdio.h>

static void slot_path(uint8_t slot, char *path) {
    snprintf(path, 24, "/card_%02d.json", slot);
}

static int hex_nibble(uint8_t n) {
    return n < 10 ? '0'+n : 'a'+(n-10);
}

static int bytes_to_hex(const uint8_t *data, int len, char *out, int max) {
    int pos = 0;
    for (int i = 0; i < len && pos < max-2; i++) {
        out[pos++] = hex_nibble((data[i] >> 4) & 0xF);
        out[pos++] = hex_nibble(data[i] & 0xF);
    }
    out[pos] = 0;
    return pos;
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static void hex_to_bytes(const char *hex, uint8_t *out, int *len) {
    int i = 0;
    while (hex[0] && hex[1]) {
        out[i++] = (hex_val(hex[0]) << 4) | hex_val(hex[1]);
        hex += 2;
    }
    *len = i;
}

static void hex_to_bytes_spaced(const char *hex, uint8_t *out, int *len, int max_out) {
    int i = 0;
    const char *p = hex;
    while (*p) {
        while (*p == ' ') p++;
        if (!p[0] || !p[1]) break;
        if (p[0] == ' ' ) { p++; continue; }
        out[i++] = (hex_val(p[0]) << 4) | hex_val(p[1]);
        p += 2;
        if (i >= max_out) break;
    }
    *len = i;
}

void cardemu_init(CardEmuState *card) {
    memset(card, 0, sizeof(CardEmuState));
    card->active_slot = -1;
    card->last_written_slot = 0xff;  // nessuno

    // Niente auto-format qui: begin(true) su partizione vuota/corrotta
    // blocca il boot per 10-20s di erase (console morta, garbage CDC USB).
    // La formattazione è manuale: POST /api/fs/format (web).
    if (!SPIFFS.begin(false)) {
        Serial.println("[CARD] SPIFFS non montato — schede non disponibili finché non si formatta");
    } else {
        Serial.println("[CARD] SPIFFS OK");
    }

    // Prima leggeva solo il nome (via conteggio di virgolette) e
    // lasciava prog_a/prog_len_a a zero finché non si chiamava
    // esplicitamente cardemu_read() per quello slot — cosa che
    // handle_card_get() e cardemu_list() in wifilink.cpp non fanno
    // mai, leggendo sempre direttamente card->slots[] in RAM. Il
    // risultato: dopo ogni riavvio (flash o semplice reset) il nome
    // della scheda compariva ma il programma risultava vuoto, finché
    // qualcosa non veniva salvato di nuovo. Ora si carica l'intero
    // JSON via cardemu_load_from_json(), la stessa funzione già usata
    // da cardemu_read(), così tutti gli slot sono completi fin da
    // subito dopo il boot.
    card->num_slots = 0;
    char path[24];
    for (int s = 0; s < CARD_SLOT_COUNT; s++) {
        slot_path(s, path);
        if (SPIFFS.exists(path)) {
            File f = SPIFFS.open(path, "r");
            if (f) {
                size_t fsize = f.size();
                char *buf = (char*)malloc(fsize + 1);
                if (buf) {
                    int n = f.readBytes(buf, fsize);
                    buf[n] = 0;
                    f.close();
                    if (cardemu_load_from_json(card, buf, s)) {
                        card->num_slots++;
                    }
                    free(buf);
                } else {
                    f.close();
                }
            }
        }
    }
    Serial.printf("[CARD] %d schede trovate\n", card->num_slots);
    pinMode(PIN_CARD_SENSE, INPUT_PULLUP);
}

bool cardemu_sense(CardEmuState *card) {
    bool current = (digitalRead(PIN_CARD_SENSE) == LOW);
    card->sense_rising = (current && !card->sense);
    card->sense = current;
    return current;
}

bool cardemu_read(CardEmuState *card, TMS1500_State *cpu, uint8_t slot) {
    if (slot >= CARD_SLOT_COUNT) return false;
    char path[24];
    slot_path(slot, path);
    if (!SPIFFS.exists(path)) return false;

    File f = SPIFFS.open(path, "r");
    if (!f) return false;

    size_t fsize = f.size();
    char *buf = (char*)malloc(fsize + 1);
    if (!buf) { f.close(); return false; }
    int n = f.readBytes(buf, fsize);
    buf[n] = 0;
    f.close();

    bool ok = cardemu_load_from_json(card, buf, slot);
    free(buf);
    return ok;
}

bool cardemu_persist_slot(CardEmuState *card, uint8_t slot) {
    if (slot >= CARD_SLOT_COUNT) return false;

    // Dimensione: nome + overhead JSON + prog_a/prog_b (fino a
    // CARD_PROG_BYTES*2 caratteri hex ciascuno) + regs
    // (CARD_REGS_BYTES*2 caratteri hex). Con margine.
    int cap = CARD_NAME_LEN + CARD_PROG_BYTES*4 + CARD_REGS_BYTES*2 + 128;
    char *json = (char*)malloc(cap);
    if (!json) return false;
    int jlen = cardemu_save_to_json(card, slot, json, cap);
    if (jlen <= 0) { free(json); return false; }

    char path[24];
    slot_path(slot, path);
    File f = SPIFFS.open(path, "w");
    if (!f) { free(json); return false; }
    f.write((uint8_t*)json, jlen);
    f.close();
    free(json);
    return true;
}

// AGGIORNAMENTO: il gap descritto qui in precedenza (i registri dati
// non venivano salvati nel JSON della scheda, solo in state.bin) è
// stato colmato: cardemu_save_to_json()/cardemu_load_from_json() ora
// serializzano anche "regs". I buffer di lettura/scrittura file sono
// stati spostati su heap (malloc) perché con regs inclusi il JSON di
// una scheda piena può superare 5 KB, troppo per uno stack fisso.
bool cardemu_write(CardEmuState *card, TMS1500_State *cpu, uint8_t slot,
                   const char *name) {
    if (slot >= CARD_SLOT_COUNT) return false;
    bool was_valid = card->slots[slot].valid;

    strncpy(card->slots[slot].name, name, CARD_NAME_LEN-1);
    uint16_t bytes_a = (cpu->prog_len > 480) ? 480 : cpu->prog_len;
    memcpy(card->slots[slot].prog_a, cpu->prog, bytes_a);
    card->slots[slot].prog_len_a = bytes_a;

    // BUGFIX: prima si copiavano solo i primi 8 nibble di ciascun
    // registro (n[0..7]) invece dei REG_WIDTH=18 reali (n[0..1]=segno,
    // n[2..3]=esponente, n[4..16]=13 cifre di mantissa). Il risultato
    // era un troncamento silenzioso a ~4 cifre di mantissa salvate su
    // scheda, con perdita delle altre 9. Va di pari passo con la
    // dimensione di card->slots[slot].regs[], che deve essere
    // 100 * REG_WIDTH byte in cardemu.h (vedi nota in fondo al file).
    for (int r = 0; r < 100; r++) {
        memcpy(&card->slots[slot].regs[r*REG_WIDTH], cpu->ram[r].n, REG_WIDTH);
    }
    if (!cardemu_persist_slot(card, slot)) return false;
    card->slots[slot].valid = true;

    if (!was_valid) card->num_slots++;   // non contare due volte una sovrascrittura
    card->last_written_slot = slot;
    Serial.printf("[CARD] Scritto slot %d: %s\n", slot, name);
    return true;
}

bool cardemu_load_to_cpu(CardEmuState *card, TMS1500_State *cpu, uint8_t slot) {
    if (slot >= CARD_SLOT_COUNT) return false;
    if (!card->slots[slot].valid) return false;

    // Carica programma nella CPU
    tms1500_load_prog(cpu, card->slots[slot].prog_a, card->slots[slot].prog_len_a);

    // Copia i 100 registri dati (REG_WIDTH nibble ciascuno — v. nota
    // gemella in cardemu_write() sul troncamento a 8 byte corretto qui)
    for (int r = 0; r < 100; r++) {
        memcpy(cpu->ram[r].n, &card->slots[slot].regs[r * REG_WIDTH], REG_WIDTH);
    }

    card->active_slot = slot;
    tms1500_mark_prog_dirty();
    Serial.printf("[CARD] Caricato slot %d in CPU: \"%s\" (%d passi)\n",
                  slot, card->slots[slot].name, card->slots[slot].prog_len_a);
    return true;
}

bool cardemu_import_text(CardEmuState *card, const char *text, uint8_t slot) {
    if (slot >= CARD_SLOT_COUNT) return false;
    bool was_valid = card->slots[slot].valid;
    if (!cardemu_load_from_json(card, text, slot)) return false;
    if (!cardemu_persist_slot(card, slot)) return false;
    if (!was_valid) card->num_slots++;
    Serial.printf("[CARD] Importato da file di testo in slot %d: %s\n",
                  slot, card->slots[slot].name);
    return true;
}

bool cardemu_delete(CardEmuState *card, uint8_t slot) {
    char path[24];
    slot_path(slot, path);
    SPIFFS.remove(path);
    card->slots[slot].valid = false;
    memset(card->slots[slot].name, 0, CARD_NAME_LEN);
    if (card->num_slots > 0) card->num_slots--;  // solo se > 0 per evitare underflow
    return true;
}

void cardemu_list(CardEmuState *card, char *out_json, int max_len) {
    int pos = 0;
    pos += snprintf(out_json+pos, max_len-pos, "[");
    bool first = true;
    for (int s = 0; s < CARD_SLOT_COUNT; s++) {
        if (!card->slots[s].valid) continue;
        if (!first) pos += snprintf(out_json+pos, max_len-pos, ",");
        pos += snprintf(out_json+pos, max_len-pos,
                        "{\"slot\":%d,\"name\":\"%s\",\"steps\":%d}",
                        s, card->slots[s].name,
                        card->slots[s].prog_len_a);
        first = false;
    }
    pos += snprintf(out_json+pos, max_len-pos, "]");
}

bool cardemu_load_from_json(CardEmuState *card, const char *json, uint8_t slot) {
    CardSlot *s = &card->slots[slot];
    memset(s, 0, sizeof(CardSlot));

    const char *p = strstr(json, "\"name\":\"");
    if (p) {
        p += 8;
        int i = 0;
        while (*p && *p != '"' && i < CARD_NAME_LEN-1)
            s->name[i++] = *p++;
        s->name[i] = 0;
    }

    p = strstr(json, "\"prog_a\":\"");
    if (p) {
        p += 10;
        char hex[CARD_PROG_BYTES*3+2];
        int i = 0;
        while (*p && *p != '"' && i < (int)sizeof(hex)-1)
            hex[i++] = *p++;
        hex[i] = 0;
        int len;
        hex_to_bytes_spaced(hex, s->prog_a, &len, CARD_PROG_BYTES);
        s->prog_len_a = len;
    }

    p = strstr(json, "\"prog_b\":\"");
    if (p) {
        p += 10;
        char hex[CARD_PROG_BYTES*3+2];
        int i = 0;
        while (*p && *p != '"' && i < (int)sizeof(hex)-1)
            hex[i++] = *p++;
        hex[i] = 0;
        int len;
        hex_to_bytes_spaced(hex, s->prog_b, &len, CARD_PROG_BYTES);
        s->prog_len_b = len;
    }

    // Registri dati (100 × REG_WIDTH nibble). Buffer heap-allocato:
    // ~5.4 KB (CARD_REGS_BYTES*3+2) è troppo per essere messo sullo
    // stack insieme al resto in un contesto embedded.
    p = strstr(json, "\"regs\":\"");
    if (p) {
        p += 8;
        int hexcap = CARD_REGS_BYTES*3+2;
        char *hex = (char*)malloc(hexcap);
        if (hex) {
            int i = 0;
            while (*p && *p != '"' && i < hexcap-1)
                hex[i++] = *p++;
            hex[i] = 0;
            int len;
            hex_to_bytes_spaced(hex, s->regs, &len, CARD_REGS_BYTES);
            free(hex);
        }
    }
    // Se il JSON non contiene "regs" (es. scheda salvata col firmware
    // precedente, prima di questo fix), s->regs resta a zero — già
    // azzerato dal memset(s,0,...) sopra: comportamento sicuro, non un
    // crash, semplicemente i registri dati non vengono ripristinati.

    s->valid = (s->prog_len_a > 0 || strlen(s->name) > 0);
    return s->valid;
}

bool cardemu_import_batch(CardEmuState *card, const char *path) {
    File f = SPIFFS.open(path, "r");
    if (!f) {
        Serial.printf("[CARD] Import: file %s non trovato\n", path);
        return false;
    }

    size_t fsize = f.size();
    char *buf = (char*)malloc(fsize + 1);
    if (!buf) { f.close(); return false; }
    f.readBytes(buf, fsize);
    buf[fsize] = 0;
    f.close();

    int imported = 0;
    const char *p = buf;
    while ((p = strstr(p, "\"slot\":")) != NULL) {
        int slot = atoi(p + 7);
        if (slot < 0 || slot >= CARD_SLOT_COUNT) { p += 7; continue; }

        const char *obj_start = p;
        while (obj_start > buf && *obj_start != '{') obj_start--;

        int depth = 0;
        const char *obj_end = obj_start;
        do {
            if (*obj_end == '{') depth++;
            else if (*obj_end == '}') depth--;
            obj_end++;
        } while (depth > 0 && *obj_end);

        int obj_len = obj_end - obj_start;
        char *obj_buf = (char*)malloc(obj_len + 1);
        if (obj_buf) {
            memcpy(obj_buf, obj_start, obj_len);
            obj_buf[obj_len] = 0;
            if (cardemu_load_from_json(card, obj_buf, slot)) {
                int cap = CARD_NAME_LEN + CARD_PROG_BYTES*4 + CARD_REGS_BYTES*2 + 128;
                char *json_out = (char*)malloc(cap);
                if (json_out) {
                    int jlen = cardemu_save_to_json(card, slot, json_out, cap);
                    char outpath[24];
                    snprintf(outpath, sizeof(outpath), "/card_%02d.json", slot);
                    File of = SPIFFS.open(outpath, "w");
                    if (of) {
                        of.write((uint8_t*)json_out, jlen);
                        of.close();
                        imported++;
                        Serial.printf("[CARD] Import slot %02d: %s (A:%d B:%d step)\n",
                                      slot, card->slots[slot].name,
                                      card->slots[slot].prog_len_a,
                                      card->slots[slot].prog_len_b);
                    }
                    free(json_out);
                }
            }
            free(obj_buf);
        }
        p = obj_end;
    }

    free(buf);
    card->num_slots += imported;
    Serial.printf("[CARD] Import completato: %d schede caricate\n", imported);
    return imported > 0;
}

int cardemu_save_to_json(CardEmuState *card, uint8_t slot, char *out, int max) {
    CardSlot *s = &card->slots[slot];
    char hex_a[CARD_PROG_BYTES*2+2];
    char hex_b[CARD_PROG_BYTES*2+2];
    bytes_to_hex(s->prog_a, s->prog_len_a, hex_a, sizeof(hex_a));
    bytes_to_hex(s->prog_b, s->prog_len_b, hex_b, sizeof(hex_b));

    // hex_regs è ~3.6 KB (CARD_REGS_BYTES*2+2): heap-allocato per non
    // gravare sullo stack insieme ai due buffer sopra.
    int regs_hexcap = CARD_REGS_BYTES*2+2;
    char *hex_regs = (char*)malloc(regs_hexcap);
    if (!hex_regs) return 0;
    bytes_to_hex(s->regs, CARD_REGS_BYTES, hex_regs, regs_hexcap);

    int written = snprintf(out, max,
        "{\"name\":\"%s\","
        "\"prog_len_a\":%d,"
        "\"prog_len_b\":%d,"
        "\"prog_a\":\"%s\","
        "\"prog_b\":\"%s\","
        "\"regs\":\"%s\"}",
        s->name, s->prog_len_a, s->prog_len_b, hex_a, hex_b, hex_regs);

    free(hex_regs);
    return written;
}

// ─── Stato persistente CPU (memoria solid state) ──────────
//
// Formato binario fixed-size, scritto atomicamente via rename:
//   2 B  magic     0x3560 (bumped da 0x3559 per il cambio formato registri, v. sotto)
//   2 B  prog_len
//   2 B  prog_pc
// 960 B  prog[960]
//  20 B  labels[10] × uint16_t   (0xFFFF = vuota)
//1800 B  ram[100] × REG_WIDTH (18 nibble/registro — prima erano 8,
//        troncando mantissa/esponente: v. BUGFIX in cardemu_write())
//   1 B  last_written_slot
// ─────────────────────────────────
// ~2787 B totali

#define STATE_FILE "/state.bin"
#define STATE_TMP  "/state.tmp"
// BUGFIX: bump del magic (era 0x3559) perché il layout dei registri è
// cambiato da 8 a REG_WIDTH(18) byte ciascuno. Con lo stesso magic un
// vecchio state.bin verrebbe letto con gli offset sbagliati (i 100
// registri occupavano 800 B, ora 1800 B): il byte finale
// last_written_slot e, prima ancora, la coda dei registri finirebbero
// disallineati. Cambiando il magic i vecchi file vengono scartati in
// modo pulito da cardemu_load_persistent() invece di essere
// interpretati male.
#define STATE_MAGIC  0x3560

static void state_file_path(char *buf, size_t sz) {
    snprintf(buf, sz, "%s", STATE_FILE);
}

void cardemu_save_persistent(CardEmuState *card, TMS1500_State *cpu) {
    File f = SPIFFS.open(STATE_TMP, "w");
    if (!f) return;

    uint16_t magic = STATE_MAGIC;
    uint16_t prog_len = cpu->prog_len;
    uint16_t prog_pc  = cpu->prog_pc;
    uint16_t labels[10];
    tms1500_get_labels(labels);

    f.write((uint8_t*)&magic, 2);
    f.write((uint8_t*)&prog_len, 2);
    f.write((uint8_t*)&prog_pc, 2);
    f.write(cpu->prog, PROG_SIZE);
    f.write((uint8_t*)labels, 20);
    for (int r = 0; r < 100; r++)
        f.write((uint8_t*)cpu->ram[r].n, REG_WIDTH);
    uint8_t slot = (card->last_written_slot < CARD_SLOT_COUNT)
                   ? card->last_written_slot : 0xff;
    f.write(&slot, 1);
    f.close();

    SPIFFS.remove(STATE_FILE);
    SPIFFS.rename(STATE_TMP, STATE_FILE);

    // Il salvataggio è riuscito: il programma ora corrisponde al file
    tms1500_mark_prog_saved();
}

void cardemu_load_persistent(CardEmuState *card, TMS1500_State *cpu) {
    if (!SPIFFS.exists(STATE_FILE)) return;
    File f = SPIFFS.open(STATE_FILE, "r");
    if (!f) return;
    if (f.size() < 7) { f.close(); return; }  // almeno magic + prog_len + prog_pc

    uint16_t magic;
    if (f.read((uint8_t*)&magic, 2) != 2 || magic != STATE_MAGIC) {
        f.close(); return;
    }

    uint16_t prog_len, prog_pc;
    if (f.read((uint8_t*)&prog_len, 2) != 2) { f.close(); return; }
    if (f.read((uint8_t*)&prog_pc, 2) != 2)  { f.close(); return; }

    if (prog_len > PROG_SIZE) prog_len = PROG_SIZE;
    uint8_t buf[PROG_SIZE];
    int n = f.read(buf, PROG_SIZE);
    if (n == PROG_SIZE) {
        tms1500_load_prog(cpu, buf, prog_len);
        cpu->prog_pc = (prog_pc < prog_len) ? prog_pc : 0;

        uint16_t labels[10];
        if (f.read((uint8_t*)labels, 20) == 20) {
            tms1500_set_labels(labels);
        }

        for (int r = 0; r < 100 && f.available() >= REG_WIDTH; r++) {
            f.read((uint8_t*)cpu->ram[r].n, REG_WIDTH);
        }

        uint8_t slot;
        if (f.read(&slot, 1) == 1 && slot < CARD_SLOT_COUNT) {
            card->last_written_slot = slot;
            Serial.printf("[CARD] Stato ripristinato: slot %d, %d passi\n", slot, prog_len);
        } else {
            Serial.printf("[CARD] Stato ripristinato: %d passi\n", prog_len);
        }
    }
    f.close();
}