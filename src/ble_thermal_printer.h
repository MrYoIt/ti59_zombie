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
#pragma once
/*
 * ble_thermal_printer.h — Backend di trasporto Bluetooth LE per la — Bluetooth LE transport backend for the
 * stampante termica, usato da printer.cpp tramite gli hook — thermal printer, used by printer.cpp through the hooks
 * printer_output_line()/printer_backend_is_connected(). — printer_output_line()/printer_backend_is_connected().
 *
 * ATTENZIONE HARDWARE — leggere prima di comprare la stampante: — HARDWARE WARNING — read before buying the printer:
 * L'ESP32-S3 ha SOLO Bluetooth Low Energy (BLE), NON Bluetooth — The ESP32-S3 has ONLY Bluetooth Low Energy (BLE), NOT Bluetooth
 * Classic/BR-EDR. Molte mini-stampanti termiche economiche in stile — Classic/BR-EDR. Many cheap thermal mini-printers in
 * "cat printer" (GB01/GB02/MX06...) o le ricevute POS generiche — "cat printer" style (GB01/GB02/MX06...) or generic POS receipts
 * usano però proprio il BLE, quindi va bene — ma bisogna verificare — do use BLE though, so it's fine — but you must check
 * che il modello scelto sia BLE e non solo Bluetooth Classic/SPP — that the chosen model is BLE and not only Bluetooth Classic/SPP
 * (quello non funzionerebbe con questa scheda). — (that would not work with this board).
 *
 * Il protocollo esatto (UUID di servizio/caratteristica, e se la — The exact protocol (service/characteristic UUIDs, and whether the
 * stampante si aspetta testo ASCII semplice, comandi ESC/POS, o un — printer expects plain ASCII text, ESC/POS commands, or a
 * protocollo proprietario a immagini come i "cat printer") dipende — proprietary image protocol like the "cat printers") depends
 * dal modello specifico. Questo modulo: — on the specific model. This module:
 *
 *   1. Fa da client BLE generico: scansiona, si connette al nome/ — Acts as a generic BLE client: scans, connects to the configured
 *      UUID configurato, scopre la caratteristica di scrittura. — name/UUID, discovers the write characteristic.
 *   2. Di default assume la convenzione "Nordic UART Service" (NUS), — By default assumes the "Nordic UART Service" (NUS) convention,
 *      usata da molti bridge BLE-seriale economici — invia semplice — used by many cheap BLE-serial bridges — sends simple
 *      testo ASCII + newline. Sostituibile con ESC/POS reale una — ASCII text + newline. Replaceable with real ESC/POS once
 *      volta identificato il protocollo corretto per il tuo modello. — the correct protocol for your model is identified.
 *
 * Per scoprire gli UUID reali della tua stampante: installa "nRF — To discover your printer's real UUIDs: install "nRF
 * Connect for Mobile" (Android/iOS), connettiti alla stampante, — Connect for Mobile" (Android/iOS), connect to the printer,
 * guarda i servizi/caratteristiche esposti e aggiorna le costanti — look at the exposed services/characteristics and update the constants
 * qui sotto. — below.
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // Nome BLE della stampante da cercare in scansione (es. "MTP-2", — BLE name of the printer to look for in scans (e.g. "MTP-2",
    // "GB02", "Printer001"...). Se vuoto, si usa target_service_uuid. — "GB02", "Printer001"...). If empty, target_service_uuid is used.
    const char *device_name;

    // UUID del servizio BLE che espone la scrittura verso la — UUID of the BLE service exposing the write path to the
    // stampante. Default: Nordic UART Service. — printer. Default: Nordic UART Service.
    const char *service_uuid;

    // UUID della caratteristica "RX" della stampante (quella su cui — UUID of the printer's "RX" characteristic (the one on which
    // l'ESP32 scrive). Default: NUS RX characteristic. — the ESP32 writes). Default: NUS RX characteristic.
    const char *write_char_uuid;

    // Timeout di scansione/connessione in millisecondi. — Scan/connect timeout in milliseconds.
    uint32_t connect_timeout_ms;

    // Se true, invia byte ESC/POS grezzi (taglio riga, avanzamento — If true, sends raw ESC/POS bytes (line cut, paper
    // carta...) invece di semplice ASCII + '\n'. Da abilitare solo — feed...) instead of plain ASCII + '\n'. Enable only
    // se il modello scelto supporta davvero ESC/POS su BLE. — if the chosen model really supports ESC/POS over BLE.
    bool use_escpos;
} BlePrinterConfig;

// Configurazione di default: Nordic UART Service, ASCII semplice. — Default configuration: Nordic UART Service, plain ASCII.
// DA VERIFICARE/ADATTARE al modello di stampante reale prima dell'uso. — TO BE VERIFIED/ADAPTED to the real printer model before use.
extern const BlePrinterConfig BLE_PRINTER_DEFAULT_CONFIG;

// ─── API — API ──────────────────────────────────────────────
// Avvia lo stack BLE e tenta la connessione in background (non — Starts the BLE stack and tries to connect in the background (non-
// bloccante: ritorna subito, la connessione prosegue su un task — blocking: returns immediately, connection continues on a separate
// separato). Da chiamare una volta in setup(). — task). Call once in setup().
void ble_printer_begin(const BlePrinterConfig *cfg);

// Da chiamare periodicamente dal loop principale (o da un task — Call periodically from the main loop (or a dedicated task):
// dedicato): gestisce riconnessione automatica se il collegamento — handles automatic reconnection if the link
// cade, e invio asincrono della coda di stampa. — drops, and asynchronous sending of the print queue.
void ble_printer_poll(void);

// Invia una riga di testo alla stampante (accodata se non connessa — Sends a text line to the printer (queued if not connected
// al momento; persa se il buffer di coda è pieno — la stampa non è — at the moment; lost if the queue buffer is full — printing is
// mai un'operazione critica per la correttezza del calcolo). — never a critical operation for calculation correctness).
bool ble_printer_send_line(const char *line);

// True se attualmente connessi a una stampante BLE funzionante. — True if currently connected to a working BLE printer.
// Questa è la funzione che soddisfa printer_backend_is_connected() — This is the function satisfying printer_backend_is_connected()
// (vedi printer.cpp) e quindi il vero Op 40 "stampante collegata". — (see printer.cpp) and thus the real Op 40 "printer connected".
bool ble_printer_is_connected(void);

#ifdef __cplusplus
}
#endif
