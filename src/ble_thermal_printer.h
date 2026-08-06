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
#pragma once
/*
 * ble_thermal_printer.h — Backend di trasporto Bluetooth LE per la
 * stampante termica, usato da printer.cpp tramite gli hook
 * printer_output_line()/printer_backend_is_connected().
 *
 * ATTENZIONE HARDWARE — leggere prima di comprare la stampante:
 * L'ESP32-S3 ha SOLO Bluetooth Low Energy (BLE), NON Bluetooth
 * Classic/BR-EDR. Molte mini-stampanti termiche economiche in stile
 * "cat printer" (GB01/GB02/MX06...) o le ricevute POS generiche
 * usano però proprio il BLE, quindi va bene — ma bisogna verificare
 * che il modello scelto sia BLE e non solo Bluetooth Classic/SPP
 * (quello non funzionerebbe con questa scheda).
 *
 * Il protocollo esatto (UUID di servizio/caratteristica, e se la
 * stampante si aspetta testo ASCII semplice, comandi ESC/POS, o un
 * protocollo proprietario a immagini come i "cat printer") dipende
 * dal modello specifico. Questo modulo:
 *
 *   1. Fa da client BLE generico: scansiona, si connette al nome/
 *      UUID configurato, scopre la caratteristica di scrittura.
 *   2. Di default assume la convenzione "Nordic UART Service" (NUS),
 *      usata da molti bridge BLE-seriale economici — invia semplice
 *      testo ASCII + newline. Sostituibile con ESC/POS reale una
 *      volta identificato il protocollo corretto per il tuo modello.
 *
 * Per scoprire gli UUID reali della tua stampante: installa "nRF
 * Connect for Mobile" (Android/iOS), connettiti alla stampante,
 * guarda i servizi/caratteristiche esposti e aggiorna le costanti
 * qui sotto.
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // Nome BLE della stampante da cercare in scansione (es. "MTP-2",
    // "GB02", "Printer001"...). Se vuoto, si usa target_service_uuid.
    const char *device_name;

    // UUID del servizio BLE che espone la scrittura verso la
    // stampante. Default: Nordic UART Service.
    const char *service_uuid;

    // UUID della caratteristica "RX" della stampante (quella su cui
    // l'ESP32 scrive). Default: NUS RX characteristic.
    const char *write_char_uuid;

    // Timeout di scansione/connessione in millisecondi.
    uint32_t connect_timeout_ms;

    // Se true, invia byte ESC/POS grezzi (taglio riga, avanzamento
    // carta...) invece di semplice ASCII + '\n'. Da abilitare solo
    // se il modello scelto supporta davvero ESC/POS su BLE.
    bool use_escpos;
} BlePrinterConfig;

// Configurazione di default: Nordic UART Service, ASCII semplice.
// DA VERIFICARE/ADATTARE al modello di stampante reale prima dell'uso.
extern const BlePrinterConfig BLE_PRINTER_DEFAULT_CONFIG;

// ─── API ────────────────────────────────────────────────────
// Avvia lo stack BLE e tenta la connessione in background (non
// bloccante: ritorna subito, la connessione prosegue su un task
// separato). Da chiamare una volta in setup().
void ble_printer_begin(const BlePrinterConfig *cfg);

// Da chiamare periodicamente dal loop principale (o da un task
// dedicato): gestisce riconnessione automatica se il collegamento
// cade, e invio asincrono della coda di stampa.
void ble_printer_poll(void);

// Invia una riga di testo alla stampante (accodata se non connessa
// al momento; persa se il buffer di coda è pieno — la stampa non è
// mai un'operazione critica per la correttezza del calcolo).
bool ble_printer_send_line(const char *line);

// True se attualmente connessi a una stampante BLE funzionante.
// Questa è la funzione che soddisfa printer_backend_is_connected()
// (vedi printer.cpp) e quindi il vero Op 40 "stampante collegata".
bool ble_printer_is_connected(void);

#ifdef __cplusplus
}
#endif
