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
#include <Arduino.h>   // sempre prima di tutto in .cpp
#include <SPI.h>
#include <WebServer.h>
#include <SHA1Builder.h>
#include <freertos/semphr.h>   // SemaphoreHandle_t, xSemaphoreCreateMutex — v. g_cpuMutex

/*
 * TI-59 ZOMBIE — Firmware v1.1
 * ESP32-S3 · Arduino framework
 *
 * Moduli:
 *   tms1500.c/h          — Emulatore CPU TMC0501 + DSCOM + BROM + RAM
 *   library_module.c/h   — Registro moduli libreria (Solid State Software)
 *   rom_XX.c/h           — ROM dei moduli libreria (generati da rom_import_validator.py)
 *   keyboard.c/h         — Scan matrice tastiera 9x5 (14 pin)
 *   display.c/h          — Driver HT16K33 (16x8, I2C) per display a 12 digit
 *   cardemu.c/h          — Emulazione schede magnetiche (SPIFFS)
 *   rfid_reader.c/h      — Lettore/scrittore schede NFC (PN532)
 *   printer.c/h          — Stampante termica (seriale + BLE)
 *   wifilink.c/h         — Web server HTTP + IDE web embedded
 *   wifilink_modules.c/h — Endpoint API moduli libreria
 *   wifilink_regs.c/h    — Endpoint registri CPU
 *
 * Moduli libreria disponibili (14 moduli, id — numero ufficiale — nome):
 *   ml1  -1-  Master Library (25)
 *   st   -2-  Applied Statistics (22)
 *   re   -3-  Real Estate (16)
 *   sv   -4-  Surveying (25)
 *   na   -5-  Marine Navigation (30)
 *   av   -6-  Aviation (23)
 *   ll   -7-  Leisure Library (21)
 *   sa   -8-  Securities Analysis (15)
 *   ee   -9-  Electrical Engineering (19)
 *   fm  -10-  Farming (16)
 *   mu  -11-  Music (21)
 *   ph  -12-  Photography (20)
 *   rp  -13-  RPN (64)
 *   se  -14-  Structural Engineering (8)
 *   (manca -15- Math Utilities, non ancora implementato nel firmware)
 *
 * WiFi endpoints:
 *   GET /                       → IDE web principale
 *   GET /manage                 → Pannello di controllo
 *   GET /wolf                   → Pannello regolatori (solo con god mode)
 *   GET /overlays               → Editor overlay ROM/schede magnetiche
 *   GET /api/status             → Stato CPU
 *   GET/POST /api/regs          → Leggi/scrivi registri A,B,R00-R08
 *   GET/POST /api/modules       → Lista/seleziona moduli libreria
 *   GET /api/modules/listing    → Listato programmi modulo
 *   GET /api/overlays           → Righe overlay di un mod/prog
 *   GET/POST /api/card_positions → Posizioni testo overlay (overlay_pos.json)
 *   GET/POST/DELETE /api/card   → Gestione schede magnetiche
 *   GET/POST /api/card/file     → Download/upload scheda
 *   GET/POST /api/program_card  → Scheda programmazione (SVG)
 *   POST /api/keypress          → Invia tasto
 *   GET/POST /api/prog          → Download/upload programma
 *   POST /api/reset             → Reset CPU
 *   POST /api/timing            → Toggle timing Old/New + moltiplicatore
 *   GET /api/eject              → Imposta durata espulsione scheda
 *   GET/POST /api/rfid/*        → Gestione lettore NFC
 *   GET/POST /api/wifi/*        → Gestione WiFi
 *   GET/POST /api/fs            → Gestione file system SPIFFS
 */

#include "src/config.h"
#include "src/tms1500.h"
#include "src/keyboard.h"
#include "src/display.h"
#include "src/cardemu.h"
#include "src/rfid_reader.h"
#include "src/wifilink.h"
#include "src/wifilink_modules.h"
#include "src/wifilink_regs.h"

#include <WiFi.h>
#include <SPIFFS.h>
#include <FS.h>


// ─── Stato globale ────────────────────────────────────────
static TMS1500_State  cpu;
static KeyboardState  kbd;
static DisplayState   disp;
static CardEmuState   card;

