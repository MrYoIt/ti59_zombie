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
/*
 * ble_thermal_printer.cpp — Backend BLE per stampante termica.
 *
 * Richiede la libreria "NimBLE-Arduino" (h2zero/NimBLE-Arduino),
 * consigliata su ESP32-S3 al posto dello stack Bluedroid integrato
 * per minore uso di RAM/flash. Aggiungila con:
 *   pio lib install "h2zero/NimBLE-Arduino"
 * oppure dal Library Manager dell'Arduino IDE.
 *
 * NON COMPILATO/TESTATO su hardware reale in questa sessione (qui
 * non è disponibile un toolchain ESP32). Prima di flashare:
 *   1. Verifica gli UUID reali della tua stampante (nRF Connect).
 *   2. Aggiorna BLE_PRINTER_DEFAULT_CONFIG o passa una config custom
 *      a ble_printer_begin().
 *   3. Se il modello richiede ESC/POS invece di ASCII semplice,
 *      imposta use_escpos=true e adatta escpos_wrap_line() sotto.
 */

#include "ble_thermal_printer.h"
#include <NimBLEDevice.h>
#include <string.h>
#include <Arduino.h>

// UUID Nordic UART Service (convenzione de-facto per molti bridge
// BLE-seriale economici, incluse diverse mini-stampanti termiche).
#define NUS_SERVICE_UUID   "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_CHAR_UUID   "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // scrittura (ESP32 -> stampante)

const BlePrinterConfig BLE_PRINTER_DEFAULT_CONFIG = {
    /* device_name        */ "",   // vuoto = cerca solo per UUID di servizio
    /* service_uuid        */ NUS_SERVICE_UUID,
    /* write_char_uuid      */ NUS_RX_CHAR_UUID,
    /* connect_timeout_ms  */ 8000,
    /* use_escpos          */ false,
};

// ─── Stato interno ──────────────────────────────────────────
static BlePrinterConfig g_cfg;
static NimBLEClient      *g_client   = nullptr;
static NimBLERemoteCharacteristic *g_write_char = nullptr;
static NimBLEAdvertisedDevice     *g_target_device = nullptr;
static volatile bool      g_connected = false;
static volatile bool      g_scanning  = false;
static uint32_t           g_last_reconnect_attempt_ms = 0;
#define RECONNECT_INTERVAL_MS 5000

// Piccola coda circolare di righe in attesa di stampa (la stampa non
// è mai unʼoperazione bloccante o critica per il calcolo).
#define QUEUE_CAPACITY 16
#define LINE_MAXLEN    64
static char     g_queue[QUEUE_CAPACITY][LINE_MAXLEN];
static uint8_t  g_queue_head = 0, g_queue_tail = 0, g_queue_count = 0;

static void queue_push(const char *line) {
    if (g_queue_count >= QUEUE_CAPACITY) return;  // coda piena: riga persa (non critico)
    strncpy(g_queue[g_queue_tail], line, LINE_MAXLEN - 1);
    g_queue[g_queue_tail][LINE_MAXLEN - 1] = '\0';
    g_queue_tail = (g_queue_tail + 1) % QUEUE_CAPACITY;
    g_queue_count++;
}

static bool queue_pop(char *out) {
    if (g_queue_count == 0) return false;
    strncpy(out, g_queue[g_queue_head], LINE_MAXLEN);
    g_queue_head = (g_queue_head + 1) % QUEUE_CAPACITY;
    g_queue_count--;
    return true;
}

// ─── Callback di connessione ────────────────────────────────
class PrinterClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient *pClient) override {
        g_connected = true;
    }
    void onDisconnect(NimBLEClient *pClient, int reason) override {
        g_connected = false;
        g_write_char = nullptr;
    }
};
static PrinterClientCallbacks g_client_callbacks;

// ─── Callback di scansione ──────────────────────────────────
class PrinterScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice *dev) override {
        bool name_match = g_cfg.device_name && strlen(g_cfg.device_name) > 0
                           && dev->getName() == g_cfg.device_name;
        bool uuid_match = g_cfg.service_uuid && dev->isAdvertisingService(NimBLEUUID(g_cfg.service_uuid));
        if (name_match || uuid_match) {
            NimBLEDevice::getScan()->stop();
            g_target_device = new NimBLEAdvertisedDevice(*dev);
        }
    }
};
static PrinterScanCallbacks g_scan_callbacks;

