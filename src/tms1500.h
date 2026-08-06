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
#pragma once
/*
 * tms1500.h — Emulatore CPU TMC0501/DSCOM/BROM/RAM
 */

#ifndef TMS1500_H
#define TMS1500_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ─── Costanti architetturali — Architectural constants ──────────────────────────────
#define REG_WIDTH     18
#define NUM_REGS      12
#define ROM_SIZE      6144
#define RAM_SIZE      100
#define PROG_SIZE     960
#define STACK_SIZE    8
#define CROM_REGS     16

// Indici registri principali — Main register indices
#define REG_A   0
#define REG_B   1
#define REG_C   2
#define REG_D   3
#define REG_E   4
#define REG_T   5
#define REG_F   6
#define REG_G   7
#define REG_H   8
#define REG_I   9
#define REG_J  10
#define REG_K  11

const char* get_mnemonic_name(uint8_t code);

// Flag di stato CPU — CPU status flags
typedef struct {
    bool carry;
    bool overflow;
    bool neg;
    bool zero;
    bool error;
    bool idle;
    bool sci;
    bool fix;
    bool eng;
    bool deg;
    bool inv;
    bool lrn;
    bool run;
    bool pause;
} CPUFlags;

// Registro a 16 nibble (BCD esteso TI) — 16-nibble register (extended TI BCD)
typedef struct {
    int8_t n[REG_WIDTH];
} BCD_Reg;

// Stato completo CPU — Complete CPU state
typedef struct {
    BCD_Reg reg[NUM_REGS];
    uint16_t  pc;
    
    // --- STACK: espanso per ricordare la posizione in ROM/RAM — expanded to remember position in ROM/RAM ---
    uint16_t  stack[STACK_SIZE];
    bool      stack_in_rom[STACK_SIZE];
    uint16_t  stack_rom_base[STACK_SIZE];
    uint16_t  stack_rom_len[STACK_SIZE];
    uint8_t   stack_pending_op[STACK_SIZE];
    BCD_Reg   stack_operand_x[STACK_SIZE];
    bool      stack_slf[STACK_SIZE];
    uint8_t   sp;
    
    BCD_Reg   ram[RAM_SIZE];
    uint8_t   prog[PROG_SIZE];
    uint16_t  prog_len;
    uint16_t  prog_pc;
    
    CPUFlags  flags;
    uint8_t   fix_digits;
    uint8_t   indirect_idx;
    int8_t    disp_buf[REG_WIDTH];
    bool      disp_dirty;
    uint8_t   active_dline;
    uint8_t   k_lines_in;
    uint64_t  total_cycles;
    uint32_t  calc_cycles;
    uint8_t   pending_op;
    BCD_Reg   operand_x;
    uint8_t   trig_mode;
    uint8_t   crom_slot;
    uint8_t   pending_reg;
    uint8_t   pending_digits;
    bool      pending_2nd;
    bool      stack_lift_enabled;
} TMS1500_State;

typedef struct _KeyboardState KeyboardState;
typedef struct _DisplayState  DisplayState;

// Numero (1-25 tipico) del programma del modulo libreria attivo — Number (typically 1-25) of the program of the active library module
// attualmente selezionato/in esecuzione "as-is" dalla ROM (0 = nessuno). — currently selected/being executed "as-is" from the ROM (0 = none).
// Usato per scegliere quale "slide" esplicativa SVG mostrare nella UI — Used to choose which explanatory SVG "slide" to show in the UI
// web (vedi /api/program_card in wifilink.cpp). — web (see /api/program_card in wifilink.cpp).
uint8_t tms1500_get_active_lib_page(void);

// ─── API pubblica — Public API ─────────────────────────────────────────
void tms1500_bind_cpu(TMS1500_State *cpu);

// Da richiamare esplicitamente da DENTRO l'implementazione già — To be called explicitly from INSIDE the existing
// esistente di library_on_module_changed() (in wifilink.cpp): reagisce — implementation of library_on_module_changed() (in wifilink.cpp): it reacts
// al cambio di modulo libreria attivo interrompendo l'esecuzione in — to a change of the active library module by interrupting the execution in
// corso se dipendeva dalla ROM del modulo precedente (come — progress if it depended on the previous module's ROM (like
// un'estrazione fisica del modulo), e ripulisce scope/etichette — a physical module extraction), and clears the scope/label
// cache. Non e' essa stessa l'hook debole (quel nome e' gia' — cache. It is not itself the weak hook (that name is already
// implementato in wifilink.cpp per la persistenza su NVS) per evitare — implemented in wifilink.cpp for NVS persistence) to avoid
// un "multiple definition" in fase di link. — a "multiple definition" at link time.
void tms1500_on_library_module_changed(const char *id);

void tms1500_init(TMS1500_State *cpu);
void tms1500_step(TMS1500_State *cpu, KeyboardState *kbd, DisplayState *disp);
void tms1500_reset(TMS1500_State *cpu);
void tms1500_keypress(TMS1500_State *cpu, KeyboardState *kbd, uint8_t row, uint8_t col);
void tms1500_load_prog(TMS1500_State *cpu, const uint8_t *data, uint16_t len);

// Listato completo del modulo libreria attivo (tutti i programmi), — Full listing of the active library module (all programs),
// formato "numero_programma passo hex comando". Alloca con malloc: — format "program_number step hex command". Allocates with malloc:
// il chiamante deve fare free(). nullptr se nessun modulo attivo. — the caller must free(). nullptr if no module is active.
char* build_library_listing(size_t *out_len);
void tms1500_save_prog(TMS1500_State *cpu, uint8_t *out, uint16_t *len);