// ─── Mutex per l'accesso a cpu/card ───────────────────────
// BUGFIX (race condition): cpu/card sono scritte da taskCPU,
// taskKeyboard, taskWiFi (quest'ultimo su un core FISICAMENTE
// diverso — pinnato al core 0 mentre gli altri tre sono sul core 1,
// quindi qui non basta la sola prelazione FreeRTOS, è vera esecuzione
// parallela) e da loop() stesso (salvataggio SPIFFS periodico).
// Prima non c'era nessuna sincronizzazione: loop() poteva leggere
// cpu.prog[]/cpu.ram[] a metà di una scrittura di taskCPU e salvare
// su flash uno snapshot incoerente. g_cpuMutex protegge ogni accesso
// a cpu/card da qualunque task — v. uso in taskCPU/taskKeyboard/loop()
// sotto. Le route HTTP in wifilink.cpp (taskWiFi) non prendono questo
// mutex: leggono cpu/card direttamente, senza la stessa protezione dei
// task interni. Per le scritture brevi (es. tasti) il rischio è
// contenuto, ma va tenuto presente.
static SemaphoreHandle_t g_cpuMutex = NULL;

// ─── Task handles ─────────────────────────────────────────
static TaskHandle_t hTaskCPU  = NULL;
static TaskHandle_t hTaskDisp = NULL;
static TaskHandle_t hTaskKbd  = NULL;
static TaskHandle_t hTaskWiFi = NULL;

// ─── Task CPU (core 1, priorità 5) ───────────────────────
void taskCPU(void *pv) {
    TickType_t lastWake = xTaskGetTickCount();
    while (1) {
        // Un solo lock/unlock per batch (non per singolo step): tenere
        // il mutex per tutto CPU_CYCLES_PER_TICK invece che riprenderlo
        // ad ogni tms1500_step() riduce l'overhead di sincronizzazione
        // sulla parte più calda del firmware, e comunque garantisce che
        // chi legge cpu da un altro task (loop(), taskKeyboard, in
        // futuro anche taskWiFi) veda sempre uno stato coerente tra un
        // tick e l'altro, mai a metà di una singola istruzione.
        if (xSemaphoreTake(g_cpuMutex, portMAX_DELAY) == pdTRUE) {
            for (int i = 0; i < CPU_CYCLES_PER_TICK; i++)
                tms1500_step(&cpu, &kbd, &disp);
            xSemaphoreGive(g_cpuMutex);
        }
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(1));
    }
}

// ─── Task Display (core 1, priorità 3) ───────────────────
//void taskDisplay(void *pv) {
//    TickType_t lastWake = xTaskGetTickCount();
//    while (1) {
//        // --- AGGIORNA DISPLAY: percorso stringa SEMPRE ---
//        char buf[16];
//        tms1500_get_display_string(&cpu, buf, sizeof(buf));
//        display_show_string(&disp, buf);
//
//        // Indicatore CALC lampeggiante
//        display_calc_indicator_tick(&disp, &cpu, millis());
//
//        // Scarica sui MAX7219
//        display_refresh(&disp);
//        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(16));
//    }
//}

// ─── Task Display (core 1, priorità 3) ───────────────────
void taskDisplay(void *pv) {
    TickType_t lastWake = xTaskGetTickCount();
    while (1) {
        char buf[16];
        bool err;
        // Solo la lettura di cpu è sotto lock — il refresh I2C verso
        // l'HT16K33 (lento rispetto a un semplice memcpy) resta fuori,
        // non tocca cpu/card e non ha senso tenere bloccato taskCPU
        // per quel tempo.
        if (xSemaphoreTake(g_cpuMutex, portMAX_DELAY) == pdTRUE) {
            tms1500_get_display_string(&cpu, buf, sizeof(buf));
            err = cpu.flags.error;
            xSemaphoreGive(g_cpuMutex);
        } else {
            buf[0] = 0; err = false;
        }

        // Implementazione dell'effetto lampeggio per l'errore (es. etichetta non trovata)
        if (err && (millis() % 1000 < 500)) {
            memset(buf, ' ', 12);
            buf[12] = '\0';
        }

        display_show_string(&disp, buf);

        // Indicatore CALC lampeggiante
        display_calc_indicator_tick(&disp, &cpu, millis());
        
        // Scarica sull'HT16K33
        display_refresh(&disp);
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(16));
    }
}

