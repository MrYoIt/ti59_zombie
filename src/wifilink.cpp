/*
 * TI-59 Zombie — emulatore TI-59 su ESP32-S3 (TMS1500) — TI-59 emulator on ESP32-S3 (TMS1500)
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
 * Dovresti aver ricevuto una copia della GNU General Public License insieme a questo programma; se non è così, vedi <https://www.gnu.org/licenses/>. — You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/*
 * wifilink.c — Web server + IDE embedded per TI-59 Zombie — Web server + embedded IDE for TI-59 Zombie
 *
 * v1.5  Fix DP posizionato a destra del digit (non nel prossimo) — DP placed to the right of the digit (not in the next one)
 * v1.6  Aggiunto indicatore 2ND nella mode-bar — added 2ND indicator in the mode-bar
 * v1.7  Tasti più piccoli, più spaziati e overlay codici tasto attivabile via web — smaller keys, more spacing and key-code overlay toggleable via web
 */

#include "wifilink.h"
#include "wifilink_modules.h"
#include "wifilink_regs.h"
#include "config.h"
#include "display.h"
#include "cardemu.h"
#include "keyboard.h"
#include "tms1500.h"
#include "rfid_reader.h"
#include "library_module.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <DNSServer.h>
#include <string.h>
#include <stdio.h>

// get_mnemonic_name() dichiarata in tms1500.h, definita in tms1500.cpp — declared in tms1500.h, defined in tms1500.cpp

/* ═══════════════════════════════════════════════════════════════════
   TABELLA DI TRADUZIONE (i18n) — file JS condiviso da TUTTE le pagine — TRANSLATION TABLE (i18n) — JS file shared by ALL pages
   (incluso via <script src="/i18n.js"> prima dello script di ogni — (included via <script src="/i18n.js"> before the script of every
   pagina), così esiste UNA sola copia in flash e UNA sola tabella da — page), so there is ONE copy in flash and ONE table to
   tenere aggiornata, non 4 duplicate. — keep updated, not 4 duplicates.

   FORMATO — una riga per chiave, colonne IT/EN affiancate: aggiungere — FORMAT — one row per key, IT/EN columns side by side: adding
   o correggere una traduzione è aggiungere/editare UNA riga qui. — or fixing a translation is adding/editing ONE row here.

   USO: — USAGE:
   - Testo statico in HTML: data-i18n="chiave" (assegna .innerHTML). — Static text in HTML: data-i18n="key" (assigns .innerHTML).
     Per placeholder: data-i18n-ph="chiave". Per title: data-i18n-title="chiave". — For placeholder: data-i18n-ph="key". For title: data-i18n-title="key".
   - Stringhe generate da JS: t('chiave') al posto del letterale italiano. — JS-generated strings: t('key') instead of the Italian literal.
   - setLang('it'|'en') cambia lingua, salva la scelta in localStorage — setLang('it'|'en') switches language, saves the choice in localStorage
     (persiste tra riavvii del browser) e re-invoca applyI18n(). — (persists across browser restarts) and re-invokes applyI18n().
   Il selettore lingua vive solo in /manage, ma localStorage è — The language selector lives only in /manage, but localStorage is
   condiviso da tutte le pagine dello stesso device: applyI18n() gira — shared by all pages of the same device: applyI18n() runs
   ovunque a DOMContentLoaded, quindi la scelta si applica dappertutto. — everywhere at DOMContentLoaded, so the choice applies everywhere.
   ═══════════════════════════════════════════════════════════════════ */
static const char I18N_JS[] = R"jsrc(
const I18N_TABLE = [
  // ── comuni a più pagine — common to multiple pages ─────────────
  ['nav_back_calc',      '&larr; Calcolatrice',                 '&larr; Calculator'],
  ['nav_manage',         'Overlay &rarr;',                       'Overlay &rarr;'],
  ['status_loading',     'Caricamento...',                       'Loading...'],
  ['status_error',       'Errore: ',                              'Error: '],
  ['status_network_error','Errore di rete: ',                    'Network error: '],
  ['confirm_delete',     'Eliminare?',                            'Delete?'],
  ['lang_label',         'Lingua',                                'Language'],

  // ── /setup (captive portal WiFi) — captive portal (WiFi) setup ──
  ['setup_h2_networks',  'Reti disponibili',                      'Available networks'],
  ['setup_scanning',     'Scansione...',                          'Scanning...'],
  ['setup_btn_refresh',  '&#x27F3; Aggiorna',                     '&#x27F3; Refresh'],
  ['setup_btn_connect',  'Connetti',                              'Connect'],
  ['setup_h2_saved',     'Reti salvate (',                        'Saved networks ('],
  ['setup_h2_new',       'Nuova rete',                            'New network'],
  ['setup_btn_save_connect','Salva &amp; Connetti',               'Save &amp; Connect'],
  ['setup_btn_clear_all','Cancella tutto',                        'Clear all'],
  ['setup_status_waiting','In attesa...',                         'Waiting...'],
  ['setup_no_networks',  'Nessuna rete',                          'No networks'],
  ['setup_scan_error',   'Errore scan',                           'Scan error'],
  ['setup_none',         'Nessuna',                                'None'],
  ['setup_select',       'Seleziona',                              'Select'],
  ['setup_enter_ssid',   'Inserisci SSID',                        'Enter SSID'],
  ['setup_connecting',   'Connessione in corso...',                'Connecting...'],
  ['setup_connected_restart','Connesso! Riavvio...',              'Connected! Restarting...'],
  ['setup_failed',       'Fallito: ',                              'Failed: '],
  ['setup_trying_connect','Tentativo connessione...',             'Attempting connection...'],
  ['setup_confirm_clear_all','Cancellare tutte?',                 'Delete all?'],
  ['setup_h2_wifi_file', 'File credenziali (wifi.json)',           'Credentials file (wifi.json)'],
  ['setup_btn_wifi_dl',  'Scarica file',                            'Download file'],
  ['setup_btn_wifi_ul',  'Carica file',                             'Upload file'],
  ['setup_choose_file',  'Scegli un file',                          'Choose a file'],
  ['setup_file_saved',   'File salvato!',                           'File saved!'],

  // ── /manage — management ──────────────────────────────────────
  ['mgr_h2_cards',       'Schede programma',                      'Program cards'],
  ['mgr_card_name_ph',   'Nome scheda',                           'Card name'],
  ['mgr_btn_write',      'WRITE',                                  'WRITE'],
  ['mgr_btn_read',       'READ',                                   'READ'],
  ['mgr_btn_upload_file','&uarr; Carica da file',                 '&uarr; Upload file'],
  ['mgr_h2_current_prog','Programma corrente',                    'Current program'],
  ['mgr_prog_ph',        'Hex passi programma...',                'Program step hex...'],
  ['mgr_btn_upload',     '&uarr; Carica',                          '&uarr; Upload'],
  ['mgr_btn_download',   '&darr; Scarica',                        '&darr; Download'],
  ['mgr_btn_reset',      'RESET',                                  'RESET'],
  ['mgr_h2_library',     'Modulo libreria (2nd Pgm)',              'Library module (2nd Pgm)'],
  ['mgr_btn_engage',     'Innesta',                                'Engage'],
  ['mgr_btn_view_listing','Vedi listato completo',                'View full listing'],
  ['mgr_h2_printer_file','Stampante / File',                      'Printer / File'],
  ['mgr_btn_print',      'Print',                                  'Print'],
  ['mgr_btn_listing',    'Listing',                                'Listing'],
  ['mgr_btn_progs',      'Progs',                                  'Progs'],
  ['mgr_h2_fs',          'File system (SPIFFS)',                  'File system (SPIFFS)'],
  ['mgr_btn_update',     'Aggiorna',                                'Refresh'],
  ['mgr_h2_system',      'Sistema',                                'System'],
  ['mgr_lbl_ram',        'RAM',                                    'RAM'],
  ['mgr_lbl_heap_free',  'Heap libero',                            'Free heap'],
  ['mgr_lbl_spiffs',     'SPIFFS',                                 'SPIFFS'],
  ['mgr_lbl_cards_saved','Schede salvate',                        'Saved cards'],
  ['mgr_lbl_prg_files',  'File .prg',                              '.prg files'],
  ['mgr_lbl_cpu_cycles', 'Cicli CPU',                              'CPU cycles'],
  ['mgr_lbl_timing',     'Velocità Old',                           'Old speed'],
  ['mgr_lbl_maxspeed',   'Velocità max raggiungibile',            'Max reachable speed'],
  ['mgr_confirm_delete_card','Cancellare scheda?',                'Delete card?'],
  ['mgr_confirm_reset',  'Reset CPU? Il programma in memoria andrà perso.', 'Reset CPU? The program in memory will be lost.'],
  ['mgr_alert_upload_failed','Upload fallito',                    'Upload failed'],
  ['mgr_alert_choose_file','Scegli un file',                       'Choose a file'],
  ['mgr_no_cards',       'Nessuna scheda',                        'No cards'],
  ['mgr_alert_empty_slot','Slot vuoto',                            'Empty slot'],
  ['mgr_status_read',    'Letta ',                                 'Read '],
  ['mgr_status_written', 'Scritta ',                               'Written '],
  ['mgr_alert_choose_text_file','Scegli un file di testo prima',   'Choose a text file first'],
  ['mgr_alert_import_failed','Import fallito (formato non riconosciuto?)', 'Import failed (unrecognized format?)'],
  ['mgr_status_imported','Importata in slot ',                     'Imported into slot '],
  ['mgr_status_imported_from',' da ',                              ' from '],
  ['mgr_confirm_delete_slot','Eliminare ',                         'Delete '],
  ['mgr_alert_delete_error','Errore cancellazione: ',              'Delete error: '],
  ['mgr_status_prog_loaded','Prog caricato',                       'Program loaded'],
  ['mgr_alert_file_not_found','File non trovato',                  'File not found'],
  ['mgr_no_prg',         'Nessun .prg',                            'No .prg files'],
  ['mgr_alert_choose_module','Scegli prima un modulo dal menu',    'Choose a module from the menu first'],
  ['mgr_alert_listing_unavailable','Listato non disponibile',      'Listing not available'],
  ['mgr_status_module_listing','Listato modulo: ',                 'Module listing: '],
  ['mgr_no_files',       'Nessun file',                            'No files'],
  ['mgr_status_fs_error','Errore FS: ',                            'FS error: '],
  ['mgr_status_fs_deleted','Cancellato ',                          'Deleted '],
  ['mgr_status_uploaded','Caricato ',                              'Uploaded '],
  ['mgr_confirm_delete_generic','Cancellare ',                     'Delete '],
  ['mgr_alert_delete_failed_http','Cancellazione fallita (HTTP ',  'Delete failed (HTTP '],
  ['mgr_status_pgm_steps',' passi',                                 ' steps'],
  ['mgr_status_module_engaged','Modulo innestato: ',                'Module engaged: '],
  ['mgr_status_module_none','Nessun modulo innestato',              'No module engaged'],

  // ── /overlays — overlays ───────────────────────────────────────
  ['ovl_h1',             'OVERLAY TASTIERA',                       'KEYBOARD OVERLAY'],
  ['nav_manage_back',    '&larr; Gestione',                        '&larr; Management'],
  ['ovl_h2_file',        'File overlay (unico, tutti i moduli)',   'Overlay file (single, all modules)'],
  ['ovl_hint_format',
    'Una riga per etichetta: <b>MOD|PROG|TYPE|KEY|ATTR|TESTO</b><br>' +
    'ATTR = s(inistra) / e(destra) / c(entro) / m(mezzo) / fN(font N)<br>' +
    'GRID: KEY = A-E/A\'-E\'. Stesso KEY = split (s + e sulla stessa riga)<br>' +
    'FREE: 4+ spazi nel TESTO = blocchi separati sulla stessa riga (2 blocchi: sinistra/destra, 3: sinistra/centro/destra)<br>' +
    'Apice/pedice: <code>^+...^^</code> apice, <code>^-...^^</code> pedice (es. <code>X^+2^^</code> = X²)<br>' +
    'Es: <code>ml1|01|FREE|2|c|L.R. INIT: SBR CLR    PRINT: mm STO 00</code>',
    'One line per label: <b>MOD|PROG|TYPE|KEY|ATTR|TEXT</b><br>' +
    'ATTR = s(tart) / e(nd) / c(enter) / m(iddle) / fN(font N)<br>' +
    'GRID: KEY = A-E/A\'-E\'. Same KEY = split (s + e on the same line)<br>' +
    'FREE: 4+ spaces in TEXT = separate blocks on the same line (2 blocks: left/right, 3: left/center/right)<br>' +
    'Superscript/subscript: <code>^+...^^</code> superscript, <code>^-...^^</code> subscript (e.g. <code>X^+2^^</code> = X²)<br>' +
    'E.g.: <code>ml1|01|FREE|2|c|L.R. INIT: SBR CLR    PRINT: mm STO 00</code>'],
  ['ovl_textarea_ph',    'Nessun overlay salvato ancora. Formato: mod|prog|TYPE|KEY|ATTR|testo', 'No overlay saved yet. Format: mod|prog|TYPE|KEY|ATTR|text'],
  ['ovl_btn_save',       'SALVA (sostituisce tutto)',              'SAVE (replaces everything)'],
  ['ovl_btn_reload',     'Ricarica dal device',                    'Reload from device'],
  ['ovl_btn_upload_file','Apri file',                               'Open file'],
  ['ovl_btn_download_file','Salva file',                            'Save file'],
  ['ovl_downloaded',     'overlays.txt scaricato',                  'overlays.txt downloaded'],
  ['ovl_uploaded_pre',   'Caricato da ',                            'Loaded from '],
  ['ovl_uploaded_post',  ' — premi SALVA per applicarlo',           ' — press SAVE to apply it'],
  ['ovl_h2_preview',     'Anteprima per programma',                'Preview for program'],
  ['ovl_btn_show',       'Mostra',                                  'Show'],
  ['ovl_h2_attrs',       'Attributi in uso',                       'Attributes in use'],
  ['ovl_hint_attrs',     'Elenco di tutti gli attributi previsti (non solo quelli già usati) — si aggiorna da sola se aggiungi un nuovo codice qui sotto.',
                          'List of every attribute code that exists (not just the ones already used) — updates itself if you add a new code below.'],
  ['status_connecting',  'Connessione al device...',               'Connecting to device...'],
  ['ovl_error_loading',  'Errore caricamento: HTTP ',              'Loading error: HTTP '],
  ['ovl_loaded_bytes_pre','Caricati ',                              'Loaded '],
  ['ovl_loaded_bytes_post',' byte dal device',                      ' bytes from device'],
  ['ovl_empty_file',     'File vuoto — nessun overlay salvato ancora (non è un errore, è normale la prima volta)',
                          'Empty file — no overlay saved yet (this is not an error, it is normal the first time)'],
  ['attr_desc_s',        'Allineamento: sinistra',                 'Alignment: left'],
  ['attr_desc_e',        'Allineamento: destra',                   'Alignment: right'],
  ['attr_desc_c',        'Allineamento: centro',                   'Alignment: center'],
  ['attr_desc_m',        'Allineamento: mezzo',                    'Alignment: middle'],
  ['attr_desc_font_pre', 'Dimensione font: ',                       'Font size: '],
  ['attr_desc_unknown',  'Sconosciuto (non documentato)',          'Unknown (undocumented)'],
  ['attr_th_code',       'Codice',                                  'Code'],
  ['attr_th_desc',       'Descrizione',                            'Description'],
  ['attr_th_rows',       'Righe',                                   'Rows'],
  ['ovl_save_failed',    'Salvataggio fallito',                    'Save failed'],
  ['ovl_saved_pre',      'Salvato (',                               'Saved ('],
  ['ovl_saved_post',     ' byte)',                                  ' bytes)'],
  ['ovl_no_rows_for',    'Nessuna riga per ',                      'No rows for '],
  ['ovl_rows_count',     ' righe per ',                            ' rows for '],
  ['ovl_card_name_only', 'Solo nome scheda per ',                  'Card name only for '],
  ['pos_h2',             'Posizioni testo (per adattare l\'SVG)',   'Text positions (to match the SVG)'],
  ['pos_hint',           'Coordinate nelle stesse unità del viewBox dell\'SVG (544 x 120). Apri il file .svg, guarda dove sono le linee divisorie, scrivi qui quei numeri. Salva, poi usa "Mostra" sopra per vedere l\'effetto.',
                          'Coordinates in the same units as the SVG viewBox (544 x 120). Open the .svg file, find the divider lines, enter those numbers here. Save, then use "Show" above to see the effect.'],
  ['pos_grid_label',     'Griglia A-E — colonne (X)',              'Grid A-E — columns (X)'],
  ['pos_grid_row_label', 'Griglia — righe (Y: normale, 2nd)',      'Grid — rows (Y: normal, 2nd)'],
  ['pos_free_label',     'Righe libere — Y per numero riga (KEY)', 'Free rows — Y per row number (KEY)'],
  ['pos_margin_label',   'Margini (allineamento s/e e blocchi multipli)', 'Margins (s/e alignment and multiple blocks)'],
  ['pos_card_name_label', 'Nome scheda magnetica — X / Y (banda superiore)',  'Magnetic card name — X / Y (top band)'],
  ['pos_templates_label', 'Template SVG — strato superiore / inferiore',  'SVG templates — top / bottom layer'],
  ['pos_tpl_top_ph',      'top.svg',                                      'top.svg'],
  ['pos_tpl_bottom_ph',   'base.svg',                                     'base.svg'],
  ['pos_templates_hint',  'Vuoti = template automatico. Primo campo = strato sopra, secondo = sotto. I file vanno caricati da /manage. I due SVG si sovrappongono a piena dimensione (stesso viewBox).',
                            'Empty = automatic template. First field = top layer, second = bottom. Upload the files from /manage. The two SVGs overlap at full size (same viewBox).'],
  ['pos_btn_add_row',    '+ Riga',                                  '+ Row'],
  ['pos_btn_save',       'Salva posizioni',                        'Save positions'],
  ['pos_btn_reset_defaults','Ripristina default',                  'Reset to defaults'],
  ['pos_saved',          'Posizioni salvate sul device',           'Positions saved to device'],
  ['pos_save_failed',    'Salvataggio posizioni fallito',          'Saving positions failed'],
  ['pos_btn_open_file',  'Apri file',                               'Open file'],
  ['pos_btn_save_file',  'Salva file',                              'Save file'],
  ['pos_downloaded',     'overlay_pos.json scaricato (backup)',     'overlay_pos.json downloaded (backup)'],
  ['pos_uploaded',       'overlay_pos.json caricato nel pannello — premi "Salva posizioni" per scriverlo sul device', 'overlay_pos.json loaded in the panel — press "Save positions" to write it to the device'],
  ['pos_invalid_file',   'File non valido: serve un overlay_pos.json', 'Invalid file: an overlay_pos.json is required'],
  ['ovl_card_preview_h2', 'Anteprima scheda magnetica',             'Magnetic card preview'],
  ['ovl_card_preview_btn', 'Mostra',                                'Show'],
  ['ovl_card_preview_status', 'Anteprima scheda magnetica slot',    'Magnetic card preview slot'],
  ['ovl_card_tpl_auto',  'Template: automatico (top.svg + base.svg)', 'Templates: automatic (top.svg + base.svg)'],
  ['ovl_card_tpl_top',   'Sopra:',                                  'Top:'],
  ['ovl_card_tpl_base',  'Sotto:',                                  'Bottom:'],
  ['ovl_card_tpl_missing', '(mancante da SPIFFS!)',                 '(missing from SPIFFS!)'],
  ['svg_missing_pre',    'Template SVG mancanti su SPIFFS: ',      'Missing SVG templates on SPIFFS: '],
  ['svg_missing_post',   ' — caricali da /manage (upload file), altrimenti il riquadro card resta vuoto.',
                          ' — upload them from /manage (file upload), otherwise the card area stays empty.'],
  ['ovl_cards_module_name', 'Schede magnetiche',                    'Magnetic cards'],

  // ── calcolatrice (WEB_IDE): NOTA: le legende dei tasti (SBR, STO, — calculator (WEB_IDE): NOTE: the key legends (SBR, STO,
  // RCL, sin, cos, Lbl, Rad, Pgm...) NON vengono tradotte di proposito: — RCL, sin, cos, Lbl, Rad, Pgm...) are intentionally NOT translated:
  // sono la riproduzione esatta dei tasti fisici del TI-59 reale, che — they are the exact reproduction of the real TI-59 physical keys, which
  // restano in inglese anche sugli esemplari venduti in Italia — non — stay in English even on units sold in Italy — not
  // sono testo di interfaccia, sono hardware. — they are interface text, they are hardware.
  ['ide_tooltip_oldnew', 'Old = timing/errori autentici TI-59, New = calcolo istantaneo. Clic per alternare (o combo +,-,x,/ sulla tastiera)',
                          'Old = authentic TI-59 timing/errors, New = instant calculation. Click to toggle (or +,-,x,/ combo on the keyboard)'],
  ['ide_tooltip_overlay','Overlay codici tasto (azzurro)',         'Key code overlay (blue)'],
  ['ide_tooltip_trace',  'Traccia passo-passo su Serial (ON = stampa ogni istruzione)', 'Step-by-step trace on Serial (ON = prints every instruction)'],
  ['ide_card_placeholder','Nessun overlay per questo programma',   'No overlay for this program'],
  ['ide_status_connecting','Connessione...',                       'Connecting...'],
  ['ide_confirm_reset',  'Reset?',                                  'Reset?'],
  ['ide_status_reset',   'Reset',                                   'Reset'],
];

const I18N = { it: {}, en: {} };
I18N_TABLE.forEach(([k, it, en]) => { I18N.it[k] = it; I18N.en[k] = en; });

function getLang() {
  return localStorage.getItem('ti59_lang') || 'it';
}

function t(key) {
  const lang = getLang();
  const v = I18N[lang] && I18N[lang][key];
  if (v === undefined) {
    console.warn('[i18n] chiave mancante:', key, 'per lingua', lang);
    return I18N.it[key] !== undefined ? I18N.it[key] : key;
  }
  return v;
}

function applyI18n() {
  const lang = getLang();
  document.documentElement.lang = lang;
  document.querySelectorAll('[data-i18n]').forEach(el => {
    el.innerHTML = t(el.getAttribute('data-i18n'));
  });
  document.querySelectorAll('[data-i18n-ph]').forEach(el => {
    el.placeholder = t(el.getAttribute('data-i18n-ph'));
  });
  document.querySelectorAll('[data-i18n-title]').forEach(el => {
    el.title = t(el.getAttribute('data-i18n-title'));
  });
}

function setLang(lang) {
  localStorage.setItem('ti59_lang', lang);
  applyI18n();
  document.querySelectorAll('[data-lang-select]').forEach(el => { el.value = lang; });
}

document.addEventListener('DOMContentLoaded', applyI18n);
)jsrc";

/* ═══════════════════════════════════════════════════════════════════
   RENDERING CARD SVG — file JS condiviso da /overlays (anteprima) e — CARD SVG RENDERING — JS file shared by /overlays (preview) and
   dalla calcolatrice (/, riquadro sopra la tastiera): un solo posto, — the calculator (/, box above the keyboard): a single place,
   così non capita più che una pagina lo veda e l'altra no — so it no longer happens that one page sees it and the other doesn't
   ("renderCardSVG is not defined").

   FIX ATTRIBUTI: ATTR (s/e/c/m/fN) applicata SEMPRE, a ogni riga, — ATTRIBUTES FIX: ATTR (s/e/c/m/fN) always applied, on every row,
   GRID o FREE che sia — prima veniva ignorata per le celle GRID e per — whether GRID or FREE — it used to be ignored for GRID cells and for
   le righe FREE con KEY 1-2 (posizione/font hardcoded lì dentro). — FREE rows with KEY 1-2 (position/font hardcoded there).

   POSIZIONI DEL TESTO — per adattarle al TUO SVG (card_grid.svg / — TEXT POSITIONS — to adapt them to YOUR SVG (card_grid.svg /
   card_free.svg): coordinate nelle stesse unità del viewBox — card_free.svg): coordinates in the same units as the SVG viewBox
   dell'SVG (qui 544 x 120). Apri il .svg (è XML), guarda dove sono le — (here 544 x 120). Open the .svg (it is XML), look at where the
   linee divisorie (<line x1=".." y1=".." x2=".." y2="..">), copia — divider lines are (<line x1=".." y1=".." x2=".." y2="..">), copy
   quei numeri in GRID_COL_X/GRID_ROW_Y/FREE_ROW_Y sotto. Solo numeri — those numbers into GRID_COL_X/GRID_ROW_Y/FREE_ROW_Y below. Only numbers
   in un file JS, nessuna ricompilazione del firmware. — in a JS file, no firmware recompilation.
   ═══════════════════════════════════════════════════════════════════ */
static const char CARDRENDER_JS[] = R"jsrc(
const CARD_W = 544, CARD_H = 120;

// Griglia A-E: 5 colonne, 2 righe (normale + 2nd). Righe libere — Grid A-E: 5 columns, 2 rows (normal + 2nd). Free rows
// (FREE): Y per numero di riga (il KEY in overlays.txt: 1, 2, 3...). — (FREE): Y per row number (the KEY in overlays.txt: 1, 2, 3...).
// MARGIN_LEFT/RIGHT: distanza dal bordo per l'allineamento 's'/'e' e — MARGIN_LEFT/RIGHT: distance from the edge for the 's'/'e' alignment and
// per il primo/ultimo blocco quando una riga FREE viene spezzata — for the first/last block when a FREE row is split
// (4+ spazi nel TESTO — v. renderCardSVG sotto). Questi sono i — (4+ spaces in TEXT — see renderCardSVG below). These are the
// DEFAULT — se esiste /overlay_pos.json sul device (v. — DEFAULTS — if /overlay_pos.json exists on the device (see
// loadCardPositions() sotto) vengono sovrascritti a runtime, senza — loadCardPositions() below) they are overwritten at runtime, without
// bisogno di riflashare per aggiustare una posizione. — needing to reflash to adjust a position.
const DEFAULT_GRID_COL_X = [54.4, 163.2, 272.0, 380.8, 489.6];
const DEFAULT_GRID_ROW_Y = [73.0, 101.0];
const DEFAULT_FREE_ROW_Y = { 1: 18, 2: 42, 3: 70, 4: 100, 5: 100 };
const DEFAULT_MARGIN_LEFT = 6;
const DEFAULT_MARGIN_RIGHT = 6;
// Posizione Y del nome della scheda magnetica, in unità viewBox — Y position of the magnetic card name, in viewBox units
// (544x120). Sulla card_card.svg il nome va nella banda superiore — (544x120). On card_card.svg the name goes in the top band
// (centro della fascia ~y=42). Modificabile dal pannello posizioni — (center of the band ~y=42). Adjustable from the positions panel
// della pagina /overlays, come tutte le altre coordinate. — of the /overlays page, like all the other coordinates.
const DEFAULT_CARD_NAME_Y = 42;
// Posizione X del nome della scheda magnetica, in unità viewBox — X position of the magnetic card name, in viewBox units
// (544x120): 272 = centro orizzontale. Modificabile dal pannello — (544x120): 272 = horizontal center. Adjustable from the panel
// posizioni della pagina /overlays, come le altre coordinate. — positions of the /overlays page, like the other coordinates.
const DEFAULT_CARD_NAME_X = 272;
let GRID_COL_X = DEFAULT_GRID_COL_X.slice();
let GRID_ROW_Y = DEFAULT_GRID_ROW_Y.slice();
let FREE_ROW_Y = Object.assign({}, DEFAULT_FREE_ROW_Y);
let MARGIN_LEFT = DEFAULT_MARGIN_LEFT;
let MARGIN_RIGHT = DEFAULT_MARGIN_RIGHT;
let CARD_NAME_Y = DEFAULT_CARD_NAME_Y;
let CARD_NAME_X = DEFAULT_CARD_NAME_X;
// Template SVG in strati (dal PRIMO = strato superiore all'ULTIMO = — SVG templates in layers (from the FIRST = top layer to the LAST =
// strato inferiore), disegnati uno sopra l'altro come background — bottom layer), drawn one on top of the other as CSS backgrounds
// multipli CSS col testo overlay sopra tutti. Vuoto = comportamento — with the overlay text on top of all. Empty = standard behaviour
// standard (un solo template scelto da renderCardSVG in base al tipo). — (a single template chosen by renderCardSVG based on the type).
let CARD_TEMPLATES = [];

// Carica /overlay_pos.json dal device, se esiste, e sovrascrive le — Loads /overlay_pos.json from the device, if present, and overwrites the
// costanti sopra. Fire-and-forget: parte subito al caricamento dello — constants above. Fire-and-forget: starts right away when the
// script, prima che l'utente interagisca — se arriva in tempo aggiorna — script loads, before the user interacts — if it arrives in time it updates
// i valori, altrimenti restano i default (mai un errore bloccante). — the values, otherwise the defaults stay (never a blocking error).
// Retry brevi (max 3): le card ora hanno i template top.svg/base.svg — Short retries (max 3): cards now have the top.svg/base.svg templates
// come default incorporato, quindi un fetch fallito non fa più cadere — as built-in default, so a failed fetch no longer makes
// il rendering sul template sbagliato né introduce attese lunghe. — the rendering fall back to the wrong template nor adds long waits.
async function loadCardPositions() {
  for (let attempt = 0; attempt < 3; attempt++) {
    try {
      const r = await fetch('/api/card_positions');
      if (!r.ok) throw new Error('http ' + r.status);
      const p = await r.json();
      if (Array.isArray(p.grid_col_x) && p.grid_col_x.length === 5) GRID_COL_X = p.grid_col_x;
      if (Array.isArray(p.grid_row_y) && p.grid_row_y.length === 2) GRID_ROW_Y = p.grid_row_y;
      if (p.free_row_y && typeof p.free_row_y === 'object') FREE_ROW_Y = p.free_row_y;
      if (typeof p.margin_left === 'number') MARGIN_LEFT = p.margin_left;
      if (typeof p.margin_right === 'number') MARGIN_RIGHT = p.margin_right;
      if (typeof p.card_name_y === 'number') CARD_NAME_Y = p.card_name_y;
      if (typeof p.card_name_x === 'number') CARD_NAME_X = p.card_name_x;
      if (Array.isArray(p.templates)) CARD_TEMPLATES = p.templates.map(t => String(t));
      return true;   // successo — success
    } catch (e) {
      if (attempt === 2) return false;   // rinuncia: restano i default — give up: the defaults stay
      await new Promise(res => setTimeout(res, 250 + attempt * 150));
    }
  }
  return false;
}
loadCardPositions();

// Verifica che i due template SVG esistano davvero su SPIFFS (causa — Verifies that the two SVG templates actually exist on SPIFFS (most
// più comune per cui il riquadro card risulta vuoto: i file caricati — common cause of an empty card box: the uploaded files
// hanno ancora il vecchio nome ml1_01.svg/ml1_02.svg invece di — still have the old name ml1_01.svg/ml1_02.svg instead of
// card_free.svg/card_grid.svg). Ritorna un array dei nomi mancanti. — card_free.svg/card_grid.svg). Returns an array of the missing names.
async function checkSvgTemplates() {
  // card_card.svg non c'è più nel design (sostituito dagli strati — card_card.svg is no longer in the design (replaced by the configured
  // configurati, es. top.svg/base.svg): lo si controlla solo se viene — layers, e.g. top.svg/base.svg): it is only checked if it is
  // richiesto esplicitamente come template dal pannello posizioni. — explicitly requested as a template from the positions panel.
  const names = ['card_free.svg', 'card_grid.svg'];
  CARD_TEMPLATES.forEach(t => {
    const n = String(t).replace(/^\/+/, '');
    if (n && !names.includes(n)) names.push(n);
  });
  const missing = [];
  for (const n of names) {
    try {
      const r = await fetch('/' + n);
      if (!r.ok) missing.push(n);
    } catch (e) { missing.push(n); }
  }
  return missing;
}

function escHtml(s) {
  const d = document.createElement('div');
  d.textContent = s;
  return d.innerHTML;
}