// Hook chiamato quando l'utente preme il tasto fisico WRITE (2nd R/S — Hook called when the user presses the physical WRITE key (2nd R/S
// sulla TI-59 reale). L'implementazione di default in tms1500.cpp è — on the real TI-59). The default implementation in tms1500.cpp is
// "debole" (no-op): un livello esterno con accesso al sottosistema — "weak" (no-op): an external layer with access to the card
// schede (es. wifilink.cpp) può fornire la propria implementazione — subsystem (e.g. wifilink.cpp) can provide its own implementation
// per agganciare il salvataggio reale, senza che il core CPU debba — to hook the real save, without the CPU core having to
// conoscere nulla di SPIFFS/cardemu. — know anything about SPIFFS/cardemu.
void tms1500_on_physical_write(TMS1500_State *cpu);

void tms1500_get_display_string(const TMS1500_State *cpu, char *out, size_t maxlen);

// Modalità timing "occupato" (indicatore C durante sqrt/trig/log/y^x): — "busy" timing mode (C indicator during sqrt/trig/log/y^x):
// true = durata autentica (blocca, come l'hardware reale); — true = authentic duration (blocks, like the real hardware);
// false (default) = calcolo istantaneo, "C" mostrato solo per una — false (default) = instant calculation, "C" shown only for a
// durata minima fissa a scopo tattile. Vedi busy_start() in tms1500.cpp. — fixed minimum duration for tactile feedback. See busy_start() in tms1500.cpp.
void tms1500_set_realistic_timing(bool enable);
bool tms1500_get_realistic_timing(void);

// Moltiplicatore della durata in modalità Old (timing autentico): — Duration multiplier in Old mode (authentic timing):
// 1.0 = identico all'originale, 0.1 = 10x più veloce, 2.0 = 2x più — 1.0 = identical to the original, 0.1 = 10x faster, 2.0 = 2x
// lento. Non tocca la modalità New (istantanea). Range accettato 0.1-2.0. — slower. Does not affect New mode (instant). Accepted range 0.1-2.0.
void tms1500_set_timing_multiplier(float mult);
float tms1500_get_timing_multiplier(void);

// Velocità massima di esecuzione misurata a runtime (in % della — Maximum execution speed measured at runtime (as % of the
// velocità del TI-59 originale; 100 = stesso ritmo del hardware reale). — speed of the original TI-59; 100 = same pace as the real hardware).
// È il soffitto pratico del device a progetto completo: se < 100, — It is the practical ceiling of the fully-populated device: if < 100,
// l'emulatore non può fisicamente raggiungere il timing dell'originale. — the emulator cannot physically reach the original timing.
unsigned int tms1500_get_max_speed_pct(void);

// Tracer passo-passo di debug: se attivo, ogni istruzione eseguita da — Step-by-step debug tracer: if enabled, every instruction executed by
// exec_program_step() viene stampata su Serial (indirizzo assoluto, — exec_program_step() is printed to Serial (absolute address,
// indirizzo locale se in esecuzione "as-is" da modulo, codice opcode e — local address if running "as-is" from a module, opcode and
// mnemonico) prima di essere eseguita. Di default spento (rallenta — mnemonic) before being executed. Off by default (it slows
// l'esecuzione e riempie il log se lasciato acceso su un programma — execution and fills the log if left on over a long
// lungo) — va attivato solo per diagnosticare un problema specifico. — program) — enable it only to diagnose a specific problem.
void tms1500_set_trace_steps(bool enable);
bool tms1500_get_trace_steps(void);

// Indicatore Old/New: true se il programma in memoria ha modifiche — Old/New indicator: true if the program in memory has changes
// non ancora salvate su scheda dall'ultimo caricamento/salvataggio. — not yet saved to card since the last load/save.
bool tms1500_is_prog_dirty(void);
void tms1500_mark_prog_dirty(void);
void tms1500_mark_prog_saved(void);

// Getter/setter per le etichette personalizzate utente (A-E / A'-E'). — Getter/setter for user custom labels (A-E / A'-E').
// out/in deve essere un array di 10 uint16_t (0xFFFF = etichetta vuota). — out/in must be an array of 10 uint16_t (0xFFFF = empty label).
void tms1500_get_labels(uint16_t *out);
void tms1500_set_labels(const uint16_t *in);

// Hook chiamato ogni volta che la modalità timing/autenticità cambia — Hook called every time the timing/authenticity mode changes
// (sia da combo fisico +,-,x,/ sia da tms1500_set_realistic_timing()). — (either from the physical +,-,x,/ combo or from tms1500_set_realistic_timing()).
// Implementazione di default no-op; un livello esterno con accesso a — Default implementation is no-op; an external layer with access to
// storage persistente (es. wifilink.cpp con Preferences/NVS) può — persistent storage (e.g. wifilink.cpp with Preferences/NVS) can
// sovrascriverla per ricordare la scelta tra un riavvio e l'altro. — override it to remember the choice across reboots.
void tms1500_on_timing_changed(bool realistic);
bool tms1500_get_trailing_dp(void);
bool tms1500_get_pending_2nd(const TMS1500_State *cpu);
bool tms1500_get_input_has_dot(void);
bool tms1500_get_input_has_ee(void);
void tms1500_get_input_buf(char *buf, unsigned int len);

// Utility BCD — BCD utilities
void   bcd_zero(BCD_Reg *r);
void   bcd_copy(BCD_Reg *dst, const BCD_Reg *src);
bool   bcd_is_zero(const BCD_Reg *r);
void   bcd_from_int(BCD_Reg *r, int32_t v);
double bcd_to_double(const BCD_Reg *r);
void   bcd_from_double(BCD_Reg *r, double v);

#endif // TMS1500_H — fine guardia include — end of include guard