// ─── Formattazione riga (ASCII semplice o wrapper ESC/POS) ──
// Comandi ESC/POS minimi utili: avanzamento riga = '\n' funziona
// già sulla maggior parte dei firmware; taglio carta = GS V (non
// tutte le mini-stampanti BLE lo supportano, va verificato).
static size_t escpos_wrap_line(const char *line, uint8_t *out, size_t out_cap) {
    size_t n = 0;
    size_t len = strlen(line);
    if (len > out_cap - 2) len = out_cap - 2;
    memcpy(out, line, len);
    n = len;
    out[n++] = '\n';   // avanzamento riga
    return n;
}

static void try_send_line(const char *line) {
    if (!g_connected || !g_write_char) return;
    if (g_cfg.use_escpos) {
        uint8_t buf[LINE_MAXLEN + 2];
        size_t n = escpos_wrap_line(line, buf, sizeof(buf));
        g_write_char->writeValue(buf, n, false);
    } else {
        char buf[LINE_MAXLEN + 1];
        size_t len = strlen(line);
        if (len > LINE_MAXLEN - 1) len = LINE_MAXLEN - 1;
        memcpy(buf, line, len);
        buf[len] = '\n';
        g_write_char->writeValue((uint8_t*)buf, len + 1, false);
    }
}

static bool connect_to_target(void) {
    if (!g_target_device) return false;

    if (g_client == nullptr) {
        g_client = NimBLEDevice::createClient();
        g_client->setClientCallbacks(&g_client_callbacks, false);
    }
    if (!g_client->connect(g_target_device)) {
        return false;
    }

    NimBLERemoteService *svc = g_client->getService(g_cfg.service_uuid);
    if (!svc) { g_client->disconnect(); return false; }

    g_write_char = svc->getCharacteristic(g_cfg.write_char_uuid);
    if (!g_write_char) { g_client->disconnect(); return false; }

    g_connected = true;
    return true;
}

static void start_scan(void) {
    if (g_scanning) return;
    g_scanning = true;
    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&g_scan_callbacks, false);
    scan->setActiveScan(true);
    scan->start(g_cfg.connect_timeout_ms / 1000, false);
    g_scanning = false;
}

// ─── API pubblica ───────────────────────────────────────────
void ble_printer_begin(const BlePrinterConfig *cfg) {
    g_cfg = cfg ? *cfg : BLE_PRINTER_DEFAULT_CONFIG;
    NimBLEDevice::init("TI59-Zombie");
    // La scansione iniziale viene avviata dal primo ble_printer_poll():
    // evitiamo di bloccare setup() con una scansione sincrona lunga.
}

void ble_printer_poll(void) {
    uint32_t now = millis();

    if (g_connected) return;   // già connessi, nulla da fare

    if (now - g_last_reconnect_attempt_ms < RECONNECT_INTERVAL_MS) return;
    g_last_reconnect_attempt_ms = now;

    if (!g_target_device) {
        start_scan();   // popola g_target_device se trova un match
    }
    if (g_target_device && !g_connected) {
        connect_to_target();
    }

    // Svuota la coda non appena la connessione torna disponibile
    if (g_connected) {
        char line[LINE_MAXLEN];
        while (queue_pop(line)) {
            try_send_line(line);
        }
    }
}

bool ble_printer_send_line(const char *line) {
    if (!line) return false;
    if (g_connected && g_write_char) {
        try_send_line(line);
        return true;
    }
    queue_push(line);   // stampante non ancora connessa: mettiamo in coda
    return false;
}

bool ble_printer_is_connected(void) {
    return g_connected;
}

// ─── Collegamento agli hook deboli di printer.cpp ───────────
extern "C" void printer_output_line(const char *line) {
    ble_printer_send_line(line);
}
extern "C" bool printer_backend_is_connected(void) {
    return ble_printer_is_connected();
}