// ── Rettangoli attorno ai comandi tastiera — Rectangles around keyboard commands ──
// Nei testi overlay i token che corrispondono a comandi della tastiera — In overlay texts the tokens matching keyboard commands
// TI-59 (mnemonici come SBR/CLR/STO/LBL e gli operatori matematici — of the TI-59 (mnemonics like SBR/CLR/STO/LBL and the math operators
// + - × ÷ =) vengono disegnati dentro un rettangolo, come nelle card — + - × ÷ =) are drawn inside a rectangle, like on the original paper
// cartacee originali. Il match è case-insensitive e richiede che il — cards. The match is case-insensitive and requires the
// comando sia "isolato" (non incollato dentro una parola): così — command to be "isolated" (not glued inside a word): so
// "STO 00" boxa solo STO, "L.R. INIT:SBR CLR" boxa SBR e CLR ma non — "STO 00" boxes only STO, "L.R. INIT:SBR CLR" boxes SBR and CLR but not
// INIT né L.R., e i numeri registro ("00") restano fuori dal box. — INIT nor L.R., and register numbers ("00") stay outside the box.
function escRe(s) { return s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'); }

const KEY_COMMANDS = [
  '2ND','INV','LNX','INX','LOG','CE','CLR','CP','TAN','LRN','XET',
  'X^2','X²','SQRT','√X','1/X','PGM','P->R','P→R','SIN','COS','IND',
  'SST','STO','RCL','SUM','Y^X','Yˣ','INS','CMS','EXC','PROD','PRD',
  'ABS','BST','EE','DEL','ENG','FIX','INT','DEG','GTO','PAUSE','X=T',
  'NOP','OP','RAD','SBR','LBL','X>=T','X≥T','SIG+','Σ+','XBAR','X̄',
  'GRAD','RST','HIR','STFL','IFFL','DMS','D.MS','PI','Π','LIST',
  'R/S','RET','WRITE','DSZ','ADV','PRT','+/-','ST FLG','IF FLG','|X|',
  '+','-','×','÷','*','/','=','(',')','.'
];
const KEY_CMD_RE = (() => {
  const sorted = KEY_COMMANDS.slice().sort((a,b) => b.length - a.length);
  return new RegExp('(?<![A-Za-z0-9])(?:' + sorted.map(escRe).join('|') + ')(?![A-Za-z0-9])', 'gi');
})();

function boxKeyCommands(text) {
  if (!text) return '';
  return text.replace(KEY_CMD_RE, m =>
    '<span style="display:inline-block;border:1px solid currentColor;border-radius:3px;padding:0 3px;margin:0 1px;line-height:1.1">'+escHtml(m)+'</span>'
  );
}

// Apice/pedice nel testo overlay, con marcatori espliciti: ^+ apre — Superscript/subscript in the overlay text, with explicit markers: ^+ opens
// l'apice, ^- apre il pedice, ^^ chiude e torna al testo normale. — superscript, ^- opens subscript, ^^ closes and returns to normal text.
// Es. "X^+2^^" = X con 2 in apice; "10^-3^^" = 10 con -3 in pedice. — E.g. "X^+2^^" = X with 2 as superscript; "10^-3^^" = 10 with -3 as subscript.
// Applicata PRIMA di boxKeyCommands, così i comandi scritti dentro un — Applied BEFORE boxKeyCommands, so the commands written inside a
// apice/pedice restano boxati come quelli normali. — superscript/subscript stay boxed like normal ones.
function supSub(text) {
  if (!text || text.indexOf('^') < 0) return text;
  let out = '', state = '';
  for (let i = 0; i < text.length; i++) {
    const ch = text[i];
    if (ch === '^' && i + 1 < text.length) {
      const nx = text[i + 1];
      if (nx === '+' || nx === '-') {
        if (state) { out += '</' + state + '>'; state = ''; }
        state = (nx === '+') ? 'sup' : 'sub';
        out += '<' + state + '>';
        i++;
        continue;
      }
      if (nx === '^') {
        if (state) { out += '</' + state + '>'; state = ''; }
        i++;
        continue;
      }
    }
    out += ch;
  }
  if (state) out += '</' + state + '>';
  return out;
}

// Decodifica ATTR (s/e/c/m/fN) in allineamento + dimensione font. — Decodes ATTR (s/e/c/m/fN) into alignment + font size.
// Applicata SEMPRE (fix: prima veniva ignorata in diversi casi). — Always applied (fix: it used to be ignored in several cases).
function applyAttr(attr, defaultFs) {
  attr = attr || '';
  let align = 'c';
  if (attr.indexOf('s') >= 0) align = 's';
  else if (attr.indexOf('e') >= 0) align = 'e';
  else if (attr.indexOf('m') >= 0) align = 'm';
  else if (attr.indexOf('c') >= 0) align = 'c';
  const fm = attr.match(/f(\d+)/);
  const fs = fm ? parseInt(fm[1]) : defaultFs;
  return { align, fs };
}

function freeRowY(kn) {
  if (FREE_ROW_Y[kn] !== undefined) return FREE_ROW_Y[kn];
  const keys = Object.keys(FREE_ROW_Y).map(Number).sort((a,b) => a-b);
  const last = keys[keys.length-1], prev = keys[keys.length-2];
  const step = (prev !== undefined) ? (FREE_ROW_Y[last]-FREE_ROW_Y[prev]) : 25;
  return FREE_ROW_Y[last] + step * (kn - last);
}

function renderCardSVG(mod, prog, rows, cardName) {
  const isCard = mod === 'card';
  const hasName = isCard && cardName;
  rows = rows || [];
  const gridRows = rows.filter(r => r.type === 'GRID');
  const freeRows = rows.filter(r => r.type === 'FREE');
  // Una scheda magnetica attiva si vede SEMPRE, anche senza righe — An active magnetic card is ALWAYS shown, even without overlay
  // overlay: strati top.svg + base.svg + nome scheda in alto. Per — rows: top.svg + base.svg layers + card name on top. For
  // tutto il resto, senza righe non c'è niente da disegnare. — everything else, without rows there is nothing to draw.
  if (!gridRows.length && !freeRows.length && !hasName && !CARD_TEMPLATES.length && !isCard) return '<div class="card-placeholder"></div>';
  // Il template (sfondo con le linee divisorie) dipende solo dalla — The template (background with the divider lines) depends only on the
  // presenza della griglia, non dalla prima riga del file. Per le schede — presence of the grid, not on the first row of the file. For magnetic
  // magnetiche il design è definito dagli strati (es. top.svg + base.svg): — cards the design is defined by the layers (e.g. top.svg + base.svg):
  // di default SONO top.svg + base.svg incorporati qui sotto, così la — by default they ARE top.svg + base.svg embedded below, so the
  // card non dipende dal fetch di /api/card_positions (che nel burst di — card does not depend on the fetch of /api/card_positions (which in the
  // richieste della pagina può essere scartato dal server e lasciare — page request burst can be dropped by the server and leave
  // CARD_TEMPLATES vuoto, facendo cadere la card sul fallback ambrato). — CARD_TEMPLATES empty, making the card fall back to the amber fallback).
  // Se /api/card_positions arriva (pannello posizioni), CARD_TEMPLATES — If /api/card_positions arrives (positions panel), CARD_TEMPLATES
  // lo sovrascrive. Le ROM restano sui template universali griglia/free. — overrides it. ROMs stay on the universal grid/free templates.
  const hasGrid = gridRows.length > 0;
  const tmpl = hasGrid ? '/card_grid.svg' : '/card_free.svg';
  const w = CARD_W, h = CARD_H;
  // Se CARD_TEMPLATES è configurato (pannello posizioni), usa QUEI — If CARD_TEMPLATES is configured (positions panel), use THOSE
  // file in strati sovrapposti (primo = sopra, ultimo = sotto) al — files in overlapping layers (first = top, last = bottom) instead
  // posto del default: background multipli CSS, il testo overlay — of the default: multiple CSS backgrounds, the overlay text
  // viene disegnato sopra a tutti. Senza config, le card magnetiche — is drawn on top of all. Without config, magnetic cards
  // usano comunque top.svg + base.svg; le ROM il template unico. — still use top.svg + base.svg; ROMs the single template.
  const defaultTmpls = isCard ? ['/top.svg', '/base.svg'] : [tmpl];
  // CARD_TEMPLATES (pannello posizioni) vale SOLO per le card — CARD_TEMPLATES (positions panel) applies ONLY to magnetic
  // magnetiche: quelle configurano i propri strati (es. top.svg + — cards: they configure their own layers (e.g. top.svg +
  // base.svg). Le ROM usano SEMPRE il template universale (card_grid / — base.svg). ROMs ALWAYS use the universal template (card_grid /
  // card_free) e devono ignorare CARD_TEMPLATES — altrimenti ereditano — card_free) and must ignore CARD_TEMPLATES — otherwise they inherit
  // gli strati delle card (che possono contenere testo stampato) e si — the card layers (which may contain printed text) and the
  // vede la stessa riga disegnata due volte: una volta come testo HTML — same row is drawn twice: once as HTML overlay text
  // overlay e una volta come parte del template SVG. — and once as part of the SVG template.
  const tmpls = isCard ? ((CARD_TEMPLATES.length) ? CARD_TEMPLATES.slice() : defaultTmpls) : defaultTmpls;
  const bgImage  = tmpls.map(t => 'url(' + t + ')').join(',');
  // Tutti gli strati riempiono per intero il riquadro (gli SVG base e — All layers fill the whole box (the base and top SVGs
  // top hanno le stesse dimensioni viewBox 544x120 e combaciano). — have the same viewBox 544x120 dimensions and match).
  const bgSize   = tmpls.map(() => '100% 100%').join(',');
  const bgPos    = tmpls.map(() => '0 0').join(',');
  const bgRepeat = tmpls.map(() => 'no-repeat').join(',');
  let html = '<div class="card-inner" style="position:relative;width:100%;height:0;padding-bottom:'+(h/w*100)+'%;background-image:'+bgImage+';background-size:'+bgSize+';background-repeat:'+bgRepeat+';background-position:'+bgPos+';overflow:hidden">';
  // Testo NERO sulle schede magnetiche (card_card.svg ha banda chiara), — BLACK text on magnetic cards (card_card.svg has a light band),
  // ambra sugli overlay ROM (card_grid/free, sfondo scuro). — amber on ROM overlays (card_grid/free, dark background).
  const color = isCard ? '#000000' : '#d36910';

  // ── Nome scheda magnetica (banda superiore della card_card.svg) — Magnetic card name (top band of card_card.svg) ──
  // Ogni card salvata ha un nome (Campo "name" del file JSON, esposto — Every saved card has a name ("name" field of the JSON file, exposed
  // da /api/status come active_card_name): viene disegnato centrato — by /api/status as active_card_name): it is drawn centered
  // nella fascia alta, dove la posizione Y si regola dal pannello — in the top band, where the Y position is adjusted from the
  // posizioni. Le righe overlay (FREE/GRID) eventuali vengono — positions panel. Any overlay rows (FREE/GRID) are then
  // sovrapposte sopra, come per le ROM. — overlaid on top, as for the ROMs.
  if (hasName) {
    const y = CARD_NAME_Y;
    const x = CARD_NAME_X;
    html += '<div style="position:absolute;left:'+(x/w*100)+'%;top:'+(y/h*100)+'%;transform:translate(-50%,-50%);font-size:20px;color:'+color+';font-weight:bold;font-family:Arial;white-space:nowrap;text-align:center">'+escHtml(cardName)+'</div>';
  }

  // ── Righe libere (intestazione + card stile ML-01) — Free rows (header + ML-01 style card) ──
  freeRows.forEach(row => {
    const kn = parseInt(row.key) || 1;
    const y = freeRowY(kn);
    const { align, fs } = applyAttr(row.attr, 14);

    // Più di 3 spazi consecutivi (>=4) nel TESTO = confine tra — More than 3 consecutive spaces (>=4) in the TEXT = boundary between
    // blocchi comandi distinti sulla stessa riga. Un solo blocco: — distinct command blocks on the same row. A single block:
    // allineamento da ATTR (s/e/c/m). Due o più: distribuiti — alignment from ATTR (s/e/c/m). Two or more: distributed
    // "space-between" — primo a sinistra, ultimo a destra, quelli — "space-between" — first on the left, last on the right, the ones
    // in mezzo equidistanti (3 blocchi = sinistra/centro/destra). — in the middle equally spaced (3 blocks = left/center/right).
    const blocks = row.text.split(/ {4,}/).map(s => s.trim()).filter(s => s.length);

    if (blocks.length <= 1) {
      let x, tx;
      if (align === 's') { x = MARGIN_LEFT; tx = '0%'; }
      else if (align === 'e') { x = w - MARGIN_RIGHT; tx = '-100%'; }
      else { x = w/2; tx = '-50%'; }
      html += '<div style="position:absolute;left:'+(x/w*100)+'%;top:'+(y/h*100)+'%;transform:translate('+tx+',-50%);font-size:'+fs+'px;color:'+color+';font-weight:bold;font-family:Arial;white-space:nowrap">'+boxKeyCommands(supSub(row.text))+'</div>';
    } else {
      const n = blocks.length;
      blocks.forEach((blockText, i) => {
        let x, tx;
        if (i === 0)        { x = MARGIN_LEFT;        tx = '0%';    }
        else if (i === n-1) { x = w - MARGIN_RIGHT;    tx = '-100%'; }
        else                { x = w * (i/(n-1)); tx = '-50%'; }
        html += '<div style="position:absolute;left:'+(x/w*100)+'%;top:'+(y/h*100)+'%;transform:translate('+tx+',-50%);font-size:'+fs+'px;color:'+color+';font-weight:bold;font-family:Arial;white-space:nowrap">'+boxKeyCommands(supSub(blockText))+'</div>';
      });
    }
  });

  // ── Griglia A-E / A'-E' — Grid A-E / A'-E' ─────────────────────
  if (hasGrid) {
    const keys = [['A','B','C','D','E'],["A'","B'","C'","D'","E'"]];
    for (let r = 0; r < 2; r++) {
      for (let c = 0; c < 5; c++) {
        const kn = keys[r][c];
        const row = gridRows.find(x => x.key === kn);
        const txt = (row && row.text) ? row.text : '';
        if (!txt) continue;
        const { fs } = applyAttr(row.attr, r === 0 ? 12 : 11);
        const x = GRID_COL_X[c]/w*100;
        const y = GRID_ROW_Y[r]/h*100;
        html += '<div style="position:absolute;left:'+x+'%;top:'+y+'%;transform:translate(-50%,-50%);font-size:'+fs+'px;color:'+color+';font-weight:bold;font-family:Arial;white-space:nowrap">'+boxKeyCommands(supSub(txt))+'</div>';
      }
    }
  }
  return html + '</div>';
}
)jsrc";

/* ═══════════════════════════════════════════════════════════════════
   PAGINA DI CONFIGURAZIONE WiFi (captive portal) — WiFi CONFIGURATION PAGE (captive portal)
   ═══════════════════════════════════════════════════════════════════ */