// ─── Task Keyboard (core 1, priorità 4) ──────────────────
void taskKeyboard(void *pv) {
    TickType_t lastWake = xTaskGetTickCount();
    while (1) {
        keyboard_scan(&kbd);
        // Card sense — fronte di salita: carica l'ultimo programma scritto
        cardemu_sense(&card);
        if (card.sense_rising) {
            if (rfid_reader_enabled()) {
                // Lettore NFC: gestisce lui tutto il flusso READ/WRITE
                // (accensione modulo, scan tag, load/save, espulsione,
                // spegnimento). Bloccante per ~1.5s: il lock della CPU
                // viene preso solo attorno alle cardemu_* all'interno
                // di rfid_reader_handle_insert.
                rfid_reader_handle_insert(&card, &cpu,
                    []() { xSemaphoreTake(g_cpuMutex, portMAX_DELAY); },
                    []() { xSemaphoreGive(g_cpuMutex); });
            } else if (card.last_written_slot < CARD_SLOT_COUNT) {
                uint8_t slot = card.last_written_slot;
                // cardemu_load_to_cpu scrive prog/ram dentro cpu — sotto
                // lock, altrimenti taskCPU potrebbe eseguire istruzioni
                // a metà caricamento (metà programma vecchio, metà nuovo).
                if (xSemaphoreTake(g_cpuMutex, portMAX_DELAY) == pdTRUE) {
                    if (cardemu_read(&card, &cpu, slot)) {
                        cardemu_load_to_cpu(&card, &cpu, slot);
                    }
                    xSemaphoreGive(g_cpuMutex);
                }
            } else {
                Serial.println("[CARD] Inserita scheda, ma nessun programma ancora scritto");
            }
        }
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(10));
    }
}

// ─── Task WiFi (core 0, priorità 2) ──────────────────────
void taskWiFi(void *pv) {
    // Registra route moduli e registri DOPO l'avvio del server in wifi_server_loop
    // (wifi_server_loop chiama setup_routes() e server.begin() internamente)
    // Le route aggiuntive sono registrate con hook pre-begin
    wifi_server_loop(&cpu, &card, &kbd);
    // Mai raggiunto
}

// Definita in rom.cpp: carica le ROM dei moduli libreria.
void rom_init(void);



// ─── setup ───────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("╔══════════════════════════════╗");
    Serial.println("║  TI-59 ZOMBIE  v1.1          ║");
    Serial.println("║  ESP32-S3 · TMS1500 Emulator ║");
    Serial.println("╚══════════════════════════════╝");

    Serial.printf("[BOOT] reset_reason=%d sdk=%s millis=%u\n",
                  (int)esp_reset_reason(), ESP.getSdkVersion(), (unsigned)millis());
    Serial.printf("[BOOT] flash=%uMB psram=%uMB cpu=%uMHz\n",
                  ESP.getFlashChipSize() / (1024 * 1024),
                  ESP.getPsramSize() / (1024 * 1024),
                  ESP.getCpuFreqMHz());

    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);

    // SPIFFS: MAI auto-format a ogni boot: begin(true) su flash
    // cancellata/partizione corrotta lancia l'erase dell'intera partizione
    // (10-20s di console morta e garbage sul CDC USB, su boot ripetuti
    // sembra un hang). begin(false): se il mount fallisce, formatta UNA
    // SOLA volta e rimonta; il sistema parte comunque.
    Serial.println("[INIT] SPIFFS...");
    if (!SPIFFS.begin(false)) {
        Serial.println("[SPIFFS] Mount fallito — formattazione uno tantum...");
        SPIFFS.format();
        if (!SPIFFS.begin(false)) {
            Serial.println("[SPIFFS] Mount fallito anche dopo il format — partenza senza file system (POST /api/fs/format via web)");
        } else {
            Serial.println("[SPIFFS] OK (formattato)");
        }
    } else {
        Serial.println("[SPIFFS] OK");
    }



    // Init sottosistemi
    Serial.println("[INIT] ROM...");
    rom_init();

    Serial.println("[INIT] CPU...");
    tms1500_init(&cpu);

    Serial.println("[INIT] Tastiera...");
    keyboard_init(&kbd);

    Serial.println("[INIT] Display...");
    display_init(&disp);

    Serial.println("[INIT] Card emulator...");
    cardemu_init(&card);

    Serial.println("[INIT] RFID reader...");
    rfid_reader_init();

    // Ripristino stato persistente CPU (memoria solid state)
    cardemu_load_persistent(&card, &cpu);

    // Import automatico del catalogo storico (solo se non già fatto)
    if (SPIFFS.exists("/import.json") && !SPIFFS.exists("/.imported")) {
        Serial.println("[INIT] Catalogo storico trovato — import in corso...");
        if (cardemu_import_batch(&card, "/import.json")) {
            File marker = SPIFFS.open("/.imported", "w");
            if (marker) { marker.write('1'); marker.close(); }
            Serial.println("[INIT] Import completato e marcato.");
        }
    }

    // Blink avvio
    for (int i = 0; i < 5; i++) {
        digitalWrite(PIN_LED_STATUS, HIGH); delay(80);
        digitalWrite(PIN_LED_STATUS, LOW);  delay(80);
    }

    // Mostra "HELLO" sul display
    display_show_string(&disp, "hELLo 59");
    display_refresh(&disp);
    delay(1000);

    Serial.println("[INIT] Avvio task FreeRTOS...");

    g_cpuMutex = xSemaphoreCreateMutex();
    if (!g_cpuMutex) {
        Serial.println("[FATAL] Impossibile creare g_cpuMutex — arresto.");
        while (1) { delay(1000); }   // non ha senso proseguire senza sincronizzazione
    }

    // Core 1: CPU emulation + keyboard + display
    xTaskCreatePinnedToCore(taskCPU,      "CPU",  16384, NULL, 5, &hTaskCPU,  1);
    xTaskCreatePinnedToCore(taskKeyboard, "KBD",  4096, NULL, 4, &hTaskKbd,  1);
    xTaskCreatePinnedToCore(taskDisplay,  "DISP", 4096, NULL, 3, &hTaskDisp, 1);

    // Core 0: WiFi + web server
    xTaskCreatePinnedToCore(taskWiFi,     "WIFI", 12288, NULL, 2, &hTaskWiFi, 0);

    Serial.println("[INIT] Completato. Sistema operativo.");
}

