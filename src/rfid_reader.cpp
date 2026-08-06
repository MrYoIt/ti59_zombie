/*
 * TI-59 Zombie — emulatore TI-59 su ESP32-S3 (TMS1500) — TI-59 emulator on ESP32-S3 (TMS1500)
 * Copyright (C) 2026 Maurizio Petruccioli (MrYo)
 *
 * Questo programma è software libero: puoi ridistribuirlo e/o modificarlo — This program is free software: you can redistribute it and/or modify
 * nei termini della GNU General Public License pubblicata dalla — it under the terms of the GNU General Public License as published by
 * Free Software Foundation, versione 3 della Licenza, o (a tua scelta) — the Free Software Foundation, either version 3 of the License, or
 * qualsiasi versione successiva — (at your option) any later version.
 *
 * Questo programma è distribuito nella speranza che sia utile — This program is distributed in the hope that it will be useful,
 * ma SENZA ALCUNA GARANZIA; senza nemmeno la garanzia implicita di — but WITHOUT ANY WARRANTY; without even the implied warranty of
 * COMMERCIABILITÀ o IDONEITÀ A UNO SCOPO PARTICOLARE. Vedi la — MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License per maggiori dettagli — GNU General Public License for more details.
 *
 * Dovresti aver ricevuto una copia della GNU General Public License — You should have received a copy of the GNU General Public License
 * unitamente a questo programma. In caso contrario, vedi — along with this program.  If not, see <https://www.gnu.org/licenses/>. — testo licenza GNU GPL — GNU GPL license text
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "rfid_reader.h"
#include "config.h"
#include <Arduino.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <string.h>
#include <stdio.h>
#include <MFRC522.h>

// ─── Stato modulo — module state ───────────────────────────
static bool      g_rfid_present = false;
static bool      g_write_armed  = false;
static int       g_armed_slot   = -1;
static uint16_t  g_eject_ms     = RFD_EJECT_MS;   // regolabile via web (NVS) — adjustable via web (NVS)

static MFRC522   g_rfid(PIN_RFID_CS, PIN_RFID_RST);

// ─── Mappa UID -> slot (fallback) — UID -> slot map (fallback) ──
typedef struct {
    uint8_t uid[7];
    uint8_t uid_len;
    int     slot;
} UidMapEntry;

static UidMapEntry g_map[RFD_MAP_MAX];
static int g_map_n = 0;

// ─── Alimentazione modulo — module power ───────────────────
static void rfid_power(bool on) {
    digitalWrite(PIN_RFID_PWR, on ? HIGH : LOW);
    if (on) delay(RFD_PWR_SETTLE_MS);
}

// ─── Scan tag (bloccante, ~RFD_SCAN_ATTEMPTS × 100ms) — tag scan (blocking, ~RFD_SCAN_ATTEMPTS × 100ms) ──
// Il modulo deve essere già alimentato e PCD_Init() chiamato. — The module must already be powered and PCD_Init() called.
static bool rfid_scan_uid(uint8_t *uid, uint8_t *uid_len) {
    for (int i = 0; i < RFD_SCAN_ATTEMPTS; i++) {
        if (g_rfid.PICC_IsNewCardPresent()) {
            if (g_rfid.PICC_ReadCardSerial()) {
                *uid_len = g_rfid.uid.size;
                if (*uid_len > 7) *uid_len = 7;
                memcpy(uid, g_rfid.uid.uidByte, *uid_len);
                return true;
            }
        }
        delay(100);
    }
    return false;
}

// ─── Leggi slot dal tag (pagina RFD_TAG_SLOT_PAGE, 3 cifre ASCII) — read slot from tag (page RFD_TAG_SLOT_PAGE, 3 ASCII digits) ──
static bool rfid_tag_read_slot(int *slot) {
    byte buf[18];
    byte bufSize = sizeof(buf);
    MFRC522::StatusCode st = g_rfid.MIFARE_Read(RFD_TAG_SLOT_PAGE, buf, &bufSize);
    if (st != MFRC522::STATUS_OK || bufSize < 3) return false;
    if (buf[0] == ' ' && buf[1] == ' ' && buf[2] == ' ') return false;
    int v = (buf[0] - '0') * 100 + (buf[1] - '0') * 10 + (buf[2] - '0');
    if (v < 0 || v >= CARD_SLOT_COUNT) return false;
    *slot = v;
    return true;
}

// ─── Scrivi slot nel tag (NTAG213 WRITE 0xA2: opcode + pagina + 4 byte dati) — write slot to tag (NTAG213 WRITE 0xA2: opcode + page + 4 data bytes) ──
static bool rfid_tag_write_slot(int slot) {
    byte cmd[6] = {
        0xA2, RFD_TAG_SLOT_PAGE,
        (byte)('0' + (slot / 100) % 10),
        (byte)('0' + (slot / 10) % 10),
        (byte)('0' + slot % 10),
        ' '
    };
    byte back[4];
    byte backLen = sizeof(back);
    MFRC522::StatusCode st = g_rfid.PCD_TransceiveData(cmd, 6, back, &backLen, 0, 0, false);
    return (st == MFRC522::STATUS_OK);
}

// ─── Espulsione scheda (motore) — card ejection (motor) ────
static void rfid_eject(void) {
    digitalWrite(PIN_CARD_MOTOR, HIGH);
    delay(g_eject_ms);
    digitalWrite(PIN_CARD_MOTOR, LOW);
}

void rfid_reader_set_eject_ms(uint16_t ms) {
    if (ms < 50) ms = 50;
    if (ms > 3000) ms = 3000;
    g_eject_ms = ms;
    Serial.printf("[RFID] Durata espulsione motore: %u ms\n", (unsigned)g_eject_ms);
}

uint16_t rfid_reader_get_eject_ms(void) {
    return g_eject_ms;
}

// ─── Mappa UID (SPIFFS /rfid_map.json) — UID map (SPIFFS /rfid_map.json) ─
static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static void rfid_map_load(void) {
    g_map_n = 0;
    if (!SPIFFS.exists(RFD_MAP_PATH)) return;
    File f = SPIFFS.open(RFD_MAP_PATH, "r");
    if (!f) return;
    size_t sz = f.size();
    if (sz > 4096) sz = 4096;
    char *buf = (char*)malloc(sz + 1);
    if (!buf) { f.close(); return; }
    int n = f.readBytes(buf, sz);
    buf[n] = 0;
    f.close();

    const char *p = buf;
    while ((p = strstr(p, "\"uid\":\"")) != NULL && g_map_n < RFD_MAP_MAX) {
        const char *u = p + 7;         // dopo "uid":" — after "uid":
        uint8_t uid[7];
        int ubytes = 0;
        while (*u && *u != '"' && ubytes < 7) {
            if (u[0] && u[1]) {
                uid[ubytes++] = (hexval(u[0]) << 4) | hexval(u[1]);
                u += 2;
            } else break;
        }
        const char *s = strstr(u, "\"slot\":");
        if (s && ubytes > 0) {
            int slot = atoi(s + 7);
            if (slot >= 0 && slot < CARD_SLOT_COUNT) {
                memcpy(g_map[g_map_n].uid, uid, ubytes);
                g_map[g_map_n].uid_len = ubytes;
                g_map[g_map_n].slot = slot;
                g_map_n++;
            }
        }
        p = u;
    }
    free(buf);
}

static bool rfid_map_save(void) {
    File f = SPIFFS.open(RFD_MAP_PATH, "w");
    if (!f) return false;
    f.print("[");
    for (int i = 0; i < g_map_n; i++) {
        if (i) f.print(",");
        f.printf("{\"uid\":\"");
        for (int k = 0; k < g_map[i].uid_len; k++)
            f.printf("%02x", g_map[i].uid[k]);
        f.printf("\",\"slot\":%d}", g_map[i].slot);
    }
    f.print("]");
    f.close();
    return true;
}

// ─── API pubbliche — public API ────────────────────────────
void rfid_reader_init(void) {
    pinMode(PIN_RFID_PWR, OUTPUT);
    digitalWrite(PIN_RFID_PWR, LOW);
    pinMode(PIN_CARD_MOTOR, OUTPUT);
    digitalWrite(PIN_CARD_MOTOR, LOW);

    rfid_map_load();

    // Auto-detect: accende il modulo e interroga la versione del chip. — Auto-detect: powers the module and queries the chip version.
    // 0x00/0xFF = nessun modulo rispondente (o assente). — 0x00/0xFF = no responding module (or absent).
    rfid_power(true);
    SPI.begin(PIN_SPI_CLK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_RFID_CS);
    g_rfid.PCD_Init();
    // BUGFIX: PCD_Init() della libreria rilancia SPI.begin() con i pin di — BUGFIX: the library's PCD_Init() re-runs SPI.begin() with the default
    // default dell'S3 (SCK=12, MISO=13, MOSI=11, SS=10) che su questo — S3 pins (SCK=12, MISO=13, MOSI=11, SS=10) which on this
    // progetto sono le COLONNE della tastiera: ripristiniamo subito il — project are the keyboard COLUMNS: we immediately restore the
    // nostro bus SPI prima di qualsiasi lettura. — our SPI bus before any read.
    SPI.begin(PIN_SPI_CLK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_RFID_CS);
    byte ver = g_rfid.PCD_ReadRegister(MFRC522::VersionReg);
    g_rfid_present = (ver != 0x00 && ver != 0xFF);
    if (g_rfid_present) {
        g_rfid.PCD_SetAntennaGain(g_rfid.RxGain_max);
        Serial.printf("[RFID] MFRC522 rilevato (v%02X), %d mappe UID\n",
                      ver, g_map_n);
    } else {
        Serial.printf("[RFID] MFRC522 non rilevato (v%02X) - lettore disabilitato\n", ver);
    }
    rfid_power(false);   // spento a riposo, si accende solo per le operazioni — off at rest, powered on only for operations
}

bool rfid_reader_enabled(void) {
    return g_rfid_present;
}

void rfid_reader_arm_write(int slot) {
    g_write_armed = true;
    g_armed_slot  = slot;
    Serial.printf("[RFID] Scrittura armata: attendere inserimento scheda (slot %d)\n", slot);
}

bool rfid_reader_write_armed(void) {
    return g_write_armed;
}

// Trova il primo slot libero (stessa regola di tms1500_on_physical_write) — Finds the first free slot (same rule as tms1500_on_physical_write)
static int rfid_next_free(const CardEmuState *card) {
    for (int s = 0; s < CARD_SLOT_COUNT; s++)
        if (!card->slots[s].valid) return s;
    return -1;
}

bool rfid_reader_handle_insert(CardEmuState *card, TMS1500_State *cpu,
                               rfid_lock_fn lock, rfid_unlock_fn unlock) {
    if (!g_rfid_present) return false;

    rfid_power(true);
    g_rfid.PCD_Init();      // il modulo è rimasto spento: re-init — the module was off: re-init
    SPI.begin(PIN_SPI_CLK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_RFID_CS);  // v. BUGFIX in init — see BUGFIX in init
    if (g_rfid.PCD_ReadRegister(MFRC522::VersionReg) == 0x00 ||
        g_rfid.PCD_ReadRegister(MFRC522::VersionReg) == 0xFF) {
        Serial.println("[RFID] Modulo non risponde dopo accensione");
        rfid_power(false);
        return true;        // gestito (tentato) — non si ripiega sul vecchio flusso
    }

    uint8_t uid[7] = {0};
    uint8_t uid_len = 0;
    bool got = rfid_scan_uid(uid, &uid_len);

    if (g_write_armed) {
        g_write_armed = false;
        int slot = (g_armed_slot >= 0) ? g_armed_slot : rfid_next_free(card);
        g_armed_slot = -1;
        if (got && slot >= 0) {
            char name[CARD_NAME_LEN];
            int n = 1;
            bool taken;
            do {
                snprintf(name, sizeof(name), "mc_%03d", n);
                taken = false;
                for (int s = 0; s < CARD_SLOT_COUNT; s++) {
                    if (card->slots[s].valid && strcmp(card->slots[s].name, name) == 0) {
                        taken = true; break;
                    }
                }
                n++;
            } while (taken && n < 1000);

            if (lock) lock();
            bool ok = cardemu_write(card, cpu, (uint8_t)slot, name);
            if (ok) tms1500_mark_prog_saved();
            if (unlock) unlock();
            if (ok) {
                bool tOk = rfid_tag_write_slot(slot);
                bool mOk = rfid_map_set_uid(uid, uid_len, slot);
                Serial.printf("[RFID] WRITE slot %d \"%s\" tag=%s mappa=%s\n",
                              slot, name, tOk ? "ok" : "FAIL", mOk ? "ok" : "FAIL");
            } else {
                Serial.println("[RFID] WRITE fallito (cardemu_write)");
            }
        } else {
            Serial.printf("[RFID] WRITE: nessun tag (%s) o nessuno slot libero\n",
                          got ? "letto" : "non letto");
        }
    } else {
        if (got) {
            int slot = -1;
            if (!rfid_tag_read_slot(&slot)) slot = rfid_uid_lookup(uid, uid_len);
            if (slot >= 0 && slot < CARD_SLOT_COUNT && card->slots[slot].valid) {
                if (lock) lock();
                bool ok = cardemu_read(card, cpu, (uint8_t)slot);
                if (ok) ok = cardemu_load_to_cpu(card, cpu, (uint8_t)slot);
                if (unlock) unlock();
                Serial.printf("[RFID] READ: slot %d %s (%s)\n", slot,
                              card->slots[slot].name, ok ? "caricato" : "ERRORE");
            } else {
                Serial.printf("[RFID] Tag non associato (slot=%d) — caricala da web\n", slot);
            }
        } else {
            Serial.println("[RFID] Nessun tag rilevato all'inserimento");
        }
    }

    rfid_eject();
    rfid_power(false);
    return true;
}

int rfid_uid_lookup(const uint8_t *uid, uint8_t len) {
    for (int i = 0; i < g_map_n; i++) {
        if (g_map[i].uid_len == len && memcmp(g_map[i].uid, uid, len) == 0)
            return g_map[i].slot;
    }
    return -1;
}

bool rfid_map_set_uid(const uint8_t *uid, uint8_t len, int slot) {
    if (len == 0 || len > 7 || slot < 0 || slot >= CARD_SLOT_COUNT) return false;
    for (int i = 0; i < g_map_n; i++) {
        if (g_map[i].uid_len == len && memcmp(g_map[i].uid, uid, len) == 0) {
            g_map[i].slot = slot;
            return rfid_map_save();
        }
    }
    if (g_map_n >= RFD_MAP_MAX) return false;
    memcpy(g_map[g_map_n].uid, uid, len);
    g_map[g_map_n].uid_len = len;
    g_map[g_map_n].slot = slot;
    g_map_n++;
    return rfid_map_save();
}

int rfid_map_count(void) {
    return g_map_n;
}

void rfid_map_list(char *out, int max_len) {
    int pos = 0;
    pos += snprintf(out + pos, max_len - pos, "[");
    for (int i = 0; i < g_map_n; i++) {
        if (i) pos += snprintf(out + pos, max_len - pos, ",");
        pos += snprintf(out + pos, max_len - pos, "{\"uid\":\"");
        for (int k = 0; k < g_map[i].uid_len; k++)
            pos += snprintf(out + pos, max_len - pos, "%02x", g_map[i].uid[k]);
        pos += snprintf(out + pos, max_len - pos, "\",\"slot\":%d}", g_map[i].slot);
    }
    pos += snprintf(out + pos, max_len - pos, "]");
}

bool rfid_reader_probe(uint8_t *uid_out, uint8_t *uid_len, int *slot_read) {
    if (!g_rfid_present) return false;
    rfid_power(true);
    g_rfid.PCD_Init();
    SPI.begin(PIN_SPI_CLK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_RFID_CS);  // v. BUGFIX in init — see BUGFIX in init
    bool got = rfid_scan_uid(uid_out, uid_len);
    if (got) {
        *slot_read = -1;
        rfid_tag_read_slot(slot_read);
    }
    rfid_power(false);
    return got;
}