static const char WEB_SETUP[] = R"rawhtml(<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TI-59 Zombie — Configurazione WiFi</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:#0d0d0b;color:#d8d6ce;font-family:'Courier New',monospace;
       display:flex;flex-direction:column;align-items:center;padding:16px;gap:12px}
  h1{color:#c49a3a;font-size:18px;letter-spacing:2px;margin-bottom:4px}
  .panel{background:#1e1e1c;border:1px solid #3a3a36;border-radius:6px;padding:12px;width:100%;max-width:380px}
  .panel h2{font-size:11px;color:#6a6a62;text-transform:uppercase;letter-spacing:1px;margin-bottom:8px}
  .net-row{display:flex;justify-content:space-between;align-items:center;padding:6px 4px;background:#0a0a08;border-radius:3px;margin-bottom:4px;font-size:12px}
  .net-row button{font-size:10px;padding:2px 8px;border:none;border-radius:2px;background:#0a5c4a;color:#fff;cursor:pointer}
  .net-row .rssi{color:#6a6a62;font-size:10px;margin-left:6px}
  .form-row{display:flex;gap:6px;margin-top:8px;flex-wrap:wrap}
  input{flex:1;background:#0a0a08;border:1px solid #3a3a36;color:#d8d6ce;padding:6px;border-radius:3px;font-size:12px}
  button.btn{padding:6px 12px;border:none;border-radius:3px;cursor:pointer;font-weight:bold;font-size:12px}
  .btn-amber{background:#c49a3a;color:#000}
  .btn-teal{background:#0a5c4a;color:#fff}
  .btn-red{background:#5a1a1a;color:#fff}
  .saved{display:flex;justify-content:space-between;padding:5px;background:#0a0a08;border-radius:3px;margin-bottom:4px;font-size:12px}
  .saved button{background:#5a1a1a;color:#fff;border:none;border-radius:2px;padding:2px 6px;font-size:10px;cursor:pointer}
  .status{font-size:10px;color:#6a6a62;text-align:center;margin-top:6px}
</style>
</head>
<body>
<h1>TI-59 ZOMBIE</h1>
<div class="panel">
  <h2 data-i18n="setup_h2_networks">Reti disponibili</h2>
  <div id="scan-list"><div class="status" data-i18n="setup_scanning">Scansione...</div></div>
  <div class="form-row">
    <button class="btn btn-teal" onclick="scan()" data-i18n="setup_btn_refresh">&#x27F3; Aggiorna</button>
    <button class="btn btn-amber" onclick="tryConnect()" data-i18n="setup_btn_connect">Connetti</button>
  </div>
</div>
<div class="panel">
  <h2><span data-i18n="setup_h2_saved">Reti salvate (</span><span id="cred-count">0</span>/4)</h2>
  <div id="cred-list"></div>
</div>
<div class="panel">
  <h2 data-i18n="setup_h2_new">Nuova rete</h2>
  <input id="ssid" placeholder="SSID" maxlength="32">
  <input id="pass" type="password" placeholder="Password" maxlength="64" style="margin-top:6px">
  <div class="form-row">
    <button class="btn btn-amber" onclick="addCred()" data-i18n="setup_btn_save_connect">Salva &amp; Connetti</button>
    <button class="btn btn-red" onclick="clearAll()" data-i18n="setup_btn_clear_all">Cancella tutto</button>
  </div>
</div>
<div class="panel">
  <h2 data-i18n="setup_h2_wifi_file">File credenziali (wifi.json)</h2>
  <input type="file" id="wifi-file" accept=".json,application/json" style="margin-top:4px">
  <div class="form-row">
    <button class="btn btn-teal" onclick="downloadWifiFile()" data-i18n="setup_btn_wifi_dl">Scarica file</button>
    <button class="btn btn-amber" onclick="uploadWifiFile()" data-i18n="setup_btn_wifi_ul">Carica file</button>
  </div>
</div>
<div class="status" id="status" data-i18n="setup_status_waiting">In attesa...</div>
<script src="/i18n.js?v=3"></script>
<script>
const API='';
async function scan(){
  document.getElementById('scan-list').innerHTML='<div class=status>'+t('setup_scanning')+'</div>';
  try{
    const r=await fetch(API+'/api/wifi/scan');
    const nets=await r.json();
    const el=document.getElementById('scan-list');
    if(!nets.length){el.innerHTML='<div class=status>'+t('setup_no_networks')+'</div>';return;}
    el.innerHTML='';
    nets.forEach(n=>{
      const div=document.createElement('div');div.className='net-row';
      div.innerHTML='<span>'+n.ssid+'<span class=rssi>'+n.rssi+'dBm</span></span>'+
        '<button onclick="pick(\''+n.ssid.replace(/\\/g,'\\\\').replace(/'/g,"\\'")+'\')">'+t('setup_select')+'</button>';
      el.appendChild(div);
    });
  }catch(e){document.getElementById('scan-list').innerHTML='<div class=status>'+t('setup_scan_error')+'</div>';}
}
async function loadCreds(){
  try{
    const r=await fetch(API+'/api/wifi/creds');
    const c=await r.json();
    document.getElementById('cred-count').textContent=c.length;
    const el=document.getElementById('cred-list');
    if(!c.length){el.innerHTML='<div class=status>'+t('setup_none')+'</div>';return;}
    el.innerHTML='';
    c.forEach((x)=>{
      const div=document.createElement('div');div.className='saved';
      div.innerHTML='<span>'+x.ssid+'</span><button onclick="delCred('+x.idx+')">&#x2715;</button>';
      el.appendChild(div);
    });
  }catch(e){}
}
function pick(ssid){document.getElementById('ssid').value=ssid;}
async function addCred(){
  const ssid=document.getElementById('ssid').value.trim();
  const pass=document.getElementById('pass').value;
  if(!ssid){alert(t('setup_enter_ssid'));return;}
  document.getElementById('status').textContent=t('setup_connecting');
  try{
    const r=await fetch(API+'/api/wifi/creds',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,pass})});
    const j=await r.json();
    if(j.ok){document.getElementById('status').textContent=t('setup_connected_restart');setTimeout(()=>location.reload(),4000);}
    else{document.getElementById('status').textContent=t('setup_failed')+(j.error||'?');}
  }catch(e){document.getElementById('status').textContent=t('status_error')+e.message;}
}
async function tryConnect(){
  document.getElementById('status').textContent=t('setup_trying_connect');
  try{
    const r=await fetch(API+'/api/wifi/connect',{method:'POST'});
    const j=await r.json();
    if(j.ok){document.getElementById('status').textContent=t('setup_connected_restart');setTimeout(()=>location.reload(),4000);}
    else{document.getElementById('status').textContent=t('setup_failed')+(j.error||'?');}
  }catch(e){document.getElementById('status').textContent=t('status_error')+e.message;}
}
async function delCred(idx){
  if(!confirm(t('confirm_delete')))return;
  await fetch(API+'/api/wifi/creds?idx='+idx,{method:'DELETE'});
  loadCreds();
}
async function clearAll(){
  if(!confirm(t('setup_confirm_clear_all')))return;
  for(let i=3;i>=0;i--) await fetch(API+'/api/wifi/creds?idx='+i,{method:'DELETE'});
  loadCreds();
}
async function downloadWifiFile(){
  try{
    const r=await fetch(API+'/api/wifi/file');
    const text=await r.text();
    const a=document.createElement('a');
    a.href='data:text/json;charset=utf-8,'+encodeURIComponent(text);
    a.download='wifi.json';a.click();
  }catch(e){document.getElementById('status').textContent=t('status_error')+e.message;}
}
async function uploadWifiFile(){
  const inp=document.getElementById('wifi-file');
  if(!inp.files.length){alert(t('setup_choose_file'));return;}
  document.getElementById('status').textContent=t('setup_connecting');
  try{
    const text=await inp.files[0].text();
    const r=await fetch(API+'/api/wifi/file',{method:'POST',body:text});
    const j=await r.json();
    if(j.ok){document.getElementById('status').textContent=t('setup_file_saved');setTimeout(()=>location.reload(),4000);}
    else{document.getElementById('status').textContent=t('setup_failed')+(j.error||'?');}
  }catch(e){document.getElementById('status').textContent=t('status_error')+e.message;}
}
scan();loadCreds();
</script>
</body>
</html>)rawhtml";

/* ═══════════════════════════════════════════════════════════════════
   PAGINA IDE
   ═══════════════════════════════════════════════════════════════════ */
static const char WEB_IDE[] = R"rawhtml(<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>TI-59 Zombie</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap');
  :root {
    --bg:#0d0d0b; --panel:#1e1e1c; --border:#3a3a36;
    --amber:#c49a3a; --amber-l:#e8c86a; --teal:#0a5c4a;
    --text:#d8d6ce; --muted:#6a6a62; --led:#ff2020;
    --key-bg:#2a2a28; --key-num:#3a3a36; --key-op:#c49a3a;
    --key-cream:#e4ddc8; --key-cream-text:#161512;
    --code-color: #00ccff; /* Azzurro per i codici tasto — Cyan for the key codes */
  }
  *{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
  body{background:var(--bg);color:var(--text);
       font-family:'Share Tech Mono','Courier New',monospace;
       display:flex;flex-direction:column;align-items:center;
       padding:6px;gap:6px;min-height:100vh}

  .header{display:flex;align-items:baseline;justify-content:space-between;
          width:100%;max-width:420px;margin-bottom:1px;gap:6px}
  .header h1{color:var(--amber);font-size:16px;letter-spacing:3px;font-weight:400;
             white-space:nowrap}
  .header .side-label{color:var(--muted);font-size:9px;letter-spacing:1px;white-space:nowrap}

  .display-wrap{background:#0a0a08;border:2px solid var(--amber);border-radius:4px;
                padding:6px 10px;width:100%;max-width:420px;
                box-shadow:inset 0 0 20px rgba(255,32,32,0.05)}
  .led-display{display:flex;justify-content:flex-end;align-items:center;
            min-height:50px;gap:2px;padding:2px 0}
  .digit-group{position:relative;width:30px;height:40px;flex-shrink:0}
  .led-digit{position:relative;width:24px;height:40px}
  .led-seg{position:absolute;background:#1a0f05;transition:all 0.15s}
  .led-seg.on{background:var(--led);box-shadow:0 0 8px rgba(255,32,32,0.7),0 0 16px rgba(255,32,32,0.3)}
  .led-seg.a{top:0;left:3px;right:3px;height:4px;clip-path:polygon(0 100%,15% 0,85% 0,100% 100%)}
  .led-seg.b{top:2px;right:0;width:4px;height:16px;clip-path:polygon(0 0,100% 15%,100% 85%,0 100%)}
  .led-seg.c{bottom:2px;right:0;width:4px;height:16px;clip-path:polygon(0 0,100% 15%,100% 85%,0 100%)}
  .led-seg.d{bottom:0;left:3px;right:3px;height:4px;clip-path:polygon(0 0,15% 100%,85% 100%,100% 0)}
  .led-seg.e{bottom:2px;left:0;width:4px;height:16px;clip-path:polygon(100% 0,0 15%,0 85%,100% 100%)}
  .led-seg.f{top:2px;left:0;width:4px;height:16px;clip-path:polygon(100% 0,0 15%,0 85%,100% 100%)}
  .led-seg.g{top:18px;left:3px;right:3px;height:4px;clip-path:polygon(0 50%,15% 0,85% 0,100% 50%,85% 100%,15% 100%)}
  .led-dp{position:absolute;bottom:0;right:0;width:6px;height:6px;
          border-radius:50%;background:#1a0f05;z-index:2}
  .led-dp.on{background:var(--led);box-shadow:0 0 8px rgba(255,32,32,0.9),0 0 16px rgba(255,32,32,0.4)}

  .led-op{position:absolute;bottom:0;right:0;width:6px;height:6px;
          border-radius:50%;background:#1a0f05;z-index:3}
  .led-op.on{background:var(--led);box-shadow:0 0 10px rgba(255,32,32,0.9)}

  /* Riquadro overlay scheda: caricato via JS che recupera il — Card overlay box: loaded via JS that fetches the
     template SVG universale (card_free.svg / card_grid.svg, uguale — universal SVG template (card_free.svg / card_grid.svg, the same
     per tutti i moduli libreria, non solo ml1) e vi sovrappone — for all library modules, not only ml1) and overlays
     il testo overlay usando HTML/CSS posizionamento assoluto. — the overlay text using absolute HTML/CSS positioning. */
  .card-slide {
    width: 100%;
    max-width: 420px;
    margin: 0 auto;
    box-sizing: border-box;
    aspect-ratio: 544 / 120;
    display: block;
    border: 1px solid var(--amber);
    border-radius: 4px;
    background: #0d0d0b;
    /* Il riquadro resta SEMPRE fisso a schermo durante lo slide: — The box ALWAYS stays fixed on screen during the slide:
       l'animazione di entrata/uscita agisce sul contenuto interno — the enter/exit animation acts on the inner content
       (.card-inner/.card-placeholder), che viene ritagliato dai — (.card-inner/.card-placeholder), which is clipped by the
       bordi del box grazie a overflow:hidden — v. updateKeyOverlay(). — box edges thanks to overflow:hidden — see updateKeyOverlay(). */
    overflow: hidden;
  }
  /* Animazione cambio overlay: quando il contenuto cambia (nuovo — Overlay change animation: when the content changes (new
     programma/modulo/scheda attiva) il vecchio overlay esce — program/module/active card) the old overlay exits
     scivolando a destra DENTRO il riquadro, poi quello nuovo entra — sliding to the right INSIDE the box, then the new one enters
     da destra — v. updateKeyOverlay() + slideInCard()/slideOutCard(). — from the right — see updateKeyOverlay() + slideInCard()/slideOutCard().
     Le classi vengono messe sul figlio diretto (il contenuto), non — The classes are put on the direct child (the content), not
     sul .card-slide stesso, così il riquadro non si muove. — on the .card-slide itself, so the box does not move. */
  @keyframes cardSlideIn {
    from { transform: translateX(60%); opacity: 0.3; }
    to   { transform: translateX(0);   opacity: 1;   }
  }
  @keyframes cardSlideOut {
    from { transform: translateX(0);   opacity: 1;   }
    to   { transform: translateX(60%); opacity: 0.3; }
  }
  .card-slide > .card-inner.card-inserting,
  .card-slide > .card-placeholder.card-inserting {
    animation: cardSlideIn 0.4s cubic-bezier(0.22, 0.61, 0.36, 1);
  }
  .card-slide > .card-inner.card-leaving,
  .card-slide > .card-placeholder.card-leaving {
    animation: cardSlideOut 0.4s cubic-bezier(0.22, 0.61, 0.36, 1);
  }
  /* Barra titolo sopra il riquadro overlay — mostra il nome della — Title bar above the overlay box — shows the name of the
     scheda magnetica attiva (letto da /api/status, campo — active magnetic card (read from /api/status, field
     active_card_name). Vuota = nascosta (:empty), niente da gestire — active_card_name). Empty = hidden (:empty), nothing to manage
     lato JS oltre a mettere/togliere il testo. — on the JS side besides adding/removing the text. */
  .card-title-bar {
    width: 100%;
    max-width: 420px;
    margin: 0 auto 4px;
    text-align: center;
    font-size: 10px;
    letter-spacing: 1px;
    color: var(--amber-l);
    text-transform: uppercase;
  }
  .card-title-bar:empty { display: none; margin: 0; }
  .card-placeholder {
    color: var(--muted);
    font-size: 10px;
    letter-spacing: 1px;
    text-transform: uppercase;
    text-align: center;
    line-height: 120px;
  }
  .mode-bar{display:flex;gap:4px;margin-top:3px;font-size:9px;color:var(--muted);
            justify-content:center;flex-wrap:wrap}
  .mode-badge{padding:1px 5px;border-radius:2px;background:var(--border);
              font-size:8px;letter-spacing:1px;cursor:default}
  .mode-badge.on{background:var(--amber);color:#000}
  

  
  #mode-oldnew,#mode-overlay{cursor:pointer;user-select:none}

  /* Modifica tastiera: più piccola e spaziata — Keyboard tweak: smaller and more spaced */
  .kbd{display:grid;grid-template-columns:repeat(5,1fr);gap:6px 20px; /*    griglia — grid    */
       width:100%;max-width:380px;
       padding:0 4px;margin-top:4px}

  .key-wrap{display:flex;flex-direction:column;align-items:center;position:relative}

  /* Etichetta tasto più piccola — Smaller key label */
  .key-label{color:var(--amber-l);font-size:14px; /* Font size invariata — Font size unchanged */
             line-height:1;font-family: 'Arial Narrow', 'Oswald', sans-serif;
			 font-weight: 700;margin-bottom:1px; /* Margine ulteriormente ridotto — Margin further reduced */
             text-align:center;min-height:16px;letter-spacing:0.5px;white-space:nowrap}
  .key-label:empty{visibility:hidden}

  /* CSS per overlay codici tasto (azzurro) — CSS for the key-code overlay (blue) */
  .key-code-overlay {
      position: absolute;
      left: -15px; /* Spinge i numeri fuori dal tasto, a sinistra — Pushes the numbers out of the key, to the left */
      bottom: 2px; /* Li allinea in basso, all'altezza del tasto — Aligns them at the bottom, at the key height */
      display: flex;
      flex-direction: column; /* Li impila uno sopra l'altro — Stacks them one above the other */
      align-items: flex-end; /* Li allinea verso il bordo del tasto — Aligns them toward the key edge */
      gap: 3px; /* Spazio tra il numero sopra e quello sotto — Space between the number above and the one below */
      pointer-events: none;
      opacity: 0;
      transition: opacity 0.3s;
  }
  .key-code-overlay.on {opacity: 1;}
  
/* Contenitore delle scritte: forza l'affiancamento sulla stessa riga — Text container: forces side-by-side placement on the same row */
.bottom-controls {
    display: flex; /*!important;*/ /* non più forzato — no longer forced */
    justify-content: space-between; /*!important; */ /* non più forzato — no longer forced */
    align-items: flex-start;
    width: 100%;
    max-width: 360px; /* Regola questo numero se la tastiera è più larga o più stretta — Adjust this number if the keyboard is wider or narrower */
    margin: 5px auto 20px auto; /* Centra il blocco sotto la tastiera — Centers the block below the keyboard */
    /*padding: 0 10px;  Sposta le scritte verso l'interno per allinearle ai tasti R/S e = — Moves the text inward to align it with the R/S and = keys */
    box-sizing: border-box;
	font-family: 'Arial Narrow', 'Oswald', sans-serif;
}

/* --- DESTRA: TI Programmable 59 (AMBRA) — RIGHT: TI Programmable 59 (AMBER) --- */
.brand-amber {
	font-family: 'Arial Narrow', 'Oswald', sans-serif;
	left: 30px;
    color: #FFBF00; 
    text-align: right;
    line-height: 1.1; 
    font-weight: bold;
    font-size: 18pt;
    user-select: none;
}

.brand-amber .sub-brand {
	left: 50px;
	font-family: 'Arial Narrow', 'Oswald', sans-serif;
    font-size: 15pt; 
    font-weight: normal;
    
}

/* --- SINISTRA: Key Code / Overlay (AZZURRO) — LEFT: Key Code / Overlay (BLUE) --- */
.blue-text-block {
    color: #00BFFF; 
	right: 50px;
	position: static;
    text-align: left;
    line-height: 1.1; 
	font-family: 'Arial Narrow', 'Oswald', sans-serif;
    font-weight: bold;
    font-size: 12pt; 
    user-select: none;
    visibility: hidden; /* Nasconde il testo ma mantiene l'ingombro per non spostare la scritta di destra — Hides the text but keeps the space so the right text is not moved */
}

/* Quando si accende l'overlay, la scritta appare — When the overlay is turned on, the text appears */
.blue-text-block.on {
    visibility: visible;
}
  
  .code-2nd, .code-normal {
      position: static; /* Rimuove il vecchio posizionamento assoluto — Removes the old absolute positioning */
      font-size: 10px; /* Leggermente più grandi per una migliore lettura — Slightly larger for better readability */
      color: #00d2ff !important; /* Azzurro acceso visibile sullo sfondo scuro — Bright cyan visible on the dark background */
      font-weight: bold;
      font-family: monospace;
      line-height: 1;
  }

  /* Tasto più piccolo — Smaller key */
  .key{background:var(--key-bg);border:1px solid var(--border);border-radius:5px;
       padding:4px 1px; margin:0 8px; /* Padding ridotto — Reduced padding */
       text-align:center;cursor:pointer;user-select:none;
       transition:all .08s;display:flex;flex-direction:column;
       align-items:center;justify-content:center;min-height:30px; /* Altezza minima ridotta — Reduced minimum height */
       width:100%}
  .key:active,.key.pressed{background:var(--amber);border-color:var(--amber-l);
                            transform:scale(.95)}
  .key:active .main,.key.pressed .main{color:#000}

  /* Testo tasto più piccolo — Smaller key text */
  .key .main{font-size:20px; /* Font size ridotta — Reduced font size */
             font-weight:bold;display:block;line-height:1;font-family: 'Arial Narrow', 'Oswald', sans-serif;}

  .key.num{background:var(--key-cream);border-color:#b8b090}
  .key.num .main{color:var(--key-cream-text)}

  .key.op,.key.amber,.key.snd{background:var(--amber);border-color:var(--amber-l)}
  .key.op .main,.key.amber .main,.key.snd .main{color:#000}

  .key.special{background:#1a1a18;border-color:#555}
  .key.special .main{color:var(--text)}

  .panel{background:var(--panel);border:1px solid var(--border);border-radius:6px;
         padding:7px;width:100%;max-width:420px}
  .panel h2{font-size:10px;color:var(--muted);text-transform:uppercase;
             letter-spacing:2px;margin-bottom:4px}
  .card-list{display:flex;flex-direction:column;gap:3px;max-height:140px;
              overflow-y:auto;font-size:10px}
  .card-item{display:flex;gap:6px;align-items:center;padding:4px 6px;
              background:var(--bg);border-radius:3px}
  .card-item .slot{color:var(--muted);width:20px;font-size:9px}
  .card-item .name{flex:1;font-size:10px}
  .card-item button{font-size:9px;padding:2px 6px;border:none;border-radius:2px;
                    cursor:pointer;background:var(--teal);color:#fff}
  .card-item button.del{background:#5a1a1a}
  .input-row{display:flex;gap:4px;margin-top:6px}
  .input-row input{flex:1;background:var(--bg);border:1px solid var(--border);
                    color:var(--text);padding:4px 6px;border-radius:3px;font-size:10px}
  .input-row button{padding:4px 8px;background:var(--amber);color:#000;
                    border:none;border-radius:3px;cursor:pointer;font-weight:bold;font-size:10px}
  .prog-area{width:100%;height:80px;background:var(--bg);color:var(--text);
              border:1px solid var(--border);border-radius:3px;font-size:9px;
              padding:4px;font-family:monospace;resize:vertical}
  .btn-row{display:flex;gap:4px;margin-top:4px;flex-wrap:wrap}
  .btn{padding:4px 8px;border:none;border-radius:3px;cursor:pointer;font-size:10px;
       font-weight:bold}
  .btn-amber{background:var(--amber);color:#000}
  .btn-teal{background:var(--teal);color:#fff}
  .btn-red{background:#5a1a1a;color:#fff}
  .btn-gray{background:var(--border);color:var(--text)}
  .status{font-size:9px;color:var(--muted);text-align:center;padding:4px}

  ::-webkit-scrollbar{width:4px}
  ::-webkit-scrollbar-track{background:var(--bg)}
  ::-webkit-scrollbar-thumb{background:var(--border);border-radius:2px}
  
  @keyframes blinker { 0%, 49% { opacity: 1; } 50%, 100% { opacity: 0; } }
  .error-blink { animation: blinker 1s infinite; }
  
</style>
</head>
<body>
<div class="header">
  <span class="side-label">..........ESP32-S3</span>
  <h1>TI-59 ZOMBIE</h1>
  <span class="side-label">TMS1500...........</span>
</div>
<div class="display-wrap">
  <div class="led-display" id="display"></div>
  <div class="mode-bar">
    <span class="mode-badge on" id="mode-deg">DEG</span>
    <span class="mode-badge" id="mode-2nd">2ND</span>
    <span class="mode-badge" id="mode-inv">INV</span>
    <span class="mode-badge" id="mode-lrn">LRN</span>
    <span class="mode-badge" id="mode-run">RUN</span>
    <span class="mode-badge" id="mode-fix">FIX</span>
    <span class="mode-badge" id="mode-oldnew" title="Old = timing/errori autentici TI-59, New = calcolo istantaneo. Clic per alternare (o combo +,-,x,/ sulla tastiera)" data-i18n-title="ide_tooltip_oldnew">OLD</span>
    <span class="mode-badge" id="mode-overlay" title="Overlay codici tasto (azzurro)" data-i18n-title="ide_tooltip_overlay">KEY CODES</span>
    <span class="mode-badge" id="mode-trace" title="Traccia passo-passo su Serial (ON = stampa ogni istruzione)" data-i18n-title="ide_tooltip_trace">TRACE</span>
  </div>
</div>
<div class="card-title-bar" id="card-title-bar"></div>
<div class="card-slide" id="card-slide">
  <div class="card-placeholder" id="card-placeholder" data-i18n="ide_card_placeholder">Nessun overlay per questo programma</div>
</div>
<div id="svg-warning" style="display:none;max-width:420px;width:100%;font-size:9px;color:#e8b84a;background:#2a1f0a;border:1px solid #6a5020;border-radius:4px;padding:5px 8px;text-align:center"></div>
<div class="kbd" id="kbd">
  <div class="key-wrap" data-r="0" data-c="0"><span class="key-label">A'</span><span class="key-code-overlay"><span class="code-2nd">16</span><span class="code-normal">11</span></span><div class="key special"><span class="main">A</span></div></div>
  <div class="key-wrap" data-r="0" data-c="1"><span class="key-label">B'</span><span class="key-code-overlay"><span class="code-2nd">17</span><span class="code-normal">12</span></span><div class="key special"><span class="main">B</span></div></div>
  <div class="key-wrap" data-r="0" data-c="2"><span class="key-label">C'</span><span class="key-code-overlay"><span class="code-2nd">18</span><span class="code-normal">13</span></span><div class="key special"><span class="main">C</span></div></div>
  <div class="key-wrap" data-r="0" data-c="3"><span class="key-label">D'</span><span class="key-code-overlay"><span class="code-2nd">19</span><span class="code-normal">14</span></span><div class="key special"><span class="main">D</span></div></div>
  <div class="key-wrap" data-r="0" data-c="4"><span class="key-label">E'</span><span class="key-code-overlay"><span class="code-2nd">10</span><span class="code-normal">15</span></span><div class="key special"><span class="main">E</span></div></div>

  <div class="key-wrap" data-r="1" data-c="0"><span class="key-label"></span><span class="key-code-overlay"><span class="code-2nd"></span><span class="code-normal"></span></span><div class="key amber"><span class="main">2nd</span></div></div>
  <div class="key-wrap" data-r="1" data-c="1"><span class="key-label"></span><span class="key-code-overlay"><span class="code-2nd">27</span><span class="code-normal">22</span></span><div class="key"><span class="main">INV</span></div></div>
  <div class="key-wrap" data-r="1" data-c="2"><span class="key-label">log</span><span class="key-code-overlay"><span class="code-2nd">28</span><span class="code-normal">23</span></span><div class="key"><span class="main">Inx</span></div></div>
  <div class="key-wrap" data-r="1" data-c="3"><span class="key-label">CP</span><span class="key-code-overlay"><span class="code-2nd">29</span><span class="code-normal">24</span></span><div class="key"><span class="main">CE</span></div></div>
  <div class="key-wrap" data-r="1" data-c="4"><span class="key-label"></span><span class="key-code-overlay"><span class="code-2nd">20</span><span class="code-normal">25</span></span><div class="key amber"><span class="main">CLR</span></div></div>

  <div class="key-wrap" data-r="2" data-c="0"><span class="key-label">Pgm</span><span class="key-code-overlay"><span class="code-2nd">36</span><span class="code-normal"></span></span><div class="key special"><span class="main">LRN</span></div></div>
  <div class="key-wrap" data-r="2" data-c="1"><span class="key-label">P→R</span><span class="key-code-overlay"><span class="code-2nd">37</span><span class="code-normal">32</span></span><div class="key"><span class="main">x⇄t</span></div></div>
  <div class="key-wrap" data-r="2" data-c="2"><span class="key-label">sin</span><span class="key-code-overlay"><span class="code-2nd">38</span><span class="code-normal">33</span></span><div class="key"><span class="main">x²</span></div></div>
  <div class="key-wrap" data-r="2" data-c="3"><span class="key-label">cos</span><span class="key-code-overlay"><span class="code-2nd">39</span><span class="code-normal">34</span></span><div class="key"><span class="main">√x</span></div></div>
  <div class="key-wrap" data-r="2" data-c="4"><span class="key-label">tan</span><span class="key-code-overlay"><span class="code-2nd">30</span><span class="code-normal">35</span></span><div class="key"><span class="main">1/x</span></div></div>

  <div class="key-wrap" data-r="3" data-c="0"><span class="key-label">Ins</span><span class="key-code-overlay"><span class="code-2nd"></span><span class="code-normal"></span></span><div class="key special"><span class="main">SST</span></div></div>
  <div class="key-wrap" data-r="3" data-c="1"><span class="key-label">CMs</span><span class="key-code-overlay"><span class="code-2nd">47</span><span class="code-normal">42</span></span><div class="key special"><span class="main">STO</span></div></div>
  <div class="key-wrap" data-r="3" data-c="2"><span class="key-label">Exc</span><span class="key-code-overlay"><span class="code-2nd">48</span><span class="code-normal">43</span></span><div class="key special"><span class="main">RCL</span></div></div>
  <div class="key-wrap" data-r="3" data-c="3"><span class="key-label">Prd</span><span class="key-code-overlay"><span class="code-2nd">49</span><span class="code-normal">44</span></span><div class="key special"><span class="main">SUM</span></div></div>
  <div class="key-wrap" data-r="3" data-c="4"><span class="key-label">Ind</span><span class="key-code-overlay"><span class="code-2nd">40</span><span class="code-normal">45</span></span><div class="key"><span class="main">yˣ</span></div></div>

  <div class="key-wrap" data-r="4" data-c="0"><span class="key-label">Del</span><span class="key-code-overlay"><span class="code-2nd"></span><span class="code-normal"></span></span><div class="key special"><span class="main">BST</span></div></div> 
  <div class="key-wrap" data-r="4" data-c="1"><span class="key-label">Eng</span><span class="key-code-overlay"><span class="code-2nd">57</span><span class="code-normal">52</span></span><div class="key special"><span class="main">EE</span></div></div>
  <div class="key-wrap" data-r="4" data-c="2"><span class="key-label">Fix</span><span class="key-code-overlay"><span class="code-2nd">58</span><span class="code-normal">53</span></span><div class="key"><span class="main">(</span></div></div>
  <div class="key-wrap" data-r="4" data-c="3"><span class="key-label">Int</span><span class="key-code-overlay"><span class="code-2nd">59</span><span class="code-normal">54</span></span><div class="key"><span class="main">)</span></div></div>
  <div class="key-wrap" data-r="4" data-c="4"><span class="key-label">|x|</span><span class="key-code-overlay"><span class="code-2nd">50</span><span class="code-normal">55</span></span><div class="key amber"><span class="main">÷</span></div></div>

  <div class="key-wrap" data-r="5" data-c="0"><span class="key-label">Pause</span><span class="key-code-overlay"><span class="code-2nd">66</span><span class="code-normal">61</span></span><div class="key special"><span class="main">GTO</span></div></div>
  <div class="key-wrap" data-r="5" data-c="1"><span class="key-label">x=t</span><span class="key-code-overlay"><span class="code-2nd">67</span><span class="code-normal">07</span></span><div class="key num"><span class="main">7</span></div></div>
  <div class="key-wrap" data-r="5" data-c="2"><span class="key-label">Nop</span><span class="key-code-overlay"><span class="code-2nd">68</span><span class="code-normal">08</span></span><div class="key num"><span class="main">8</span></div></div>
  <div class="key-wrap" data-r="5" data-c="3"><span class="key-label">Op</span><span class="key-code-overlay"><span class="code-2nd">69</span><span class="code-normal">09</span></span><div class="key num"><span class="main">9</span></div></div>
  <div class="key-wrap" data-r="5" data-c="4"><span class="key-label">Deg</span><span class="key-code-overlay"><span class="code-2nd">60</span><span class="code-normal">65</span></span><div class="key amber"><span class="main">×</span></div></div>

  <div class="key-wrap" data-r="6" data-c="0"><span class="key-label">Lbl</span><span class="key-code-overlay"><span class="code-2nd">76</span><span class="code-normal">71</span></span><div class="key special"><span class="main">SBR</span></div></div>
  <div class="key-wrap" data-r="6" data-c="1"><span class="key-label">x≥t</span><span class="key-code-overlay"><span class="code-2nd">77</span><span class="code-normal">04</span></span><div class="key num"><span class="main">4</span></div></div>
  <div class="key-wrap" data-r="6" data-c="2"><span class="key-label">Σ+</span><span class="key-code-overlay"><span class="code-2nd">78</span><span class="code-normal">05</span></span><div class="key num"><span class="main">5</span></div></div>
  <div class="key-wrap" data-r="6" data-c="3"><span class="key-label">x̄</span><span class="key-code-overlay"><span class="code-2nd">79</span><span class="code-normal">06</span></span><div class="key num"><span class="main">6</span></div></div>
  <div class="key-wrap" data-r="6" data-c="4"><span class="key-label">Rad</span><span class="key-code-overlay"><span class="code-2nd">70</span><span class="code-normal">75</span></span><div class="key amber"><span class="main">-</span></div></div>

  <div class="key-wrap" data-r="7" data-c="0"><span class="key-label">St flg</span><span class="key-code-overlay"><span class="code-2nd">86</span><span class="code-normal">81</span></span><div class="key special"><span class="main">RST</span></div></div>
  <div class="key-wrap" data-r="7" data-c="1"><span class="key-label">If flg</span><span class="key-code-overlay"><span class="code-2nd">87</span><span class="code-normal">01</span></span><div class="key num"><span class="main">1</span></div></div>
  <div class="key-wrap" data-r="7" data-c="2"><span class="key-label">D.MS</span><span class="key-code-overlay"><span class="code-2nd">88</span><span class="code-normal">02</span></span><div class="key num"><span class="main">2</span></div></div>
  <div class="key-wrap" data-r="7" data-c="3"><span class="key-label">π</span><span class="key-code-overlay"><span class="code-2nd">89</span><span class="code-normal">03</span></span><div class="key num"><span class="main">3</span></div></div>
  <div class="key-wrap" data-r="7" data-c="4"><span class="key-label">Grad</span><span class="key-code-overlay"><span class="code-2nd">80</span><span class="code-normal">85</span></span><div class="key amber"><span class="main">+</span></div></div>

  <div class="key-wrap" data-r="8" data-c="0"><span class="key-label">Write</span><span class="key-code-overlay"><span class="code-2nd">96</span><span class="code-normal">91</span></span><div class="key special"><span class="main">R/S</span></div></div>
  <div class="key-wrap" data-r="8" data-c="1"><span class="key-label">Dsz</span><span class="key-code-overlay"><span class="code-2nd">97</span><span class="code-normal">00</span></span><div class="key num"><span class="main">0</span></div></div>
  <div class="key-wrap" data-r="8" data-c="2"><span class="key-label">Adv</span><span class="key-code-overlay"><span class="code-2nd">98</span><span class="code-normal">93</span></span><div class="key num"><span class="main">.</span></div></div>
  <div class="key-wrap" data-r="8" data-c="3"><span class="key-label">Prt</span><span class="key-code-overlay"><span class="code-2nd">99</span><span class="code-normal">94</span></span><div class="key num"><span class="main">+/-</span></div></div>
  <div class="key-wrap" data-r="8" data-c="4"><span class="key-label">List</span><span class="key-code-overlay"><span class="code-2nd">90</span><span class="code-normal">95</span></span><div class="key amber"><span class="main">=</span></div></div>
  
</div> 

<div class="bottom-controls">
    
    <div class="blue-text-block key-code-overlay">
        Key Code<br>Overlay
	</div>
     
    <div class="brand-amber">
        TI Programmable 59<br>
        <span class="sub-brand">Solid State Software</span>
    </div>

</div>



<div class="status" id="status" data-i18n="ide_status_connecting">Connessione...</div>
<script src="/i18n.js?v=3"></script>
<script src="/cardrender.js?v=3"></script>
<script>
const API = '';

const SEG_MAP = {
  '0':'abcdef','1':'bc','2':'abged','3':'abgcd','4':'fbgc',
  '5':'afgcd','6':'afedcg','7':'abc','8':'abcdefg','9':'abfgcd',
  '-':'g','E':'afged','r':'eg','o':'cdge','C':'afed',' ':'','.':''
};

// Stato per l'overlay dei codici tasto (azzurro) — State for the key-code overlay (blue)
let isCodeOverlayEnabled = false;

function renderDisplay(buf, opPending, isError) {
  const el = document.getElementById('display');
  if (!buf) { buf = '            0'; }

  // Applica o rimuovi la classe di lampeggio all'intero display a led — Applies or removes the blink class on the whole LED display
  if (isError) {
      el.classList.add('error-blink');
  } else {
      el.classList.remove('error-blink');
  }

  const digits = [];
  let pending_dp = false;

  for (let i = buf.length - 1; i >= 0 && digits.length < 12; i--) {
    const ch = buf[i];
    if (ch === '.') {
      pending_dp = true;
    } else {
      digits.unshift({ch: ch, dp: pending_dp});
      pending_dp = false;
    }
  }
  /* Se la stringa del display inizia con '.', inserisci uno 0 iniziale con DP — If display string starts with '.', insert leading 0 with DP */
  if (pending_dp) {
    digits.unshift({ch: '0', dp: true});
    pending_dp = false;
  }
  while (digits.length < 12) digits.unshift({ch: ' ', dp: false});

  el.innerHTML = '';
  for (let i = 0; i < 12; i++) {
    const d = digits[i];
    const group = document.createElement('div');
    group.className = 'digit-group';

    const digit = document.createElement('div');
    digit.className = 'led-digit';
    const segs = 'abcdefg';
    const onSegs = SEG_MAP[d.ch] || '';
    for (const s of segs) {
      const seg = document.createElement('div');
      seg.className = 'led-seg ' + s + (onSegs.includes(s) ? ' on' : '');
      digit.appendChild(seg);
    }
    group.appendChild(digit);

    const dp = document.createElement('div');
    dp.className = 'led-dp' + (d.dp ? ' on' : '');
    group.appendChild(dp);

    el.appendChild(group);
  }

  if (opPending) {
    let lastDigitIdx = 11;
    while (lastDigitIdx >= 0 && digits[lastDigitIdx].ch === ' ') {
      lastDigitIdx--;
    }
    if (lastDigitIdx >= 0) {
      const lastGroup = el.children[lastDigitIdx];
      const op = document.createElement('div');
      op.className = 'led-op on';
      lastGroup.appendChild(op);
    }
  }
}

// Cambia questa funzione — Change this function
async function refreshDisplay() {
  try {
    const r = await fetch(API+'/api/status');
    const d = await r.json();
    renderDisplay(d.display, d.flags && d.flags.op_pending, d.flags && d.flags.err);
    updateModes(d);
    updateKeyOverlay(d);
  } catch(e) {
    document.getElementById('status').textContent = t('status_error')+e.message;
  }
}

// escHtml() e renderCardSVG() sono nel file condiviso /cardrender.js — escHtml() and renderCardSVG() live in the shared file /cardrender.js
// (incluso in cima allo script di questa pagina) — usate anche da — (included at the top of this page's script) — also used by
// /overlays per l'anteprima; prima erano duplicate solo qui e — /overlays for the preview; they used to be duplicated only here and
// /overlays otteneva "renderCardSVG is not defined". — /overlays got "renderCardSVG is not defined".

let lastOverlayKey = null;
// Sequenza uscita → entrata: ogni cambio overlay fa prima scivolare — Exit → enter sequence: every overlay change first slides
// via a destra quello vecchio (cardSlideOut, 0.4s) e poi entra quello — the old one away to the right (cardSlideOut, 0.4s) and then the new
// nuovo da destra (cardSlideIn). seq è un token: se durante l'attesa — one enters from the right (cardSlideIn). seq is a token: if while waiting
// (animazione uscita o fetch) lo stato cambia di nuovo, la transizione — (exit animation or fetch) the state changes again, the old transition
// vecchia viene abbandonata e si parte con quella nuova. — is abandoned and the new one starts.
let overlaySeq = 0;
const OVERLAY_ANIM_MS = 400;

// Forza il riavvio dell'animazione anche se la classe c'era già — Forces the animation to restart even if the class was already there
// (altrimenti il secondo cambio consecutivo non la rifarebbe partire, — (otherwise the second consecutive change would not restart it,
// il browser la considera "già applicata"): la toglie, forza un — the browser considers it "already applied"): it removes it, forces a
// reflow, la rimette. — reflow, then puts it back.
// Le classi vanno sul CONTENUTO (figlio diretto del .card-slide), non — The classes go on the CONTENT (direct child of the .card-slide), not
// sul riquadro: così durante lo slide il box resta fisso a schermo e — on the box: so during the slide the box stays fixed on screen and
// scivola solo l'overlay dentro di esso (ritagliato da overflow:hidden). — only the overlay slides inside it (clipped by overflow:hidden).
function slideInCard(el) {
  const target = el.firstElementChild || el;
  target.classList.remove('card-leaving');
  target.classList.add('card-inserting');
  void target.offsetWidth;
}
function slideOutCard(el) {
  const target = el.firstElementChild || el;
  target.classList.remove('card-inserting');
  target.classList.add('card-leaving');
  void target.offsetWidth;
}

// Priorità overlay (dalla più alta alla più bassa): — Overlay priority (from highest to lowest):
// 1) Programma ROM selezionato (lib_page > 0): la slide della libreria — 1) Selected ROM program (lib_page > 0): the library slide
//    SOSTITUISCE l'overlay del programma in memoria — come la carta — REPLACES the in-memory program overlay — like the paper card
//    cartacea che sulla TI-59 reale si appoggia sopra la tastiera per — that on the real TI-59 rests on top of the keyboard for
//    i programmi libreria. Resta in vista anche dopo la cancellazione — library programs. It stays in view even after clearing
//    della memoria utente (prog_len = 0). — the user memory (prog_len = 0).
// 2) Scheda magnetica caricata in memoria: solo se c'è davvero un — 2) Magnetic card loaded in memory: only if there is really a
//    programma (prog_len > 0). Dopo la cancellazione della memoria LRN — program (prog_len > 0). After clearing the LRN memory
//    (prog_len = 0) l'overlay della scheda scompare. — (prog_len = 0) the card overlay disappears.
// 3) Placeholder (nessun overlay). — 3) Placeholder (no overlay).
function updateKeyOverlay(d) {
  const el = document.getElementById('card-slide');
  const titleEl = document.getElementById('card-title-bar');

  const cardSlot = (typeof d.active_card_slot === 'number') ? d.active_card_slot : -1;
  const libMod  = d.lib_module || '';
  const libPage = d.lib_page || 0;
  const progLen = d.prog_len || 0;

  // Cancellazione memoria LRN (prog_len = 0) => via l'overlay del — Clearing LRN memory (prog_len = 0) => removes the overlay of the
  // programma in memoria (scheda magnetica), ma la ROM caricata resta — in-memory program (magnetic card), but the loaded ROM stays
  // comunque in vista: l'overlay ROM è legato alla selezione del — in view anyway: the ROM overlay is tied to the library
  // programma libreria (lib_page), non al programma utente. Priorità — program selection (lib_page), not to the user program. Priority
  // altrimenti: ROM attiva > scheda in memoria. — otherwise: active ROM > card in memory.
  const hasMem = progLen > 0;
  const isRom  = !!(libMod && libPage > 0);
  const isCard = !isRom && hasMem && cardSlot >= 0;

  let mod, page;
  if (isRom)            { mod = libMod; page = libPage; }
  else if (isCard)      { mod = 'card'; page = cardSlot; }
  else                  { mod = '';     page = 0;        }
  // FIX: page è un numero e per lo slot 0 vale 0 (falsy in JS) — con — FIX: page is a number and for slot 0 it is 0 (falsy in JS) — with
  // "(mod && page)" la scheda magnetica nello slot 0 non produceva mai — "(mod && page)" the magnetic card in slot 0 never produced
  // una key e l'overlay non veniva disegnato (active_card_slot restava — a key and the overlay was never drawn (active_card_slot stayed
  // 0 ma il riquadro card era vuoto). Controllare solo mod. — 0 but the card box was empty). Check only mod.
  const key = mod ? (mod + ':' + page) : '';

  // Card magnetica XX: in assenza di programmi caricati (memoria LRN — Blank magnetic card XX: with no programs loaded (empty LRN
  // vuota e nessuna ROM selezionata) si mostra sempre la scheda vuota, — memory and no ROM selected) the empty card is always shown,
  // con la firma dell'autore al posto del nome del programma. — with the author signature instead of the program name.
  const BLANK_CARD_KEY = 'card:XX';
  if (!key) {
    if (lastOverlayKey === BLANK_CARD_KEY) return;   // già in vista — already in view
    lastOverlayKey = BLANK_CARD_KEY;
    titleEl.textContent = '';
    const seq = ++overlaySeq;
    slideOutCard(el);
    const exitStart = Date.now();
    loadCardPositions().catch(() => {}).then(() => {
      if (seq !== overlaySeq) return;   // nel frattempo è arrivato un altro overlay: lascia fare a lui — another overlay arrived in the meantime: let it handle it
      const wait = Math.max(0, OVERLAY_ANIM_MS - (Date.now() - exitStart));
      setTimeout(() => {
        if (seq !== overlaySeq) return;
        el.innerHTML = renderCardSVG('card', 'XX', [], 'Coded by MrYo');
        slideInCard(el);
      }, wait);
    });
    return;
  }
  if (key === lastOverlayKey) return;
  lastOverlayKey = key;
  const seq = ++overlaySeq;

  // Il nome della scheda magnetica attiva viene ora renderizzato — The active magnetic card name is now rendered
  // direttamente dentro l'overlay (card_card.svg, banda superiore, v. — directly inside the overlay (card_card.svg, top band, see
  // renderCardSVG) — la barra titolo sopra resta sempre vuota per — renderCardSVG) — the title bar above stays always empty to
  // evitare il doppione. — avoid the duplicate.
  titleEl.textContent = '';

  // 1) Uscita: il vecchio overlay scivola via a destra. — 1) Exit: the old overlay slides away to the right.
  slideOutCard(el);
  const exitStart = Date.now();

  // 2) Carica il nuovo contenuto in parallelo all'uscita, ma NON lo — 2) Loads the new content in parallel with the exit, but does NOT
  // disegna ancora: viene iniettato solo quando l'uscita è finita, — draw it yet: it is injected only when the exit is over,
  // altrimenti il nuovo overlay parteciperebbe alla slide-out. — otherwise the new overlay would take part in the slide-out.
  const prog = String(page).padStart(2, '0');
  let pendingHtml = '';
  fetch(API+'/api/overlays?mod='+encodeURIComponent(mod)+'&prog='+prog)
    .then(r => r.json())
    .then(async rows => {
      if (seq !== overlaySeq) return;   // è cambiato di nuovo: ignora — it changed again: ignore
      // Assicura che i template (es. top.svg/base.svg) siano caricati — Makes sure the templates (e.g. top.svg/base.svg) are loaded
      // dal device PRIMA di renderizzare: altrimenti CARD_TEMPLATES resta — from the device BEFORE rendering: otherwise CARD_TEMPLATES stays
      // vuoto e l'overlay card cadrebbe sul fallback card_free.svg. — empty and the card overlay would fall back to card_free.svg.
      await loadCardPositions();
      if (seq !== overlaySeq) return;   // è cambiato di nuovo: ignora — it changed again: ignore
      pendingHtml = renderCardSVG(mod, prog, rows, isCard ? (d.active_card_name || '') : '');
    })
    .catch(() => {
      if (seq !== overlaySeq) return;   // è cambiato di nuovo: ignora — it changed again: ignore
      pendingHtml = '<div class="card-placeholder"></div>';
    })
    .finally(() => {
      if (seq !== overlaySeq) return;   // è cambiato di nuovo: ignora — it changed again: ignore
      // 3) Entrata: aspetta che l'uscita sia finita (o che il fetch sia — 3) Entry: waits for the exit to be over (or for the fetch to
      // arrivato, se è stato più lento), poi entra da destra. — arrive, if it was slower), then enters from the right.
      const wait = Math.max(0, OVERLAY_ANIM_MS - (Date.now() - exitStart));
      setTimeout(() => {
        if (seq !== overlaySeq) return;
        el.innerHTML = pendingHtml;
        slideInCard(el);
      }, wait);
    });
}

function updateModes(d) {
  const set = (id, on) => {
    document.getElementById(id).className = 'mode-badge' + (on ? ' on' : '');
  };
  if (!d.flags) return;
  const degEl = document.getElementById('mode-deg');
  degEl.textContent = d.flags.angle || 'DEG';
  degEl.className = 'mode-badge on';
  set('mode-2nd', d.flags['2nd']);
  set('mode-inv', d.flags.inv);
  set('mode-lrn', d.flags.lrn);
  set('mode-run', d.flags.run);
  set('mode-fix', d.flags.fix);
  const oldnewEl = document.getElementById('mode-oldnew');
  const realistic = !!d.realistic_timing;
  oldnewEl.textContent = realistic ? 'NEW' : 'OLD';
  oldnewEl.className = 'mode-badge' + (realistic ? ' on' : '');
  set('mode-trace', d.trace_steps);
  document.getElementById('status').textContent =
    'IP:'+(d.ip||'?')+' C:'+(d.cycles||0)+' H:'+(d.heap||0)+'B';
}

document.getElementById('mode-oldnew').addEventListener('click', async () => {
  try {
    await fetch(API+'/api/timing', {method:'POST'});
    await refreshDisplay();
  } catch(e) {}
});

document.getElementById('mode-trace').addEventListener('click', async () => {
  try {
    await fetch(API+'/api/trace', {method:'POST'});
    await refreshDisplay();
  } catch(e) {}
});

// Gestione clic sull'overlay codici tasto (azzurro) — Click handling for the key-code overlay (blue)
document.getElementById('mode-overlay').addEventListener('click', toggleCodeOverlay);

function toggleCodeOverlay() {
  isCodeOverlayEnabled = !isCodeOverlayEnabled;
  updateCodeOverlayUI();
  saveOverlayState();
}

function updateCodeOverlayUI() {
  const badge = document.getElementById('mode-overlay');
  const overlays = document.querySelectorAll('.key-code-overlay');
  if (isCodeOverlayEnabled) {
    badge.classList.add('on');
    overlays.forEach(el => el.classList.add('on'));
  } else {
    badge.classList.remove('on');
    overlays.forEach(el => el.classList.remove('on'));
  }
}

function saveOverlayState() {
  localStorage.setItem('ti59_overlay', isCodeOverlayEnabled ? 'true' : 'false');
}

function loadOverlayState() {
  const saved = localStorage.getItem('ti59_overlay');
  isCodeOverlayEnabled = (saved === 'true');
}

document.getElementById('kbd').addEventListener('click', async e => {
  const wrap = e.target.closest('.key-wrap');
  if (!wrap) return;
  const r = wrap.dataset.r, c = wrap.dataset.c;
  const key = wrap.querySelector('.key');
  key.classList.add('pressed');
  setTimeout(() => key.classList.remove('pressed'), 120);
  try {
    await fetch(API+'/api/keypress?row='+r+'&col='+c, {method:'POST'});
    setTimeout(refreshDisplay, 80);
  } catch(e) {}
});

document.querySelector('.bottom-controls').addEventListener('click', () => {
  window.location.href = '/manage';
});

async function resetCPU() {
  if (!confirm(t('ide_confirm_reset'))) return;
  await fetch(API+'/api/reset', {method:'POST'});
  document.getElementById('status').textContent = t('ide_status_reset');
  await refreshDisplay();
}

setInterval(refreshDisplay, 500);
loadOverlayState();
updateCodeOverlayUI();
refreshDisplay();

checkSvgTemplates().then(missing => {
  if (!missing.length) return;
  const el = document.getElementById('svg-warning');
  el.style.display = 'block';
  el.textContent = t('svg_missing_pre') + missing.join(', ') + t('svg_missing_post');
});
</script>
<footer style="margin-top:14px;font-size:10px;color:#9a8f7a;text-align:center;padding-bottom:8px">TI-59 Zombie — © 2026 Maurizio Petruccioli (MrYo) — GNU GPL v3</footer>
</body>
</html>)rawhtml";

/* ═══════════════════════════════════════════════════════════════════
   PAGINA "MR. WOLF" (god mode) — tuning motore + modalità Old/New — "MR. WOLF" PAGE (god mode) — engine tuning + Old/New mode
   Raggiungibile SOLO se su SPIFFS esiste /god_mode.txt contenente — Reachable ONLY if SPIFFS contains /god_mode.txt holding
   "ora faccio quello che voglio" (vedi god_mode_enabled()). — "now I do what I want" (see god_mode_enabled()).
   ═══════════════════════════════════════════════════════════════════ */
static const char WEB_WOLF[] = R"rawhtml(<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Mr. Wolf</title>
<style>
  :root{--bg:#0d0d0b;--panel:#1e1e1c;--border:#3a3a36;--amber:#c49a3a;--amber-l:#e8c86a;--muted:#9a8f7a}
  *{box-sizing:border-box;margin:0;padding:0}
  body{font-family:Arial,sans-serif;background:var(--bg);color:#ddd;padding:24px;display:flex;flex-direction:column;align-items:center;gap:14px}
  h1{color:var(--amber);letter-spacing:3px;font-size:22px}
  .panel{background:var(--panel);border:1px solid var(--border);border-radius:6px;padding:16px;width:100%;max-width:420px}
  .row{display:flex;align-items:center;justify-content:space-between;gap:10px;margin:10px 0}
  .lbl{font-size:12px;color:var(--amber-l)}
  .val{color:#eee;font-size:12px;min-width:70px;text-align:right}
  input[type=range]{width:130px;accent-color:var(--amber)}
  button{background:#2a2a26;border:1px solid var(--border);color:#eee;padding:8px 14px;border-radius:4px;cursor:pointer;font-size:12px;min-width:110px}
  button.on{background:var(--amber);color:#1a1a12;font-weight:bold}
  .foot{font-size:9px;color:var(--muted)}
  .desc{font-size:9px;color:var(--muted);margin:-4px 0 8px 0;line-height:1.45}
</style>
</head>
<body>
<h1>MR. WOLF</h1>
<div class="panel">
  <div class="row"><span class="lbl">Modalit&agrave; Old/New</span><button id="mode-btn" onclick="toggleMode()">--</button></div>
  <div class="desc">Old = timing/errori autentici del TI-59 (lento e fedele all&#39;originale), New = calcolo istantaneo (moderno). Clic per alternare.</div>
  <div class="row"><span class="lbl">Velocit&agrave; Old</span><input id="pct" type="range" min="10" max="200" step="5" value="100" oninput="document.getElementById('pct-lbl').textContent=this.value+'%'" onchange="setMult(this.value)"><span class="val" id="pct-lbl">100%</span></div>
  <div class="desc">100% = stessa velocit&agrave; del TI-59 originale. Regola per allineare.</div>
  <div class="row"><span class="lbl">Espulsione scheda</span><input id="ej" type="range" min="50" max="3000" step="50" value="500" oninput="document.getElementById('ej-lbl').textContent=this.value+' ms'" onchange="setEj(this.value)"><span class="val" id="ej-lbl">500 ms</span></div>
  <div class="desc">Durata di accensione del motore che trascina fuori la scheda magnetica (50..3000 ms).</div>
</div>
<div class="foot">Mr. Wolf — god mode</div>
<script>
const API='';
async function load() {
  try {
    const d = await (await fetch(API+'/api/sysinfo')).json();
    const b = document.getElementById('mode-btn');
    if (d.realistic_timing) {
      b.textContent = 'OLD';
      b.className = 'on';
    } else {
      b.textContent = 'NEW';
      b.className = '';
    }
    const p = document.getElementById('pct');
    if (typeof d.timing_mult === 'number') { p.value = d.timing_mult; document.getElementById('pct-lbl').textContent = d.timing_mult+'%'; }
    const e = document.getElementById('ej');
    if (typeof d.eject_ms === 'number') { e.value = d.eject_ms; document.getElementById('ej-lbl').textContent = d.eject_ms+' ms'; }
  } catch(e) {}
}
function toggleMode() { fetch(API+'/api/timing', {method:'POST'}).then(load); }
function setMult(v)   { fetch(API+'/api/timing?mult='+v, {method:'POST'}); }
function setEj(v)     { fetch(API+'/api/eject?ms='+v); }
load();
</script>
</body>
</html>)rawhtml";

/* ═══════════════════════════════════════════════════════════════════
   PAGINA DI GESTIONE (programmi, moduli, card) — MANAGEMENT PAGE (programs, modules, cards)
   ═══════════════════════════════════════════════════════════════════ */
static const char WEB_MANAGE[] = R"rawhtml(<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TI-59 Zombie — Gestione</title>
<style>
  :root {
    --bg:#0d0d0b; --panel:#1e1e1c; --border:#3a3a36;
    --amber:#c49a3a; --amber-l:#e8c86a; --teal:#0a5c4a;
    --text:#d8d6ce; --muted:#6a6a62; --led:#ff5500;
  }
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--text);
       font-family:'Courier New',monospace;
       display:flex;flex-direction:column;align-items:center;
       padding:10px;gap:8px}
  .header{display:flex;align-items:baseline;justify-content:space-between;
          width:100%;max-width:420px}
  .header h1{color:var(--amber);font-size:14px;letter-spacing:2px;font-weight:400}
  .nav-link{color:var(--teal);font-size:10px;letter-spacing:1px;text-decoration:none;
            padding:4px 10px;border:1px solid var(--teal);border-radius:3px;
            transition:all .15s}
  .nav-link:hover{background:var(--teal);color:#fff}
  .panel{background:var(--panel);border:1px solid var(--border);border-radius:6px;
         padding:7px;width:100%;max-width:420px}
  .panel h2{font-size:10px;color:var(--muted);text-transform:uppercase;
             letter-spacing:2px;margin-bottom:4px}
  .card-list{display:flex;flex-direction:column;gap:3px;max-height:140px;
              overflow-y:auto;font-size:10px}
  .card-item{display:flex;gap:6px;align-items:center;padding:4px 6px;
              background:var(--bg);border-radius:3px}
  .card-item .slot{color:var(--muted);width:20px;font-size:9px}
  .card-item .name{flex:1;font-size:10px}
  .card-item button{font-size:9px;padding:2px 6px;border:none;border-radius:2px;
                    cursor:pointer;background:var(--teal);color:#fff}
  .card-item button.del{background:#5a1a1a}
  .input-row{display:flex;gap:4px;margin-top:6px}
  .input-row input{flex:1;background:var(--bg);border:1px solid var(--border);
                    color:var(--text);padding:4px 6px;border-radius:3px;font-size:10px}
  .input-row button{padding:4px 8px;background:var(--amber);color:#000;
                    border:none;border-radius:3px;cursor:pointer;font-weight:bold;font-size:10px}
  .prog-area{width:100%;height:80px;background:var(--bg);color:var(--text);
              border:1px solid var(--border);border-radius:3px;font-size:9px;
              padding:4px;font-family:monospace;resize:vertical}
  .btn-row{display:flex;gap:4px;margin-top:4px;flex-wrap:wrap}
  .btn{padding:4px 8px;border:none;border-radius:3px;cursor:pointer;font-size:10px;
       font-weight:bold}
  .btn-amber{background:var(--amber);color:#000}
  .btn-teal{background:var(--teal);color:#fff}
  .btn-red{background:#5a1a1a;color:#fff}
  .btn-gray{background:var(--border);color:var(--text)}
  .status{font-size:9px;color:var(--muted);text-align:center;padding:4px}
  .info-grid{display:grid;grid-template-columns:auto 1fr;gap:3px 12px;font-size:10px;margin-top:2px}
  .info-grid .lbl{color:var(--muted);letter-spacing:0.5px}
  .info-grid .val{color:var(--text);text-align:right;font-family:monospace}
  ::-webkit-scrollbar{width:4px}
  ::-webkit-scrollbar-track{background:var(--bg)}
  ::-webkit-scrollbar-thumb{background:var(--border);border-radius:2px}
</style>
</head>
<body>
<div class="header">
  <h1>TI-59 ZOMBIE</h1>
  <div style="display:flex;gap:6px;align-items:center">
    <!--GOD-->
    <select data-lang-select onchange="setLang(this.value)" style="background:var(--panel);color:var(--text);border:1px solid var(--border);border-radius:3px;font-size:9px;padding:2px 4px">
      <option value="it">IT</option>
      <option value="en">EN</option>
    </select>
    <a href="/overlays" class="nav-link" data-i18n="nav_manage">Overlay &rarr;</a>
    <a href="/" class="nav-link" data-i18n="nav_back_calc">&larr; Calcolatrice</a>
  </div>
</div>
<div class="panel">
  <h2 data-i18n="mgr_h2_cards">Schede programma</h2>
  <div class="card-list" id="card-list"><em style="color:var(--muted);font-size:10px" data-i18n="status_loading">Caricamento...</em></div>
  <div class="input-row" style="margin-top:8px">
    <input id="card-slot" type="number" min="0" max="49" value="0" style="width:50px;flex:none">
    <input id="card-name" type="text" placeholder="Nome scheda" data-i18n-ph="mgr_card_name_ph" maxlength="23">
    <button class="btn btn-amber" onclick="writeCard()" data-i18n="mgr_btn_write">WRITE</button>
    <button class="btn btn-teal" onclick="readCard()" data-i18n="mgr_btn_read">READ</button>
  </div>
  <div class="input-row" style="margin-top:4px">
    <input id="card-file" type="file" accept=".txt,.json" style="flex:1;font-size:10px">
    <button class="btn btn-teal" onclick="uploadCardFile()" data-i18n="mgr_btn_upload_file">&uarr; Carica da file</button>
  </div>
</div>
<div class="panel">
  <h2 data-i18n="mgr_h2_current_prog">Programma corrente</h2>
  <textarea class="prog-area" id="prog-hex" placeholder="Hex passi programma..." data-i18n-ph="mgr_prog_ph"></textarea>
  <div class="btn-row">
    <button class="btn btn-amber" onclick="uploadProg()" data-i18n="mgr_btn_upload">&uarr; Carica</button>
    <button class="btn btn-teal"  onclick="downloadProg()" data-i18n="mgr_btn_download">&darr; Scarica</button>
    <button class="btn btn-red"   onclick="resetCPU()" data-i18n="mgr_btn_reset">RESET</button>
  </div>
</div>
<div class="panel">
  <h2 data-i18n="mgr_h2_library">Modulo libreria (2nd Pgm)</h2>
  <div class="input-row">
    <select id="module-select" style="flex:1"></select>
    <button class="btn btn-amber" onclick="selectModule()" data-i18n="mgr_btn_engage">Innesta</button>
  </div>
  <div class="btn-row" style="margin-top:4px">
    <button class="btn btn-teal" onclick="viewModuleListing()" data-i18n="mgr_btn_view_listing">Vedi listato completo</button>
  </div>
  <div id="module-programs" class="card-list" style="margin-top:6px;font-size:9px"></div>
</div>
<div class="panel">
  <h2 data-i18n="mgr_h2_printer_file">Stampante / File</h2>
  <div class="btn-row">
    <button class="btn btn-teal" onclick="downloadFile('/api/print','print.txt')" data-i18n="mgr_btn_print">Print</button>
    <button class="btn btn-teal" onclick="downloadFile('/api/listing','listing.txt')" data-i18n="mgr_btn_listing">Listing</button>
    <button class="btn btn-gray" onclick="loadProgs()" data-i18n="mgr_btn_progs">Progs</button>
  </div>
  <div id="prog-files" class="card-list" style="margin-top:6px"></div>
</div>
<div class="panel">
  <h2 data-i18n="mgr_h2_fs">File system (SPIFFS)</h2>
  <div class="card-list" id="fs-list"><em style="color:var(--muted);font-size:10px" data-i18n="status_loading">Caricamento...</em></div>
  <div class="input-row" style="margin-top:4px">
    <input id="fs-file" type="file" style="flex:1;font-size:10px">
    <button class="btn btn-teal" onclick="uploadFS()" data-i18n="mgr_btn_upload">&uarr; Carica</button>
    <button class="btn btn-gray" onclick="listFS()" data-i18n="mgr_btn_update">Aggiorna</button>
  </div>
</div>
<div class="panel">
  <h2 data-i18n="mgr_h2_system">Sistema</h2>
  <div class="info-grid" id="sysinfo">
    <span class="lbl" data-i18n="mgr_lbl_ram">RAM</span><span class="val" id="si-heap">--</span>
    <span class="lbl" data-i18n="mgr_lbl_heap_free">Heap libero</span><span class="val" id="si-heap-free">--</span>
    <span class="lbl" data-i18n="mgr_lbl_spiffs">SPIFFS</span><span class="val" id="si-spiffs">--</span>
    <span class="lbl" data-i18n="mgr_lbl_cards_saved">Schede salvate</span><span class="val" id="si-slots">--</span>
    <span class="lbl" data-i18n="mgr_lbl_prg_files">File .prg</span><span class="val" id="si-prgs">--</span>
    <span class="lbl" data-i18n="mgr_lbl_cpu_cycles">Cicli CPU</span><span class="val" id="si-cycles">--</span>
    <span class="lbl">Lunghezza pgm</span><span class="val" id="si-proglen">--</span>
    <span class="lbl">Uptime</span><span class="val" id="si-uptime">--</span>
    <span class="lbl">WiFi RSSI</span><span class="val" id="si-rssi">--</span>
    <span class="lbl">WiFi IP</span><span class="val" id="si-ip">--</span>
    <span class="lbl" data-i18n="mgr_lbl_maxspeed">Velocità max raggiungibile</span><span class="val" id="si-maxspeed">--</span>
  </div>
</div>
<div class="status" id="status" data-i18n="status_loading">Caricamento...</div>
<script src="/i18n.js?v=3"></script>
<script>
const API = '';
(function(){ const sel=document.querySelector('[data-lang-select]'); if(sel) sel.value=getLang(); })();

async function loadCardList() {
  try {
    const r = await fetch(API+'/api/cards');
    const cards = await r.json();
    const el = document.getElementById('card-list');
    if (!cards.length) {
      el.innerHTML = '<em style="color:var(--muted);font-size:10px">'+t('mgr_no_cards')+'</em>';
      return;
    }
    el.innerHTML = '';
    cards.forEach(c => {
      const div = document.createElement('div');
      div.className = 'card-item';
      div.innerHTML = '<span class="slot">'+String(c.slot).padStart(2,'0')+'</span>'+
        '<span class="name">'+c.name+'</span>'+
        '<span style="color:var(--muted);font-size:9px">'+c.steps+'p</span>'+
        '<button onclick="loadSlot('+c.slot+')">R</button>'+
        '<button onclick="downloadFile(\'/api/card/file?slot='+c.slot+'\',\''+(c.name||'card'+c.slot)+'.txt\')">&darr;</button>'+
        '<button class="del" onclick="deleteSlot('+c.slot+')">X</button>';
      el.appendChild(div);
    });
  } catch(e) {}
}

async function readCard() {
  const slot = document.getElementById('card-slot').value;
  try {
    const r = await fetch(API+'/api/card?slot='+slot);
    if (!r.ok) { alert(t('mgr_alert_empty_slot')); return; }
    const d = await r.json();
    document.getElementById('prog-hex').value = d.prog_listing || d.prog_hex || '';
    document.getElementById('status').textContent = t('mgr_status_read')+slot+':'+d.name;
  } catch(e) { alert(e.message); }
}

async function writeCard() {
  const slot = document.getElementById('card-slot').value;
  const name = document.getElementById('card-name').value || 'C'+slot;
  try {
    await fetch(API+'/api/card?slot='+slot+'&name='+encodeURIComponent(name), {method:'POST'});
    document.getElementById('status').textContent = t('mgr_status_written')+slot+':'+name;
    await loadCardList();
  } catch(e) { alert(e.message); }
}

async function uploadCardFile() {
  const slot = document.getElementById('card-slot').value;
  const fileInput = document.getElementById('card-file');
  const file = fileInput.files[0];
  if (!file) { alert(t('mgr_alert_choose_text_file')); return; }
  try {
    const text = await file.text();
    const r = await fetch(API+'/api/card/file?slot='+slot, {method:'POST', body:text});
    if (!r.ok) { alert(t('mgr_alert_import_failed')); return; }
    document.getElementById('status').textContent = t('mgr_status_imported')+slot+t('mgr_status_imported_from')+file.name;
    fileInput.value = '';
    await loadCardList();
  } catch(e) { alert(e.message); }
}

async function loadSlot(slot) {
  document.getElementById('card-slot').value = slot;
  try {
    const r = await fetch(API+'/api/card/load?slot='+slot, {method:'POST'});
    if (!r.ok) { alert(t('mgr_alert_empty_slot')); return; }
    await readCard();
  } catch(e) { alert(e.message); }
}

async function deleteSlot(slot) {
  if (!confirm(t('mgr_confirm_delete_slot')+slot+'?')) return;
  try {
    const r = await fetch(API+'/api/card/delete?slot='+slot);
    if (!r.ok) { alert(t('mgr_alert_delete_error')+(await r.text())); return; }
  } catch(e) { alert(e.message); return; }
  await loadCardList();
}

async function uploadProg() {
  const hex = document.getElementById('prog-hex').value.trim();
  if (!hex) return;
  try {
    await fetch(API+'/api/prog', {
      method:'POST', headers:{'Content-Type':'text/plain'}, body:hex
    });
    document.getElementById('status').textContent = t('mgr_status_prog_loaded');
  } catch(e) { alert(e.message); }
}

async function downloadProg() {
  try {
    const r = await fetch(API+'/api/prog');
    const text = await r.text();
    document.getElementById('prog-hex').value = text;
  } catch(e) { alert(e.message); }
}

async function resetCPU() {
  if (!confirm(t('confirm_delete'))) return;
  await fetch(API+'/api/reset', {method:'POST'});
  document.getElementById('status').textContent = t('mgr_btn_reset');
}

async function downloadFile(url, name) {
  try {
    const r = await fetch(url);
    if (!r.ok) { alert(t('mgr_alert_file_not_found')); return; }
    const blob = await r.blob();
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = name;
    a.click();
  } catch(e) { alert(e.message); }
}

async function loadProgs() {
  try {
    const r = await fetch('/api/progs');
    const files = await r.json();
    const el = document.getElementById('prog-files');
    if (!files.length) { el.innerHTML = '<em style="color:var(--muted)">'+t('mgr_no_prg')+'</em>'; return; }
    el.innerHTML = '';
    files.forEach(f => {
      const div = document.createElement('div'); div.className = 'card-item';
      div.innerHTML = '<span class="name">'+f.name+'</span><span style="color:var(--muted)">'+f.size+'B</span>'+
        '<button onclick="downloadFile(\'/api/progfile?file='+f.name+'\',\''+f.name+'\')">&darr;</button>';
      el.appendChild(div);
    });
  } catch(e) {}
}

let modulesCache = [];

async function loadModules() {
  try {
    const r = await fetch(API+'/api/modules');
    const d = await r.json();
    modulesCache = d.modules || [];
    const sel = document.getElementById('module-select');
    sel.innerHTML = '<option value="">-- nessuno --</option>';
    modulesCache.forEach(m => {
      const opt = document.createElement('option');
      opt.value = m.id; opt.textContent = m.name;
      if (m.id === d.active) opt.selected = true;
      sel.appendChild(opt);
    });
    renderModulePrograms(d.active);
  } catch(e) {}
}

function renderModulePrograms(activeId) {
  const el = document.getElementById('module-programs');
  const m = modulesCache.find(x => x.id === activeId);
  if (!m) { el.innerHTML = ''; return; }
  el.innerHTML = m.programs.map(p =>
    '<div class="card-item"><span class="slot">'+String(p.num).padStart(2,'0')+
    '</span><span class="name">'+p.title+'</span></div>'
  ).join('');
}

async function selectModule() {
  const id = document.getElementById('module-select').value;
  try {
    await fetch(API+'/api/modules?id='+encodeURIComponent(id), {method:'POST'});
    renderModulePrograms(id);
    document.getElementById('status').textContent =
      id ? t('mgr_status_module_engaged')+id : t('mgr_status_module_none');
  } catch(e) { alert(e.message); }
}

async function viewModuleListing() {
  const id = document.getElementById('module-select').value;
  if (!id) { alert(t('mgr_alert_choose_module')); return; }
  try {
    const r = await fetch(API+'/api/modules/listing?id='+encodeURIComponent(id));
    if (!r.ok) { alert(t('mgr_alert_listing_unavailable')); return; }
    const text = await r.text();
    document.getElementById('prog-hex').value = text;
    document.getElementById('status').textContent = t('mgr_status_module_listing')+id;
  } catch(e) { alert(e.message); }
}

async function listFS() {
  try {
    const r = await fetch(API+'/api/fs');
    const data = await r.json();
    const el = document.getElementById('fs-list');
    if (!data.files || !data.files.length) {
      el.innerHTML = '<em style="color:var(--muted);font-size:10px">'+t('mgr_no_files')+'</em>';
      return;
    }
    el.innerHTML = '';
    data.files.forEach(f => {
      const div = document.createElement('div');
      div.className = 'card-item';
      div.innerHTML = '<span class="name">'+escHtml(f.name)+'</span>'+
        '<span style="color:var(--muted);font-size:9px">'+f.size+'B</span>'+
        '<button class="del" onclick="deleteFS(\''+escHtml(f.name).replace(/'/g,"\\'")+'\')">X</button>';
      el.appendChild(div);
    });
  } catch(e) { document.getElementById('status').textContent = t('mgr_status_fs_error')+e.message; }
}
function escHtml(s) {
  const d = document.createElement('div');
  d.textContent = s;
  return d.innerHTML;
}
async function uploadFS() {
  const input = document.getElementById('fs-file');
  const file = input.files[0];
  if (!file) { alert(t('mgr_alert_choose_file')); return; }
  // Multipart: il server lo riceve in STREAMING a pezzi (niente — Multipart: the server receives it in STREAMING chunks (no
  // body da 20 KB bufferizzato in RAM, era la causa del fallimento — 20 KB body buffered in RAM, that was the cause of the failure
  // con gli SVG grossi). — with large SVGs).
  const fd = new FormData();
  fd.append('file', file, file.name);
  const r = await fetch(API+'/api/fs/upload', {method:'POST', body:fd});
  if (!r.ok) { alert(t('mgr_alert_upload_failed')); return; }
  document.getElementById('status').textContent = t('mgr_status_uploaded')+file.name;
  input.value = '';
  listFS();
}
async function deleteFS(path) {
  if (!confirm(t('mgr_confirm_delete_generic')+path+'?')) return;
  try {
    const r = await fetch(API+'/api/fs/delete?path='+encodeURIComponent(path), {method:'POST'});
    if (!r.ok) {
      const msg = await r.text();
      alert(t('mgr_alert_delete_failed_http')+r.status+'): '+msg);
      return;
    }
    document.getElementById('status').textContent = t('mgr_status_fs_deleted')+path;
  } catch(e) {
    alert(t('status_network_error')+e.message);
    return;
  }
  listFS();
}
async function loadSysInfo() {
  try {
    const r = await fetch(API+'/api/sysinfo');
    const d = await r.json();
    const fmt = v => typeof v === 'number' ? v.toLocaleString() : v;
    const heapUsed = d.heap_total - d.heap_free;
    const heapPct = d.heap_total ? (heapUsed / d.heap_total * 100).toFixed(1) : 0;
    document.getElementById('si-heap').textContent = fmt(heapUsed) + ' / ' + fmt(d.heap_total) + ' B (' + heapPct + '%)';
    document.getElementById('si-heap-free').textContent = fmt(d.heap_free) + ' B (min ' + fmt(d.heap_min) + ' B)';
    const spiffsPct = d.spiffs_total ? (d.spiffs_used / d.spiffs_total * 100).toFixed(1) : 0;
    document.getElementById('si-spiffs').textContent = fmt(d.spiffs_used) + ' / ' + fmt(d.spiffs_total) + ' B (' + spiffsPct + '%)';
    document.getElementById('si-slots').textContent = d.slots_filled + ' / ' + d.slots_total;
    document.getElementById('si-prgs').textContent = d.prg_files;
    document.getElementById('si-cycles').textContent = fmt(d.cycles);
    document.getElementById('si-proglen').textContent = d.prog_len + t('mgr_status_pgm_steps');
    const upt = d.uptime_ms;
    const sec = Math.floor(upt / 1000) % 60;
    const min = Math.floor(upt / 60000) % 60;
    const hrs = Math.floor(upt / 3600000);
    document.getElementById('si-uptime').textContent = hrs + 'h ' + min + 'm ' + sec + 's';
    document.getElementById('si-rssi').textContent = d.wifi_rssi + ' dBm';
    document.getElementById('si-ip').textContent = d.wifi_ip;
    const ms = document.getElementById('si-maxspeed');
    if (ms) ms.textContent = (d.max_speed_pct===0?'—':d.max_speed_pct+' %')+' · 100% = TI-59';
  } catch(e) {}
}

loadCardList();
loadModules();
loadSysInfo();
listFS();
</script>
</body>
</html>)rawhtml";

/* ═══════════════════════════════════════════════════════════════════
   PAGINA OVERLAY TASTIERA (editor testuale + anteprima per mod/prog) — KEYBOARD OVERLAY PAGE (text editor + preview for mod/prog)
   Terza pagina, raggiungibile da /manage. Editor "tutto o niente": — Third page, reachable from /manage. "All or nothing" editor:
   un unico textarea con l'intero file /overlays.txt, salvato per — a single textarea with the whole /overlays.txt file, saved
   intero ad ogni SAVE (stesso approccio già discusso per cardemu: — in full on every SAVE (same approach already discussed for cardemu:
   un file solo, riscritto atomicamente). L'anteprima riusa /api/modules — a single file, rewritten atomically). The preview reuses /api/modules
   per popolare le stesse select di modulo/programma già presenti in — to populate the same module/program selects already present in
   /manage, poi chiama /api/overlays?mod=..&prog=.. per mostrare le — /manage, then calls /api/overlays?mod=..&prog=.. to show the
   righe già filtrate. — already filtered rows.
   ═══════════════════════════════════════════════════════════════════ */
static const char WEB_OVERLAYS[] = R"rawhtml(<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TI-59 Zombie — Overlay tastiera</title>
<style>
  :root {
    --bg:#0d0d0b; --panel:#1e1e1c; --border:#3a3a36;
    --amber:#c49a3a; --amber-l:#e8c86a; --teal:#0a5c4a;
    --text:#d8d6ce; --muted:#6a6a62; --led:#ff5500;
  }
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--text);
       font-family:'Courier New',monospace;
       display:flex;flex-direction:column;align-items:center;
       padding:10px;gap:8px}
  .header{display:flex;align-items:baseline;justify-content:space-between;
          width:100%;max-width:420px}
  .header h1{color:var(--amber);font-size:14px;letter-spacing:2px;font-weight:400}
  .nav-link{color:var(--teal);font-size:10px;letter-spacing:1px;text-decoration:none;
            padding:4px 10px;border:1px solid var(--teal);border-radius:3px}
  .nav-link:hover{background:var(--teal);color:#fff}
  .panel{background:var(--panel);border:1px solid var(--border);border-radius:6px;
         padding:7px;width:100%;max-width:420px}
  .panel h2{font-size:10px;color:var(--muted);text-transform:uppercase;
             letter-spacing:2px;margin-bottom:4px}
  .hint{font-size:9px;color:var(--muted);margin-bottom:6px;line-height:1.4}
  textarea{width:100%;height:220px;background:var(--bg);color:var(--text);
              border:1px solid var(--border);border-radius:3px;font-size:10px;
              padding:6px;font-family:monospace;resize:vertical}
  .input-row{display:flex;gap:4px;margin-top:6px}
  select{flex:1;min-width:0;max-width:100%;background:var(--bg);color:var(--text);
         border:1px solid var(--border);padding:4px 6px;border-radius:3px;font-size:10px}
  .panel-title{display:flex;align-items:center;justify-content:space-between;gap:6px}
  .panel-title h2{margin-bottom:0}
  .btn{padding:4px 8px;border:none;border-radius:3px;cursor:pointer;font-size:10px;
       font-weight:bold}
  .btn-amber{background:var(--amber);color:#000}
  .btn-teal{background:var(--teal);color:#fff}
  .rows{display:flex;flex-direction:column;gap:3px;margin-top:6px;font-size:10px}
  .row-item{display:flex;gap:6px;padding:4px 6px;background:var(--bg);border-radius:3px}
  .row-item .type{color:var(--muted);width:38px;font-size:9px}
  .row-item .key{color:var(--amber-l);width:24px;font-size:9px}
  .row-item .text{flex:1}
  .status{font-size:9px;color:var(--muted);text-align:center;padding:4px}
  .attr-tbl{width:100%;border-collapse:collapse;font-size:10px;margin-top:4px}
  .attr-tbl th{text-align:left;color:var(--muted);font-size:9px;text-transform:uppercase;
               letter-spacing:1px;padding:3px 6px;border-bottom:1px solid var(--border)}
  .attr-tbl td{padding:3px 6px;border-bottom:1px solid var(--border)}
  .attr-tbl td.code{color:var(--amber-l);font-weight:bold;width:36px}
  .attr-tbl td.count{color:var(--muted);width:36px;text-align:right}
</style>
</head>
<body>
<div class="header">
  <h1 data-i18n="ovl_h1">OVERLAY TASTIERA</h1>
  <a href="/manage" class="nav-link" data-i18n="nav_manage_back">&larr; Gestione</a>
</div>
<div class="panel">
  <h2 data-i18n="ovl_h2_file">File overlay (unico, tutti i moduli)</h2>
  <div class="hint" data-i18n="ovl_hint_format">
    Una riga per etichetta: <b>MOD|PROG|TYPE|KEY|ATTR|TESTO</b><br>
    ATTR = s(inistra) / e(destra) / c(entro) / m(mezzo) / fN(font N)<br>
    GRID: KEY = A-E/A'-E'. Stesso KEY = split (s + e sulla stessa riga)<br>
    FREE: 4+ spazi nel TESTO = blocchi separati sulla stessa riga (2 blocchi: sinistra/destra, 3: sinistra/centro/destra)<br>
    Es: <code>ml1|01|FREE|2|c|L.R. INIT: SBR CLR    PRINT: mm STO 00</code>
  </div>
  <textarea id="raw" placeholder="Nessun overlay salvato ancora. Formato: mod|prog|TYPE|KEY|ATTR|testo" data-i18n-ph="ovl_textarea_ph" oninput="updateAttrTable()"></textarea>
  <div class="input-row">
    <input type="file" id="ovl-file" accept=".txt,.json" style="flex:1;font-size:10px">
    <button class="btn btn-teal" onclick="uploadRawFile()" data-i18n="ovl_btn_upload_file">Apri file</button>
    <button class="btn btn-gray" onclick="downloadRawFile()" data-i18n="ovl_btn_download_file">Salva file</button>
  </div>
  <div class="input-row">
    <button class="btn btn-amber" onclick="saveRaw()" data-i18n="ovl_btn_save">SALVA (sostituisce tutto)</button>
    <button class="btn btn-teal" onclick="loadRaw()" data-i18n="ovl_btn_reload">Ricarica dal device</button>
  </div>
</div>
<div class="panel">
  <div class="panel-title">
    <h2 data-i18n="ovl_h2_preview">Anteprima per programma</h2>
    <button class="btn btn-teal" onclick="previewOverlay()" data-i18n="ovl_btn_show">Mostra</button>
  </div>
  <div class="input-row">
    <select id="mod-select"></select>
    <select id="prog-select"></select>
  </div>
  <div class="rows" id="rows"></div>
</div>
<div class="panel">
  <div class="panel-title">
    <h2 data-i18n="ovl_card_preview_h2">Anteprima scheda magnetica</h2>
    <button class="btn btn-teal" onclick="previewCard()" data-i18n="ovl_card_preview_btn">Mostra</button>
  </div>
  <div class="input-row">
    <select id="card-slot-select"></select>
  </div>
  <div style="font-size:8px;color:var(--muted);margin-top:4px;line-height:1.4" id="card-tpl-status"></div>
  <div class="rows" id="card-preview" style="margin-top:6px"></div>
</div>
<div class="panel">
  <h2 data-i18n="ovl_h2_attrs">Attributi in uso</h2>
  <div class="hint" data-i18n="ovl_hint_attrs">Elenco di tutti gli attributi previsti (non solo quelli già usati) — si aggiorna da sola se aggiungi un nuovo codice qui sotto.</div>
  <div id="attr-table"></div>
</div>
<div class="panel" id="pos-panel">
  <h2 data-i18n="pos_h2">Posizioni testo (per adattare l'SVG)</h2>
  <div class="hint" data-i18n="pos_hint">Coordinate nelle stesse unità del viewBox dell'SVG (544 x 120). Apri il file .svg, guarda dove sono le linee divisorie, scrivi qui quei numeri. Salva, poi usa "Mostra" sopra per vedere l'effetto.</div>
  <div style="font-size:9px;color:var(--muted);margin-bottom:2px" data-i18n="pos_grid_label">Griglia A-E — colonne (X)</div>
  <div class="input-row">
    <input type="number" step="0.1" id="pos-colx-0" style="width:58px">
    <input type="number" step="0.1" id="pos-colx-1" style="width:58px">
    <input type="number" step="0.1" id="pos-colx-2" style="width:58px">
    <input type="number" step="0.1" id="pos-colx-3" style="width:58px">
    <input type="number" step="0.1" id="pos-colx-4" style="width:58px">
  </div>
  <div style="font-size:9px;color:var(--muted);margin:6px 0 2px" data-i18n="pos_grid_row_label">Griglia — righe (Y: normale, 2nd)</div>
  <div class="input-row">
    <input type="number" step="0.1" id="pos-rowy-0" style="width:58px">
    <input type="number" step="0.1" id="pos-rowy-1" style="width:58px">
  </div>
  <div style="font-size:9px;color:var(--muted);margin:6px 0 2px" data-i18n="pos_free_label">Righe libere — Y per numero riga (KEY)</div>
  <div id="pos-free-rows"></div>
  <div class="input-row">
    <button class="btn btn-teal" onclick="addFreeRowPos()" data-i18n="pos_btn_add_row">+ Riga</button>
  </div>
  <div style="font-size:9px;color:var(--muted);margin:6px 0 2px" data-i18n="pos_margin_label">Margini (allineamento s/e e blocchi multipli)</div>
  <div class="input-row">
    <input type="number" step="0.1" id="pos-margin-left" style="width:58px" placeholder="sinistro">
    <input type="number" step="0.1" id="pos-margin-right" style="width:58px" placeholder="destro">
  </div>
  <div style="font-size:9px;color:var(--muted);margin:6px 0 2px" data-i18n="pos_card_name_label">Nome scheda magnetica — X / Y (banda superiore)</div>
  <div class="input-row">
    <input type="number" step="0.1" id="pos-card-name-x" style="width:58px" placeholder="X">
    <input type="number" step="0.1" id="pos-card-name-y" style="width:58px" placeholder="Y">
  </div>
  <div style="font-size:9px;color:var(--muted);margin:6px 0 2px" data-i18n="pos_templates_label">Template SVG — strato superiore / inferiore</div>
  <div class="input-row">
    <input type="text" id="pos-tpl-top" placeholder="top.svg" data-i18n-ph="pos_tpl_top_ph">
    <input type="text" id="pos-tpl-bottom" placeholder="base.svg" data-i18n-ph="pos_tpl_bottom_ph">
  </div>
  <div style="font-size:8px;color:var(--muted);margin-top:2px" data-i18n="pos_templates_hint">Vuoti = template automatico. Primo campo = strato sopra, secondo = sotto. I file vanno caricati da /manage. I due SVG si sovrappongono a piena dimensione (devono avere lo stesso viewBox).</div>
  <div class="input-row" style="margin-top:6px">
    <input type="file" id="pos-file" accept=".json" style="flex:1;font-size:10px">
    <button class="btn btn-teal" onclick="uploadPosFile()" data-i18n="pos_btn_open_file">Apri file</button>
    <button class="btn btn-gray" onclick="downloadPosFile()" data-i18n="pos_btn_save_file">Salva file</button>
  </div>
  <div class="input-row" style="margin-top:8px">
    <button class="btn btn-amber" onclick="savePositions()" data-i18n="pos_btn_save">Salva posizioni</button>
    <button class="btn btn-teal" onclick="resetPositionsToDefault()" data-i18n="pos_btn_reset_defaults">Ripristina default</button>
  </div>
</div>
<div id="svg-warning" style="display:none;font-size:9px;color:#e8b84a;background:#2a1f0a;border:1px solid #6a5020;border-radius:4px;padding:5px 8px;text-align:center;max-width:420px;width:100%"></div>
<div class="status" id="status" data-i18n="status_connecting">Connessione al device...</div>
<script src="/i18n.js?v=3"></script>
<script src="/cardrender.js?v=3"></script>
<script>
const API = '';
let modulesCache = [];
// escHtml() è nel file condiviso /cardrender.js incluso sopra. — escHtml() is in the shared file /cardrender.js included above.

async function loadRaw() {
  const statusEl = document.getElementById('status');
  statusEl.textContent = t('status_loading');
  try {
    const r = await fetch(API+'/api/overlays/raw');
    if (!r.ok) {
      statusEl.textContent = t('ovl_error_loading') + r.status;
      return;
    }
    const text = await r.text();
    document.getElementById('raw').value = text;
    statusEl.textContent = text.length
      ? t('ovl_loaded_bytes_pre') + text.length + t('ovl_loaded_bytes_post')
      : t('ovl_empty_file');
  } catch(e) {
    statusEl.textContent = t('status_network_error') + e.message;
  }
  updateAttrTable();
}

/* ═══════════════════════════════════════════════════════════════════
   TABELLA ATTRIBUTI — mostra SEMPRE tutti gli attributi PREVISTI — ATTRIBUTES TABLE — always shows all the EXPECTED attributes
   (ATTR_CODES), non solo quelli effettivamente usati nel testo. La — (ATTR_CODES), not only the ones actually used in the text. The
   colonna "Righe" si aggiorna da sola a ogni modifica del textarea e — "Rows" column updates by itself on every textarea change and
   dice quante righe usano ciascun attributo (0 se non lo usi ancora, — says how many rows use each attribute (0 if you do not use it yet,
   il che non significa che non esista). — which does not mean it does not exist).
   ═══════════════════════════════════════════════════════════════════ */
const ATTR_CODES = ['s', 'e', 'c', 'm', 'f'];
function describeAttrCode(code) {
  if (code === 'f') return t('attr_desc_font_pre') + 'N (es. f14)';
  const map = { s: 'attr_desc_s', e: 'attr_desc_e', c: 'attr_desc_c', m: 'attr_desc_m' };
  return t(map[code]);
}
function decomposeAttr(attr) {
  const tokens = [];
  let i = 0;
  while (i < attr.length) {
    if ('secm'.indexOf(attr[i]) >= 0) { tokens.push(attr[i]); i++; continue; }
    if (attr[i] === 'f') {
      const m = attr.slice(i).match(/^f\d+/);
      if (m) { tokens.push(m[0]); i += m[0].length; continue; }
    }
    tokens.push(attr[i]); i++;
  }
  return tokens;
}
function updateAttrTable() {
  const text = document.getElementById('raw').value;
  const found = {};
  text.split('\n').forEach(line => {
    line = line.trim();
    if (!line) return;
    const parts = line.split('|');
    if (parts.length < 6) return;
    const attr = parts[4];
    if (!attr) return;
    decomposeAttr(attr).forEach(tok => {
      const code = tok[0] === 'f' ? 'f' : tok;
      found[code] = (found[code]||0) + 1;
    });
  });
  const el = document.getElementById('attr-table');
  el.innerHTML = '<table class="attr-tbl"><tr><th>'+t('attr_th_code')+'</th><th>'+t('attr_th_desc')+'</th><th>'+t('attr_th_rows')+'</th></tr>' +
    ATTR_CODES.map(k => '<tr><td class="code">'+escHtml(k)+'</td><td>'+describeAttrCode(k)+'</td><td class="count">'+(found[k]||0)+'</td></tr>').join('') +
    '</table>';
}

async function saveRaw() {
  const text = document.getElementById('raw').value;
  try {
    const r = await fetch(API+'/api/overlays', {
      method:'POST', headers:{'Content-Type':'text/plain'}, body:text
    });
    if (!r.ok) { alert(t('ovl_save_failed')); return; }
    document.getElementById('status').textContent = t('ovl_saved_pre')+text.length+t('ovl_saved_post');
  } catch(e) { alert(e.message); }
}

// Scarica l'intero file /overlays.txt sul PC (backup locale). — Downloads the whole /overlays.txt file to the PC (local backup).
async function downloadRawFile() {
  try {
    const r = await fetch(API+'/api/overlays/raw');
    const text = await r.text();
    const blob = new Blob([text], {type:'text/plain'});
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'overlays.txt';
    document.body.appendChild(a);
    a.click();
    setTimeout(() => { URL.revokeObjectURL(a.href); a.remove(); }, 500);
    document.getElementById('status').textContent = t('ovl_downloaded');
  } catch(e) { alert(e.message); }
}

// Apre un file overlays.txt dal PC e lo carica nel textarea (non — Opens an overlays.txt file from the PC and loads it in the textarea (it does
// salva subito: serve premere SALVA, per controllare prima). — not save right away: you must press SAVE, to check first).
async function uploadRawFile() {
  const input = document.getElementById('ovl-file');
  const file = input.files[0];
  if (!file) { alert(t('mgr_alert_choose_file')); return; }
  try {
    const text = await file.text();
    document.getElementById('raw').value = text;
    document.getElementById('status').textContent = t('ovl_uploaded_pre')+file.name+t('ovl_uploaded_post');
    input.value = '';
    updateAttrTable();
  } catch(e) { alert(e.message); }
}

async function loadModulesForSelect() {
  try {
    const r = await fetch(API+'/api/modules');
    const d = await r.json();
    modulesCache = d.modules || [];

    // Schede magnetiche: stessa struttura {id,name,programs:[{num,title}]} — Magnetic cards: same structure {id,name,programs:[{num,title}]}
    // dei moduli ROM, così fillProgramSelect() le gestisce senza — as ROM modules, so fillProgramSelect() handles them without
    // bisogno di codice separato — "num" è lo slot, "title" è il nome — separate code — "num" is the slot, "title" is the name
    // dato alla scheda in fase di scrittura (letto da CardEmuState). — given to the card when writing it (read from CardEmuState).
    // Presente SEMPRE (anche con zero schede salvate): così si possono — Always present (even with zero saved cards): so you can
    // definire/editare righe, attributi e posizioni degli overlay — define/edit rows, attributes and positions of card overlays
    // scheda esattamente come per le ROM, per qualunque slot. — exactly like for ROMs, for any slot.
    const CARDS_ALL_SLOTS = 50;
    try {
      const rc = await fetch(API+'/api/cards');
      const cards = await rc.json();
      const bySlot = {};
      (Array.isArray(cards) ? cards : []).forEach(c => { bySlot[c.slot] = c.name; });
      const programs = [];
      for (let s = 0; s < CARDS_ALL_SLOTS; s++) {
        programs.push({ num: s, title: bySlot[s] || ('Slot ' + String(s).padStart(2, '0')) });
      }
      modulesCache.push({
        id: 'card',
        name: t('ovl_cards_module_name'),
        programs: programs
      });
    } catch(e) {}

    const sel = document.getElementById('mod-select');
    sel.innerHTML = '';
    modulesCache.forEach(m => {
      const opt = document.createElement('option');
      opt.value = m.id; opt.textContent = m.name;
      sel.appendChild(opt);
    });
    sel.addEventListener('change', fillProgramSelect);
    fillProgramSelect();
  } catch(e) {}
}

function fillProgramSelect() {
  const modId = document.getElementById('mod-select').value;
  const m = modulesCache.find(x => x.id === modId);
  const sel = document.getElementById('prog-select');
  sel.innerHTML = '';
  if (!m) return;
  m.programs.forEach(p => {
    const opt = document.createElement('option');
    opt.value = String(p.num).padStart(2,'0');
    opt.textContent = p.num + ' — ' + p.title;
    sel.appendChild(opt);
  });
}

async function previewOverlay() {
  const mod = document.getElementById('mod-select').value;
  const prog = document.getElementById('prog-select').value;
  if (!mod || !prog) return;
  try {
    const r = await fetch(API+'/api/overlays?mod='+encodeURIComponent(mod)+'&prog='+encodeURIComponent(prog));
    const rows = await r.json();
    const el = document.getElementById('rows');
    // Nome della scheda magnetica selezionata, per l'anteprima — Name of the selected magnetic card, for the preview
    // (la card_card.svg mostra il nome in alto — stesso percorso del — (card_card.svg shows the name on top — same path as
    // WEB_IDE, dove arriva da /api/status active_card_name). — WEB_IDE, where it comes from /api/status active_card_name).
    let cardName = '';
    if (mod === 'card') {
      const m = modulesCache.find(x => x.id === 'card');
      const p = m && m.programs.find(x => String(x.num).padStart(2,'0') === prog);
      if (p) cardName = p.title;
    }
    if (!rows.length && !cardName) { el.innerHTML = '<em style="color:var(--muted)">'+t('ovl_no_rows_for')+mod+' '+prog+'</em>'; return; }
    el.innerHTML = renderCardSVG(mod, prog, rows, cardName);
    document.getElementById('status').textContent = (rows.length ? rows.length+t('ovl_rows_count') : t('ovl_card_name_only'))+mod+' '+prog + (rows.length ? ' — '+rows[0].type : '');
  } catch(e) { alert(e.message); }
}

/* ═══════════════════════════════════════════════════════════════════
   ANTEPRIMA SCHEDA MAGNETICA — pannello dedicato in /overlays. — Magnetic card preview — dedicated panel in /overlays.
   Mostra la card (sempre, anche senza file in SPIFFS) usando lo stack — Shows the card (always, even without files in SPIFFS) using the configured template stack
   di template configurato (top = strato superiore già impostato sopra — of configured template (top = upper layer already set above
   nella cardrender.js) e indica quali template risultano mancanti, — in cardrender.js) and indicates which templates are missing,
   così da evidenziare "vedo le linee di default perché manca top.svg". — thus highlighting "I see default lines because top.svg is missing".
   ═══════════════════════════════════════════════════════════════════ */
// Applica alla globals di /cardrender.js i valori correnti dei campi — Applies to the /cardrender.js globals the current field values
// del pannello posizioni. "Mostra" la usa PRIMA di renderizzare: così — of the positions panel. "Show" uses it BEFORE rendering: so
// la preview riflette subito titolo e template digitati (anche non — the preview reflects right away the typed title and template (even not
// ancora salvati), invece di rileggere dal device un file vecchio. — yet saved), instead of re-reading an old file from the device.
function applyPositionsFromUI() {
  const num = id => parseFloat(document.getElementById(id).value);
  const colx = [0,1,2,3,4].map(i => num('pos-colx-'+i) || 0);
  const rowy = [0,1].map(i => num('pos-rowy-'+i) || 0);
  if (colx.every(v => v) && rowy.every(v => v)) {
    GRID_COL_X = colx; GRID_ROW_Y = rowy;
  }
  FREE_ROW_Y = collectFreeRowInputs();
  MARGIN_LEFT = num('pos-margin-left') || 0;
  MARGIN_RIGHT = num('pos-margin-right') || 0;
  CARD_NAME_X = num('pos-card-name-x') || 0;
  CARD_NAME_Y = num('pos-card-name-y') || 0;
  const norm = s => s.indexOf('/') === 0 ? s : ('/' + s);
  const tplTop = document.getElementById('pos-tpl-top').value.trim();
  const tplBottom = document.getElementById('pos-tpl-bottom').value.trim();
  CARD_TEMPLATES = [];
  if (tplTop) CARD_TEMPLATES.push(norm(tplTop));
  if (tplBottom) CARD_TEMPLATES.push(norm(tplBottom));
}
async function previewCard() {
  const sel = document.getElementById('card-slot-select');
  if (!sel) return;
  const prog = sel.value || 'XX';
  const mod = 'card';
  // Usa SEMPRE i valori correnti dei campi del pannello posizioni — ALWAYS uses the current field values of the positions panel
  // (popolati all'apertura da loadPositionsUI): così "Mostra" e — (populated on open by loadPositionsUI): so "Show" and
  // l'anteprima live riflettono subito W/H/X/Y digitati, senza — the live preview reflect right away the typed W/H/X/Y, without
  // rileggere dal device un file vecchio né attendere retry. — re-reading an old file from the device nor waiting for retry.
  applyPositionsFromUI();
  let cardName = '';
  if (prog !== 'XX') {
    try {
      const rc = await fetch(API+'/api/cards');
      const cards = await rc.json();
      const c = (Array.isArray(cards) ? cards : []).find(x => x && String(x.slot).padStart(2,'0') === prog);
      if (c && c.name) cardName = c.name;
    } catch(e) {}
  }
  let rows = [];
  try {
    const r = await fetch(API+'/api/overlays?mod=card&prog='+encodeURIComponent(prog));
    rows = await r.json();
  } catch(e) {}
  const el = document.getElementById('card-preview');
  if (!el) return;
  el.innerHTML = renderCardSVG(mod, prog, rows, cardName);
  showCardTemplatesStatus();
  const st = document.getElementById('status');
  if (st) st.textContent = t('ovl_card_preview_status')+' '+prog;
}
async function showCardTemplatesStatus() {
  const el = document.getElementById('card-tpl-status');
  if (!el) return;
  if (!CARD_TEMPLATES.length) { el.textContent = t('ovl_card_tpl_auto'); return; }
  const parts = [];
  for (let i = 0; i < CARD_TEMPLATES.length; i++) {
    const tp = CARD_TEMPLATES[i];
    let ok = false;
    try { const f = await fetch(tp); if (f) ok = f.ok; } catch(e) {}
    const label = (i === 0 ? t('ovl_card_tpl_top') : t('ovl_card_tpl_base'));
    parts.push(label+' '+tp+(ok ? '' : ' '+t('ovl_card_tpl_missing')));
  }
  el.textContent = parts.join('   |   ');
}
function initCardPreview() {
  const sel = document.getElementById('card-slot-select');
  if (!sel) return;
  sel.innerHTML = '';
  // XX = nessuna card (default): lo slot 00 è una vera card magnetica. — XX = no card (default): slot 00 is a real magnetic card.
  const none = document.createElement('option');
  none.value = 'XX';
  none.textContent = 'XX';
  sel.appendChild(none);
  for (let s = 0; s < 50; s++) {
    const opt = document.createElement('option');
    opt.value = String(s).padStart(2,'0');
    opt.textContent = String(s).padStart(2,'0');
    sel.appendChild(opt);
  }
  // Anteprima LIVE: ogni modifica a un campo del pannello posizioni — LIVE preview: every change to a positions panel field
  // ri-renderizza subito la preview card, così in fase di creazione — re-renders the card preview right away, so while creating
  // si vede l'effetto di W/H/X/Y senza premere "Mostra" né salvare. — you see the effect of W/H/X/Y without pressing "Show" nor saving.
  document.querySelectorAll('#pos-panel input, #pos-panel select').forEach(inp => {
    inp.addEventListener('input', () => { if (document.getElementById('card-preview')) previewCard(); });
  });
  try {
    fetch(API+'/api/status').then(r => r.json()).then(st => {
      if (st && typeof st.active_card_slot === 'number' && st.active_card_slot >= 0) {
        sel.value = String(st.active_card_slot).padStart(2,'0');
      }
      previewCard();   // sempre: senza card attiva resta su XX — always: stays on XX without an active card
    }).catch(e => previewCard());
  } catch(e) { previewCard(); }
}

/* ═══════════════════════════════════════════════════════════════════
   PANNELLO POSIZIONI TESTO — legge/scrive GRID_COL_X/GRID_ROW_Y/ — TEXT POSITIONS PANEL — reads/writes GRID_COL_X/GRID_ROW_Y/
   FREE_ROW_Y (variabili globali di /cardrender.js, incluso sopra) e — FREE_ROW_Y (global variables of /cardrender.js, included above) and
   le persiste su /api/card_positions. Le righe FREE sono a numero — persists them to /api/card_positions. FREE rows have a variable
   variabile: gestite come lista di coppie KEY/Y aggiungibili/ — count: handled as a list of addable/removable KEY/Y pairs,
   rimuovibili, non un numero fisso di campi. — not a fixed number of fields.
   ═══════════════════════════════════════════════════════════════════ */
function renderFreeRowInputs(map) {
  const el = document.getElementById('pos-free-rows');
  el.innerHTML = '';
  Object.keys(map).sort((a,b) => a-b).forEach(k => addFreeRowPos(k, map[k]));
}
function addFreeRowPos(key, val) {
  const el = document.getElementById('pos-free-rows');
  const row = document.createElement('div');
  row.className = 'input-row';
  row.innerHTML =
    '<input type="number" value="'+(key !== undefined ? key : '')+'" placeholder="KEY" style="width:50px" class="pos-free-key">' +
    '<input type="number" step="0.1" value="'+(val !== undefined ? val : '')+'" placeholder="Y" style="width:58px" class="pos-free-val">' +
    '<button class="btn btn-teal" onclick="this.parentElement.remove()">&times;</button>';
  el.appendChild(row);
}
function collectFreeRowInputs() {
  const out = {};
  document.querySelectorAll('#pos-free-rows .input-row').forEach(row => {
    const k = row.querySelector('.pos-free-key').value;
    const v = row.querySelector('.pos-free-val').value;
    if (k !== '' && v !== '') out[k] = parseFloat(v);
  });
  return out;
}
async function loadPositionsUI() {
  await loadCardPositions();   // in /cardrender.js — aggiorna GRID_COL_X ecc. dal device — updates GRID_COL_X etc. from the device
  GRID_COL_X.forEach((v,i) => { const el = document.getElementById('pos-colx-'+i); if (el) el.value = v; });
  GRID_ROW_Y.forEach((v,i) => { const el = document.getElementById('pos-rowy-'+i); if (el) el.value = v; });
  renderFreeRowInputs(FREE_ROW_Y);
  document.getElementById('pos-margin-left').value = MARGIN_LEFT;
  document.getElementById('pos-margin-right').value = MARGIN_RIGHT;
  document.getElementById('pos-card-name-x').value = CARD_NAME_X;
  document.getElementById('pos-card-name-y').value = CARD_NAME_Y;
  document.getElementById('pos-tpl-top').value = CARD_TEMPLATES[0] || '';
  document.getElementById('pos-tpl-bottom').value = CARD_TEMPLATES[1] || '';
}
async function savePositions() {
  const colx = [0,1,2,3,4].map(i => parseFloat(document.getElementById('pos-colx-'+i).value) || 0);
  const rowy = [0,1].map(i => parseFloat(document.getElementById('pos-rowy-'+i).value) || 0);
  const freey = collectFreeRowInputs();
  const marginLeft = parseFloat(document.getElementById('pos-margin-left').value) || 0;
  const marginRight = parseFloat(document.getElementById('pos-margin-right').value) || 0;
  const cardNameX = parseFloat(document.getElementById('pos-card-name-x').value) || 0;
  const cardNameY = parseFloat(document.getElementById('pos-card-name-y').value) || 0;
  const tplTop = document.getElementById('pos-tpl-top').value.trim();
  const tplBottom = document.getElementById('pos-tpl-bottom').value.trim();
  const norm = s => s.indexOf('/') === 0 ? s : ('/' + s);
  const templates = [];
  if (tplTop) templates.push(norm(tplTop));
  if (tplBottom) templates.push(norm(tplBottom));
  const payload = JSON.stringify({
    grid_col_x: colx, grid_row_y: rowy, free_row_y: freey,
    margin_left: marginLeft, margin_right: marginRight,
    card_name_x: cardNameX, card_name_y: cardNameY,
    templates: templates
  });
  // Salvataggio verificato: il device può perdere una POST (instabilità — Verified save: the device may drop a POST (instability
  // WiFi/stack) lasciando il vecchio file — sintomo "le posizioni — WiFi/stack) leaving the old file — symptom "the positions
  // tornano a zero". Quindi: scrivi, rileggi, confronta; ritenta fino a — go back to zero". So: write, re-read, compare; retry up to
  // 3 volte; segnala solo se nessun tentativo è andato a buon fine. — 3 times; report only if no attempt succeeded.
  let saved = false;
  for (let attempt = 0; attempt < 3 && !saved; attempt++) {
    try {
      const r = await fetch('/api/card_positions', { method:'POST', headers:{'Content-Type':'application/json'}, body: payload });
      if (r.ok) {
        const v = await (await fetch('/api/card_positions')).json();
        saved = JSON.stringify(v) === payload;
      }
    } catch(e) {}
    if (!saved && attempt < 2) await new Promise(res => setTimeout(res, 600));
  }
  if (!saved) { alert(t('pos_save_failed')); return; }
  GRID_COL_X = colx; GRID_ROW_Y = rowy; FREE_ROW_Y = freey;
  MARGIN_LEFT = marginLeft; MARGIN_RIGHT = marginRight;
  CARD_NAME_X = cardNameX; CARD_NAME_Y = cardNameY;
  CARD_TEMPLATES = templates;
  document.getElementById('status').textContent = t('pos_saved');
}
function resetPositionsToDefault() {
  DEFAULT_GRID_COL_X.forEach((v,i) => { document.getElementById('pos-colx-'+i).value = v; });
  DEFAULT_GRID_ROW_Y.forEach((v,i) => { document.getElementById('pos-rowy-'+i).value = v; });
  renderFreeRowInputs(DEFAULT_FREE_ROW_Y);
  document.getElementById('pos-margin-left').value = DEFAULT_MARGIN_LEFT;
  document.getElementById('pos-margin-right').value = DEFAULT_MARGIN_RIGHT;
  document.getElementById('pos-card-name-x').value = DEFAULT_CARD_NAME_X;
  document.getElementById('pos-card-name-y').value = DEFAULT_CARD_NAME_Y;
  document.getElementById('pos-tpl-top').value = '';
  document.getElementById('pos-tpl-bottom').value = '';
  // Non salva da solo — resta sui campi finché non premi "Salva posizioni", — Doesn't save by itself — stays on the fields until you press "Save positions",
  // così puoi controllare prima di sovrascrivere quanto avevi sul device. — so you can check before overwriting what you had on the device.
}

// Backup: scarica il file /overlay_pos.json dal device sul PC. — Backup: downloads the /overlay_pos.json file from the device to the PC.
async function downloadPosFile() {
  try {
    const r = await fetch(API+'/api/card_positions');
    const text = await r.text();
    const blob = new Blob([text], {type:'application/json'});
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'overlay_pos.json';
    document.body.appendChild(a);
    a.click();
    setTimeout(() => { URL.revokeObjectURL(a.href); a.remove(); }, 500);
    document.getElementById('status').textContent = t('pos_downloaded');
  } catch(e) { alert(e.message); }
}

// Restore: apre un overlay_pos.json dal PC e ne carica i valori nel — Restore: opens an overlay_pos.json from the PC and loads its values into the
// pannello (stessa logica di loadPositionsUI). Non scrive subito sul — panel (same logic as loadPositionsUI). Doesn't write right away to the
// device: serve premere "Salva posizioni" per controllare prima. — device: you must press "Save positions" to check first.
async function uploadPosFile() {
  const input = document.getElementById('pos-file');
  const file = input.files[0];
  if (!file) { alert(t('mgr_alert_choose_file')); return; }
  let data;
  try {
    data = JSON.parse(await file.text());
  } catch(e) { alert(t('pos_invalid_file')); return; }
  if (typeof data !== 'object' || data === null || Array.isArray(data)) {
    alert(t('pos_invalid_file')); return;
  }
  if (Array.isArray(data.grid_col_x)) {
    data.grid_col_x.forEach((v,i) => { const el = document.getElementById('pos-colx-'+i); if (el) el.value = v; });
  }
  if (Array.isArray(data.grid_row_y)) {
    data.grid_row_y.forEach((v,i) => { const el = document.getElementById('pos-rowy-'+i); if (el) el.value = v; });
  }
  renderFreeRowInputs((data.free_row_y && typeof data.free_row_y === 'object') ? data.free_row_y : {});
  document.getElementById('pos-margin-left').value = typeof data.margin_left === 'number' ? data.margin_left : 0;
  document.getElementById('pos-margin-right').value = typeof data.margin_right === 'number' ? data.margin_right : 0;
  document.getElementById('pos-card-name-x').value = typeof data.card_name_x === 'number' ? data.card_name_x : 0;
  document.getElementById('pos-card-name-y').value = typeof data.card_name_y === 'number' ? data.card_name_y : 0;
  const tpl = Array.isArray(data.templates) ? data.templates : [];
  document.getElementById('pos-tpl-top').value = tpl[0] !== undefined ? String(tpl[0]) : '';
  document.getElementById('pos-tpl-bottom').value = tpl[1] !== undefined ? String(tpl[1]) : '';
  input.value = '';
  document.getElementById('status').textContent = t('pos_uploaded');
  previewCard();
}

// loadRaw()/loadModulesForSelect() NON devono poter impedire l'init — loadRaw()/loadModulesForSelect() must NOT be able to prevent the preview init
// dell'anteprima: un loro errore sincrono fermerebbe lo script prima di — a synchronous error of theirs would stop the script before
// arrivare a posReady e il pannello resterebbe vuoto. Isolali. — reaching posReady and the panel would stay empty. Isolate them.
try { loadRaw(); } catch(e) {}
try { loadModulesForSelect(); } catch(e) {}
const posReady = loadPositionsUI();
checkSvgTemplates().then(missing => {
  if (!missing.length) return;
  const el = document.getElementById('svg-warning');
  if (!el) return;
  el.style.display = 'block';
  el.textContent = t('svg_missing_pre') + missing.join(', ') + t('svg_missing_post');
}).catch(() => {});
// L'anteprima deve partire SOLO dopo che loadPositionsUI() (che attende — The preview must start ONLY after loadPositionsUI() (which waits
// loadCardPositions() -> /api/card_positions) ha impostato CARD_TEMPLATES: — loadCardPositions() -> /api/card_positions) has set CARD_TEMPLATES:
// altrimenti renderizza il fallback perché i template non sono ancora — otherwise it renders the fallback because templates are not yet
// caricati (race condition). Via Promise.resolve, e con .catch, così — loaded (race condition). Via Promise.resolve, and with .catch, so
// initCardPreview parte SEMPRE — anche se loadPositionsUI fallisse. — initCardPreview ALWAYS runs — even if loadPositionsUI fails.
Promise.resolve(posReady)
  .then(() => initCardPreview())
  .catch(() => initCardPreview());
</script>
</body>
</html>)rawhtml";

/* ═══════════════════════════════════════════════════════════════════
   VARIABILI GLOBALI — GLOBAL VARIABLES
   ═══════════════════════════════════════════════════════════════════ */
WebServer server(WIFI_PORT);
TMS1500_State *g_cpu  = nullptr;
CardEmuState  *g_card = nullptr;
KeyboardState *g_kbd  = nullptr;

static bool captive_mode = false;
static DNSServer dnsServer;

// ── Watchdog riconnessione WiFi — WiFi reconnection watchdog ─────────
// WiFi.setAutoReconnect(true) (in wifi_try_connect) gestisce da solo — WiFi.setAutoReconnect(true) (in wifi_try_connect) handles by itself
// le interruzioni brevi (es. router che si riavvia). Questi — short interruptions (e.g. a router that reboots). These
// parametri controllano l'intervento manuale quando quello non basta: — parameters control the manual intervention when that is not enough:
// nessun controllo periodico esisteva prima, quindi una rete persa — no periodic check existed before, so a truly lost network
// per davvero lasciava il dispositivo bloccato in silenzio, con il — left the device silently stuck, with the
// LED di stato che continuava a segnalare "connesso". — status LED that kept reporting "connected".
#define WIFI_STATUS_POLL_MS      5000  
 // ogni quanto controllare WiFi.status() — how often to check WiFi.status()
#define WIFI_GRACE_BEFORE_RETRY_MS 20000 // margine di tempo dato all'auto-reconnect nativo — grace time given to the native auto-reconnect
#define WIFI_RETRY_COOLDOWN_MS   60000  // minimo tra due tentativi manuali di riconnessione — minimum between two manual reconnection attempts
static unsigned long wifi_last_poll_ms = 0;
static unsigned long wifi_disconnected_since_ms = 0;  // 0 = attualmente connesso — 0 = currently connected
static unsigned long wifi_last_retry_ms = 0;
#define MAX_CREDENTIALS 4
typedef struct {
  char ssid[32];
  char pass[64];
} WiFiCredential;

static WiFiCredential wifi_creds[MAX_CREDENTIALS];
static int wifi_cred_count = 0;
/* ═══════════════════════════════════════════════════════════════════
   HELPER JSON grezzo (senza librerie esterne) — Raw JSON helper (without external libraries)
   ═══════════════════════════════════════════════════════════════════ */
static String json_extract(const String& json, const char* key) {
    String k = String("\"") + key + "\"";
int idx = json.indexOf(k);
    if (idx < 0) return "";
    int colon = json.indexOf(':', idx + k.length());
if (colon < 0) return "";
    int start = colon + 1;
// Salta eventuali spazi vuoti — Skip any blank spaces
    while (start < json.length() && (json[start] == ' ' || json[start] == '\t')) start++;
if (json[start] == '\"') {
        int end = start + 1;
        while (end < json.length()) {
            if (json[end] == '\"' && json[end-1] != '\\') break;
// Rispetta l'escape \ " — Respects the \ " escape
            end++;
        }
        return json.substring(start + 1, end);
    }
    
    int end = start;
    while (end < json.length() && json[end] != ',' && json[end] != '}' && json[end] != '\n' && json[end] != '\r' && json[end] != ' ') end++;
    return json.substring(start, end);
}

/* ═══════════════════════════════════════════════════════════════════
   PERSISTENZA CREDENZIALI (file su SPIFFS — /wifi.json) — CREDENTIALS PERSISTENCE (SPIFFS file — /wifi.json)
   ═══════════════════════════════════════════════════════════════════ */
// Le credenziali WiFi vivono in un file JSON su SPIFFS: salvabile,
// modificabile e ricaricabile dall'utente via web. Il firmware le legge
// all'avvio (wifi_load_creds), le riscrive a ogni modifica
// (wifi_save_creds, atomico tmp+rename) e le espone per download/upload
// su /api/wifi/file. Niente più credenziali in NVS.
// WiFi credentials live in a JSON file on SPIFFS: savable, editable and
// reloadable by the user via web. The firmware reads them at boot
// (wifi_load_creds), rewrites them on every change (wifi_save_creds,
// atomic tmp+rename) and exposes them for download/upload at
// /api/wifi/file. No more credentials in NVS.
#include <Preferences.h>

static String json_escape(const char *s);   // definita più sotto — defined below
static String json_unescape(const String &s); // definita più sotto — defined below
static bool wifi_save_creds();              // definita più sotto — defined below

#define WIFI_CREDS_FILE "/wifi.json"   // elenco credenziali WiFi — WiFi credentials list
#define WIFI_CREDS_TMP  "/wifi.tmp"    // file temporaneo per scrittura atomica — temp file for atomic write

// ─── Impostazioni persistenti generiche (NVS, namespace separato) — generic persistent settings (NVS, separate namespace) ──
static Preferences settings_prefs;
#define NVS_SETTINGS_NS   "ti59cfg"
#define NVS_KEY_REALTIME  "realtime"
#define NVS_KEY_TIMINGPCT "timingpct"
#define NVS_KEY_LIBMOD    "libmod"
#define NVS_KEY_EJECTMS   "ejectms"

static void settings_load_and_apply() {
    if (!settings_prefs.begin(NVS_SETTINGS_NS, true)) {
        Serial.println("[CFG] Nessuna impostazione salvata, uso i default");
        return;
    }
    bool realistic = settings_prefs.getBool(NVS_KEY_REALTIME, false);
    uint8_t timing_pct = settings_prefs.getUChar(NVS_KEY_TIMINGPCT, 100);
    uint16_t eject_ms  = settings_prefs.getUShort(NVS_KEY_EJECTMS, RFD_EJECT_MS);
String libmod  = settings_prefs.getString(NVS_KEY_LIBMOD, "");
    settings_prefs.end();

    tms1500_set_realistic_timing(realistic);
    tms1500_set_timing_multiplier(timing_pct / 100.0f);
    rfid_reader_set_eject_ms(eject_ms);
    Serial.printf("[CFG] Timing ripristinato: %s @ %d%%\n", realistic ? "OLD (reale)" : "NEW (moderna)", (int)timing_pct);
if (libmod.length() > 0) {
        if (library_set_active(libmod.c_str())) {
            Serial.printf("[CFG] Modulo libreria ripristinato: %s\n", libmod.c_str());
} else {
            Serial.printf("[CFG] Modulo libreria salvato \"%s\" non trovato (rimosso dal firmware?)\n", libmod.c_str());
}
    }
}

// Implementazione reale dell'hook "debole" dichiarato in tms1500.h: — Real implementation of the "weak" hook declared in tms1500.h:
// sovrascrive quello no-op di tms1500.cpp. — overrides the no-op one in tms1500.cpp.
//Chiamato sia dal toggle web — Called both from the web toggle
// (/api/timing) sia dal combo fisico +,-,x,/ sulla tastiera — in — (/api/timing) and from the physical key combo +,-,x,/ on the keypad — in
// entrambi i casi la scelta viene ricordata tra un riavvio e l'altro. — both cases the choice is remembered across reboots.
void tms1500_on_timing_changed(bool realistic) {
    if (!settings_prefs.begin(NVS_SETTINGS_NS, false)) {
        Serial.println("[CFG] Impossibile aprire NVS per salvare il timing");
return;
    }
    settings_prefs.putBool(NVS_KEY_REALTIME, realistic);
    settings_prefs.end();
    Serial.printf("[CFG] Timing salvato: %s\n", realistic ? "OLD (reale)" : "NEW (moderna)");
}

// Implementazione reale dell'hook "debole" dichiarato in — Real implementation of the "weak" hook declared in
// library_module.h: ricorda l'ultimo modulo innestato (o "nessuno") — library_module.h: remembers the last module plugged in (or "none")
// tra un riavvio e l'altro, esattamente come il timing sopra. — between reboots, exactly like the timing above.
void library_on_module_changed(const char *id) {
    tms1500_on_library_module_changed(id);   // reset stato CPU/ROM (v. tms1500.cpp) — reset CPU/ROM state (see tms1500.cpp)
    if (!settings_prefs.begin(NVS_SETTINGS_NS, false)) {
        Serial.println("[CFG] Impossibile aprire NVS per salvare il modulo");
return;
    }
    settings_prefs.putString(NVS_KEY_LIBMOD, id ? id : "");
    settings_prefs.end();
Serial.printf("[CFG] Modulo libreria salvato: %s\n", (id && id[0]) ? id : "(nessuno)");
}
// Parser dell'array JSON di credenziali ({ssid,pass}): azzera e popola
// wifi_creds[]/wifi_cred_count. Usato sia da wifi_load_creds (boot) sia
// da /api/wifi/file (upload). Ritorna il numero di voci caricate.
// Parses the JSON credentials array ({ssid,pass}): clears and fills
// wifi_creds[]/wifi_cred_count. Used both by wifi_load_creds (boot) and
// /api/wifi/file (upload). Returns the number of loaded entries.
static int wifi_parse_creds_json(const String &json) {
    memset(wifi_creds, 0, sizeof(wifi_creds));
    wifi_cred_count = 0;
    int pos = 0;
    while (wifi_cred_count < MAX_CREDENTIALS) {
        int b = json.indexOf('{', pos);
        if (b < 0) break;
        int e = json.indexOf('}', b);
        if (e < 0) break;
        String obj = json.substring(b, e + 1);
        String ssid = json_unescape(json_extract(obj, "ssid"));
        String pass = json_unescape(json_extract(obj, "pass"));
        if (ssid.length() > 0 && ssid.length() <= 31) {
            strncpy(wifi_creds[wifi_cred_count].ssid, ssid.c_str(), 31);
            wifi_creds[wifi_cred_count].ssid[31] = 0;
            if (pass.length() > 63) pass = pass.substring(0, 63);
            strncpy(wifi_creds[wifi_cred_count].pass, pass.c_str(), 63);
            wifi_creds[wifi_cred_count].pass[63] = 0;
            wifi_cred_count++;
        }
        pos = e + 1;
    }
    return wifi_cred_count;
}

// Inverte json_escape: ricostruisce la stringa originale dai codici
// \" \\ \n \r \t (e \/). Serve per un round-trip fedele di SSID/password
// che contengono caratteri speciali.
// Reverses json_escape: rebuilds the original string from the \" \\
// \n \r \t (and \/) codes. Needed for a faithful round-trip of
// SSID/passwords containing special characters.
static String json_unescape(const String &s) {
    String out;
    out.reserve(s.length());
    for (int i = 0; i < (int)s.length(); i++) {
        char c = s[i];
        if (c == '\\' && i + 1 < (int)s.length()) {
            char n = s[++i];
            switch (n) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case '/':  out += '/';  break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                default:   out += n;    break;
            }
        } else {
            out += c;
        }
    }
    return out;
}

static bool wifi_load_creds() {
    if (SPIFFS.exists(WIFI_CREDS_FILE)) {
        File f = SPIFFS.open(WIFI_CREDS_FILE, FILE_READ);
        if (f) {
            String json = f.readString();
            f.close();
            wifi_parse_creds_json(json);
            Serial.printf("[WiFi] Caricate %d credenziali da %s\n",
                          wifi_cred_count, WIFI_CREDS_FILE);
            return wifi_cred_count > 0;
        }
    }

    // Migrazione una tantum: se il file non esiste ancora ma c'erano
    // credenziali salvate in NVS dalla versione precedente, le sposta
    // nel file e ripulisce il namespace (dimenticato per sempre).
    // One-time migration: if the file does not exist yet but credentials
    // were stored in NVS by the previous version, moves them to the file
    // and clears the namespace (forgotten forever).
    Preferences legacy;
    if (legacy.begin("ti59_wifi", true)) {
        int n = legacy.getInt("count", 0);
        if (n > 0) {
            memset(wifi_creds, 0, sizeof(wifi_creds));
            wifi_cred_count = 0;
            for (int i = 0; i < MAX_CREDENTIALS && i < n; i++) {
                String ssid = legacy.getString(("ssid" + String(i)).c_str(), "");
                String pass = legacy.getString(("pass" + String(i)).c_str(), "");
                if (ssid.length() > 0 && ssid.length() <= 31) {
                    strncpy(wifi_creds[wifi_cred_count].ssid, ssid.c_str(), 31);
                    wifi_creds[wifi_cred_count].ssid[31] = 0;
                    if (pass.length() > 63) pass = pass.substring(0, 63);
                    strncpy(wifi_creds[wifi_cred_count].pass, pass.c_str(), 63);
                    wifi_creds[wifi_cred_count].pass[63] = 0;
                    wifi_cred_count++;
                }
            }
            legacy.end();
            if (wifi_save_creds()) {
                Preferences wipe;
                wipe.begin("ti59_wifi", false);
                for (int i = 0; i < MAX_CREDENTIALS; i++) {
                    wipe.remove(("ssid" + String(i)).c_str());
                    wipe.remove(("pass" + String(i)).c_str());
                }
                wipe.remove("count");
                wipe.end();
                Serial.printf("[WiFi] Migrate %d credenziali da NVS a %s\n",
                              wifi_cred_count, WIFI_CREDS_FILE);
                return wifi_cred_count > 0;
            }
            legacy.begin("ti59_wifi", true);   // riapri se il save è fallito — reopen if the save failed
        }
        legacy.end();
    }

    wifi_cred_count = 0;
    Serial.printf("[WiFi] Nessun file %s, elenco credenziali vuoto\n", WIFI_CREDS_FILE);
    return false;
}

static bool wifi_save_creds() {
    // Ricostruisce l'intero file (unica copia, nessun namespace NVS).
    // Rebuilds the whole file (single copy, no NVS namespace).
    String json = "[";
    bool first = true;
    for (int i = 0; i < MAX_CREDENTIALS; i++) {
        if (!wifi_creds[i].ssid[0]) continue;
        if (!first) json += ",";
        first = false;
        json += "{\"ssid\":\"" + json_escape(wifi_creds[i].ssid)
             +  "\",\"pass\":\"" + json_escape(wifi_creds[i].pass) + "\"}";
    }
    json += "]";

    // Scrittura atomica tmp+rename (stesso pattern delle posizioni overlay).
    // Atomic tmp+rename write (same pattern as the overlay positions).
    File f = SPIFFS.open(WIFI_CREDS_TMP, FILE_WRITE);
    if (!f) {
        Serial.println("[WiFi] Impossibile scrivere il file credenziali");
        return false;
    }
    f.print(json);
    f.close();
    SPIFFS.remove(WIFI_CREDS_FILE);
    if (!SPIFFS.rename(WIFI_CREDS_TMP, WIFI_CREDS_FILE)) {
        Serial.println("[WiFi] Rename file credenziali fallito");
        return false;
    }
    Serial.printf("[WiFi] Salvate %d credenziali su %s\n", wifi_cred_count, WIFI_CREDS_FILE);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════
   CONNESSIONE — CONNECTION
   ═══════════════════════════════════════════════════════════════════ */
static bool wifi_try_connect(const char* ssid, const char* pass, int timeout_ms) {
    WiFi.disconnect(true);
delay(100);
    WiFi.mode(WIFI_STA);
    delay(100);
    if (pass && pass[0])
        WiFi.begin(ssid, pass);
else
        WiFi.begin(ssid);
    int retries = 0;
    int max_retries = timeout_ms / 500;
Serial.printf("[WiFi] Connessione a %s (timeout %d ms)...\n", ssid, timeout_ms);
    while (WiFi.status() != WL_CONNECTED && retries < max_retries) {
        vTaskDelay(pdMS_TO_TICKS(500));
retries++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connesso a %s, IP %s\n", ssid, WiFi.localIP().toString().c_str());
        // Disabilita il modem sleep: su reti affollate/instabili il power — Disables modem sleep: on crowded/unstable networks the power
        // save causa drop frequenti e riconnessioni lente ("fatica a — save causes frequent drops and slow reconnections ("struggles to
        // riconnettersi"). Costa più energia ma rende il link stabile. — reconnect"). Costs more energy but makes the link stable.
        WiFi.setSleep(false);
        WiFi.setAutoReconnect(true);
        return true;
    }
    Serial.printf("[WiFi] Timeout connessione a %s\n", ssid);
    WiFi.disconnect();
    return false;
}

static bool wifi_try_stored_creds() {
    if (wifi_cred_count == 0) return false;
    // 1) Tentativo DIRETTO senza scansione: il driver ricorda la rete, — 1) DIRECT attempt without scanning: the driver remembers the network,
    //    quindi su una rete affollata la riconnessione non deve dipendere —    so on a crowded network reconnection must not depend
    //    da una scansione completa (che può fallire o rallentare molto). —    on a full scan (which can fail or slow down a lot).
    for (int c = 0; c < MAX_CREDENTIALS; c++) {
        if (wifi_creds[c].ssid[0] && wifi_try_connect(wifi_creds[c].ssid, wifi_creds[c].pass, 15000)) {
            return true;
        }
    }
    // 2) Fallback: scansione (con retry) e connessione alla rete trovata. — 2) Fallback: scan (with retry) and connection to the found network.
    WiFi.mode(WIFI_STA);
    delay(100);
    int n = -1;
    for (int attempt = 0; attempt < 3 && n < 0; attempt++) {
        n = WiFi.scanNetworks();
        if (n < 0) vTaskDelay(pdMS_TO_TICKS(1500));   // scan non pronta/fallita, riprova — scan not ready/failed, retry
    }
    if (n <= 0) { WiFi.scanDelete(); return false; }
    for (int i = 0; i < n; i++) {
        String found = WiFi.SSID(i);
for (int c = 0; c < MAX_CREDENTIALS; c++) {
            if (wifi_creds[c].ssid[0] && found.equals(wifi_creds[c].ssid)) {
                if (wifi_try_connect(wifi_creds[c].ssid, wifi_creds[c].pass, 30000)) {
                    WiFi.scanDelete();
return true;
                }
            }
        }
    }
    WiFi.scanDelete();
return false;
}

static void wifi_start_ap() {
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_AP_STA);
    delay(100);
    WiFi.softAP("TI59-Zombie-Setup", nullptr);
Serial.printf("[WiFi] AP Setup IP: %s\n", WiFi.softAPIP().toString().c_str());
    dnsServer.start(53, "*", WiFi.softAPIP());
}

/* ═══════════════════════════════════════════════════════════════════
   RISPOSTE HTTP — HTTP RESPONSES
   ═══════════════════════════════════════════════════════════════════ */

// Escaping minimo per inserire una stringa ARBITRARIA dentro un valore — Minimal escaping to insert an ARBITRARY string into a value
// JSON costruito a mano con concatenazione (id "+String(...)+" ecc.). — in hand-built JSON via concatenation (id "+String(...)+" etc.).
// Ogni stringa che non è un letterale scritto da noi (nomi scheda, — Every string that is not a literal written by us (card names,
// id/nome modulo, titoli programma, nomi file) deve passare da qui: — module id/name, program titles, file names) must go through here:
// senza escaping, un singolo carattere '"' o '\' nel valore rompe la — without escaping, a single '"' or '\' character in the value breaks the
// struttura del JSON — nella migliore delle ipotesi un parse error — JSON structure — at best a parse error
// lato client, nella peggiore un'iniezione di chiavi/valori JSON — on the client side, at worst an injection of arbitrary JSON keys/values
// arbitrari se quella stringa arriva da input utente (es. nome scheda — if that string comes from user input (e.g. card name
// scelto da web). — chosen from the web).
static String json_escape(const char *s) {
    String out;
    if (!s) return out;
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\r': out += "\\r";  break;
            case '\n': out += "\\n";  break;
            case '\t': out += "\\t";  break;
            default:
                if ((unsigned char)*p < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)*p);
                    out += buf;
                } else {
                    out += *p;
                }
        }
    }
    return out;
}
static String json_escape(const String &s) { return json_escape(s.c_str()); }

static void send_json(int code, const char *json) {
    server.sendHeader("Access-Control-Allow-Origin","*");
server.send(code, "application/json", json);
}
static void send_ok()  { send_json(200, "{\"ok\":true}");
}
static void send_err(const char *msg) {
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"error\":\"%s\"}", msg);
    send_json(400, buf);
}

// Risposta STREAMING (chunked). Evita di costruire una String gigante — STREAMING (chunked) response. Avoids building a giant String
// con centinaia di "+=" (frantumava l'heap e il singolo server.send() — with hundreds of "+=" (it fragmented the heap and the single server.send()
// di una risposta da 15-32 KB si bloccava a metà, consegnando solo — of a 15-32 KB response got stuck midway, delivering only
// una parte del body). Si apre con i soli header (Transfer-Encoding: — part of the body). It opens with headers only (Transfer-Encoding:
// chunked), poi il body viene inviato a pezzetti piccoli con — chunked), then the body is sent in small pieces with
// sendContent, ciascuno un write() TCP di dimensioni ridotte. — sendContent, each one a reduced-size TCP write().
static void begin_stream(int code, const char *content_type) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(code, content_type, "");
}
static void stream_flush(String &chunk) {
    if (chunk.length()) { server.sendContent(chunk); chunk = ""; }
    server.sendContent("");
}

/* ═══════════════════════════════════════════════════════════════════
   HANDLER PAGINA RADICE — ROOT PAGE HANDLER
   ═══════════════════════════════════════════════════════════════════ */
// HEAD/GET per /cardrender.js e /i18n.js: il browser riceve — HEAD/GET for /cardrender.js and /i18n.js: the browser receives
// Cache-Control: no-cache così non riusa mai copie vecchie dopo un — Cache-Control: no-cache so it never reuses old copies after a
// aggiornamento firmware (era causa di pagine/JS stantii). — firmware update (it was the cause of stale pages/JS).
static void no_cache() { server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate"); }

static void handle_i18n_js() {
    no_cache();
    server.send_P(200, "application/javascript", I18N_JS);
}

static void handle_cardrender_js() {
    no_cache();
    server.send_P(200, "application/javascript", CARDRENDER_JS);
}

static void handle_root() {
    no_cache();
    if (captive_mode)
        server.send_P(200, "text/html", WEB_SETUP);
else
        server.send_P(200, "text/html", WEB_IDE);
}

/* ═══════════════════════════════════════════════════════════════════
   HANDLER PAGINA GESTIONE — MANAGEMENT PAGE HANDLER
   ═══════════════════════════════════════════════════════════════════ */
static bool god_mode_enabled(void);

static void handle_manage() {
    no_cache();
    String html = WEB_MANAGE;
    if (god_mode_enabled()) {
        html.replace("<!--GOD-->", "<a href=\"/wolf\" class=\"nav-link\" style=\"color:#ffd700;border-color:#ffd700\" title=\"Mr. Wolf\">GOD!</a>");
    } else {
        html.replace("<!--GOD-->", "");
    }
    server.send(200, "text/html", html);
}

// God mode: true solo se /god_mode.txt esiste su SPIFFS e contiene la — God mode: true only if /god_mode.txt exists on SPIFFS and contains the
// frase chiave. Senza il file la pagina Mr. Wolf risponde 404. — key phrase. Without the file the Mr. Wolf page answers 404.
static bool god_mode_enabled() {
    if (!SPIFFS.exists("/god_mode.txt")) return false;
    File f = SPIFFS.open("/god_mode.txt", "r");
    if (!f) return false;
    String content = f.readString();
    f.close();
    return content.indexOf("ora faccio quello che voglio") >= 0;
}

static void handle_wolf() {
    no_cache();
    if (!god_mode_enabled()) {
        server.send(404, "text/plain", "Not Found");
        return;
    }
    server.send_P(200, "text/html", WEB_WOLF);
}

/* ═══════════════════════════════════════════════════════════════════
   HANDLER PAGINA OVERLAY TASTIERA — KEYBOARD OVERLAY PAGE HANDLER
   ═══════════════════════════════════════════════════════════════════ */
static void handle_overlays_page() {
    no_cache();
    server.send_P(200, "text/html", WEB_OVERLAYS);
}

/* ═══════════════════════════════════════════════════════════════════
   HANDLER ESISTENTI — EXISTING HANDLERS
   ═══════════════════════════════════════════════════════════════════ */
static void handle_timing_toggle() {
    // Se viene passato ?mult=NN (10..200, percentuale del timing Old), — If ?mult=NN is passed (10..200, percentage of Old timing),
    // regola il moltiplicatore e lo salva in NVS, senza toccare la — it adjusts the multiplier and saves it to NVS, without touching the
    // modalità reale/moderna. — real/modern mode.
    if (server.hasArg("mult")) {
        int pct = server.arg("mult").toInt();
        if (pct < 10) pct = 10;
        if (pct > 200) pct = 200;
        tms1500_set_timing_multiplier(pct / 100.0f);
        if (settings_prefs.begin(NVS_SETTINGS_NS, false)) {
            settings_prefs.putUChar(NVS_KEY_TIMINGPCT, (uint8_t)pct);
            settings_prefs.end();
            Serial.printf("[CFG] Moltiplicatore timing salvato: %d%%\n", pct);
        }
        char buf[64];
        snprintf(buf, sizeof(buf), "{\"ok\":true,\"timing_mult\":%d}", pct);
        send_json(200, buf);
        return;
    }
    bool enable;
    if (server.hasArg("enable")) {
        enable = server.arg("enable").toInt() != 0;
} else {
        enable = !tms1500_get_realistic_timing();
// nessun arg = toggle — no arg = toggle
    }
    tms1500_set_realistic_timing(enable);
    char buf[64];
snprintf(buf, sizeof(buf), "{\"ok\":true,\"realistic_timing\":%s}",
             enable ? "true" : "false");
send_json(200, buf);
}

// Regola la durata di accensione del motore di espulsione scheda: — Adjusts the on-time of the card ejection motor:
// GET/POST /api/eject?ms=NNN (50..3000). Senza argomento restituisce — GET/POST /api/eject?ms=NNN (50..3000). Without argument it returns
// il valore corrente. Salvato in NVS come le altre impostazioni. — the current value. Saved in NVS like the other settings.
static void handle_eject_set() {
    uint16_t ms = rfid_reader_get_eject_ms();
    if (server.hasArg("ms")) {
        int v = server.arg("ms").toInt();
        if (v < 50) v = 50;
        if (v > 3000) v = 3000;
        ms = (uint16_t)v;
        rfid_reader_set_eject_ms(ms);
        if (settings_prefs.begin(NVS_SETTINGS_NS, false)) {
            settings_prefs.putUShort(NVS_KEY_EJECTMS, ms);
            settings_prefs.end();
            Serial.printf("[CFG] Durata espulsione salvata: %u ms\n", (unsigned)ms);
        }
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"ok\":true,\"eject_ms\":%u}", (unsigned)ms);
    send_json(200, buf);
}

// Attiva/disattiva il tracer passo-passo di debug (stampa su Serial ogni — Enables/disables the step-by-step debug tracer (prints to Serial every
// istruzione eseguita da exec_program_step, vedi tms1500_set_trace_steps — instruction executed by exec_program_step, see tms1500_set_trace_steps
// in tms1500.cpp). Stesso pattern di handle_timing_toggle: POST — in tms1500.cpp). Same pattern as handle_timing_toggle: POST
// /api/trace con ?enable=0|1, o senza argomento per fare toggle. — /api/trace with ?enable=0|1, or without argument to toggle.
static void handle_trace_toggle() {
    bool enable;
    if (server.hasArg("enable")) {
        enable = server.arg("enable").toInt() != 0;
    } else {
        enable = !tms1500_get_trace_steps();
// nessun arg = toggle — no arg = toggle
    }
    tms1500_set_trace_steps(enable);
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"ok\":true,\"trace_steps\":%s}",
             enable ? "true" : "false");
    send_json(200, buf);
}

static void handle_status() {
    char buf[1024];
    char disp_str[32];
    tms1500_get_display_string(g_cpu, disp_str, sizeof(disp_str));
const char *angle_mode = (g_cpu->trig_mode == 1) ? "RAD"
                            : (g_cpu->trig_mode == 2) ?
"GRAD"
                            : "DEG";
    const LibraryModule *active_mod = library_get_active();
    uint8_t active_page = tms1500_get_active_lib_page();
    // Nome della scheda magnetica attiva, se ce n'è una — Name of the active magnetic card, if any — explicit bounds check
    // esplicito (active_slot è int8_t, può essere -1 = nessuna scheda, — (active_slot is int8_t, can be -1 = no card,
    // stessa cautela già usata per gli altri accessi a slots[]). — same caution already used for the other accesses to slots[]).
    String active_card_name = "";
    if (g_card->active_slot >= 0 && g_card->active_slot < CARD_SLOT_COUNT &&
        g_card->slots[g_card->active_slot].valid) {
        active_card_name = json_escape(g_card->slots[g_card->active_slot].name);
    }
snprintf(buf, sizeof(buf),
        "{"
        "\"display\":\"%s\","
        "\"flags\":{"
          "\"angle\":\"%s\",\"2nd\":%s,\"inv\":%s,\"lrn\":%s,\"run\":%s,\"fix\":%s,\"err\":%s,"
          "\"op_pending\":%s"
        "},"
        "\"realistic_timing\":%s,"
        "\"trace_steps\":%s,"
        "\"prog_dirty\":%s,"
        "\"cycles\":%llu,"
        "\"prog_len\":%d,"
        "\"heap\":%d,"
        "\"lib_module\":\"%s\","
        "\"lib_page\":%d,"
        "\"active_card_slot\":%d,"
        "\"active_card_name\":\"%s\","
       
 "\"ip\":\"%s\""
        "}",
        disp_str,
        angle_mode,
        tms1500_get_pending_2nd(g_cpu)?"true":"false",
        g_cpu->flags.inv  ?"true":"false",
        g_cpu->flags.lrn  ?"true":"false",
        g_cpu->flags.run  ?"true":"false",
        g_cpu->flags.fix  ?"true":"false",
        g_cpu->flags.error?"true":"false",
        (g_cpu->pending_op != 0)?"true":"false",
        tms1500_get_realistic_timing()?"true":"false",
        tms1500_get_trace_steps()?"true":"false",
 
       tms1500_is_prog_dirty()?"true":"false",
        g_cpu->total_cycles,
        g_cpu->prog_len,
        ESP.getFreeHeap(),
        active_mod ? active_mod->id : "",
        (int)active_page,
        (int)g_card->active_slot,
        active_card_name.c_str(),
        WiFi.localIP().toString().c_str()
    );
send_json(200, buf);
}

static void handle_cards_get() {
    // Dimensionato sul vero worst-case (CARD_SLOT_COUNT slot, ciascuno — Sized for the real worst case (CARD_SLOT_COUNT slots, each one
    // con nome alla lunghezza massima), non più un char[2048] fisso — with a name at maximum length), no longer a fixed char[2048]
    // sullo stack: quel valore era insufficiente per liste con molte — on the stack: that value was insufficient for lists with many
    // schede (vedi bugfix in cardemu_list, cardemu.cpp) — qui aggiungo — cards (see bugfix in cardemu_list, cardemu.cpp) — here I add
    // un secondo livello di difesa, indipendente dal primo. — a second layer of defense, independent from the first.
    int cap = CARD_SLOT_COUNT * (CARD_NAME_LEN + 40) + 8;
    char *buf = (char*)malloc(cap);
    if (!buf) { send_err("out of memory"); return; }
    cardemu_list(g_card, buf, cap);
    send_json(200, buf);
    free(buf);
}

static void handle_card_get() {
    if (!server.hasArg("slot")) { send_err("missing slot"); return;
}
    int slot = server.arg("slot").toInt();
    // BUGFIX SICUREZZA (OOB read, stessa famiglia del bug in — SECURITY BUGFIX (OOB read, same family as the bug in
    // cardemu_delete): mancava del tutto il controllo dei limiti. — cardemu_delete): the bounds check was missing entirely.
    // Con slot fuori range, g_card->slots[slot] legge fuori — With an out-of-range slot, g_card->slots[slot] reads outside
    // dall'array — e il codice sotto usa s->prog_len_a (letto da — the array — and the code below uses s->prog_len_a (read from
    // quella memoria arbitraria) come contatore di un loop che scrive — that arbitrary memory) as the counter of a loop that writes
    // in hex_a[]: un valore garbage grande vorrebbe dire un SECONDO — into hex_a[]: a large garbage value would mean a SECOND
    // overflow a cascata, questa volta in scrittura. Raggiungibile — cascading overflow, this time on write. Reachable
    // senza autenticazione via GET /api/card?slot=99 (o slot=-1). — without authentication via GET /api/card?slot=99 (or slot=-1).
    if (slot < 0 || slot >= CARD_SLOT_COUNT || !g_card->slots[slot].valid) {
        send_err("empty slot"); return;
}

    const CardSlot *s = &g_card->slots[slot];

    // prog_hex: hex grezzo, tenuto per compatibilità con chi consuma — prog_hex: raw hex, kept for compatibility with those who consume
    // già questo campo (round-trip di editing puro). — this field already (pure editing round-trip).
char hex_a[CARD_PROG_BYTES*3+2];
    int pos = 0;
    for (int i = 0; i < s->prog_len_a; i++) {
        snprintf(hex_a+pos, 4, "%02X ", s->prog_a[i]);
pos += 3;
    }
    if (pos > 0) hex_a[pos-1] = 0;
    else hex_a[0] = 0;
// prog_listing: stesso formato "passo | hex | comando" usato da — prog_listing: same "step | hex | command" format used by
    // /api/prog, così caricare una scheda mostra la lista leggibile — /api/prog, so loading a card shows the readable list
    // esattamente come scaricare il programma corrente — prima qui si — exactly like downloading the current program — before, here you
    // vedevano solo i codici esadecimali grezzi. — only saw the raw hexadecimal codes.
String listing;
    listing.reserve((size_t)s->prog_len_a * 20 + 8);
		listing += " Passo | Hex | Comando\r\n";
		listing += " ---------------------\r\n";
for (int i = 0; i < s->prog_len_a; i++) {
        uint8_t codice = s->prog_a[i];
char row[48];		
        snprintf(row, sizeof(row), "  %03d  |  %02X | %s\r\n", i, codice, get_mnemonic_name(codice));
        listing += row;
}

    // JSON: prog_listing incorporata come stringa, con newline ed — JSON: prog_listing embedded as a string, with newlines and
    // eventuali virgolette già presenti nei nomi comando (nessuna qui, — any quotes already present in command names (none here,
    // ma restiamo prudenti) escapate correttamente. — but let's stay cautious) escaped correctly.
String json = "{\"slot\":" + String(slot) +
                  ",\"name\":\"" + json_escape(s->name) +
                  "\",\"steps\":" + String(s->prog_len_a) +
                  ",\"prog_hex\":\"" + String(hex_a) + "\"" +
                  ",\"prog_listing\":\"";
    for (size_t i = 0; i < 
listing.length(); i++) {
        char c = listing[i];
        if (c == '"' || c == '\\') { json += '\\';
json += c; }
        else if (c == '\r') json += "\\r";
else if (c == '\n') json += "\\n";
        else json += c;
}
    json += "\"}";
    send_json(200, json.c_str());
}

static void handle_card_post() {
    if (!server.hasArg("slot")) { send_err("missing slot"); return; }
    int slot = server.arg("slot").toInt();
    // BUG: `server.arg("name")` restituisce una String TEMPORANEA. — BUG: `server.arg("name")` returns a TEMPORARY String.
    // Chiamare .c_str() direttamente su di essa dà un puntatore che — Calling .c_str() directly on it gives a pointer that
    // diventa non valido appena finisce questa istruzione (la String — becomes invalid as soon as this statement ends (the String
    // temporanea viene distrutta a fine espressione) — cardemu_write() — temporary is destroyed at the end of the expression) — cardemu_write()
    // riceve quindi un puntatore già "pendente" (dangling). Il — therefore receives an already "dangling" pointer. The
    // comportamento è indefinito:  — behavior is undefined: 
	//può sembrare funzionare se quella — it may seem to work if that
    // zona di memoria non è ancora stata sovrascritta (es. la prima — memory area has not been overwritten yet (e.g. the first
    // scheda salvata in sessione), e fallire silenziosamente non — card saved in session), and fail silently as soon as
    // appena altre allocazioni (richieste WiFi, altre String) hanno — other allocations (WiFi requests, other Strings) have
    // riusato quell'area — esattamente il motivo per cui la seconda — reused that area — exactly why the second
    // scheda perdeva il nome mentre la prima no. La String va tenuta — card lost its name while the first didn't. The String must be kept
    // in una variabile con vita propria per tutta la funzione. — in a variable with its own lifetime for the whole function.
	String nameStr = server.hasArg("name") ? server.arg("name") : String("Card");
    const char *name = nameStr.c_str();
    if (cardemu_write(g_card, g_cpu, slot, name)) { tms1500_mark_prog_saved();
send_ok(); }
    else send_err("write failed");
}

// Implementazione reale dell'hook "debole" dichiarato in tms1500.h: — Real implementation of the "weak" hook declared in tms1500.h:
// sovrascrive quello no-op di tms1500.cpp perché questo file (compilato — overrides the no-op one in tms1500.cpp because this file (compiled
// nello stesso binario) fornisce una definizione forte dello stesso — into the same binary) provides a strong definition of the same
// simbolo. — symbol.
//Trova il primo slot libero e genera un nome sequenziale — Finds the first free slot and generates a sequential name
// univoco "mc_NNN" (non sovrascrive mai schede esistenti), poi appoggia — unique "mc_NNN" (never overwrites existing cards), then relies
// tutto sullo stesso cardemu_write() già usato dal salvataggio via web — on the same cardemu_write() already used by web saving
// — così le schede salvate dal tasto fisico compaiono/si gestiscono — so cards saved from the physical key appear/are managed
// esattamente come quelle salvate da browser. — exactly like those saved from the browser.
void tms1500_on_physical_write(TMS1500_State *cpu) {
    if (!g_card) return;

    // Lettore NFC presente: il WRITE fisico diventa "arma la scrittura" — — NFC reader present: the physical WRITE becomes "arm the write" —
    // la TI-59 attende l'inserimento della scheda, che verrà scritta — the TI-59 waits for the card insertion, which will be written
    // (slot + tag) e poi espulsa da rfid_reader_handle_insert(). — (slot + tag) and then ejected by rfid_reader_handle_insert().
    if (rfid_reader_enabled()) {
        rfid_reader_arm_write(-1);
        Serial.println("[CARD] WRITE fisico: attendo inserimento scheda NFC...");
        return;
    }

    int slot = -1;
for (int s = 0; s < CARD_SLOT_COUNT; s++) {
        if (!g_card->slots[s].valid) { slot = s;
break; }
    }
    if (slot == -1) {
        Serial.println("[CARD] WRITE fisico: nessuno slot libero (50/50 schede occupate)");
return;
    }

    char name[CARD_NAME_LEN];
    int n = 1;
    bool taken;
do {
        snprintf(name, sizeof(name), "mc_%03d", n);
        taken = false;
for (int s = 0; s < CARD_SLOT_COUNT; s++) {
            if (g_card->slots[s].valid && strcmp(g_card->slots[s].name, name) == 0) {
                taken = true;
break;
            }
        }
        n++;
} while (taken && n < 1000);

    if (cardemu_write(g_card, cpu, (uint8_t)slot, name)) {
        tms1500_mark_prog_saved();
Serial.printf("[CARD] WRITE fisico: salvato slot %d come \"%s\"\n", slot, name);
} else {
        Serial.println("[CARD] WRITE fisico: salvataggio fallito");
}
}

// GET /api/modules — elenca i moduli libreria compilati/registrati e — GET /api/modules — lists the compiled/registered library modules and
// indica quale è attivo (uno solo alla volta, come lo slot fisico — indicates which one is active (only one at a time, like the real physical
// reale). — slot).
//Ogni modulo include l'elenco dei suoi programmi numerati, — Each module includes the list of its numbered programs,
// così l'interfaccia può mostrare "Op 09 nn -> nome programma". — so the UI can show "Op 09 nn -> program name".
static void handle_modules_get() {
    const LibraryModule *active = library_get_active();
    begin_stream(200, "application/json");
    String chunk;
    chunk.reserve(768);
    chunk += "{\"active\":\"";
    chunk += json_escape(active ? active->id : "");
    chunk += "\",\"modules\":[";
    int n = library_module_count();
    for (int i = 0; i < n; i++) {
        const LibraryModule *m = library_module_at(i);
        if (!m) continue;
        if (i > 0) chunk += ",";
        if (chunk.length() > 512) { server.sendContent(chunk); chunk = ""; }
        chunk += "{\"id\":\"";
        chunk += json_escape(m->id);
        chunk += "\",\"name\":\"";
        chunk += json_escape(m->name);
        chunk += "\",\"programs\":[";
        for (int p = 0; p < m->program_count; p++) {
            if (p > 0) chunk += ",";
            if (chunk.length() > 512) { server.sendContent(chunk); chunk = ""; }
            const LibraryProgram *lp = &m->programs[p];
            chunk += "{\"num\":" + String(lp->num) + ",\"title\":\"";
            chunk += json_escape(lp->title);
            chunk += "\"}";
        }
        chunk += "]}";
    }
    chunk += "]}";
    stream_flush(chunk);
}

// POST /api/modules — imposta il modulo attivo (arg "id"; vuoto o — POST /api/modules — sets the active module (arg "id"; empty or
// assente = nessun modulo innestato, come slot vuoto sull'hardware). — absent = no module plugged in, like an empty slot on the hardware).
static void handle_modules_post() {
    String id = server.hasArg("id") ? server.arg("id") : String("");
    if (library_set_active(id.c_str())) {
    
    Serial.printf("[LIB] Modulo attivo: %s\n", id.length() ? id.c_str() : "(nessuno)");
        send_ok();
    } else {
        send_err("modulo sconosciuto");
    }
}

// GET /api/modules/listing?id=ml1 — listato completo di TUTTI i — GET /api/modules/listing?id=ml1 — complete listing of ALL the
// programmi di un modulo (di default quello attivo, se ?id non è — programs of a module (by default the active one, if ?id is not
// specificato), non solo di quello eventualmente già scaricato in — specified), not only the one eventually already downloaded in
// esecuzione. Formato a colonne "PP SSS HH COMANDO": — execution. Column format "PP SSS HH COMMAND":
//   PP  = numero programma (2 cifre, quello da digitare dopo Pgm) —   PP  = program number (2 digits, the one to type after Pgm)
//   SSS = passo all'interno del programma (3  —   SSS = step inside the program (3
//cifre, riparte da 000) — digits, restarts from 000)
//   HH  = codice esadecimale —   HH  = hexadecimal code
// stessa idea del listato passo/hex/comando già usato altrove, con — same idea as the step/hex/command listing used elsewhere, with
// in più la colonna del numero programma dato che qui ce ne sono 25 — additionally the program number column since here there are 25
// concatenati. — concatenated.
//Utile per sfogliare il contenuto del modulo senza — Useful for browsing the module content without
// doverli scaricare uno per uno con Pgm nn solo per vederli. — having to download them one by one with Pgm nn just to see them.
static void handle_modules_listing() {
    const LibraryModule *m = server.hasArg("id")
        ?
library_module_find(server.arg("id").c_str())
        : library_get_active();
    if (!m) { send_err("modulo non trovato o nessuno attivo"); return;
}

    begin_stream(200, "text/plain");
    String chunk;
    chunk.reserve(1024);
    chunk += "; " + String(m->name) + "\r\n";
chunk += "; PP  SSS HH Comando\r\n";
    chunk += "; --------------------\r\n";
for (int p = 0; p < m->program_count; p++) {
        const LibraryProgram *lp = &m->programs[p];
chunk += "; --- " + String(lp->num) + ": " + String(lp->title) + " ---\r\n";
for (uint16_t i = 0; i < lp->len; i++) {
            uint8_t codice = m->rom[lp->addr + i];
char row[48];
            snprintf(row, sizeof(row), "%02d  %03u %02X %s\r\n",
                     lp->num, i, codice, get_mnemonic_name(codice));
chunk += row;
            if (chunk.length() > 1024) { server.sendContent(chunk); chunk = ""; }
        }
    }
    stream_flush(chunk);
}

static void handle_card_delete() {
    if (!server.hasArg("slot")) { send_err("missing slot"); return;
}
    int slot = server.arg("slot").toInt();
    // Difesa in profondità (oltre al bounds check ora presente anche — Defense in depth (besides the bounds check now also present
    // in cardemu_delete()): valida qui l'input grezzo dell'utente, — in cardemu_delete()): validate here the user's raw input,
    // PRIMA che l'int si restringa a uint8_t nella chiamata sotto — BEFORE the int narrows to uint8_t in the call below
    // (dove slot=-1 diventerebbe 255). — (where slot=-1 would become 255).
    if (slot < 0 || slot >= CARD_SLOT_COUNT) { send_err("slot fuori range"); return; }
    if (cardemu_delete(g_card, slot)) send_ok();
    else send_err("delete failed");
}

// GET /api/card/file?slot=N — scarica una scheda come file di testo — GET /api/card/file?slot=N — downloads a card as a text file
// (lo stesso JSON già usato internamente su SPIFFS: nome, passi, — (the same JSON already used internally on SPIFFS: name, steps,
// esadecimale lato A/B — un formato completo e senza perdite, non un — side A/B hex — a complete and lossless format, not a
// nuovo formato da re-implementare). — new format to re-implement).
//Il download vero e proprio lo fa — The actual download is done by
// il JS lato client (downloadFile()), qui basta restituire il testo. — the client-side JS (downloadFile()), here it's enough to return the text.
// ═══════════════════════════════════════════════════════════════════
// SLIDE ESPLICATIVE PROGRAMMI ROM (SVG trasparenti, come le card — ROM PROGRAM EXPLANATORY SLIDES (transparent SVGs, like the cards
// cartacee originali TI-59 sopra la tastiera). Da NON confondere con — original paper TI-59 cards above the keyboard). NOT to be confused with
// le "card" sopra (quelle sono schede MAGNETICHE per cardemu — dati, — the "cards" above (those are MAGNETIC cards for cardemu — data,
// non immagini). Convenzione di storage: un file per programma, — not images). Storage convention: one file per program,
// "/romcard_<id modulo>_<numero programma a 2 cifre>.svg". — "/romcard_<module id>_<2-digit program number>.svg".
// ═══════════════════════════════════════════════════════════════════
static String program_card_path(const char *module_id, int page) {
    char path[64];
    snprintf(path, sizeof(path), "/romcard_%s_%02d.svg", module_id, page);
    return String(path);
}

// GET /api/program_card[?module=ml1&page=1] — se module/page non sono — GET /api/program_card[?module=ml1&page=1] — if module/page are not
// passati, usa il modulo/programma attualmente selezionato sulla ROM — passed, uses the module/program currently selected on the active
// attiva. 404 se non c'e' alcuna slide per quella combinazione. — ROM. 404 if there is no slide for that combination.
static void handle_program_card_get() {
    String module_id;
    int page;
    if (server.hasArg("module") && server.hasArg("page")) {
        module_id = server.arg("module");
        page = server.arg("page").toInt();
    } else {
        const LibraryModule *m = library_get_active();
        if (!m) { server.send(404, "text/plain", "nessun modulo attivo"); return; }
        module_id = m->id;
        page = tms1500_get_active_lib_page();
        if (page == 0) { server.send(404, "text/plain", "nessun programma selezionato"); return; }
    }
    String path = program_card_path(module_id.c_str(), page);
    File f = SPIFFS.open(path, FILE_READ);
    if (!f) { server.send(404, "text/plain", "nessuna slide per questo programma"); return; }
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.streamFile(f, "image/svg+xml");
    f.close();
}

// POST /api/program_card?module=ml1&page=1 — corpo: contenuto SVG — POST /api/program_card?module=ml1&page=1 — body: raw SVG content
// grezzo (stesso pattern "plain body" gia' usato per handle_card_upload). — (same "plain body" pattern already used for handle_card_upload).
static void handle_program_card_post() {
    if (!server.hasArg("module") || !server.hasArg("page")) {
        send_err("missing module/page"); return;
    }
    String module_id = server.arg("module");
    int page = server.arg("page").toInt();
    if (module_id.isEmpty() || page <= 0 || page > 99) { send_err("parametri non validi"); return; }
    String body = server.arg("plain");
    if (body.isEmpty()) { send_err("corpo SVG vuoto"); return; }
    String path = program_card_path(module_id.c_str(), page);
    File f = SPIFFS.open(path, FILE_WRITE);
    if (!f) { send_err("impossibile scrivere su SPIFFS"); return; }
    f.print(body);
    f.close();
    Serial.printf("[CARD] Slide salvata: %s (%u byte)\n", path.c_str(), (unsigned)body.length());
    send_ok();
}



/* ═══════════════════════════════════════════════════════════════════
   OVERLAY TASTIERA (dati testuali, non immagini) — KEYBOARD OVERLAY (textual data, not images)
   Copre lo stesso bisogno delle slide SVG sopra ma solo per la — Covers the same need as the SVG slides above but only for the
   striscia overlay dei tasti A-E/A'-E' (o righe libere tipo ML-01 — overlay strip of keys A-E/A'-E' (or free rows like ML-01
   MASTER LIBRARY DIAGNOSTIC): niente immagini, un unico file di testo — MASTER LIBRARY DIAGNOSTIC): no images, a single text file
   con TUTTE le etichette di TUTTI i moduli/programmi, tenuto in RAM e — with ALL the labels of ALL modules/programs, kept in RAM and
   riscritto per intero ad ogni salvataggio (stesso pattern atomico — fully rewritten on every save (same atomic pattern
   tmp+rename di cardemu_save_persistent() in cardemu.cpp). — tmp+rename of cardemu_save_persistent() in cardemu.cpp).

   Formato riga:  MOD|PROG|TYPE|KEY|TESTO — Line format:  MOD|PROG|TYPE|KEY|TEXT
     MOD  = id modulo libreria, come restituito da /api/modules (es. "ml1") — MOD  = library module id, as returned by /api/modules (e.g. "ml1")
     PROG = numero programma, 2 cifre (es. "01") — PROG = program number, 2 digits (e.g. "01")
     TYPE = "GRID" — slot standard A,B,C,D,E,A',B',C',D',E' — TYPE = "GRID" — standard slots A,B,C,D,E,A',B',C',D',E'
            "FREE" — riga libera non riconducibile alla griglia (caso — "FREE" — free row not traceable to the grid (the ML-01 case)
                     ML-01): KEY è solo l'ordine di stampa (1,2,3...), — ML-01): KEY is only the print order (1,2,3...),
                     TESTO è la riga così com'è, letterale — TEXT is the row as-is, literal
     TESTO = etichetta/riga in UTF-8; non deve contenere '|' né a-capo — TEXT = label/row in UTF-8; must not contain '|' nor newline

   Il frontend (pagina /overlays, o in futuro la mode-bar della — The frontend (/overlays page, or in the future the mode-bar of the
   calcolatrice) decide come disegnare GRID vs FREE; qui il parsing è — calculator) decides how to draw GRID vs FREE; here the parsing is
   identico per entrambi i TYPE, nessuna struct diversa per riga. — identical for both TYPEs, no different struct per row.
   ═══════════════════════════════════════════════════════════════════ */
#define OVERLAY_FILE "/overlays.txt"
#define OVERLAY_TMP  "/overlays.tmp"

static String g_overlays_raw;   // intero contenuto file, cache in RAM — whole file content, cache in RAM

// Carica /overlays.txt in RAM all'avvio (va richiamata da — Loads /overlays.txt into RAM at startup (must be called from
// wifi_server_loop() come lrn_autosave_restore()). File assente non è — wifi_server_loop() like lrn_autosave_restore()). A missing file is not
// un errore: prima esecuzione, nessun overlay ancora definito. — an error: first run, no overlay defined yet.
static void overlays_init() {
    if (!SPIFFS.exists(OVERLAY_FILE)) { g_overlays_raw = ""; return; }
    File f = SPIFFS.open(OVERLAY_FILE, FILE_READ);
    if (!f) { g_overlays_raw = ""; return; }
    g_overlays_raw = f.readString();
    f.close();
    Serial.printf("[OVERLAY] Caricati %u byte da %s\n",
                  (unsigned)g_overlays_raw.length(), OVERLAY_FILE);
}

// Riscrive per intero il file (save-and-replace, come discusso: un — Rewrites the whole file (save-and-replace, as discussed: a
// solo file batte tanti piccoli file per overhead SPIFFS e semplicità — single file beats many small files for SPIFFS overhead and simplicity
// di scrittura atomica). Aggiorna anche la cache RAM. — of atomic writing). Also updates the RAM cache.
static bool overlays_save_raw(const String &text) {
    File f = SPIFFS.open(OVERLAY_TMP, FILE_WRITE);
    if (!f) return false;
    f.print(text);
    f.close();
    SPIFFS.remove(OVERLAY_FILE);
    SPIFFS.rename(OVERLAY_TMP, OVERLAY_FILE);
    g_overlays_raw = text;
    Serial.printf("[OVERLAY] Salvati %u byte in %s\n",
                  (unsigned)text.length(), OVERLAY_FILE);
    return true;
}

// Estrae dalla cache RAM le sole righe di un mod/prog e le converte — Extracts from the RAM cache only the rows of a mod/prog and converts them
// in JSON: [{"type":"GRID","key":"A","attr":"m","text":"..."}, ...]. — to JSON: [{"type":"GRID","key":"A","attr":"m","text":"..."}, ...].
// Formato: MOD|PROG|TYPE|KEY|ATTR|TESTO. Parsing manuale riga per riga. — Format: MOD|PROG|TYPE|KEY|ATTR|TEXT. Manual row-by-row parsing.
static String overlays_json_for(const String &mod, const String &prog) {
    String prefix = mod + "|" + prog + "|";
    String out = "[";
    bool first = true;

    int pos = 0;
    int total = g_overlays_raw.length();
    while (pos < total) {
        int nl = g_overlays_raw.indexOf('\n', pos);
        int end = (nl == -1) ? total : nl;
        String line = g_overlays_raw.substring(pos, end);
        line.trim();
        pos = (nl == -1) ? total : nl + 1;

        if (!line.startsWith(prefix)) continue;
        // resto: "TYPE|KEY|ATTR|TESTO" — remainder: "TYPE|KEY|ATTR|TEXT"
        String rest = line.substring(prefix.length());
        int p1 = rest.indexOf('|');
        if (p1 < 0) continue;
        String type = rest.substring(0, p1);
        String rest2 = rest.substring(p1 + 1);
        int p2 = rest2.indexOf('|');
        if (p2 < 0) continue;
        String key = rest2.substring(0, p2);
        String rest3 = rest2.substring(p2 + 1);
        int p3 = rest3.indexOf('|');
        String attr, text;
        if (p3 < 0) { attr = ""; text = rest3; }
        else        { attr = rest3.substring(0, p3); text = rest3.substring(p3 + 1); }

        if (!first) out += ",";
        first = false;
        out += "{\"type\":\"" + json_escape(type) +
               "\",\"key\":\"" + json_escape(key) +
               "\",\"attr\":\"" + json_escape(attr) +
               "\",\"text\":\"" + json_escape(text) + "\"}";
    }
    out += "]";
    return out;
}

// GET /api/overlays?mod=ml1&prog=01 — righe overlay per quel — GET /api/overlays?mod=ml1&prog=01 — overlay rows for that
// programma, già filtrate e in JSON pronto per il frontend. — program, already filtered and JSON-ready for the frontend.
static void handle_overlays_get() {
    if (!server.hasArg("mod") || !server.hasArg("prog")) {
        send_err("missing mod/prog"); return;
    }
    String json = overlays_json_for(server.arg("mod"), server.arg("prog"));
    send_json(200, json.c_str());
}

// GET /api/overlays/raw — l'intero file grezzo, per l'editor a tutto — GET /api/overlays/raw — the whole raw file, for the full-text
// testo nella pagina /overlays. — editor in the /overlays page.
static void handle_overlays_raw_get() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", g_overlays_raw);
}

// POST /api/overlays — corpo: intero file di testo, sostituisce tutto — POST /api/overlays — body: whole text file, replaces everything
// (stesso pattern "plain body" di handle_card_upload/handle_program_card_post). — (same "plain body" pattern of handle_card_upload/handle_program_card_post).
static void handle_overlays_post() {
    String body = server.arg("plain");
    if (!overlays_save_raw(body)) { send_err("impossibile scrivere su SPIFFS"); return; }
    send_ok();
}

/* ═══════════════════════════════════════════════════════════════════
   POSIZIONI TESTO OVERLAY (GRID_COL_X/GRID_ROW_Y/FREE_ROW_Y) — OVERLAY TEXT POSITIONS (GRID_COL_X/GRID_ROW_Y/FREE_ROW_Y)
   Prima erano solo costanti dentro CARDRENDER_JS: per cambiarle — They used to be constants only inside CARDRENDER_JS: to change them
   bisognava editare il codice e riflashare. Ora sono un piccolo file — you had to edit code and reflash. Now they are a small JSON file
   JSON su SPIFFS, modificabile dalla UI in /overlays — stesso pattern — on SPIFFS, editable from the UI in /overlays — same pattern
   "un file solo, riscritto per intero" già usato per overlays.txt. — "one single file, fully rewritten" already used for overlays.txt.
   Se il file non esiste ancora (prima volta), GET restituisce i — If the file does not exist yet (first time), GET returns the
   default hardcoded in CARDRENDER_JS lato client (nessun errore). — defaults hardcoded in the client-side CARDRENDER_JS (no error).
   ═══════════════════════════════════════════════════════════════════ */
#define POSITIONS_FILE "/overlay_pos.json"
#define POSITIONS_TMP  "/overlay_pos.tmp"

static String g_positions_raw;   // "" = usa i default lato client — "" = use the client-side defaults

static void positions_init() {
    if (!SPIFFS.exists(POSITIONS_FILE)) { g_positions_raw = ""; return; }
    File f = SPIFFS.open(POSITIONS_FILE, FILE_READ);
    if (!f) { g_positions_raw = ""; return; }
    g_positions_raw = f.readString();
    f.close();
    Serial.printf("[POS] Caricate posizioni custom (%u byte) da %s\n",
                  (unsigned)g_positions_raw.length(), POSITIONS_FILE);
}

static bool positions_save_raw(const String &text) {
    File f = SPIFFS.open(POSITIONS_TMP, FILE_WRITE);
    if (!f) return false;
    f.print(text);
    f.close();
    SPIFFS.remove(POSITIONS_FILE);
    SPIFFS.rename(POSITIONS_TMP, POSITIONS_FILE);
    g_positions_raw = text;
    Serial.printf("[POS] Salvate posizioni custom (%u byte) in %s\n",
                  (unsigned)text.length(), POSITIONS_FILE);
    return true;
}

// GET /api/card_positions — JSON grezzo così com'è su SPIFFS. Se non — GET /api/card_positions — raw JSON as-is on SPIFFS. If never
// è mai stato salvato nulla, risponde con un oggetto vuoto: il — anything was saved, it answers with an empty object: the
// frontend (cardrender.js) usa i propri default in quel caso. — frontend (cardrender.js) uses its own defaults in that case.
static void handle_positions_get() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", g_positions_raw.length() ? g_positions_raw : "{}");
}

// POST /api/card_positions — corpo: JSON completo, sostituisce tutto — POST /api/card_positions — body: full JSON, replaces everything
// (stesso pattern "plain body" di handle_overlays_post). — (same "plain body" pattern of handle_overlays_post).
static void handle_positions_post() {
    String body = server.arg("plain");
    if (!positions_save_raw(body)) { send_err("impossibile scrivere su SPIFFS"); return; }
    send_ok();
}

static void handle_card_download() {
    if (!server.hasArg("slot")) { send_err("missing slot"); return;
}
    int slot = server.arg("slot").toInt();
    if (slot < 0 || slot >= CARD_SLOT_COUNT || !g_card->slots[slot].valid) {
        send_err("empty slot");
return;
    }
    // BUGFIX: 4096 byte non bastano per il caso peggiore (nome + — BUGFIX: 4096 bytes are not enough for the worst case (name +
    // prog_a/b + regs tutti al massimo ≈ 5,6 KB) — causava — prog_a/b + regs all at maximum ≈ 5.6 KB) — it caused
    // troncamento silenzioso del JSON scaricato (snprintf si — silent truncation of the downloaded JSON (snprintf
    // autolimita, quindi non è un overflow, ma il file scaricato — self-limits, so it's not an overflow, but the downloaded file
    // risultava JSON invalido/tagliato a metà). Stessa formula cap — ended up invalid/cut in half). Same cap formula
    // già usata in cardemu_persist_slot/cardemu_import_batch. — already used in cardemu_persist_slot/cardemu_import_batch.
    int cap = CARD_NAME_LEN + CARD_PROG_BYTES*4 + CARD_REGS_BYTES*2 + 128;
    char *json = (char*)malloc(cap);
    if (!json) { send_err("out of memory"); return; }
    int jlen = cardemu_save_to_json(g_card, (uint8_t)slot, json, cap);
    if (jlen <= 0 || jlen >= cap) { free(json); send_err("export failed"); return; }
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", json);
    free(json);
}

// POST /api/card/file?slot=N — carica una scheda da un file di testo — POST /api/card/file?slot=N — loads a card from a text file
// (stesso formato di handle_card_download, per lo scambio con un PC: — (same format as handle_card_download, for exchange with a PC:
// scarichi un .txt da uno slot, lo passi ad un altro TI-59 Zombie, lo — you download a .txt from a slot, pass it to another TI-59 Zombie,
// ricarichi in un altro slot lì). — and reload it into another slot there).
// POST /api/card/load?slot=N — carica una scheda già salvata in CPU — POST /api/card/load?slot=N — loads a card already saved into CPU
// (cardemu_load_to_cpu): il pulsante "R" del manager fa solo una GET di — (cardemu_load_to_cpu): the "R" button of the manager only does a
// lettura, che non basta a far comparire l'overlay. Qui invece si carica — read GET, which is not enough to make the overlay appear. Here instead
// davvero il programma + registri in memoria, come farebbe la scheda — the program + registers are really loaded into memory, like the physical
// fisica letta dal lettore. — card read by the reader would.
static void handle_card_load() {
    if (!server.hasArg("slot")) { send_err("missing slot"); return; }
    int slot = server.arg("slot").toInt();
    if (slot < 0 || slot >= CARD_SLOT_COUNT) { send_err("invalid slot"); return; }
    if (!g_card->slots[slot].valid) { send_err("empty slot"); return; }
    if (cardemu_load_to_cpu(g_card, g_cpu, (uint8_t)slot)) send_ok();
    else send_err("load failed");
}

static void handle_card_upload() {
    if (!server.hasArg("slot")) { send_err("missing slot"); return;
}
    int slot = server.arg("slot").toInt();
    if (slot < 0 || slot >= CARD_SLOT_COUNT) { send_err("invalid slot"); return;
}
    String body = server.arg("plain");
    if (body.isEmpty()) { send_err("empty body"); return;
}
    // Oltre ad importarla nello slot, la scheda viene anche caricata in — Besides importing it into the slot, the card is also loaded into
    // CPU (cardemu_load_to_cpu) così l'overlay per la scheda magnetica — CPU (cardemu_load_to_cpu) so the magnetic card overlay
    // compare subito sull'IDE: senza, active_slot resterebbe -1 e — appears right away on the IDE: without it, active_slot would stay -1 and
    // /api/status non riporterebbe active_card_slot/name. — /api/status would not report active_card_slot/name.
    if (cardemu_import_text(g_card, body.c_str(), (uint8_t)slot)) {
        cardemu_load_to_cpu(g_card, g_cpu, (uint8_t)slot);
        send_ok();
    }
    else send_err("import failed (formato non riconosciuto?)");
}

static void handle_keypress() {
    if (!server.hasArg("row") || !server.hasArg("col")) {
        send_err("missing row/col");
return;
    }
    uint8_t row = server.arg("row").toInt();
    uint8_t col = server.arg("col").toInt();
    keyboard_enqueue(g_kbd, row, col);
    send_ok();
}

void handle_prog_get() {
    // Elenco leggibile passo | codice esadecimale | — Readable list step | hexadecimal code |
//nome comando, al posto — command name, instead of
    // dei soli byte esadecimali. — the bare hexadecimal bytes.
//Testo semplice (non HTML): questa — Plain text (not HTML): this
    // risposta finisce nel .value di una <textarea>, quindi eventuali tag — response ends up in the .value of a <textarea>, so any tags
    // comparirebbero come testo letterale invece di essere interpretati. — would appear as literal text instead of being interpreted.
//
    // NOTA: prima il ciclo andava fisso a 50 passi indipendentemente dalla — NOTE: previously the loop ran fixed to 50 steps regardless of the
    // lunghezza reale del programma (cpu->prog_len) — con un programma più — real program length (cpu->prog_len) — with a shorter program
    // corto stampava righe finte "000 | 00 | 0" oltre la fine, con uno più — it printed fake rows "000 | 00 | 0" past the end, with a longer one
    // lungo tagliava il resto. — it cut off the rest.
//Corretto usando g_cpu->prog_len. — Fixed by using g_cpu->prog_len.
    String output;
    output.reserve((size_t)g_cpu->prog_len * 20 + 64);
    output += " Passo | Hex | Comando\r\n";
output += " ---------------------\r\n";

    for (uint16_t i = 0; i < g_cpu->prog_len; i++) {
        uint8_t codice = g_cpu->prog[i];
const char* comando = get_mnemonic_name(codice);
        char row[48];
        snprintf(row, sizeof(row), "  %03u  |  %02X | %s\r\n", i, codice, comando);
output += row;
    }

    server.send(200, "text/plain", output);
}

static void handle_prog_post() {
    String body = server.arg("plain");
    if (body.isEmpty()) { send_err("empty body"); return;
}

    uint8_t prog_data[PROG_SIZE];
    int count = 0;
if (body.indexOf('|') >= 0) {
        // Formato leggibile "passo | hex | comando" (quello ora restituito — Readable "step | hex | command" format (the one now returned
        // da /api/prog GET): prendiamo solo la colonna centrale, riga per — by the /api/prog GET): we take only the central column, line by
        // riga, ignorando intestazioni/commenti che iniziano per ';'. — line, ignoring headers/comments that start with ';'.
int start = 0;
        while (start < (int)body.length() && count < (int)sizeof(prog_data)) {
            int nl = body.indexOf('\n', start);
String line = (nl >= 0) ? body.substring(start, nl) : body.substring(start);
            start = (nl >= 0) ?
nl + 1 : body.length();

            line.trim();
            if (line.length() == 0 || line.startsWith(";")) continue;

            int p1 = line.indexOf('|');
if (p1 < 0) continue;
            int p2 = line.indexOf('|', p1 + 1);
            String hexTok = (p2 >= 0) ?
line.substring(p1 + 1, p2) : line.substring(p1 + 1);
            hexTok.trim();
            if (hexTok.length() < 2) continue;
            prog_data[count++] = (uint8_t)strtol(hexTok.c_str(), nullptr, 16);
}
    } else {
        // Formato originale: byte esadecimali grezzi separati da spazi — Original format: raw hexadecimal bytes separated by spaces
        // (compatibilità con hex incollato a mano o generato altrove). — (compatibility with hex pasted by hand or generated elsewhere).
const char *p = body.c_str();
        while (*p && count < (int)sizeof(prog_data)) {
            while (*p == ' ' || *p == '\n' || *p == '\r') p++;
if (!*p) break;
            char hex[3] = {p[0], p[1], 0};
            prog_data[count++] = strtol(hex, nullptr, 16);
            p += 2;
}
    }

    tms1500_load_prog(g_cpu, prog_data, count);
    send_ok();
}

static void handle_reset() {
    tms1500_reset(g_cpu);
send_ok();
}

static void handle_options() {
    server.sendHeader("Access-Control-Allow-Origin",  "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET,POST,DELETE,OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(204);
}

/* ═══════════════════════════════════════════════════════════════════
   NUOVI HANDLER WiFi — NEW WIFI HANDLERS
   ═══════════════════════════════════════════════════════════════════ */
static void handle_wifi_scan() {
    int n = WiFi.scanNetworks();
String json = "[";
    for (int i = 0; i < n; i++) {
        if (i) json += ",";
json += "{\"ssid\":\"" + json_escape(WiFi.SSID(i)) + "\",\"rssi\":" + WiFi.RSSI(i) + "}";
    }
    json += "]";
    WiFi.scanDelete();
server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
}

static void handle_wifi_creds_get() {
    String json = "[";
    int cnt = 0;
for (int i = 0; i < MAX_CREDENTIALS; i++) {
        if (!wifi_creds[i].ssid[0]) continue;
if (cnt) json += ",";
        json += "{\"idx\":" + String(i) + ",\"ssid\":\"" + json_escape(wifi_creds[i].ssid) + "\"}";
        cnt++;
}
    json += "]";
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
}

static void handle_wifi_creds_post() {
    String body = server.arg("plain");
    String ssid = json_extract(body, "ssid");
String pass = json_extract(body, "pass");

    if (ssid.length() == 0 || ssid.length() > 31) { send_err("invalid ssid"); return;
}
    if (pass.length() > 63) { send_err("password too long"); return;
}

    int slot = -1;
    for (int i = 0; i < MAX_CREDENTIALS; i++) {
        if (strcmp(wifi_creds[i].ssid, ssid.c_str()) == 0) { slot = i;
break; }
        if (slot == -1 && !wifi_creds[i].ssid[0]) slot = i;
}
    if (slot == -1) { send_err("max 4 credentials reached"); return;
}

    strncpy(wifi_creds[slot].ssid, ssid.c_str(), 31);
    wifi_creds[slot].ssid[31] = 0;
    strncpy(wifi_creds[slot].pass, pass.c_str(), 63);
    wifi_creds[slot].pass[63] = 0;

    wifi_cred_count = 0;
for (int i = 0; i < MAX_CREDENTIALS; i++) if (wifi_creds[i].ssid[0]) wifi_cred_count++;
    wifi_save_creds();
if (wifi_try_connect(wifi_creds[slot].ssid, wifi_creds[slot].pass, 20000)) {
        send_ok();
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP.restart();
} else {
        send_err("connection failed, saved anyway");
}
}

static void handle_wifi_creds_delete() {
    if (!server.hasArg("idx")) { send_err("missing idx"); return;
}
    int idx = server.arg("idx").toInt();
    if (idx < 0 || idx >= MAX_CREDENTIALS) { send_err("bad idx"); return;
}
    memset(wifi_creds[idx].ssid, 0, sizeof(wifi_creds[idx].ssid));
    memset(wifi_creds[idx].pass, 0, sizeof(wifi_creds[idx].pass));
    wifi_cred_count = 0;
for (int i = 0; i < MAX_CREDENTIALS; i++) if (wifi_creds[i].ssid[0]) wifi_cred_count++;
    wifi_save_creds();
    send_ok();
}

static void handle_wifi_connect() {
    if (wifi_try_stored_creds()) {
        send_ok();
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP.restart();
    } else {
        send_err("no known network found");
    }
}

/* ═══════════════════════════════════════════════════════════════════
   FILE CREDENZIALI (/api/wifi/file) — CREDENTIALS FILE (/api/wifi/file)
   ═══════════════════════════════════════════════════════════════════ */
// GET: restituisce il file /wifi.json così com'è (da salvare/editarre).
// Nota: contiene le password in chiaro, è per il proprietario del device.
// GET: returns the /wifi.json file as-is (to save/edit).
// Note: it contains plaintext passwords, it is for the device owner.
static void handle_wifi_file_get() {
    server.sendHeader("Content-Disposition", "attachment; filename=\"wifi.json\"");
    if (SPIFFS.exists(WIFI_CREDS_FILE)) {
        File f = SPIFFS.open(WIFI_CREDS_FILE, FILE_READ);
        server.streamFile(f, "application/json");
        f.close();
    } else {
        server.send(200, "application/json", "[]");
    }
}

// POST: riceve il JSON completo delle credenziali (array {ssid,pass}),
// valida, sostituisce l'elenco in RAM e riscrive il file. Se poi trova
// una rete tra quelle nuove, si connette e riavvia; altrimenti rimane
// salvato per l'avvio successivo.
// POST: receives the full credentials JSON (array {ssid,pass}), validates,
// replaces the in-RAM list and rewrites the file. If one of the new
// networks is reachable, connects and reboots; otherwise the list stays
// saved for the next boot.
static void handle_wifi_file_post() {
    String body = server.arg("plain");
    if (body.length() == 0) { send_err("empty body"); return; }
    if (body.indexOf('{') < 0 || body.indexOf("\"ssid\"") < 0) {
        send_err("not a wifi.json file");
        return;
    }
    int n = wifi_parse_creds_json(body);
    if (n == 0) { send_err("no valid credentials in file"); return; }
    if (!wifi_save_creds()) { send_err("write failed"); return; }
    if (wifi_try_stored_creds()) {
        send_ok();
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP.restart();
    } else {
        send_ok();
    }
}

/* ═══════════════════════════════════════════════════════════════════
   HANDLER DOWNLOAD FILE SPIFFS — SPIFFS FILE DOWNLOAD HANDLERS
   ═══════════════════════════════════════════════════════════════════ */
static void handle_download_print() {
    if (SPIFFS.exists("/print.txt")) {
        File f = SPIFFS.open("/print.txt", FILE_READ);
server.streamFile(f, "text/plain");
        f.close();
    } else {
        server.send(404, "text/plain", "No print file");
}
}

static void handle_download_listing() {
    if (SPIFFS.exists("/listing.txt")) {
        File f = SPIFFS.open("/listing.txt", FILE_READ);
server.streamFile(f, "text/plain");
        f.close();
    } else {
        server.send(404, "text/plain", "No listing file");
}
}

static void handle_download_progs() {
    String json = "[";
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
bool first = true;
    while (file) {
        String name = file.name();
if (name.endsWith(".prg")) {
            if (!first) json += ",";
first = false;
            json += "{\"name\":\"" + json_escape(name) + "\",\"size\":" + file.size() + "}";
}
        file = root.openNextFile();
    }
    json += "]";
    send_json(200, json.c_str());
}

static void handle_download_prog() {
    if (!server.hasArg("file")) { send_err("missing file"); return;
}
    String name = server.arg("file");
    if (!name.startsWith("/")) name = "/" + name;
if (!SPIFFS.exists(name)) { send_err("file not found"); return; }
    File f = SPIFFS.open(name, FILE_READ);
    server.streamFile(f, "application/octet-stream");
    f.close();
}

/* ═══════════════════════════════════════════════════════════════════
   HANDLER INFORMAZIONI SISTEMA — SYSTEM INFORMATION HANDLERS
   ═══════════════════════════════════════════════════════════════════ */
static void handle_sysinfo() {
    // Conteggio slot popolati — Count of populated slots
    int filled = 0;
    for (int i = 0; i < CARD_SLOT_COUNT; i++) {
        if (g_card && g_card->slots[i].valid) filled++;
    }

    // Conteggio file .prg su SPIFFS — Count of .prg files on SPIFFS
    int prg_count = 0;
    File root = SPIFFS.open("/");
    if (root) {
        File f = root.openNextFile();
        while (f) {
            String name = f.name();
            if (name.endsWith(".prg")) prg_count++;
            f = root.openNextFile();
        }
    }

    // Informazioni SPIFFS — SPIFFS information
    size_t spiffs_total = SPIFFS.totalBytes();
    size_t spiffs_used  = SPIFFS.usedBytes();

    char buf[640];
    uint32_t heap_total = ESP.getHeapSize();
    uint32_t heap_free  = ESP.getFreeHeap();
    snprintf(buf, sizeof(buf),
        "{"
        "\"heap_total\":%u,"
        "\"heap_free\":%u,"
        "\"heap_min\":%u,"
        "\"spiffs_total\":%u,"
        "\"spiffs_used\":%u,"
        "\"slots_filled\":%d,"
        "\"slots_total\":%d,"
        "\"prg_files\":%d,"
        "\"uptime_ms\":%lu,"
        "\"wifi_rssi\":%d,"
        "\"wifi_ip\":\"%s\","
        "\"cycles\":%llu,"
        "\"prog_len\":%d,"
        "\"timing_mult\":%d,"
        "\"realistic_timing\":%s,"
        "\"max_speed_pct\":%u,"
        "\"eject_ms\":%u"
        "}",
        heap_total,
        heap_free,
        ESP.getMinFreeHeap(),
        (unsigned)spiffs_total,
        (unsigned)spiffs_used,
        filled,
        CARD_SLOT_COUNT,
        prg_count,
        millis(),
        WiFi.RSSI(),
        WiFi.localIP().toString().c_str(),
        g_cpu ? g_cpu->total_cycles : 0ULL,
        g_cpu ? g_cpu->prog_len : 0,
        (int)(tms1500_get_timing_multiplier() * 100.0f + 0.5f),
        tms1500_get_realistic_timing() ? "true" : "false",
        tms1500_get_max_speed_pct(),
        rfid_reader_get_eject_ms()
    );
    send_json(200, buf);
}

/* ═══════════════════════════════════════════════════════════════════
   FILE SYSTEM — lista, upload, delete — FILE SYSTEM — list, upload, delete
   ═══════════════════════════════════════════════════════════════════ */
static void handle_fs_list() {
    String json = "{\"files\":[";
    File root = SPIFFS.open("/");
    if (root) {
        File f = root.openNextFile();
        bool first = true;
        while (f) {
            // Normalizza con slash iniziale (f.name() su ESP32 spesso — Normalizes with a leading slash (f.name() on ESP32 often
            // lo omette) — altrimenti il "path" che il frontend — omits it) — otherwise the "path" that the frontend
            // rimanda indietro per la cancellazione non combacia con — sends back for deletion doesn't match
            // quello che SPIFFS.exists()/SPIFFS.remove() si aspettano — what SPIFFS.exists()/SPIFFS.remove() expect
            // e la cancellazione fallisce silenziosamente. — and deletion fails silently.
            String nm = String(f.name());
            if (!nm.startsWith("/")) nm = "/" + nm;
            if (!first) json += ",";
            json += "{\"name\":\"" + nm + "\",\"size\":" + String(f.size()) + "}";
            first = false;
            f = root.openNextFile();
        }
        root.close();
    }
    json += "]}";
    send_json(200, json.c_str());
}

// Upload FILE in STREAMING (multipart/form-data). L'handler HTTP — FILE upload in STREAMING (multipart/form-data). The default HTTP
// predefinito bufferizza l'intero body in RAM (malloc(Content-Length) — handler buffers the whole body in RAM (malloc(Content-Length)
// + String(plainBuf)): con un SVG da 20 KB servivano ~40 KB di heap — + String(plainBuf)): with a 20 KB SVG you needed ~40 KB of free heap
// liberi e l'upload falliva. Qui il file arriva a pezzi da — and the upload failed. Here the file arrives in pieces of
// HTTP_UPLOAD_BUFLEN byte (1436) scritti direttamente su SPIFFS — — HTTP_UPLOAD_BUFLEN bytes (1436) written directly to SPIFFS —
// nessun buffer gigante, funziona con file di qualunque dimensione. — no giant buffer, works with files of any size.
// Il nome file arriva nel multipart (Content-Disposition filename), — The file name arrives in the multipart (Content-Disposition filename),
// non più nel query string. Pattern standard del WebServer ESP32: — no longer in the query string. Standard ESP32 WebServer pattern:
// on(uri, HTTP_POST, risposta, progresso). — on(uri, HTTP_POST, response, progress).
static File fs_upload_file;
static String fs_upload_name;

static void handle_fs_upload() {   // risposta, chiamata a upload terminato — response, called when upload finished
    if (!fs_upload_name.length()) { send_err("missing file"); return; }
    if (!fs_upload_file) { fs_upload_name = ""; send_err("write error"); return; }
    fs_upload_file.close();
    String ok = "{\"ok\":true,\"file\":\"" + json_escape(fs_upload_name) + "\"}";
    fs_upload_name = "";
    send_json(200, ok.c_str());
}

static void handle_fs_upload_progress() {
    HTTPUpload &u = server.upload();
    if (u.status == UPLOAD_FILE_START) {
        String name = u.filename;
        int slash = name.lastIndexOf('/');
        if (slash >= 0) name = name.substring(slash + 1);
        if (name.length() == 0 ||
            (!name.endsWith(".svg") && !name.endsWith(".txt") && !name.endsWith(".json"))) {
            fs_upload_name = "";   // estensione non consentita: ignora — disallowed extension: ignore
            return;
        }
        fs_upload_name = "/" + name;
        fs_upload_file = SPIFFS.open(fs_upload_name, FILE_WRITE);
        if (!fs_upload_file) fs_upload_name = "";
    } else if (u.status == UPLOAD_FILE_WRITE) {
        if (fs_upload_file && u.currentSize) fs_upload_file.write(u.buf, u.currentSize);
    } else if (u.status == UPLOAD_FILE_END) {
        // file lasciato aperto: lo chiude handle_fs_upload dopo la risposta — file left open: handle_fs_upload closes it after the response
    } else if (u.status == UPLOAD_FILE_ABORTED) {
        if (fs_upload_file) fs_upload_file.close();
        if (fs_upload_name.length()) SPIFFS.remove(fs_upload_name);
        fs_upload_name = "";
    }
}

static void handle_fs_delete() {
    if (!server.hasArg("path")) { send_err("missing path"); return; }
    String path = server.arg("path");
    if (!SPIFFS.exists(path)) { send_err("not found"); return; }
    SPIFFS.remove(path);
    send_json(200, "{\"ok\":true}");
}

/* ═══════════════════════════════════════════════════════════════════
   REGISTRAZIONE ROUTE — ROUTE REGISTRATION
   ═══════════════════════════════════════════════════════════════════ */

// ─── RFID / NFC (schede magnetiche virtuali) — RFID / NFC (virtual magnetic cards) ─────
// POST /api/rfid/arm — arma la scrittura: la prossima scheda — POST /api/rfid/arm — arms the write: the next card
// inserita riceve il programma corrente. Arg opzionale "slot" — inserted receives the current program. Optional arg "slot"
// forza uno slot specifico (default: primo libero, come il — forces a specific slot (default: first free, like the
// WRITE da tastiera). — WRITE from the keypad).
static void handle_rfid_arm() {
    if (!rfid_reader_enabled()) { send_err("RFID disabled"); return; }
    int slot = server.hasArg("slot") ? server.arg("slot").toInt() : -1;
    if (slot < -1 || slot >= CARD_SLOT_COUNT) { send_err("bad slot"); return; }
    rfid_reader_arm_write(slot);
    send_ok();
}

// GET /api/rfid/read — sonda il lettore (tag appoggiato?): — GET /api/rfid/read — probes the reader (tag placed?):
// riporta uid + slot letto dal tag, senza toccare la CPU. — reports uid + slot read from the tag, without touching the CPU.
static void handle_rfid_probe() {
    if (!rfid_reader_enabled()) { send_err("RFID disabled"); return; }
    uint8_t uid[7]; uint8_t len = 0; int slot = -1;
    bool got = rfid_reader_probe(uid, &len, &slot);
    if (!got) { send_json(200, "{\"ok\":true,\"tag\":false}"); return; }
    char json[160];
    int pos = snprintf(json, sizeof(json), "{\"ok\":true,\"tag\":true,\"uid\":\"");
    for (int i = 0; i < len; i++)
        pos += snprintf(json + pos, sizeof(json) - pos, "%02x", uid[i]);
    pos += snprintf(json + pos, sizeof(json) - pos, "\",\"slot\":%d}", slot);
    send_json(200, json);
}

// GET /api/rfid/map — associazioni UID -> slot (fallback se il — GET /api/rfid/map — UID -> slot mappings (fallback if the
// tag non ha lo slot in pagina 4). — tag has no slot in page 4).
static void handle_rfid_map_get() {
    char json[2048];
    rfid_map_list(json, sizeof(json));
    send_json(200, json);
}

// POST /api/rfid/map?uid=04A3B2C1D5E6&slot=N — associa/aggiorna — POST /api/rfid/map?uid=04A3B2C1D5E6&slot=N — associates/updates
// (il client può passare l'uid letto con /api/rfid/read). — (the client can pass the uid read with /api/rfid/read).
static void handle_rfid_map_post() {
    if (!server.hasArg("uid") || !server.hasArg("slot")) { send_err("missing uid/slot"); return; }
    String u = server.arg("uid");
    int slot = server.arg("slot").toInt();
    if (slot < 0 || slot >= CARD_SLOT_COUNT) { send_err("bad slot"); return; }
    uint8_t uid[7]; int n = 0; int nib = 0;
    for (unsigned i = 0; i < u.length() && n < 7; i++) {
        char c = u[i];
        int h = (c >= '0' && c <= '9') ? c - '0' :
                (c >= 'a' && c <= 'f') ? c - 'a' + 10 :
                (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
        if (h < 0) continue;
        if ((nib & 1) == 0) uid[n] = (uint8_t)(h << 4);
        else { uid[n] |= (uint8_t)h; n++; }
        nib++;
    }
    if (n == 0) { send_err("bad uid"); return; }
    if (rfid_map_set_uid(uid, (uint8_t)n, slot)) send_ok();
    else send_err("map full or save failed");
}

static void handle_fs_format() {
    // Formattazione SPIFFS on-demand. Non farla mai automaticamente nel — On-demand SPIFFS formatting. Never do it automatically at
    // boot: erase dell'intera partizione = 10-20s senza servire la rete; — boot: erasing the whole partition = 10-20s without serving the network;
    // il browser può andare in timeout, è normale — poi si ricarica. — the browser may time out, that's normal — then you reload.
    Serial.println("[FS] Formattazione SPIFFS in corso (10-20s)...");
    bool ok = SPIFFS.format();
    if (ok) ok = SPIFFS.begin(false);
    if (ok) {
        Serial.println("[FS] Formattazione completata.");
        if (g_card) cardemu_init(g_card);
        send_ok();
    } else {
        Serial.println("[FS] Formattazione FALLITA.");
        send_err("format failed");
    }
}

static void setup_routes() {
    server.on("/",              HTTP_GET,    handle_root);
    server.on("/manage",        HTTP_GET,    handle_manage);
    server.on("/wolf",          HTTP_GET,    handle_wolf);
    server.on("/overlays",      HTTP_GET,    handle_overlays_page);
    server.on("/api/overlays",     HTTP_GET,  handle_overlays_get);
    server.on("/api/overlays",     HTTP_POST, handle_overlays_post);
    server.on("/api/overlays/raw", HTTP_GET,  handle_overlays_raw_get);
    server.on("/api/card_positions", HTTP_GET,  handle_positions_get);
    server.on("/api/card_positions", HTTP_POST, handle_positions_post);
server.on("/api/status",    HTTP_GET,    handle_status);
    server.on("/api/timing",    HTTP_POST,   handle_timing_toggle);
    server.on("/api/eject",     HTTP_GET,    handle_eject_set);
    server.on("/api/trace",     HTTP_POST,   handle_trace_toggle);
server.on("/api/cards",     HTTP_GET,    handle_cards_get);
    server.on("/api/card",      HTTP_GET,    handle_card_get);
    server.on("/api/card",      HTTP_POST,   handle_card_post);
    server.on("/api/card/load", HTTP_POST,   handle_card_load);
    server.on("/api/card/delete", HTTP_GET,    handle_card_delete);
    server.on("/api/card",      HTTP_DELETE, handle_card_delete);
server.on("/api/card/file", HTTP_GET,    handle_card_download);
    server.on("/api/card/file", HTTP_POST,   handle_card_upload);
    server.on("/api/program_card", HTTP_GET,  handle_program_card_get);
    server.on("/api/program_card", HTTP_POST, handle_program_card_post);
    server.on("/api/rfid/arm",  HTTP_GET,  handle_rfid_arm);
    server.on("/api/rfid/read", HTTP_GET,  handle_rfid_probe);
    server.on("/api/rfid/map",  HTTP_GET,  handle_rfid_map_get);
    server.on("/api/rfid/map",  HTTP_POST, handle_rfid_map_post);
    server.on("/i18n.js",       HTTP_GET,    handle_i18n_js);
    server.on("/cardrender.js", HTTP_GET,    handle_cardrender_js);
    // Template SVG universali (linee divisorie), condivisi da tutti i — Universal SVG templates (divider lines), shared by all the
    // moduli libreria — non più legati a ml1. free = righe libere — library modules — no longer tied to ml1. free = free rows
    // stile ML-01, grid = griglia A-E stile ML-02/03/04. — style ML-01, grid = A-E grid style ML-02/03/04.
    server.on("/card_free.svg", HTTP_GET, [](){
        File f = SPIFFS.open("/card_free.svg", FILE_READ);
        if (!f) { send_err("not found"); return; }
        String svg = f.readString(); f.close();
        server.sendHeader("Cache-Control","no-store");
        server.send(200, "image/svg+xml", svg);
    });
    server.on("/card_grid.svg", HTTP_GET, [](){
        File f = SPIFFS.open("/card_grid.svg", FILE_READ);
        if (!f) { send_err("not found"); return; }
        String svg = f.readString(); f.close();
        server.sendHeader("Cache-Control","no-store");
        server.send(200, "image/svg+xml", svg);
    });
    // Template SVG dedicato alle schede magnetiche (mod="card" nel — SVG template dedicated to magnetic cards (mod="card" in the
    // formato overlay) — la base per il nome della scheda + eventuali — overlay format) — the base for the card name + any
    // righe overlay. Separato da quelli delle ROM libreria sopra, così — overlay rows. Separate from the library ROM ones above, so
    // un domani carichi un design diverso per le une senza toccare le — someday you can load a different design for the former without touching the
    // altre. Fino a quando questi due file non vengono caricati, il — latter. Until these two files are uploaded, the
    // fetch dà semplicemente 404 (gestito lato client in — fetch simply gives 404 (handled client-side in
    // checkSvgTemplates()) e lo sfondo resta vuoto ma il testo overlay — checkSvgTemplates()) and the background stays empty but the overlay text
    // si vede comunque. — is still visible.
    server.on("/magcard_free.svg", HTTP_GET, [](){
        File f = SPIFFS.open("/magcard_free.svg", FILE_READ);
        if (!f) { send_err("not found"); return; }
        String svg = f.readString(); f.close();
        server.sendHeader("Cache-Control","no-store");
        server.send(200, "image/svg+xml", svg);
    });
    server.on("/magcard_grid.svg", HTTP_GET, [](){
        File f = SPIFFS.open("/magcard_grid.svg", FILE_READ);
        if (!f) { send_err("not found"); return; }
        String svg = f.readString(); f.close();
        server.sendHeader("Cache-Control","no-store");
        server.send(200, "image/svg+xml", svg);
    });
    server.on("/api/fs",          HTTP_GET,    handle_fs_list);
    server.on("/api/fs/upload",   HTTP_POST,   handle_fs_upload, handle_fs_upload_progress);
    server.on("/api/fs/delete",   HTTP_POST,   handle_fs_delete);
    server.on("/api/fs/format",   HTTP_POST,   handle_fs_format);
    server.on("/api/modules",   HTTP_GET,    handle_modules_get);
server.on("/api/modules",   HTTP_POST,   handle_modules_post);
    server.on("/api/modules/listing", HTTP_GET, handle_modules_listing);
    server.on("/api/keypress",  HTTP_POST,   handle_keypress);
server.on("/api/prog",      HTTP_GET,    handle_prog_get);
server.on("/api/prog",      HTTP_POST,   handle_prog_post);
    server.on("/api/reset",     HTTP_POST,   handle_reset);
server.on("/api/wifi/scan",    HTTP_GET,    handle_wifi_scan);
    server.on("/api/wifi/creds",   HTTP_GET,    handle_wifi_creds_get);
server.on("/api/wifi/creds",   HTTP_POST,   handle_wifi_creds_post);
    server.on("/api/wifi/creds",   HTTP_DELETE, handle_wifi_creds_delete);
    server.on("/api/wifi/connect", HTTP_POST,   handle_wifi_connect);
    server.on("/api/wifi/file",    HTTP_GET,    handle_wifi_file_get);
    server.on("/api/wifi/file",    HTTP_POST,   handle_wifi_file_post);
    server.on("/api/print",     HTTP_GET, handle_download_print);
    server.on("/api/listing",   HTTP_GET, handle_download_listing);
    server.on("/api/progs",     HTTP_GET, handle_download_progs);
server.on("/api/progfile",  HTTP_GET, handle_download_prog);
    server.on("/api/sysinfo",   HTTP_GET, handle_sysinfo);

    register_module_routes();
    register_regs_routes();
    server.onNotFound([](){
        if (server.method() == HTTP_OPTIONS) { handle_options(); return; }
        if (captive_mode) { server.send_P(200, "text/html", WEB_SETUP); return; }
        // Template SVG generici: serve QUALSIASI .svg presente su SPIFFS — Generic SVG templates: serves ANY .svg present on SPIFFS
        // (top.svg, base.svg, o qualunque file scelto come strato dal — (top.svg, base.svg, or any file chosen as the layer from the
        // pannello posizioni), così l'anteprima disegna davvero gli strati — positions panel), so the preview really draws the configured layers
        // configurati invece di restituire 404. Limite all'estensione .svg — instead of returning 404. Limit to the .svg extension
        // per non esporre binari/json (state.bin, schede...). — so as not to expose binaries/json (state.bin, cards...).
        String uri = server.uri();
        if (uri.endsWith(".svg")) {
            File f = SPIFFS.open(uri, FILE_READ);
            if (f) {
                String svg = f.readString(); f.close();
                server.sendHeader("Cache-Control", "no-store");
                server.send(200, "image/svg+xml", svg);
                return;
            }
        }
        send_err("not found");
    });
}

/* ═══════════════════════════════════════════════════════════════════
   LOOP PRINCIPALE WiFi (task FreeRTOS) — MAIN WiFi LOOP (FreeRTOS task)
   ═══════════════════════════════════════════════════════════════════ */
// Controllo periodico non bloccante (rate-limited via millis()) dello — Non-blocking periodic check (rate-limited via millis()) of the
// stato WiFi, con intervento manuale se la riconnessione automatica — WiFi status, with manual intervention if the driver's automatic reconnection
// del driver non basta. — is not enough.
//Va chiamata ad ogni giro del loop principale: — Must be called on every main loop iteration:
// esce subito se non è ancora passato WIFI_STATUS_POLL_MS. — it returns immediately if WIFI_STATUS_POLL_MS hasn't passed yet.
static void wifi_watchdog_tick() {
    unsigned long now = millis();
    if (now - wifi_last_poll_ms < WIFI_STATUS_POLL_MS) return;
wifi_last_poll_ms = now;

    if (!captive_mode) {
        // Modalità normale STA/IDE: dovremmo essere connessi. — Normal STA/IDE mode: we should be connected.
if (WiFi.status() == WL_CONNECTED) {
            wifi_disconnected_since_ms = 0;
digitalWrite(PIN_LED_STATUS, HIGH);
            return;
        }

        // Persa la connessione: segnala subito sul LED, anche se il — Connection lost: signal it right away on the LED, even if the
        // riconnect automatico del driver potrebbe farcela da solo. — driver's automatic reconnect might handle it alone.
digitalWrite(PIN_LED_STATUS, LOW);
        if (wifi_disconnected_since_ms == 0) wifi_disconnected_since_ms = now;

        bool grace_expired = (now - wifi_disconnected_since_ms) > WIFI_GRACE_BEFORE_RETRY_MS;
bool cooldown_ok   = (now - wifi_last_retry_ms) > WIFI_RETRY_COOLDOWN_MS;
if (grace_expired && cooldown_ok) {
            wifi_last_retry_ms = now;
Serial.println("[WiFi] Connessione persa, provo a riconnettermi alle reti note...");
            if (wifi_try_stored_creds()) {
                Serial.println("[WiFi] Riconnesso.");
wifi_disconnected_since_ms = 0;
                digitalWrite(PIN_LED_STATUS, HIGH);
            } else {
                Serial.println("[WiFi] Riconnessione fallita, apro AP di configurazione.");
wifi_start_ap();
                captive_mode = true;
                digitalWrite(PIN_LED_STATUS, LOW);
            }
        }
    } else {
        // Modalità AP di setup: ogni tanto ricontrolla se una rete — Setup AP mode: every now and then re-checks if a known network
        // nota è tornata disponibile, per tornare in modalità IDE. — is available again, to return to IDE mode.
bool cooldown_ok = (now - wifi_last_retry_ms) > WIFI_RETRY_COOLDOWN_MS;
        if (wifi_cred_count > 0 && cooldown_ok) {
            wifi_last_retry_ms = now;
Serial.println("[WiFi] In AP di setup, ricontrollo reti note...");
            if (wifi_try_stored_creds()) {
                Serial.println("[WiFi] Rete nota ritrovata, torno in modalità IDE.");
dnsServer.stop();
                captive_mode = false;
                wifi_disconnected_since_ms = 0;
                digitalWrite(PIN_LED_STATUS, HIGH);
            } else {
                // wifi_try_stored_creds ha messo la radio in WIFI_STA — wifi_try_stored_creds put the radio in WIFI_STA
                // per la scansione: ripristina l'AP di setup. — for the scan: restore the setup AP.
wifi_start_ap();
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// Persistenza automatica memoria LRN (programma utente) attraverso — Automatic persistence of the LRN memory (user program) across
// spegnimento/riavvio. Sull'hardware originale la RAM e' volatile e — power-off/reboot. On the original hardware the RAM is volatile and
// il programma si perde spegnendo la calcolatrice (eccetto il raro — the program is lost when the calculator is switched off (except the rare
// TI-58C con RAM a basso consumo alimentata costantemente); qui, — TI-58C with low-power RAM constantly powered); here,
// potendo contare su SPIFFS, la manteniamo intatta come richiesto: — being able to rely on SPIFFS, we keep it intact as required:
// viene ripristinata all'avvio e salvata automaticamente ogni volta — it is restored at startup and saved automatically every time
// che risulta "dirty" (modificata dall'ultimo salvataggio), invece — it results "dirty" (modified since the last save), instead
// di affidarsi a un evento di spegnimento pulito che su un device — of relying on a clean shutdown event that on a battery-powered
// alimentato a batteria/interruttore fisico potrebbe non arrivare — device with a physical switch might never arrive
// mai in tempo — il salvataggio periodico e' l'unica strategia — in time — the periodic save is the only reliable
// affidabile in questo scenario. — strategy in this scenario.
// ═══════════════════════════════════════════════════════════════
#define LRN_AUTOSAVE_PATH "/lrn_autosave.prg"
#define LRN_AUTOSAVE_INTERVAL_MS 3000   // non scrivere in flash a ogni tasto — don't write to flash on every keypress

static void lrn_autosave_restore(TMS1500_State *cpu) {
    File f = SPIFFS.open(LRN_AUTOSAVE_PATH, FILE_READ);
    if (!f) {
        Serial.println("[LRN] Nessun autosave trovato, memoria vuota all'avvio.");
        return;
    }
    uint8_t buf[PROG_SIZE];
    size_t n = f.read(buf, sizeof(buf));
    f.close();
    if (n == 0) return;
    tms1500_load_prog(cpu, buf, (uint16_t)n);
    tms1500_mark_prog_saved();   // appena caricato: non e' "dirty" — just loaded: not "dirty"
    Serial.printf("[LRN] Ripristinati %u byte di programma dall'autosave.\n", (unsigned)n);
}

static void lrn_autosave_save(TMS1500_State *cpu) {
    uint8_t buf[PROG_SIZE];
    uint16_t len = 0;
    tms1500_save_prog(cpu, buf, &len);
    File f = SPIFFS.open(LRN_AUTOSAVE_PATH, FILE_WRITE);
    if (!f) {
        Serial.println("[LRN] Impossibile aprire il file di autosave in scrittura");
        return;
    }
    f.write(buf, len);
    f.close();
    tms1500_mark_prog_saved();
    Serial.printf("[LRN] Autosave: salvati %u byte.\n", (unsigned)len);
}

void wifi_server_loop(TMS1500_State *cpu, CardEmuState *card, KeyboardState *kbd) {
    g_cpu  = cpu;
	g_card = card;
    g_kbd  = kbd;
	tms1500_bind_cpu(g_cpu);

    lrn_autosave_restore(g_cpu);   // ripristina il programma prima di tutto il resto — restores the program before everything else
    overlays_init();               // carica /overlays.txt in RAM (cache) — v. sopra — loads /overlays.txt into RAM (cache) — see above
    positions_init();              // carica /overlay_pos.json in RAM (cache) — v. sopra — loads /overlay_pos.json into RAM (cache) — see above

    settings_load_and_apply();

    wifi_load_creds();

    WiFi.persistent(false);
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    delay(100);

    Serial.println("[WiFi] Provo reti memorizzate...");
    bool connected = wifi_try_stored_creds();

    if (connected) {
        Serial.printf("[WiFi] Connesso a %s — IP: %s\n",
                      WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
digitalWrite(PIN_LED_STATUS, HIGH);
        captive_mode = false;
    } else {
        Serial.println("[WiFi] Nessuna rete nota — avvio AP di configurazione");
wifi_start_ap();
        captive_mode = true;
    }

    setup_routes();
    server.begin();
Serial.printf("[WiFi] Server avviato su porta %d (mode=%s)\n",
                  WIFI_PORT, captive_mode ? "AP/setup" : "STA/IDE");
while (1) {
        if (captive_mode) dnsServer.processNextRequest();
        server.handleClient();
        wifi_watchdog_tick();

        static unsigned long last_autosave_check = 0;
        unsigned long now = millis();
        if (now - last_autosave_check > LRN_AUTOSAVE_INTERVAL_MS) {
            last_autosave_check = now;
            if (tms1500_is_prog_dirty()) lrn_autosave_save(g_cpu);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}