// ─── loop ────────────────────────────────────────────────
void loop() {
    // Watchdog e diagnostica seriale ogni 10s
    delay(10000);

    // BUGFIX (race condition, questa era la segnalazione originale):
    // prima si leggeva cpu.total_cycles/cpu.prog_len qui e si chiamava
    // cardemu_save_persistent(&card, &cpu) più sotto SENZA alcuna
    // sincronizzazione con taskCPU, che nel frattempo continua a
    // mutare prog[]/ram[] — su un altro core rispetto a taskWiFi,
    // quindi non basta la sola prelazione, è vera concorrenza. Un
    // salvataggio a metà di una scrittura di taskCPU produceva uno
    // snapshot incoerente su flash (es. metà registro nuovo, metà
    // vecchio). Ora sia la lettura diagnostica sia il salvataggio
    // avvengono sotto lock — taskCPU si ferma per la (breve) durata
    // di questi due blocchi, garantendo uno stato coerente.
    uint64_t cycles = 0; uint16_t proglen = 0;
    if (xSemaphoreTake(g_cpuMutex, portMAX_DELAY) == pdTRUE) {
        cycles  = cpu.total_cycles;
        proglen = cpu.prog_len;
        xSemaphoreGive(g_cpuMutex);
    }
    Serial.printf("[SYS] heap=%u  cycles=%llu  prog=%d  wifi=%s\n",
        ESP.getFreeHeap(),
        cycles,
        proglen,
        WiFi.isConnected() ? WiFi.localIP().toString().c_str() : "AP"
    );
    // LED heartbeat
    digitalWrite(PIN_LED_STATUS, HIGH); delay(50);
    digitalWrite(PIN_LED_STATUS, LOW);

    // Salva su SPIFFS se il programma è stato modificato.
    // cardemu_save_persistent() chiama tms1500_mark_prog_saved()
    // dopo il salvataggio, quindi il dirty flag viene resettato.
    // tms1500_is_prog_dirty() è una lettura di un singolo bool, non
    // serve il lock per quella — solo per il salvataggio vero e
    // proprio, che legge un pezzo consistente di cpu (prog/ram/pc).
    if (tms1500_is_prog_dirty()) {
        if (xSemaphoreTake(g_cpuMutex, portMAX_DELAY) == pdTRUE) {
            cardemu_save_persistent(&card, &cpu);
            xSemaphoreGive(g_cpuMutex);
        }
    }
}