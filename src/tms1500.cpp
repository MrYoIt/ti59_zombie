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
 * tms1500.c — Emulatore CPU TMS0500 (TI-58/59)
 * Keycodes remapped to match real TI-59 hardware ROM (00-99)
 */

#include <Arduino.h>
#include "tms1500.h"
#include "keyboard.h"
#include "display.h"
#include "config.h"
#include "printer.h"
#include "library_module.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>   // per atof, malloc, free
#include <stdint.h>   // per uint8_t, uint16_t, int32_t (sicurezza extra)
#include <SPIFFS.h>
#include <FS.h>

// ═══════════════════════════════════════════════════════════
// KEYCODE TI-59 — REAL HARDWARE CODES (00-99)
// Based on TI-58/59 ROM dump and hardware manual
// ═══════════════════════════════════════════════════════════

// Row 0 — User Definable Keys (A–E, A'–E')
#define KC_E_PRIME  10   // E′ (2nd E) — code 10 on real TI-59
#define KC_A        11   // A
#define KC_B        12   // B
#define KC_C        13   // C
#define KC_D        14   // D
#define KC_E        15   // E
#define KC_A_PRIME  16   // A′ (2nd A)
#define KC_B_PRIME  17   // B′ (2nd B)
#define KC_C_PRIME  18   // C′ (2nd C)
#define KC_D_PRIME  19   // D′ (2nd D)

// Row 1 — Modifier / Clear Keys
#define KC_CLR_2ND  20   // Clear (2nd CLR) — single code 20
#define KC_2ND      21   // 2nd
#define KC_INV      22   // INV
#define KC_LNX      23   // LNx
#define KC_CE       24   // CE
#define KC_CLR      25   // CLR
#define KC_2ND_2ND  26   // 2nd 2nd (code 26)
#define KC_2ND_INV  27   // 2nd INV (code 27)
#define KC_LOG      28   // log (2nd LNx)
#define KC_CP       29   // CP (2nd CE)
#define KC_TAN      30   // tan (2nd 1/x)

// Row 2 — LRN / Trig Primitives
#define KC_LRN      31   // LRN
#define KC_XET      32   // x↔t
#define KC_X2       33   // x²
#define KC_SQRT     34   // √x
#define KC_INV_X    35   // 1/x
#define KC_PGM      36   // PGM (2nd LRN)
#define KC_P_R      37   // P→R (2nd x↔t)
#define KC_SIN      38   // sin (2nd x²)
#define KC_COS      39   // cos (2nd √x)
#define KC_IND      40   // IND (2nd yˣ)

// Row 3 — Memory / Data Manipulation
#define KC_SST      41   // SST
#define KC_STO      42   // STO
#define KC_RCL      43   // RCL
#define KC_SUM      44   // SUM
#define KC_YX       45   // yˣ
#define KC_INS      46   // Ins (2nd SST)
#define KC_CMS      47   // CMs (2nd STO)
#define KC_EXC      48   // EXC (2nd RCL)
#define KC_PROD     49   // Prod (2nd SUM)
#define KC_ABS      50   // |x| (2nd ÷)

// Row 4 — Navigation / Parens / Divide
#define KC_BST      51   // BST
#define KC_EE       52   // EE
#define KC_LPAR     53   // (
#define KC_RPAR     54   // )
#define KC_DIV      55   // ÷
#define KC_DEL      56   // Del (2nd BST)
#define KC_ENG      57   // ENG (2nd EE)
#define KC_FIX      58   // Fix (2nd ()
#define KC_INT      59   // Int (2nd ))
#define KC_DEG      60   // Deg (2nd ×)

// Row 5 — GTO / Digits 7–9 / Multiply
#define KC_GTO      61   // GTO
// 07 = 7, 08 = 8, 09 = 9 (digits map to themselves)
#define KC_MUL      65   // ×
#define KC_PAUSE    66   // Pause (2nd GTO)
#define KC_XEQ_T    67   // x=t (2nd 7)
#define KC_NOP      68   // Nop (2nd 8)
#define KC_OP       69   // Op (2nd 9)
#define KC_RAD      70   // Rad (2nd −)

// Row 6 — SBR / Digits 4–6 / Subtract
#define KC_SBR      71   // SBR
// 04 = 4, 05 = 5, 06 = 6 (digits map to themselves)
#define KC_SUB      75   // −
#define KC_LBL      76   // Lbl (2nd SBR)
#define KC_XGE_T    77   // x≥t (2nd 4)
#define KC_SIGP     78   // Σ+ (2nd 5)
#define KC_XBAR     79   // x̄ (2nd 6)
#define KC_GRAD     80   // Grad (2nd +)

// Row 7 — RST / Digits 1–3 / Add
#define KC_RST      81   // RST
// 01 = 1, 02 = 2, 03 = 3 (digits map to themselves)
#define KC_ADD      85   // +
#define KC_STFL     86   // St Flg (2nd RST)
#define KC_IFFL     87   // If Flg (2nd 1)
#define KC_DMS      88   // D.MS (2nd 2)
#define KC_PI       89   // π (2nd 3)
#define KC_LIST     90   // List (2nd =)

// Row 8 — R/S / Digit 0 / Dot / Sign / Equals
#define KC_RS       91   // R/S
// 00 = 0
#define KC_DOT      93   // .
#define KC_PM       94   // +/−
#define KC_EQ       95   // =
#define KC_WRITE    96   // Write (2nd R/S, TI-59 only)
#define KC_DSZ      97   // DSZ (2nd 0)
#define KC_ADV      98   // Adv (2nd .)
#define KC_PRT      99   // Prt (2nd +/−)

// Special / Indirect codes (not direct keys but valid in programs)
#define KC_PGM_IND  62   // Pgm Ind (2nd PGM 2nd IND)
#define KC_EXC_IND  63   // EXC Ind (2nd EXC 2nd IND)
#define KC_PROD_IND 64   // Prod Ind (2nd Prod 2nd IND)
#define KC_STO_IND  72   // STO Ind (STO 2nd IND)
#define KC_RCL_IND  73   // RCL Ind (RCL 2nd IND)
#define KC_SUM_IND  74   // SUM Ind (SUM 2nd IND)
#define KC_GTO_IND  83   // GTO Ind
#define KC_OP_IND   84   // Op Ind
#define KC_RETURN   92   // INV SBR = Return (single step!)

#define KC_NONE     0xFF // No key

// ── Pending arithmetic operation codes (cpu->pending_op) ───────────────
// Consumed by exec_pending(), hir_pop(), and apply_pending_op().
#define PENDING_OP_NONE  0   // No pending arithmetic operator
#define PENDING_OP_ADD   1   // +
#define PENDING_OP_SUB   2   // −
#define PENDING_OP_MUL   3   // ×
#define PENDING_OP_DIV   4   // ÷
#define PENDING_OP_YX    5   // yˣ

// ── Pending register action codes (cpu->pending_reg) ───────────────────
// Set by STO/RCL/GTO/SBR/SUM/StFlg/IfFlg/EXC/Prod key handlers;
// consumed by the digit-accumulation block in process_keycode().
#define PENDING_REG_NONE  0xFF  // No pending register action
#define PENDING_REG_STO   1     // Store A → register
#define PENDING_REG_RCL   2     // Recall register → A
#define PENDING_REG_GTO   3     // Go to address or label
#define PENDING_REG_SBR   4     // Subroutine call (pushes return address)
#define PENDING_REG_SUM   5     // Add A to register
#define PENDING_REG_STFL  6     // Store flag
#define PENDING_REG_IFFL  7     // Test flag
#define PENDING_REG_EXC   8     // Exchange A with register
#define PENDING_REG_PROD  9     // Multiply A into register

// ═══════════════════════════════════════════════════════════
// TABELLA MNEMONICI (MAPPATURA CODICE -> STRINGA)
// ═══════════════════════════════════════════════════════════
const char* const KEYCODE_MNEMONICS[] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",       // 00-09
    "E'", "A", "B", "C", "D", "E", "A'", "B'", "C'", "D'",  // 10-19
    "CLR", "2nd", "INV", "LNx", "CE", "CLR", "2nd2nd", "2ndINV", "LOG", "CP", // 20-29
    "TAN", "LRN", "XET", "X^2", "SQRT", "1/x", "PGM", "P->R", "SIN", "COS", // 30-39
    "IND", "SST", "STO", "RCL", "SUM", "Y^X", "INS", "CMS", "EXC", "PROD", // 40-49
    "ABS", "BST", "EE", "(", ")", "/", "DEL", "ENG", "FIX", "INT", // 50-59
    "DEG", "GTO", "PGM_IND", "EXC_IND", "PROD_IND", "*", "PAUSE", "X=T", "NOP", "OP", // 60-69
    "RAD", "SBR", "STO_IND", "RCL_IND", "SUM_IND", "-", "LBL", "X>=T", "SIG+", "XBAR", // 70-79
    "GRAD", "RST", "HIR", "GTO_IND", "OP_IND", "+", "STFL", "IFFL", "DMS", "PI", // 80-89
    "LIST", "R/S", "RET", ".", "+/-", "=", "WRITE", "DSZ", "ADV", "PRT" // 90-99
};

// Questa funzione può vedere la tabella static perché è nello stesso file
const char* get_mnemonic_name(uint8_t code) {
    if (code > 99) return "???";
    return KEYCODE_MNEMONICS[code];
}

// ═══════════════════════════════════════════════════════════
// VARIABILI STATICHE
// ═══════════════════════════════════════════════════════════

static uint16_t custom_label_pc[10] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
static bool pending_indirect = false;
static int  pending_value = 0;
static bool user_flags[10] = {false};
static bool inv_pending = false;   // INV prefix pending in LRN mode
static bool fix_pending = false;
static bool op_pending = false;
static int  op_code = 0;
static int  op_digits = 0;

// ─── Pgm (modulo libreria): raccolta delle 2 cifre successive ──
// (il numero di programma da designare, es. "2nd Pgm 01" per ML-01).
// Stesso pattern a fasi già usato per DSZ.
static bool lib_page_pending = false;
static int  lib_page_digits  = 0;
static int  lib_page_val     = 0;

// ─── Modulo libreria: due modalità reali, non una ──────────
// Da manuale TI-59 + chiarimento diretto: "2nd Pgm mm" DESIGNA quale
// programma, senza fare altro. Cosa succede dopo dipende dal tasto
// successivo:
//   • "2nd Op 09"      → SCARICA il programma in memoria principale
//                         (passo 000, sovrascrive) per poterlo
//                         modificare — memoria/etichette dell'utente,
//                         nessuna area separata (v. exec_op case 9).
//   • A..E / A'..E'     → esegue il programma COSÌ COM'È, direttamente
//                         dalla ROM del modulo, SENZA toccare la
//                         memoria LRN dell'utente né i suoi registri
//                         dati — usa un proprio banco STO/RCL
//                         separato ("ecosistema della ROM").
static bool     lib_page_selected = false;  // "Pgm mm" appena designato, in attesa di Op09 o di un'etichetta
static uint8_t  lib_selected_page = 0;
static uint16_t lib_scope_addr    = 0;      // indirizzo di partenza del programma designato
static uint16_t lib_scope_len     = 0;      // sua lunghezza (per dare priorità alle SUE etichette)

// Scope "in attesa": impostato da PGM (sia da tastiera sia incontrato
// dentro un programma) quando designa un nuovo programma, ma NON ancora
// commesso a lib_scope_addr/lib_scope_len — quel commit avviene solo
// quando la SBR/GTO/etichetta che segue lo attiva davvero. Necessario
// per le chiamate annidate ("PGM mm SBR label" dentro un programma di
// libreria già in esecuzione): se PGM scrivesse subito in
// lib_scope_addr/lib_scope_len, distruggerebbe lo scope del chiamante
// prima che la SBR possa salvarlo per il ritorno.
static uint16_t lib_pending_addr = 0;
static uint16_t lib_pending_len  = 0;
static uint8_t  lib_pending_page = 0;

static bool     showing_lib_prog  = false;  // esecuzione "as-is" dalla ROM attiva
static uint16_t lib_custom_label_pc[10] = {0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF};
static BCD_Reg  lib_ram[RAM_SIZE];          // registri dati del modulo, separati da cpu->ram[]

// Converte un keycode etichetta (A-E / A'-E') nell'indice 0-9.
static int label_index_for_key(uint8_t kc) {
    if (kc >= KC_A && kc <= KC_E) return kc - KC_A;
    if (kc >= KC_A_PRIME && kc <= KC_D_PRIME) return kc - KC_A_PRIME + 5;
    if (kc == KC_E_PRIME) return 9;
    return -1;
}

// Lunghezza del "programma" corrente ai fini del wraparound di prog_pc:
// in esecuzione "as-is" da modulo è la lunghezza LOCALE del solo
// programma designato (lib_scope_len, es. 189 per ML-01) — NON la
// dimensione dell'intera ROM da 5000 step. cpu->prog_pc resta comunque
// un indice ASSOLUTO nella ROM in quel caso: chi usa questa lunghezza
// per calcolare un nuovo prog_pc deve ricollocare col relativo
// lib_scope_addr (vedi advance_pc_wrap()/relocate_target() sotto).
// Altrimenti, in esecuzione utente normale, è cpu->prog_len come sempre.
static inline uint16_t exec_prog_len(const TMS1500_State *cpu) {
    if (showing_lib_prog) {
        return lib_scope_len;
    }
    return cpu->prog_len;
}

// Avanza cpu->prog_pc di N byte con wraparound, restando nello spazio
// giusto: in esecuzione "as-is" da modulo, prog_pc è un indice ASSOLUTO
// nella ROM ma il wrap deve avvenire sulla lunghezza LOCALE del solo
// programma designato (lib_scope_len), poi va ricollocato sommando
// lib_scope_addr.
static inline uint16_t advance_pc_by(TMS1500_State *cpu, uint16_t nbytes) {
    uint16_t len = exec_prog_len(cpu);
    if (len == 0) return cpu->prog_pc;
    if (showing_lib_prog) {
        // Only wrap addresses within the current lib_scope range.
        // Addresses OUTSIDE lib_scope (e.g. after PGM call/return, or
        // SBR/GTO to a shared subroutine in another part of the ROM)
        // must advance without wrapping — otherwise pc gets remapped
        // into the wrong program.
        if (cpu->prog_pc >= lib_scope_addr) {
            uint16_t local = (uint16_t)(cpu->prog_pc - lib_scope_addr);
            if (local < len)
                return (uint16_t)(lib_scope_addr + ((local + nbytes) % len));
        }
        return (uint16_t)(cpu->prog_pc + nbytes);
    }
    return (uint16_t)((cpu->prog_pc + nbytes) % len);
}

// Compatibilità: avanza di un solo byte (usata da SST, che gestisce
// l'istruzione tramite process_keycode() a parte).
static inline uint16_t advance_pc_wrap(TMS1500_State *cpu) {
    return advance_pc_by(cpu, 1);
}

uint8_t prog_read_step(TMS1500_State *cpu, uint16_t addr); // fwd decl
static void format_value_string(const TMS1500_State *cpu, char *buf, unsigned int len); // fwd decl

// Lunghezza in byte dell'istruzione che inizia a 'addr', secondo le
// stesse regole di decodifica di exec_program_step()/read_2digit()/
// read_3digit()/read_label() — serve per "saltare la prossima
// istruzione" (x≥t, IF flag) senza disallinearsi quando quell'istruzione
// occupa più di un byte (es. una GTO/STO con operando). Un +1 fisso qui
// era il bug: dopo lo skip il decoder ripartiva a metà di un'istruzione
// multi-byte, leggendo byte a caso come se fossero un nuovo opcode.
static uint16_t instruction_byte_length(TMS1500_State *cpu, uint16_t addr) {
    uint16_t prog_end = showing_lib_prog ? (uint16_t)(lib_scope_addr + lib_scope_len)
                                          : cpu->prog_len;
    if (addr >= prog_end) return 1;
    uint8_t op = prog_read_step(cpu, addr);
    uint16_t f2 = showing_lib_prog ? 1 : 2;   // read_2digit: 1 byte in ROM libreria, 2 da tastiera
    uint16_t f3 = showing_lib_prog ? 2 : 3;   // read_3digit: 2 byte in ROM libreria, 3 da tastiera

    if (op == KC_LBL) return 2;               // opcode + 1 byte etichetta
    if (op == KC_INV) {                       // prefisso: 1 + lunghezza dell'istruzione che segue
        if (addr + 1 >= prog_end) return 1;
        return (uint16_t)(1 + instruction_byte_length(cpu, (uint16_t)(addr + 1)));
    }
    if (op == KC_DSZ) {
        // In libreria ROM la codifica dopo il registro è sempre indirizzo
        // (mai etichetta come keycode singolo, perchè i registri ≥ 10 sono
        // valori validi e > 9, indistinguibili da un'etichetta col test >9).
        if (showing_lib_prog)
            return (uint16_t)(1 + f2 + f3);    // DSZ reg, indirizzo letterale
        uint16_t after_reg = (uint16_t)(addr + 1 + f2);
        if (after_reg < prog_end && prog_read_step(cpu, after_reg) > 9)
            return (uint16_t)(1 + f2 + 1);     // DSZ reg, LBL
        return (uint16_t)(1 + f2 + f3);        // DSZ reg, indirizzo letterale
    }
    if (op == KC_SBR || op == KC_GTO || op == KC_XEQ_T || op == KC_XGE_T) {
        uint16_t after_op = (uint16_t)(addr + 1);
        if (after_op < prog_end && prog_read_step(cpu, after_op) > 9)
            return 2;                          // opcode + LBL
        return (uint16_t)(1 + f3);             // opcode + indirizzo letterale
    }
    if (op == KC_STO || op == KC_RCL || op == KC_SUM || op == KC_EXC || op == KC_PROD ||
        op == KC_STO_IND || op == KC_RCL_IND || op == KC_SUM_IND || op == KC_EXC_IND ||
        op == KC_PROD_IND || op == KC_GTO_IND || op == KC_OP) {
        return (uint16_t)(1 + f2);             // opcode + registro/parametro a 2 cifre
    }
    if (op == KC_FIX || op == KC_STFL) return 2;   // opcode + 1 cifra
    if (op == KC_PGM || op == KC_PGM_IND) return (uint16_t)(1 + f2);  // Pgm + 2 cifre
    if (op == KC_IFFL) {
        // IFFL flagnum + label (1 byte) o + indirizzo (f3 byte)
        uint16_t after_flag = (uint16_t)(addr + 2);
        if (after_flag < prog_end && prog_read_step(cpu, after_flag) > 9)
            return 3;                          // opcode + flag + LBL
        return (uint16_t)(2 + f3);             // opcode + flag + indirizzo
    }
    return 1;                                  // default: opcode a singolo byte
}

// Ricolloca un indirizzo di destinazione "grezzo" (es. il valore letto da
// un registro per GTO/SBR indiretto) nello spazio di esecuzione corrente:
// locale al programma designato + lib_scope_addr in modalità libreria,
// assoluto altrimenti. Stesso principio di advance_pc_wrap() ma per un
// indirizzo arbitrario invece che "prog_pc + 1".
static inline uint16_t relocate_target(TMS1500_State *cpu, uint16_t raw_local) {
    uint16_t len = exec_prog_len(cpu);
    if (len == 0) return raw_local;
    uint16_t wrapped = raw_local % len;
    return showing_lib_prog ? (uint16_t)(lib_scope_addr + wrapped) : wrapped;
}

// Banco registri dati attivo: quello del modulo se in esecuzione
// "as-is", altrimenti quello dell'utente come sempre. Così STO/RCL/SUM
// e DSZ durante un programma da modulo non toccano mai cpu->ram[].
static inline BCD_Reg* active_ram_bank(TMS1500_State *cpu) {
    return showing_lib_prog ? lib_ram : cpu->ram;
}

// Ricostruisce la tabella etichette per l'esecuzione "as-is": prima
// scandisce SOLO l'intervallo del programma designato (così "LBL A"
// di QUESTO programma ha la precedenza — più programmi nello stesso
// modulo possono definire ciascuno una propria "LBL A"), poi riempie
// gli indici ancora mancanti scandendo il resto della ROM (per le
// subroutine condivise richiamate per etichetta da fuori del proprio
// intervallo).
static void rebuild_lib_labels(uint16_t scope_addr, uint16_t scope_len) {
    for (int i = 0; i < 10; i++) lib_custom_label_pc[i] = 0xFFFF;
    const LibraryModule *m = library_get_active();
    if (!m) return;

    uint16_t scope_end = scope_addr + scope_len;
    if (scope_end > m->rom_size) scope_end = m->rom_size;
    for (uint16_t i = scope_addr; i + 1 < scope_end; i++) {
        if (m->rom[i] == KC_LBL) {
            int idx = label_index_for_key(m->rom[i + 1]);
            if (idx >= 0 && lib_custom_label_pc[idx] == 0xFFFF) lib_custom_label_pc[idx] = i;
        }
    }
    for (uint16_t i = 0; i + 1 < m->rom_size; i++) {
        if (m->rom[i] == KC_LBL) {
            int idx = label_index_for_key(m->rom[i + 1]);
            if (idx >= 0 && lib_custom_label_pc[idx] == 0xFFFF) lib_custom_label_pc[idx] = i;
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// Collega questo modulo al cpu attivo, così library_on_module_changed()
// (sotto) può intervenire sull'esecuzione in corso quando l'utente
// cambia modulo da web. Va chiamata una volta sola, subito dopo che
// g_cpu viene creato/assegnato (g_cpu vive in wifilink.cpp — vedi
// "TMS1500_State *g_cpu = ...").
//   IMPORTANTE: aggiungere in tms1500.h la dichiarazione
//     void tms1500_bind_cpu(TMS1500_State *cpu);
//   e chiamare tms1500_bind_cpu(g_cpu); in wifilink.cpp subito dopo
//   l'assegnazione di g_cpu.
// ═══════════════════════════════════════════════════════════════
static TMS1500_State *g_bound_cpu = nullptr;
void tms1500_bind_cpu(TMS1500_State *cpu) { g_bound_cpu = cpu; }

// ═══════════════════════════════════════════════════════════════
// Hook chiamato da library_module.cpp ogni volta che l'utente
// cambia modulo attivo (endpoint /api/modules in wifilink.cpp).
// Oltre a ripulire lo stato di scope/etichette (già fatto sopra),
// simula l'estrazione FISICA del modulo: se in questo momento
// l'esecuzione dipende davvero dalla ROM che sta per sparire — o
// perché ci si trova dentro di essa (showing_lib_prog era true), o
// perché lo stack di ritorno ha almeno una chiamata pendente verso
// di essa (stack_in_rom[i]==true per qualche frame) — il programma
// viene interrotto sul colpo, esattamente come farebbe la calcolatrice
// reale se le si strappasse via il modulo dallo slot a metà
// esecuzione: RUN si ferma, il flag errore si accende, l'eventuale
// operazione/cifra a metà inserimento viene scartata, e lo stack di
// ritorno viene svuotato (non si può "srotolare" in modo pulito un
// mix di frame utente/ROM quando la ROM stessa non c'è più).
// Se invece il programma in corso è interamente in memoria utente e
// non ha alcuna chiamata pendente verso la ROM, NON viene toccato:
// esattamente come sull'hardware reale, il modulo può essere cambiato
// senza conseguenze finché non viene davvero richiamato.
// ═══════════════════════════════════════════════════════════════
// NOTA: NON si chiama "library_on_module_changed" — quel nome è
// l'hook debole dichiarato in library_module.h, e wifilink.cpp ne
// fornisce già la propria implementazione forte (persistenza su NVS
// dell'ultimo modulo innestato). Due implementazioni forti dello
// stesso simbolo darebbero "multiple definition" in fase di link.
// Questa funzione va quindi richiamata ESPLICITAMENTE da dentro
// l'implementazione di wifilink.cpp, non sovrascrive nulla da sola.
//   Aggiungere in wifilink.cpp, dentro il suo library_on_module_changed
//   già esistente:  tms1500_on_library_module_changed(id);
void tms1500_on_library_module_changed(const char *id) {
    (void)id;

    bool was_using_rom = showing_lib_prog;
    if (!was_using_rom && g_bound_cpu) {
        for (uint8_t i = 0; i < g_bound_cpu->sp; i++) {
            if (g_bound_cpu->stack_in_rom[i]) { was_using_rom = true; break; }
        }
    }

    showing_lib_prog  = false;
    lib_scope_addr    = 0;
    lib_scope_len     = 0;
    lib_page_selected = false;
    lib_selected_page = 0;
    lib_pending_addr  = 0;
    lib_pending_len   = 0;
    lib_pending_page  = 0;
    for (int i = 0; i < 10; i++) lib_custom_label_pc[i] = 0xFFFF;

    if (was_using_rom && g_bound_cpu) {
        Serial.printf("[LIB] Modulo cambiato mentre l'esecuzione dipendeva dalla ROM "
                       "precedente: interrompo il programma (come estrazione fisica).\n");
        g_bound_cpu->flags.run   = false;
        g_bound_cpu->flags.error = true;
        g_bound_cpu->pending_op  = 0;
        g_bound_cpu->pending_reg = PENDING_REG_NONE;
        g_bound_cpu->pending_digits = 0;
        g_bound_cpu->sp = 0;          // niente ritorni puliti verso una ROM sparita
        g_bound_cpu->prog_pc = 0;     // scarta un indirizzo assoluto ora privo di senso
    }
}

uint8_t tms1500_get_active_lib_page(void) {
    return lib_selected_page;
}

static bool display_trailing_dp = false;

#define HIR_STACK_SIZE 6
static BCD_Reg hir_stack[HIR_STACK_SIZE];
static BCD_Reg hir_operand[HIR_STACK_SIZE];
static uint8_t hir_op[HIR_STACK_SIZE];
static int hir_sp = 0;
// Paren nesting tracker: each '(' records HIR depth so ')' can restore without
// executing the outer pending op. Must be >= HIR_STACK_SIZE for nesting safety.
static int paren_depth = 0;
static int hir_paren_base[HIR_STACK_SIZE];

// DSZ (Decrement and Skip if Zero) — sulla TI-59 reale è un'istruzione a
// DUE operandi: "Dsz nn LLL" decrementa il registro dati nn e, se il
// risultato NON è zero, salta all'indirizzo/etichetta LLL; se il risultato
// è zero, l'esecuzione prosegue in sequenza (fine ciclo). Servono quindi
// due fasi di raccolta cifre, esattamente come per GTO/SBR.
static int dsz_phase       = 0;   // 0=inattivo, 1=raccolta registro, 2=raccolta indirizzo
static int dsz_reg_val     = 0;
static int dsz_reg_digits  = 0;
static int dsz_addr_val    = 0;
static int dsz_addr_digits = 0;

// ─── Indicatore "occupato" (funzioni lente: sqrt, trig, log, y^x...) ──
// Sulla TMS1500 reale, durante il calcolo di queste funzioni lo
// schermo si svuotava e mostrava solo "C" nell'ultima posizione a
// sinistra finché il risultato non era pronto — l'hardware era
// letteralmente occupato per centinaia di millisecondi/qualche
// secondo. Due modalità:
//   - "reale": blocca per la durata autentica (approssimata), proprio
//     come l'originale — il "C" resta visibile per tutto quel tempo.
//   - "moderna" (default): il calcolo sottostante resta istantaneo
//     (nessun blocco di tastiera/WiFi), ma "C" resta comunque visibile
//     per una durata minima fissa, solo per il feeling tattile.
//
// NOTA SULLE DURATE: valori approssimativi presi dalla memoria
// collettiva della comunità di appassionati TI-58/59, NON misurati su
// una fonte primaria in questa sessione. Se hai ancora una macchina
// reale funzionante, cronometrala con un cellulare e aggiorna pure
// queste costanti di conseguenza.
#define BUSY_MS_SQRT        150   // radice quadrata
#define BUSY_MS_TRIG        700   // sin/cos/tan
#define BUSY_MS_ATRIG      1000   // asin/acos/atan (INV sin/cos/tan)
#define BUSY_MS_LOG         500   // ln/log10
#define BUSY_MS_EXP         600   // e^x/10^x
#define BUSY_MS_YX         1000   // y^x
#define BUSY_MODERN_MIN_MS  650   // durata minima "C" in modalità moderna
                                   // (>500ms: il polling della pagina web
                                   // interroga lo stato ogni 500ms — sotto
                                   // quella soglia la finestra "occupato"
                                   // rischia di cadere sempre tra due poll
                                   // e non venire mai vista dall'interfaccia)

static bool          g_realistic_timing = false;   // default: modalità moderna
static float         g_timing_multiplier = 1.0f;   // 1.0 = timing autentico
static volatile bool busy_active   = false;
static unsigned long busy_until_ms = 0;

// ─── Pacing a tempo reale dell'esecuzione programmi ──────────
// Il TI-59 originale esegue ~TI59_INSTR_PER_SEC passi/s (v. config.h).
// In modalità Old (g_realistic_timing) l'esecuzione dei programmi viene
// rallentata a tempo reale per un feel autentico: si accumulano le
// istruzioni "dovute" in base al tempo reale trascorso e il rate target è
// originale × g_timing_multiplier (100% = stesso ritmo del hardware reale).
// Così la velocità NON dipende dal carico del device. g_step_rate misura
// il loop reale (passi/s) = soffitto pratico: se < target, l'emulatore
// gira comunque al massimo che può (e /api/sysinfo lo riporta).
static unsigned long g_inst_acc      = 0;   // istruzioni "in arretrato"
static unsigned long g_pace_last_ms  = 0;
static unsigned long g_steps_win     = 0;
static unsigned long g_step_rate     = 0;
static unsigned long g_rate_last_ms  = 0;

static void update_step_rate() {
    g_steps_win++;
    unsigned long now = millis();
    if (g_rate_last_ms == 0) g_rate_last_ms = now;
    unsigned long d = now - g_rate_last_ms;
    if (d >= 1000) {
        g_step_rate = (unsigned long)((float)g_steps_win * 1000.0f / (float)d);
        g_steps_win = 0;
        g_rate_last_ms = now;
    }
}

// PAUSE (66, 2nd GTO) non bloccante: la vecchia vTaskDelay(500) congelava
// il task che esegue l'emulazione (e che serve anche display/web), quindi
// durante la pausa non si poteva nemmeno fermare il programma con R/S.
// Ora si imposta solo un timestamp e tms1500_step trattiene l'esecuzione;
// tastiera, display e web restano reattivi durante i 500ms.
static unsigned long pause_until_ms = 0;

// ─── Tracer passo-passo (debug) ──────────────────────────────────
// Se attivo, ogni istruzione eseguita da exec_program_step() viene
// stampata su Serial come "[STEP] pc=.... (local=....) op=NN (mnemonico)"
// prima di essere decodificata/eseguita — utile per seguire dal vivo
// dove va un programma (anche "as-is" da modulo libreria) senza dover
// aggiungere printf sparsi ogni volta. Di default spento: su un
// programma che gira per migliaia di cicli stamperebbe moltissimo e
// rallenterebbe l'esecuzione; va acceso solo mentre si sta
// diagnosticando un problema.
static bool g_trace_steps = false;
void tms1500_set_trace_steps(bool enable) { g_trace_steps = enable; }
bool tms1500_get_trace_steps(void) { return g_trace_steps; }

// ─── Indicatore Old/New (programma modificato dall'ultimo salvataggio) ──
// "New" = ci sono modifiche non salvate su scheda dall'ultima
// operazione di caricamento/salvataggio; "Old" = combacia con quanto
// già persistito. Diventa "New" ad ogni scrittura nel buffer
// programma durante LRN (prog_store_step, DEL, INS); torna "Old"
// quando un programma viene caricato da fonte esterna
// (tms1500_load_prog) o quando un livello esterno con accesso al
// sottosistema schede conferma un salvataggio riuscito
// (tms1500_mark_prog_saved(), da chiamare da wifilink.cpp dopo
// cardemu_write()/handle_prog_post() o dal WRITE fisico).
static bool prog_dirty = false;
void tms1500_mark_prog_saved(void) { prog_dirty = false; }
void tms1500_mark_prog_dirty(void) { prog_dirty = true; }
bool tms1500_is_prog_dirty(void) { return prog_dirty; }

void tms1500_get_labels(uint16_t *out) {
    memcpy(out, custom_label_pc, sizeof(custom_label_pc));
}
void tms1500_set_labels(const uint16_t *in) {
    memcpy(custom_label_pc, in, sizeof(custom_label_pc));
}

__attribute__((weak)) void tms1500_on_timing_changed(bool realistic) { (void)realistic; }

void tms1500_set_realistic_timing(bool enable) {
    if (enable == g_realistic_timing) return;   // nessun cambiamento reale
    g_realistic_timing = enable;
    tms1500_on_timing_changed(enable);
}
bool tms1500_get_realistic_timing(void) { return g_realistic_timing; }

void tms1500_set_timing_multiplier(float mult) {
    if (mult < 0.1f) mult = 0.1f;
    if (mult > 2.0f) mult = 2.0f;
    g_timing_multiplier = mult;
}
float tms1500_get_timing_multiplier(void) { return g_timing_multiplier; }

unsigned int tms1500_get_max_speed_pct(void) {
    if (g_step_rate == 0) return 0;
    unsigned long pct = (unsigned long)((double)g_step_rate * 100.0 / TI59_INSTR_PER_SEC);
    return (unsigned int)(pct > 65535 ? 65535 : pct);
}

static void busy_start(unsigned long real_ms) {
    if (real_ms == 0) return;
    // Prima, in modalità Old, questa funzione bloccava con vTaskDelay()
    // per la durata autentica: busy_active passava a true e tornava a
    // false PRIMA che la chiamata HTTP che ha originato la pressione
    // tasto potesse anche solo rispondere — nessun poll da web poteva
    // mai osservare lo stato "occupato", perché avveniva interamente
    // dentro una singola chiamata sincrona sullo stesso task che serve
    // anche il server web. Ora entrambe le modalità sono non
    // bloccanti: il calcolo resta istantaneo, ma "C" resta visibile
    // (controllato pigramente in tms1500_get_display_string) per la
    // durata autentica in Old, o per il minimo fisso in New — così è
    // finalmente osservabile da un poll web in entrambi i casi.
    busy_active = true;
    unsigned long duration = g_realistic_timing
        ? (unsigned long)((float)real_ms * g_timing_multiplier)
        : BUSY_MODERN_MIN_MS;
    busy_until_ms = millis() + duration;
}

// ─── Combo +,-,×,÷ "premuti insieme": alterna timing reale/moderno ──
// Con una tastiera a matrice scandita in sequenza, la "simultaneità"
// vera non esiste: si approssima registrando l'istante di arrivo di
// ciascuno dei 4 tasti operatore e controllando che tutti e 4 siano
// arrivati entro una finestra breve (400ms, ragionevole per una
// pressione a mano "a ventaglio" con più dita). Finché non arrivano
// tutti e 4 nella finestra, il normale comportamento aritmetico dei
// tasti +,-,×,÷ resta invariato — il controllo si limita ad
// aggiungersi PRIMA di esso, senza toglierlo: premere in sequenza
// questi tasti senza cifre in mezzo è già di per sé innocuo sul
// risultato (nessuna cifra nuova = exec_pending() non ha nulla da
// risolvere), quindi non serve "annullare" nulla se il combo scatta.
#define TIMING_TOGGLE_WINDOW_MS 400
static unsigned long op_combo_ms[4] = {0, 0, 0, 0};   // +, -, ×, ÷

static void check_timing_toggle_combo(uint8_t kc) {
    int idx = (kc == KC_ADD) ? 0 : (kc == KC_SUB) ? 1 : (kc == KC_MUL) ? 2 : 3;
    unsigned long now = millis();
    op_combo_ms[idx] = now;

    unsigned long oldest = now;
    for (int i = 0; i < 4; i++) {
        if (op_combo_ms[i] == 0) return;   // non tutti e 4 ancora premuti
        if (op_combo_ms[i] < oldest) oldest = op_combo_ms[i];
    }
    if (now - oldest <= TIMING_TOGGLE_WINDOW_MS) {
        bool new_mode = !tms1500_get_realistic_timing();
        tms1500_set_realistic_timing(new_mode);
        Serial.printf("[TIMING] Combo +,-,x,/ rilevato -> modalita' %s\n",
                      new_mode ? "REALE (timing autentico)" : "MODERNA (istantanea)");
        for (int i = 0; i < 4; i++) op_combo_ms[i] = 0;   // consuma il combo
    }
}

static char input_buf[16];
static int  input_len = 0;

static PrinterState g_printer;
static bool input_has_dot = false;
static bool input_has_ee  = false;
static int  input_ee_len  = 0;

// KEY_MAP[row][col] — maps physical keyboard to TI-59 keycodes
// Row 0: A, B, C, D, E
// Row 1: 2nd, INV, LNx, CE, CLR
// Row 2: LRN, x↔t, x², √x, 1/x
// Row 3: SST, STO, RCL, SUM, yˣ
// Row 4: BST, EE, (, ), ÷
// Row 5: GTO, 7, 8, 9, ×
// Row 6: SBR, 4, 5, 6, −
// Row 7: RST, 1, 2, 3, +
// Row 8: R/S, 0, ., +/−, =
static const uint8_t KEY_MAP[9][5] = {
    {KC_A,        KC_B,        KC_C,        KC_D,        KC_E       },
    {KC_2ND,      KC_INV,      KC_LNX,      KC_CE,       KC_CLR     },
    {KC_LRN,      KC_XET,      KC_X2,       KC_SQRT,     KC_INV_X   },
    {KC_SST,      KC_STO,      KC_RCL,      KC_SUM,      KC_YX      },
    {KC_BST,      KC_EE,       KC_LPAR,     KC_RPAR,     KC_DIV     },
    {KC_GTO,      7,           8,           9,           KC_MUL     },
    {KC_SBR,      4,           5,           6,           KC_SUB     },
    {KC_RST,      1,           2,           3,           KC_ADD     },
    {KC_RS,       0,           KC_DOT,      KC_PM,       KC_EQ      },
};

// keycode_2nd() — maps a base keycode to its 2nd-function keycode
// This is used at RUNTIME when 2nd is pressed before a key.
// In LRN mode, the raw keycode + 2nd prefix are stored separately.
static uint8_t keycode_2nd(uint8_t kc) {
    switch (kc) {
        case KC_A:       return KC_A_PRIME;
        case KC_B:       return KC_B_PRIME;
        case KC_C:       return KC_C_PRIME;
        case KC_D:       return KC_D_PRIME;
        case KC_E:       return KC_E_PRIME;
        case KC_LNX:     return KC_LOG;
        case KC_LRN:     return KC_PGM;
        case KC_XET:     return KC_P_R;
        case KC_X2:      return KC_SIN;
        case KC_SQRT:    return KC_COS;
        case KC_INV_X:   return KC_TAN;
        case KC_SST:     return KC_INS;
        case KC_STO:     return KC_CMS;
        case KC_RCL:     return KC_EXC;
        case KC_SUM:     return KC_PROD;
        case KC_YX:      return KC_IND;
        case KC_BST:     return KC_DEL;
        case KC_EE:      return KC_ENG;
        case KC_LPAR:    return KC_FIX;
        case KC_RPAR:    return KC_INT;
        case KC_DIV:     return KC_ABS;
        case KC_GTO:     return KC_PAUSE;
        case 7:          return KC_XEQ_T;
        case 8:          return KC_NOP;
        case 9:          return KC_OP;
        case KC_MUL:     return KC_DEG;
        case KC_SBR:     return KC_LBL;
        case 4:          return KC_XGE_T;
        case 5:          return KC_SIGP;
        case 6:          return KC_XBAR;
        case KC_SUB:     return KC_RAD;
        case KC_RST:     return KC_STFL;
        case 1:          return KC_IFFL;
        case 2:          return KC_DMS;
        case 3:          return KC_PI;
        case KC_ADD:     return KC_GRAD;
        case KC_RS:      return KC_WRITE;
        case 0:          return KC_DSZ;
        case KC_DOT:     return KC_ADV;
        case KC_PM:      return KC_PRT;
        case KC_EQ:      return KC_LIST;
        case KC_CLR:     return KC_CLR_2ND;   // 2nd CLR = Clear (code 20)
        case KC_CE:      return KC_CP;        // 2nd CE = CP (code 29)
        default:         return kc;
    }
}

// ═══════════════════════════════════════════════════════════
// HELPER FILE I/O
// ═══════════════════════════════════════════════════════════

static void append_to_file(const char* path, const char* data) {
    File f = SPIFFS.open(path, FILE_APPEND);
    if (!f) f = SPIFFS.open(path, FILE_WRITE);
    if (f) { f.print(data); f.close(); }
}

static void write_file(const char* path, const char* data) {
    File f = SPIFFS.open(path, FILE_WRITE);
    if (f) { f.print(data); f.close(); }
}

// ═══════════════════════════════════════════════════════════
// BCD ARITHMETIC
// ═══════════════════════════════════════════════════════════

// ── Forward declarations ───────────────────────────────────

static int  bcd_get_exp(const BCD_Reg *r);
static void bcd_set_exp(BCD_Reg *r, int exp);
static void bcd_get_mantissa(const BCD_Reg *r, uint8_t *mant);
static void bcd_set_mantissa(BCD_Reg *r, const uint8_t *mant);
static int  cmp_mant(const uint8_t *a, const uint8_t *b);
static void shr_mant(uint8_t *mant, int n);
static void shl_mant(uint8_t *mant, int n);
static int  add_mant(uint8_t *res, const uint8_t *a, const uint8_t *b);
static int  sub_mant(uint8_t *res, const uint8_t *a, const uint8_t *b);
static void mul_mant(const uint8_t *a, const uint8_t *b, uint8_t *res);
static void bcd_clamp(BCD_Reg *r, CPUFlags *flags);
static void bcd_add(BCD_Reg *result, const BCD_Reg *a, const BCD_Reg *b, CPUFlags *flags);
static void bcd_sub(BCD_Reg *result, const BCD_Reg *a, const BCD_Reg *b, CPUFlags *flags);
static void bcd_mul(BCD_Reg *result, const BCD_Reg *a, const BCD_Reg *b, CPUFlags *flags);
static void bcd_div(BCD_Reg *result, const BCD_Reg *a, const BCD_Reg *b, CPUFlags *flags);
static void bcd_inv(BCD_Reg *result, const BCD_Reg *a, CPUFlags *flags);
// Forward declarations for external functions
void format_display(TMS1500_State *cpu);
static void rebuild_labels(TMS1500_State *cpu);
void prog_store_step(TMS1500_State *cpu, uint8_t kc);
uint8_t prog_read_step(TMS1500_State *cpu, uint16_t addr);
char* build_library_listing(size_t *out_len);

// ── Interfaccia pubblica ───────────────────────────────────

void bcd_zero(BCD_Reg *r) { memset(r->n, 0, REG_WIDTH); }
void bcd_copy(BCD_Reg *dst, const BCD_Reg *src) { memcpy(dst->n, src->n, REG_WIDTH); }

bool bcd_is_zero(const BCD_Reg *r) {
    for (int i = 2; i < REG_WIDTH; i++) if (r->n[i] != 0) return false;
    return true;
}

void bcd_from_int(BCD_Reg *r, int32_t v) {
    bcd_from_double(r, (double)v);
}

// Precisione interna della mantissa: 13 cifre, come il TI-59 reale
// (che internamente calcola con 13 cifre pur mostrandone solo 10-11 a
// display, per arrotondamenti fedeli). REG_WIDTH (18, in tms1500.h) ha
// spazio a sufficienza: n[0]=segno valore, n[1]=segno esponente,
// n[2..3]=esponente (2 cifre), n[4..4+MANT_DIGITS-1]=mantissa, il resto
// resta di riserva.
#define MANT_DIGITS 13

double bcd_to_double(const BCD_Reg *r) {
    double mantissa = 0.0, place = 1.0;
    for (int i = 4 + MANT_DIGITS - 1; i >= 4; i--) {
        int d = r->n[i]; if (d < 0) d = 0; if (d > 9) d = 9;
        mantissa += d * place; place *= 10.0;
    }
    mantissa /= pow(10.0, MANT_DIGITS - 1);
    int exp = r->n[2] * 10 + r->n[3];
    if (r->n[1] < 0) exp = -exp;
    double result = mantissa * pow(10.0, exp);
    if (r->n[0] < 0) result = -result;
    return result;
}

void bcd_from_double(BCD_Reg *r, double v) {
    bcd_zero(r);
    if (isinf(v) || isnan(v)) {
        for (int i = 0; i < REG_WIDTH; i++) r->n[i] = 9;
        // Overflow/invalido: segnala l'errore come fanno bcd_clamp/bcd_div,
        // altrimenti l'overflow restava silenzioso e il calcolo proseguiva
        // con 9.9999999 99 come se nulla fosse. La CPU mostrerà il
        // lampeggio di errore (display.cpp) e il programma si fermerà.
        if (g_bound_cpu) g_bound_cpu->flags.error = true;
        return;
    }
    if (v < 0) { r->n[0] = -1; v = -v; }
    if (v == 0.0) return;
    double logv = log10(v);
    if (logv > 99.0) {
        r->n[1] = 0; r->n[2] = 9; r->n[3] = 9;
        for (int i = 4; i < REG_WIDTH; i++) r->n[i] = 9;
        if (g_bound_cpu) g_bound_cpu->flags.error = true;
        return;
    }
    if (logv < -99.0) {
        // Underflow: value is too small to represent; return +0.
        // (The sign-restore that was here was dead code: bcd_zero() had
        // already cleared n[0], so the conditional always evaluated to 0.)
        bcd_zero(r);
        return;
    }
    int exp = (int)floor(logv);
    double mantissa = v / pow(10.0, exp - (MANT_DIGITS - 1));
    mantissa = round(mantissa);
    if (mantissa >= pow(10.0, MANT_DIGITS)) { mantissa /= 10.0; exp++; }
    int aexp = abs(exp);
    r->n[1] = (exp < 0) ? -1 : 0;
    r->n[2] = (aexp / 10) % 10;
    r->n[3] = aexp % 10;
    long long mant_int = (long long)mantissa;
    for (int i = 4 + MANT_DIGITS - 1; i >= 4; i--) { r->n[i] = mant_int % 10; mant_int /= 10; }
}

// ── Helper BCD interni ─────────────────────────────────────

static int bcd_get_exp(const BCD_Reg *r) {
    int exp = r->n[2] * 10 + r->n[3];
    if (r->n[1] < 0) exp = -exp;
    return exp;
}

static void bcd_set_exp(BCD_Reg *r, int exp) {
    if (exp < 0) { r->n[1] = -1; exp = -exp; } else { r->n[1] = 0; }
    r->n[2] = (exp / 10) % 10;
    r->n[3] = exp % 10;
}

static void bcd_get_mantissa(const BCD_Reg *r, uint8_t *mant) {
    for (int i = 0; i < MANT_DIGITS; i++) {
        int d = r->n[4 + i];
        if (d < 0) d = 0; if (d > 9) d = 9;
        mant[i] = (uint8_t)d;
    }
}

static void bcd_set_mantissa(BCD_Reg *r, const uint8_t *mant) {
    for (int i = 0; i < MANT_DIGITS; i++) r->n[4 + i] = mant[i];
}

static int cmp_mant(const uint8_t *a, const uint8_t *b) {
    for (int i = 0; i < MANT_DIGITS; i++) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }
    return 0;
}

static void shr_mant(uint8_t *mant, int n) {
    if (n <= 0) return;
    if (n >= MANT_DIGITS) { memset(mant, 0, MANT_DIGITS); return; }
    for (int i = MANT_DIGITS - 1; i >= n; i--) mant[i] = mant[i - n];
    for (int i = 0; i < n; i++) mant[i] = 0;
}

static void shl_mant(uint8_t *mant, int n) {
    if (n <= 0) return;
    if (n >= MANT_DIGITS) { memset(mant, 0, MANT_DIGITS); return; }
    for (int i = 0; i < MANT_DIGITS - n; i++) mant[i] = mant[i + n];
    for (int i = MANT_DIGITS - n; i < MANT_DIGITS; i++) mant[i] = 0;
}

static int add_mant(uint8_t *res, const uint8_t *a, const uint8_t *b) {
    int carry = 0;
    for (int i = MANT_DIGITS - 1; i >= 0; i--) {
        int sum = a[i] + b[i] + carry;
        res[i] = (uint8_t)(sum % 10);
        carry = sum / 10;
    }
    return carry;
}

static int sub_mant(uint8_t *res, const uint8_t *a, const uint8_t *b) {
    int borrow = 0;
    for (int i = MANT_DIGITS - 1; i >= 0; i--) {
        int diff = a[i] - b[i] - borrow;
        if (diff < 0) { diff += 10; borrow = 1; } else { borrow = 0; }
        res[i] = (uint8_t)diff;
    }
    return borrow;
}

static void mul_mant(const uint8_t *a, const uint8_t *b, uint8_t *res) {
    memset(res, 0, 2 * MANT_DIGITS);
    for (int i = MANT_DIGITS - 1; i >= 0; i--) {
        for (int j = MANT_DIGITS - 1; j >= 0; j--) {
            int k = i + j + 1;
            int prod = a[i] * b[j] + res[k];
            res[k] = (uint8_t)(prod % 10);
            int carry = prod / 10;
            int m = k - 1;
            while (carry > 0 && m >= 0) {
                int sum = res[m] + carry;
                res[m] = (uint8_t)(sum % 10);
                carry = sum / 10;
                m--;
            }
        }
    }
}

// ── Divisione lunga BCD ────────────────────────────────────
// cmp_mant_ext/sub_mant_ext operano su buffer larghi MANT_DIGITS+1 (una
// cifra in più per l'allineamento durante la divisione lunga) — il nome
// storico "12" viene da quando MANT_DIGITS era 11; la dimensione reale
// ora è MANT_DIGITS+1.
static int cmp_mant_ext(const uint8_t *a, const uint8_t *b) {
    for (int i = 0; i < MANT_DIGITS + 1; i++) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }
    return 0;
}

static int sub_mant_ext(uint8_t *res, const uint8_t *a, const uint8_t *b) {
    int borrow = 0;
    for (int i = MANT_DIGITS; i >= 0; i--) {
        int diff = a[i] - b[i] - borrow;
        if (diff < 0) { diff += 10; borrow = 1; } else { borrow = 0; }
        res[i] = (uint8_t)diff;
    }
    return borrow;
}

static void div_mant(const uint8_t *a, const uint8_t *b, uint8_t *quot, uint8_t *rem) {
    uint8_t w[MANT_DIGITS + 1];
    memset(w, 0, MANT_DIGITS + 1);
    w[0] = 0; // Spazio per l'allineamento e il carry
    for (int i = 0; i < MANT_DIGITS; i++) w[i + 1] = a[i];

    memset(quot, 0, 2 * MANT_DIGITS);

    // MANT_DIGITS+2 iterazioni per ottenere MANT_DIGITS cifre valide +
    // eventuale shift + una cifra per l'arrotondamento.
    for (int i = 0; i < MANT_DIGITS + 2; i++) {
        int d = 0;

        // Trova la cifra massima 'd' (0..9) tale che d * b <= w
        for (d = 9; d >= 0; d--) {
            uint8_t trial[MANT_DIGITS + 1];
            int carry = 0;
            for (int j = MANT_DIGITS - 1; j >= 0; j--) {
                int p = d * b[j] + carry;
                trial[j + 1] = (uint8_t)(p % 10);
                carry = p / 10;
            }
            trial[0] = (uint8_t)carry;

            if (cmp_mant_ext(w, trial) >= 0) break;
        }

        quot[i] = (uint8_t)d;

        // w = w - d * b
        if (d > 0) {
            uint8_t trial[MANT_DIGITS + 1];
            int carry = 0;
            for (int j = MANT_DIGITS - 1; j >= 0; j--) {
                int p = d * b[j] + carry;
                trial[j + 1] = (uint8_t)(p % 10);
                carry = p / 10;
            }
            trial[0] = (uint8_t)carry;

            sub_mant_ext(w, w, trial);
        }

        // w = w * 10 (Shift a sinistra per la prossima iterazione)
        for (int j = 0; j < MANT_DIGITS; j++) w[j] = w[j + 1];
        w[MANT_DIGITS] = 0;
    }

    if (rem) {
        for (int i = 0; i < MANT_DIGITS + 1; i++) rem[i] = w[i];
    }
}

// ── Overflow / Underflow ───────────────────────────────────

static void bcd_clamp(BCD_Reg *r, CPUFlags *flags) {
    int exp = bcd_get_exp(r);
    if (exp > 99) {
        int sign = (r->n[0] < 0) ? -1 : 1;
        for (int i = 0; i < REG_WIDTH; i++) r->n[i] = 9;
        r->n[0] = (sign < 0) ? -1 : 0;
        r->n[1] = 0; r->n[2] = 9; r->n[3] = 9;
        if (flags) flags->error = true;
    } else if (exp < -99) {
        bcd_zero(r);
    }
}

// ── Operazioni fondamentali BCD ──────────────────────────────

static void bcd_add(BCD_Reg *result, const BCD_Reg *a, const BCD_Reg *b, CPUFlags *flags) {
    if (bcd_is_zero(a)) { bcd_copy(result, b); return; }
    if (bcd_is_zero(b)) { bcd_copy(result, a); return; }

    int sign_a = (a->n[0] < 0) ? -1 : 1;
    int sign_b = (b->n[0] < 0) ? -1 : 1;
    int exp_a  = bcd_get_exp(a);
    int exp_b  = bcd_get_exp(b);

    uint8_t mant_a[MANT_DIGITS], mant_b[MANT_DIGITS];
    bcd_get_mantissa(a, mant_a);
    bcd_get_mantissa(b, mant_b);

    if (sign_a != sign_b) {
        BCD_Reg tmp;
        bcd_copy(&tmp, b);
        tmp.n[0] = (tmp.n[0] == 0) ? -1 : 0;   // inverti segno: 0 -> -1, -1 -> 0
        bcd_sub(result, a, &tmp, flags);
        return;
    }

    int diff = exp_a - exp_b;
    if (diff >= MANT_DIGITS) { bcd_copy(result, a); return; }
    if (diff <= -MANT_DIGITS) { bcd_copy(result, b); return; }

    if (diff > 0) {
        shr_mant(mant_b, diff);
    } else if (diff < 0) {
        shr_mant(mant_a, -diff);
        exp_a = exp_b;
    }

    uint8_t res_mant[MANT_DIGITS];
    int carry = add_mant(res_mant, mant_a, mant_b);

    if (carry) {
        for (int i = MANT_DIGITS - 1; i > 0; i--) res_mant[i] = res_mant[i - 1];
        res_mant[0] = 1;
        exp_a++;
    }

    bcd_zero(result);
    result->n[0] = (sign_a < 0) ? -1 : 0;
    bcd_set_mantissa(result, res_mant);
    bcd_set_exp(result, exp_a);
    bcd_clamp(result, flags);
}

static void bcd_sub(BCD_Reg *result, const BCD_Reg *a, const BCD_Reg *b, CPUFlags *flags) {
    if (bcd_is_zero(a)) {
        bcd_copy(result, b);
        result->n[0] = (result->n[0] == 0) ? -1 : 0;
        return;
    }
    if (bcd_is_zero(b)) {
        bcd_copy(result, a);
        return;
    }

    int sign_a = (a->n[0] < 0) ? -1 : 1;
    int sign_b = (b->n[0] < 0) ? -1 : 1;
    int exp_a  = bcd_get_exp(a);
    int exp_b  = bcd_get_exp(b);

    uint8_t mant_a[MANT_DIGITS], mant_b[MANT_DIGITS];
    bcd_get_mantissa(a, mant_a);
    bcd_get_mantissa(b, mant_b);

    if (sign_a != sign_b) {
        BCD_Reg tmp;
        bcd_copy(&tmp, b);
        tmp.n[0] = (tmp.n[0] == 0) ? -1 : 0;   // inverti segno: 0 -> -1, -1 -> 0
        bcd_add(result, a, &tmp, flags);
        return;
    }

    int diff = exp_a - exp_b;
    if (diff >= MANT_DIGITS) { bcd_copy(result, a); return; }
    if (diff <= -MANT_DIGITS) {
        bcd_copy(result, b);
        result->n[0] = (result->n[0] == 0) ? -1 : 0;
        return;
    }

    if (diff > 0) {
        shr_mant(mant_b, diff);
    } else if (diff < 0) {
        shr_mant(mant_a, -diff);
        exp_a = exp_b;
    }

    int cmp = cmp_mant(mant_a, mant_b);
    uint8_t res_mant[MANT_DIGITS];
    int res_sign = sign_a;
    int res_exp  = exp_a;

    if (cmp == 0) {
        bcd_zero(result);
        return;
    } else if (cmp < 0) {
        sub_mant(res_mant, mant_b, mant_a);
        res_sign = -sign_a;
    } else {
        sub_mant(res_mant, mant_a, mant_b);
    }

    int lz = 0;
    while (lz < MANT_DIGITS - 1 && res_mant[lz] == 0) lz++;
    if (lz > 0) {
        shl_mant(res_mant, lz);
        res_exp -= lz;
    }

    bcd_zero(result);
    result->n[0] = (res_sign < 0) ? -1 : 0;
    bcd_set_mantissa(result, res_mant);
    bcd_set_exp(result, res_exp);
    bcd_clamp(result, flags);
}

static void bcd_mul(BCD_Reg *result, const BCD_Reg *a, const BCD_Reg *b, CPUFlags *flags) {
    if (bcd_is_zero(a) || bcd_is_zero(b)) {
        bcd_zero(result);
        return;
    }

    int sign_a = (a->n[0] < 0) ? -1 : 1;
    int sign_b = (b->n[0] < 0) ? -1 : 1;
    int exp_a  = bcd_get_exp(a);
    int exp_b  = bcd_get_exp(b);

    uint8_t mant_a[MANT_DIGITS], mant_b[MANT_DIGITS];
    bcd_get_mantissa(a, mant_a);
    bcd_get_mantissa(b, mant_b);

    uint8_t prod[2 * MANT_DIGITS];
    mul_mant(mant_a, mant_b, prod);

    int res_exp = exp_a + exp_b;
    uint8_t res_mant[MANT_DIGITS];

    if (prod[0] != 0) {
        // Prodotto in [10.0, 100.0): le prime MANT_DIGITS cifre sono la
        // mantissa (rappresentano un valore in [1.0,10.0) dopo il bump
        // di esponente sotto), prod[MANT_DIGITS] è la cifra di guardia.
        for (int i = 0; i < MANT_DIGITS; i++) res_mant[i] = prod[i];
        if (prod[MANT_DIGITS] >= 5) {
            uint8_t one[MANT_DIGITS] = {0};
            one[MANT_DIGITS - 1] = 1;
            int c = add_mant(res_mant, res_mant, one);
            if (c) {
                for (int i = MANT_DIGITS - 1; i > 0; i--) res_mant[i] = res_mant[i - 1];
                res_mant[0] = 1;
                res_exp++;
            }
        }
        res_exp++;
    } else {
        // Prodotto in [1.0, 10.0): salta lo zero iniziale in prod[0] e
        // prendi le cifre 1..MANT_DIGITS come mantissa, prod[MANT_DIGITS+1]
        // come cifra di guardia.
        for (int i = 0; i < MANT_DIGITS; i++) res_mant[i] = prod[i + 1];
        if (prod[MANT_DIGITS + 1] >= 5) {
            uint8_t one[MANT_DIGITS] = {0};
            one[MANT_DIGITS - 1] = 1;
            int c = add_mant(res_mant, res_mant, one);
            if (c) {
                for (int i = MANT_DIGITS - 1; i > 0; i--) res_mant[i] = res_mant[i - 1];
                res_mant[0] = 1;
                res_exp++;
            }
        }
    }

    bcd_zero(result);
    result->n[0] = (sign_a * sign_b < 0) ? -1 : 0;
    bcd_set_mantissa(result, res_mant);
    bcd_set_exp(result, res_exp);
    bcd_clamp(result, flags);
}

static void bcd_div(BCD_Reg *result, const BCD_Reg *a, const BCD_Reg *b, CPUFlags *flags) {
    if (bcd_is_zero(b)) {
        if (flags) flags->error = true;
        bcd_zero(result);
        for (int i = 0; i < REG_WIDTH; i++) result->n[i] = 9;
        return;
    }
    if (bcd_is_zero(a)) {
        bcd_zero(result);
        return;
    }

    int sign_a = (a->n[0] < 0) ? -1 : 1;
    int sign_b = (b->n[0] < 0) ? -1 : 1;
    int exp_a  = bcd_get_exp(a);
    int exp_b  = bcd_get_exp(b);

    uint8_t mant_a[MANT_DIGITS], mant_b[MANT_DIGITS];
    bcd_get_mantissa(a, mant_a);
    bcd_get_mantissa(b, mant_b);

    uint8_t quot[2 * MANT_DIGITS], rem[MANT_DIGITS + 1];

    // CORRETTO: Dividendo (mant_a) e poi Divisore (mant_b)
    div_mant(mant_a, mant_b, quot, rem);

    int res_exp = exp_a - exp_b;
    uint8_t res_mant[MANT_DIGITS];

    // Se quot[0] è != 0 (es. 4/2 = 2.0), mantissa è già posizionata
    if (quot[0] != 0) {
        for (int i = 0; i < MANT_DIGITS; i++) res_mant[i] = quot[i];

        // Arrotondamento (cifra di guardia in quot[MANT_DIGITS])
        if (quot[MANT_DIGITS] >= 5) {
            uint8_t one[MANT_DIGITS] = {0};
            one[MANT_DIGITS - 1] = 1;
            int c = add_mant(res_mant, res_mant, one);
            if (c) {
                for (int i = MANT_DIGITS - 1; i > 0; i--) res_mant[i] = res_mant[i - 1];
                res_mant[0] = 1;
                res_exp++;
            }
        }
    }
    // Se quot[0] == 0 (es. 2/4 = 0.5), scaliamo di una posizione
    else {
        for (int i = 0; i < MANT_DIGITS; i++) res_mant[i] = quot[i + 1];
        res_exp--;

        // Arrotondamento (cifra di guardia in quot[MANT_DIGITS+1] dopo lo shift)
        if (quot[MANT_DIGITS + 1] >= 5) {
            uint8_t one[MANT_DIGITS] = {0};
            one[MANT_DIGITS - 1] = 1;
            int c = add_mant(res_mant, res_mant, one);
            if (c) {
                for (int i = MANT_DIGITS - 1; i > 0; i--) res_mant[i] = res_mant[i - 1];
                res_mant[0] = 1;
                res_exp++; // Gestisce casi come 0.9999... -> 1.0000...
            }
        }
    }

    bcd_zero(result);
    result->n[0] = (sign_a * sign_b < 0) ? -1 : 0;
    bcd_set_mantissa(result, res_mant);
    bcd_set_exp(result, res_exp);
    bcd_clamp(result, flags);
}

// ── Operazioni BCD aggiuntive ──────────────────────────────

static void bcd_inv(BCD_Reg *result, const BCD_Reg *a, CPUFlags *flags) {
    BCD_Reg one;
    bcd_zero(&one);
    one.n[4] = 1;
    bcd_div(result, &one, a, flags);
}

// ═══════════════════════════════════════════════════════════
// MATEMATICA — wrapper funzioni standard C
// ═══════════════════════════════════════════════════════════

static void math_sqrt(TMS1500_State *cpu) {
    busy_start(BUSY_MS_SQRT);
    double v = bcd_to_double(&cpu->reg[REG_A]);
    if (v < 0.0) { cpu->flags.error = true; return; }
    bcd_from_double(&cpu->reg[REG_A], sqrt(v));
}
static void math_x2(TMS1500_State *cpu) {
    bcd_mul(&cpu->reg[REG_A], &cpu->reg[REG_A], &cpu->reg[REG_A], &cpu->flags);
}
static void math_inv(TMS1500_State *cpu) {
    bcd_inv(&cpu->reg[REG_A], &cpu->reg[REG_A], &cpu->flags);
}
static void math_sin(TMS1500_State *cpu, bool arc) {
    busy_start(arc ? BUSY_MS_ATRIG : BUSY_MS_TRIG);
    double v = bcd_to_double(&cpu->reg[REG_A]);
    if (!arc) {
        double rad = v;
        if (cpu->trig_mode == 0) rad = v * M_PI / 180.0;
        else if (cpu->trig_mode == 2) rad = v * M_PI / 200.0;
        v = sin(rad);
    } else {
        if (fabs(v) > 1.0) { cpu->flags.error = true; return; }
        v = asin(v);
        if (cpu->trig_mode == 0) v = v * 180.0 / M_PI;
        else if (cpu->trig_mode == 2) v = v * 200.0 / M_PI;
    }
    bcd_from_double(&cpu->reg[REG_A], v);
}
static void math_cos(TMS1500_State *cpu, bool arc) {
    busy_start(arc ? BUSY_MS_ATRIG : BUSY_MS_TRIG);
    double v = bcd_to_double(&cpu->reg[REG_A]);
    if (!arc) {
        double rad = v;
        if (cpu->trig_mode == 0) rad = v * M_PI / 180.0;
        else if (cpu->trig_mode == 2) rad = v * M_PI / 200.0;
        v = cos(rad);
    } else {
        if (fabs(v) > 1.0) { cpu->flags.error = true; return; }
        v = acos(v);
        if (cpu->trig_mode == 0) v = v * 180.0 / M_PI;
        else if (cpu->trig_mode == 2) v = v * 200.0 / M_PI;
    }
    bcd_from_double(&cpu->reg[REG_A], v);
}
static void math_tan(TMS1500_State *cpu, bool arc) {
    busy_start(arc ? BUSY_MS_ATRIG : BUSY_MS_TRIG);
    double v = bcd_to_double(&cpu->reg[REG_A]);
    if (!arc) {
        double rad = v;
        if (cpu->trig_mode == 0) {
            /* tan(90°), tan(270°), etc. are undefined */
            double reduced = fmod(fabs(v), 180.0);
            if (fabs(reduced - 90.0) < 1e-12) {
                cpu->flags.error = true; return;
            }
            rad = v * M_PI / 180.0;
        }
        else if (cpu->trig_mode == 2) rad = v * M_PI / 200.0;
        v = tan(rad);
    } else {
        v = atan(v);
        if (cpu->trig_mode == 0) v = v * 180.0 / M_PI;
        else if (cpu->trig_mode == 2) v = v * 200.0 / M_PI;
    }
    bcd_from_double(&cpu->reg[REG_A], v);
}
static void math_log(TMS1500_State *cpu, bool natural) {
    busy_start(BUSY_MS_LOG);
    double v = bcd_to_double(&cpu->reg[REG_A]);
    if (v <= 0.0) { cpu->flags.error = true; return; }
    if (natural) v = log(v);
    else v = log10(v);
    bcd_from_double(&cpu->reg[REG_A], v);
}
static void math_exp(TMS1500_State *cpu, bool base10) {
    busy_start(BUSY_MS_EXP);
    double v = bcd_to_double(&cpu->reg[REG_A]);
    if (base10) v = pow(10.0, v);
    else v = exp(v);
    bcd_from_double(&cpu->reg[REG_A], v);
}
static void math_p2r(TMS1500_State *cpu) {
    // P→R (TI-59 reale): si digita prima θ (l'ENTER/stack-lift lo sposta
    // in B), poi r che resta in display/A. Risultato: x→display(A), y→B.
    double r     = bcd_to_double(&cpu->reg[REG_A]);   // A = r (ultimo digitato)
    double theta = bcd_to_double(&cpu->reg[REG_B]);   // B = θ (spinto su dallo stack-lift)
    double rad = theta;
    if (cpu->trig_mode == 0) rad = theta * M_PI / 180.0;
    else if (cpu->trig_mode == 2) rad = theta * M_PI / 200.0;
    double x = r * cos(rad);
    double y = r * sin(rad);
    bcd_from_double(&cpu->reg[REG_A], x);   // x → display
    bcd_from_double(&cpu->reg[REG_B], y);   // y → B
}
static void math_r2p(TMS1500_State *cpu) {
    // INV P→R = R→P: si digita prima y (spinto in B), poi x che resta in
    // display/A. Risultato: r→display(A), θ→B.
    double x = bcd_to_double(&cpu->reg[REG_A]);   // A = x (ultimo digitato)
    double y = bcd_to_double(&cpu->reg[REG_B]);   // B = y (spinto su dallo stack-lift)
    double r = sqrt(x*x + y*y);
    double theta = atan2(y, x);
    if (cpu->trig_mode == 0) theta = theta * 180.0 / M_PI;
    else if (cpu->trig_mode == 2) theta = theta * 200.0 / M_PI;
    bcd_from_double(&cpu->reg[REG_A], r);   // r → display
    bcd_from_double(&cpu->reg[REG_B], theta);  // θ → B
}

static double dms_to_decimal(double dms) {
    int sign = (dms < 0) ? -1 : 1;
    dms = fabs(dms);
    int deg = (int)dms;
    double min_sec = (dms - deg) * 100.0;
    int min = (int)min_sec;
    double sec = (min_sec - min) * 100.0;
    return sign * (deg + min / 60.0 + sec / 3600.0);
}
static double decimal_to_dms(double dec) {
    int sign = (dec < 0) ? -1 : 1;
    dec = fabs(dec);
    int deg = (int)dec;
    double min_f = (dec - deg) * 60.0;
    int min = (int)min_f;
    double sec = (min_f - min) * 60.0;
    return sign * (deg + min / 100.0 + sec / 10000.0);
}

// ═══════════════════════════════════════════════════════════
// HELPER
// ═══════════════════════════════════════════════════════════

static uint16_t find_label(TMS1500_State *cpu, uint8_t label_kc) {
    if (showing_lib_prog) {
        int idx = label_index_for_key(label_kc);
        if (idx >= 0) return lib_custom_label_pc[idx];

        // Label is not A-E/A'-E' (e.g. internal ROM labels like "=" = 95,
        // or "CE" = 24, or "CLR" = 25). The precomputed lib_custom_label_pc
        // table only covers the 10 user-accessible labels; for any other
        // keycode used as a label inside the ROM (SBR/GTO targets that are
        // internal to the library firmware), we do a scope-aware linear scan.
        //
        // Priority mirrors rebuild_lib_labels:
        //   1. Within the designated program's own scope (highest priority)
        //   2. Before scope (shared ROM routines earlier in the image)
        //   3. After scope  (shared ROM routines later in the image)
        const LibraryModule *m = library_get_active();
        if (!m) return 0xFFFF;

        uint16_t scope_end = lib_scope_addr + lib_scope_len;
        if (scope_end > m->rom_size) scope_end = m->rom_size;

        // Pass 1: within the designated program's scope
        for (uint16_t i = lib_scope_addr; i + 1 < scope_end; i++) {
            if (m->rom[i] == KC_LBL && m->rom[i + 1] == label_kc) return i;
        }
        // Pass 2: before the scope (shared subroutines preceding the program)
        for (uint16_t i = 0; i + 1 < lib_scope_addr; i++) {
            if (m->rom[i] == KC_LBL && m->rom[i + 1] == label_kc) return i;
        }
        // Pass 3: after the scope (shared subroutines following the program)
        for (uint16_t i = scope_end; i + 1 < m->rom_size; i++) {
            if (m->rom[i] == KC_LBL && m->rom[i + 1] == label_kc) return i;
        }
        return 0xFFFF;
    }
    for (uint16_t i = 0; i + 1 < cpu->prog_len; i++) {
        if (cpu->prog[i] == KC_LBL && cpu->prog[i+1] == label_kc) return i;
    }
    return 0xFFFF;
}

/* Esegue il DSZ vero e proprio: decrementa il VALORE ASSOLUTO del registro
 * verso zero (senza mai superarlo, comportamento documentato della TI-59
 * reale — un contatore negativo va verso 0, non verso -N). Se il risultato
 * non è zero, salta a target_pc (il loop continua); se è zero, non salta
 * (l'esecuzione prosegue con l'istruzione successiva, il loop finisce). */
static void dsz_do(TMS1500_State *cpu, int reg, uint16_t target_pc) {
    reg %= 100;
    BCD_Reg *bank = active_ram_bank(cpu);
    double v = bcd_to_double(&bank[reg]);
    if (v > 0)      { v -= 1.0; if (v < 0) v = 0; }
    else if (v < 0) { v += 1.0; if (v > 0) v = 0; }
    bcd_from_double(&bank[reg], v);
    if (fabs(v) > 1e-9) {
        /* target_pc arriva già risolto e assoluto dal chiamante (label
         * via find_label(), oppure indirizzo a 3 cifre già rilocato con
         * lib_scope_addr se in esecuzione "as-is" da modulo — vedi
         * chiamate in process_keycode()/exec_program_step()). Non va
         * più ridotto qui con "% exec_prog_len(cpu)": in modalità
         * libreria exec_prog_len() restituisce la lunghezza del solo
         * programma designato (es. 189 per ML-01), non l'indirizzo
         * assoluto nella ROM — wrappare qui perdeva l'offset
         * lib_scope_addr e faceva atterrare il salto fuori dal
         * programma (spesso nell'area prima del suo inizio). */
        cpu->prog_pc = target_pc;
    }
    /* v == 0: nessun salto, si prosegue in sequenza */
}

static uint8_t bcd_to_int_reg(const BCD_Reg *r) {
    // REG_WIDTH è 18: il loop accumula 14 cifre (i da 4 a 17). Un "int" a
    // 32 bit overflow oltre 10 cifre (2147483647), quindi i registri con
    // moltiplicando/indice grande (es. 99999999999999) producevano un
    // valore negativo e l'operazione indiretta puntava al registro sbagliato.
    // Accumulo in uint64_t: 14 cifre (max ~1e14) ci stanno senza problemi.
    uint64_t v = 0;
    for (int i = 4; i < REG_WIDTH; i++) {
        int d = r->n[i]; if (d < 0) d = 0; if (d > 9) d = 9;
        v = v * 10 + (uint64_t)d;
    }
    return (uint8_t)(v % 100);
}

/* Variante per GTO/SBR indiretto: il registro puntato contiene uno step
   di programma (000-959), non un numero di registro dati (00-99). */
static uint16_t bcd_to_int_step(const BCD_Reg *r) {
    uint64_t v = 0;
    for (int i = 4; i < REG_WIDTH; i++) {
        int d = r->n[i]; if (d < 0) d = 0; if (d > 9) d = 9;
        v = v * 10 + (uint64_t)d;
    }
    return (uint16_t)(v % 1000);
}

// Forward declarations: exec_op usa queste funzioni statistiche
static void stat_stddev_x(TMS1500_State *cpu);
static void stat_stddev_y(TMS1500_State *cpu);
static void stat_lr_slope(TMS1500_State *cpu);
static void stat_lr_intercept(TMS1500_State *cpu);
static void stat_correlation(TMS1500_State *cpu);
static bool stat_lr_coeffs(TMS1500_State *cpu, double *a, double *b);
static void input_clear(void);
static void input_commit(TMS1500_State *cpu);

static void exec_op(TMS1500_State *cpu, int op) {
    input_clear();
    switch (op) {
        // ═══════════════════════════════════════════════════════
        // Op 00-08: stampante/plotter PC-100A. Op 00, 05, 06, 07
        // sono ora collegati alla vera emulazione del buffer di
        // stampa (vedi printer.h/.cpp) e al backend BLE, quando
        // collegato — nessun effetto sui registri di calcolo, come
        // sull'hardware reale. Op 01-04 (riempimento gruppi
        // alfanumerici) restano no-op finché non è disponibile la
        // tabella completa a 64 simboli (Table VII, brevetto
        // US4153937) usata per decodificare i codici carattere
        // digitati: senza quella, decodificare in modo scorretto
        // sarebbe peggio che non stampare nulla. Op 08 (lista
        // etichette) richiede la stessa tabella per i mnemonici a 3
        // caratteri — no-op per lo stesso motivo.
        // ═══════════════════════════════════════════════════════
        case 0:
            printer_op00_init(&g_printer);
            break;
        case 1: case 2: case 3: case 4: {
            // Legge 10 cifre di mantissa da REG_A (n[4]..n[13]),
            // le interpreta come 5 coppie di codici carattere Table VII,
            // e le spedisce al gruppo stampante corrispondente.
            char pairs[6]; // 5 caratteri + NUL
            for (int i = 0; i < 5; i++) {
                int hi = cpu->reg[REG_A].n[4 + i * 2];
                int lo = cpu->reg[REG_A].n[4 + i * 2 + 1];
                if (hi < 0) hi = 0; if (hi > 9) hi = 9;
                if (lo < 0) lo = 0; if (lo > 9) lo = 9;
                int code = hi * 10 + lo;
                pairs[i] = printer_charcode_to_ascii((uint8_t)code);
            }
            pairs[5] = '\0';
            switch (op) {
                case 1: printer_op01_group1(&g_printer, pairs); break;
                case 2: printer_op02_group2(&g_printer, pairs); break;
                case 3: printer_op03_group3(&g_printer, pairs); break;
                case 4: printer_op04_group4(&g_printer, pairs); break;
            }
            break;
        }
        case 5:
            printer_op05_print_line(&g_printer);
            break;
        case 6: {
            char disp[16];
            format_value_string(cpu, disp, sizeof(disp));
            printer_op06_print_display(&g_printer, disp);
            break;
        }
        case 7: {
            double v = bcd_to_double(&cpu->reg[REG_A]);
            printer_op07_curve(&g_printer, (int)v);
            break;
        }
        case 8: {
            // Lista etichette del programma corrente
            char list[160] = "";
            int pos = 0;
            uint16_t len = exec_prog_len(cpu);
            const uint8_t *prog = showing_lib_prog ? library_get_active()->rom + lib_scope_addr : cpu->prog;
            for (uint16_t i = 0; i + 1 < len && pos < (int)sizeof(list) - 12; i++) {
                if (prog[i] == KC_LBL) {
                    uint8_t lab = prog[i + 1];
                    const char *name = NULL;
                    if (lab == KC_E_PRIME)       name = "E'";
                    else if (lab == KC_A)        name = "A";
                    else if (lab == KC_B)        name = "B";
                    else if (lab == KC_C)        name = "C";
                    else if (lab == KC_D)        name = "D";
                    else if (lab == KC_E)        name = "E";
                    else if (lab == KC_A_PRIME)  name = "A'";
                    else if (lab == KC_B_PRIME)  name = "B'";
                    else if (lab == KC_C_PRIME)  name = "C'";
                    else if (lab == KC_D_PRIME)  name = "D'";
                    if (name) {
                        pos += snprintf(list + pos, sizeof(list) - pos, "%s=%03u ", name, (unsigned)i);
                    }
                }
            }
            printer_op08_list_labels(&g_printer, list);
            break;
        }

        // ═══════════════════════════════════════════════════════
        // Op 09: scarica il programma designato con "2nd Pgm mm" in
        // memoria principale, a partire dal passo 000, sovrascrivendo
        // quanto c'era prima — esattamente come da manuale TI-59
        // (Personal Programming): "This procedure places the requested
        // program into program memory beginning at program location
        // 000. The downloaded program writes over any instructions
        // previously stored in that part of program memory." Da quel
        // momento è un programma come un altro: stessa memoria, stesse
        // LBL/GTO/SBR di sempre. Percorso alternativo a quello "as-is"
        // (un tasto etichetta dopo Pgm mm, senza passare da qui): qui
        // invece la modalità as-is va sempre chiusa, se per caso era
        // attiva, dato che si sta passando alla memoria normale.
        // ═══════════════════════════════════════════════════════
        case 9: {
            if (!lib_page_selected) {
                cpu->flags.error = true;   // Op 09 senza un Pgm mm precedente
                break;
            }
            uint16_t addr = 0, plen = 0;
            const char *title = nullptr;
            if (library_find_program(lib_selected_page, &addr, &plen, &title) &&
                plen <= PROG_SIZE) {
                const LibraryModule *m = library_get_active();
                memcpy(cpu->prog, m->rom + addr, plen);
                cpu->prog_len = plen;
                cpu->prog_pc  = 0;
                rebuild_labels(cpu);
                tms1500_mark_prog_saved();   // combacia con la sorgente (il modulo), non "modificato"
                Serial.printf("[LIB] Programma %02d scaricato: %s (%u passi, da modulo %s)\n",
                              lib_selected_page, title ? title : "?", plen, m->name);
            } else {
                cpu->flags.error = true;   // programma inesistente, o nessun modulo attivo
                Serial.printf("[LIB] Scarico fallito per il programma %02d (nessun modulo attivo, o numero inesistente)\n",
                              lib_selected_page);
            }
            lib_page_selected = false;
            showing_lib_prog  = false;   // torna comunque alla memoria normale
            break;
        }

        // ═══════════════════════════════════════════════════════
        // Op 10: funzione segno — restituisce il segno del valore
        // nel registro A: +1, 0, -1.
        // Op 11: varianza campionaria.
        // Op 12: pendenza e intercetta della regressione lineare.
        // Op 13: coefficiente di correlazione.
        // Op 14: stima di y (y') per x in A.
        // Op 15: stima di x (x') per y in A.
        // ═══════════════════════════════════════════════════════
        case 10: {
            double v = bcd_to_double(&cpu->reg[REG_A]);
            bcd_from_double(&cpu->reg[REG_A], (v > 0) ? 1.0 : (v < 0) ? -1.0 : 0.0);
            format_display(cpu);
            break;
        }
        case 11: {
            BCD_Reg *bank = active_ram_bank(cpu);
            double n = bcd_to_double(&bank[3]);
            if (n < 2) { cpu->flags.error = true; break; }
            double sum_x  = bcd_to_double(&bank[4]);
            double sum_x2 = bcd_to_double(&bank[5]);
            double num = n * sum_x2 - sum_x * sum_x;
            if (num < 0) num = 0;
            bcd_from_double(&cpu->reg[REG_A], num / (n * (n - 1)));
            format_display(cpu);
            break;
        }
        case 12: {
            double a, b;
            if (!stat_lr_coeffs(cpu, &a, &b)) { cpu->flags.error = true; break; }
            bcd_from_double(&cpu->reg[REG_A], b);
            format_display(cpu);
            break;
        }
        case 13: {
            stat_correlation(cpu);
            format_display(cpu);
            break;
        }
        case 14: {
            BCD_Reg *bank = active_ram_bank(cpu);
            double a, b;
            if (!stat_lr_coeffs(cpu, &a, &b)) { cpu->flags.error = true; break; }
            double x = bcd_to_double(&cpu->reg[REG_A]);
            bcd_from_double(&cpu->reg[REG_A], a + b * x);
            format_display(cpu);
            break;
        }
        case 15: {
            BCD_Reg *bank = active_ram_bank(cpu);
            double a, b;
            if (!stat_lr_coeffs(cpu, &a, &b)) { cpu->flags.error = true; break; }
            double y = bcd_to_double(&cpu->reg[REG_A]);
            if (fabs(b) < 1e-12) { cpu->flags.error = true; break; }
            bcd_from_double(&cpu->reg[REG_A], (y - a) / b);
            format_display(cpu);
            break;
        }

        // ═══════════════════════════════════════════════════════
        // Op 16: mostra la partizione corrente memoria/passi.
        // Op 17: imposta la partizione (uno dei codici Op più usati
        // nei programmi reali — confermato da due fonti indipendenti,
        // inclusa la documentazione dell'emulatore TI59C).
        // Questo firmware NON implementa un pool di memoria condiviso
        // e ripartizionabile: PROG_SIZE (960 passi) e RAM_SIZE (100
        // registri) sono sempre entrambi al massimo contemporaneamente,
        // quindi non possiamo davvero cambiare la partizione. Il punto
        // critico è che ora Op 17 non produce PIÙ un effetto collaterale
        // sbagliato (prima calcolava un coefficiente di correlazione
        // statistica e lo scriveva in un registro!). I programmi che la
        // usano solo per fissare la partizione continuano a funzionare
        // correttamente: la richiesta viene semplicemente ignorata,
        // dato che qui c'è comunque sempre la capacità massima.
        // ═══════════════════════════════════════════════════════
        case 16: case 17:
            break;

        // ═══════════════════════════════════════════════════════
        // Op 18-19: test flag di errore. CONFERMATO da fonte primaria
        // (TI Master Library Quick Reference Guide, "Special Control
        // Operations"): "18 If no error condition exists in a program,
        // set flag 7" / "19 If an error condition exists in a program,
        // set flag 7". Numerazione e semantica esatte, nessuna ipotesi.
        // ═══════════════════════════════════════════════════════
        case 18: if (!cpu->flags.error) user_flags[7] = true; break;
        case 19: if (cpu->flags.error)  user_flags[7] = true; break;

        // ═══════════════════════════════════════════════════════
        // Op 20-39: incrementa/decrementa registri dati 00-09.
        // CONFERMATO da fonte primaria (TI Master Library Quick
        // Reference Guide, "Special Control Operations"): "20-29
        // Increment a data register 0-9 by 1" / "30-39 Decrement a
        // data register 0-9 by 1". Codici Op reali della TI-59, non
        // un'estensione dell'emulatore come si pensava in precedenza.
        // ═══════════════════════════════════════════════════════
        case 20: case 21: case 22: case 23: case 24:
        case 25: case 26: case 27: case 28: case 29: {
            int reg = op % 10;
            BCD_Reg *bank = active_ram_bank(cpu);
            double v = bcd_to_double(&bank[reg]);
            bcd_from_double(&bank[reg], v + 1.0);
            break;
        }
        case 30: case 31: case 32: case 33: case 34:
        case 35: case 36: case 37: case 38: case 39: {
            int reg = op % 10;
            BCD_Reg *bank = active_ram_bank(cpu);
            double v = bcd_to_double(&bank[reg]);
            bcd_from_double(&bank[reg], v - 1.0);
            break;
        }

        // ═══════════════════════════════════════════════════════
        // Op 40: test "stampante collegata". ATTENZIONE: la fonte
        // primaria (TI Master Library QRG) documenta ufficialmente
        // solo i codici Op 00-39; Op 40 resta un'estensione non
        // verificata di questo emulatore e non un codice Op reale
        // confermato — possibile collisione futura se emergesse un
        // significato diverso per questo codice su hardware originale.
        // Nel frattempo riflette lo stato reale del backend BLE
        // (v. printer.h/ble_thermal_printer.h): flag 7 impostato solo
        // se una stampante è davvero connessa, così un programma che
        // dirama su questo test salta correttamente le sezioni di
        // stampa quando non c'è nulla collegato, invece di crederle
        // eseguite con successo.
        // ═══════════════════════════════════════════════════════
        case 40: if (printer_is_connected(&g_printer)) user_flags[7] = true; break;

        // ═══════════════════════════════════════════════════════
        // Op 90-94: funzioni statistiche dell'emulatore (deviazione
        // standard x/y, pendenza e intercetta della regressione
        // lineare, coefficiente di correlazione). NON corrispondono a
        // codici Op reali della TI-59 — su hardware originale queste
        // funzioni si richiamano con tasti dedicati (2nd s, 2nd LR...)
        // non ancora cablati sulla tastiera fisica di questo progetto.
        // Le ho spostate qui, fuori dalla fascia 00-19 potenzialmente
        // reale, per non rischiare più collisioni con la numerazione
        // originale mentre restano comunque disponibili.
        // ═══════════════════════════════════════════════════════
        case 90: stat_stddev_x(cpu);     format_display(cpu); break;
        case 91: stat_stddev_y(cpu);     format_display(cpu); break;
        case 92: stat_lr_slope(cpu);     format_display(cpu); break;
        case 93: stat_lr_intercept(cpu); format_display(cpu); break;
        case 94: stat_correlation(cpu);  format_display(cpu); break;

        // ═══════════════════════════════════════════════════════
        // Resto: non implementato / riservato.
        // ═══════════════════════════════════════════════════════
        default: break;
    }
}

// Registri statistici standard TI-59: R01=Σy, R02=Σy², R03=N, R04=Σx,
// R05=Σx², R06=Σxy — mappatura documentata sia da ML-01 ("Linear
// Regression Init... clears registers R01 through R06") sia dalla
// tabella Register Contents di ML-15 (Random Number Generator, che
// riusa questi stessi registri per calcolare media e deviazione
// standard dei numeri generati). Nessun accumulatore nascosto separato:
// i registri STESSI sono l'accumulatore, esattamente come sull'hardware
// reale — così un programma che scrive/legge R01-R06 direttamente resta
// sempre sincronizzato con Σ+/Σ-/x̄/deviazione standard/regressione.
static void stat_sigma_plus(TMS1500_State *cpu) {
    double x = bcd_to_double(&cpu->reg[REG_A]);
    double y = bcd_to_double(&cpu->reg[REG_B]);
    BCD_Reg *bank = active_ram_bank(cpu);
    double n     = bcd_to_double(&bank[3]) + 1;
    double sum_y  = bcd_to_double(&bank[1]) + y;
    double sum_y2 = bcd_to_double(&bank[2]) + y * y;
    double sum_x  = bcd_to_double(&bank[4]) + x;
    double sum_x2 = bcd_to_double(&bank[5]) + x * x;
    double sum_xy = bcd_to_double(&bank[6]) + x * y;
    bcd_from_double(&bank[1], sum_y);
    bcd_from_double(&bank[2], sum_y2);
    bcd_from_double(&bank[3], n);
    bcd_from_double(&bank[4], sum_x);
    bcd_from_double(&bank[5], sum_x2);
    bcd_from_double(&bank[6], sum_xy);
}

static void stat_sigma_minus(TMS1500_State *cpu) {
    BCD_Reg *bank = active_ram_bank(cpu);
    double n = bcd_to_double(&bank[3]);
    if (n <= 0) return;
    double x = bcd_to_double(&cpu->reg[REG_A]);
    double y = bcd_to_double(&cpu->reg[REG_B]);
    n -= 1;
    double sum_y  = bcd_to_double(&bank[1]) - y;
    double sum_y2 = bcd_to_double(&bank[2]) - y * y;
    double sum_x  = bcd_to_double(&bank[4]) - x;
    double sum_x2 = bcd_to_double(&bank[5]) - x * x;
    double sum_xy = bcd_to_double(&bank[6]) - x * y;
    bcd_from_double(&bank[1], sum_y);
    bcd_from_double(&bank[2], sum_y2);
    bcd_from_double(&bank[3], n);
    bcd_from_double(&bank[4], sum_x);
    bcd_from_double(&bank[5], sum_x2);
    bcd_from_double(&bank[6], sum_xy);
}

static void stat_mean(TMS1500_State *cpu) {
    BCD_Reg *bank = active_ram_bank(cpu);
    double n = bcd_to_double(&bank[3]);
    if (n > 0) {
        double sum_x = bcd_to_double(&bank[4]);
        bcd_from_double(&cpu->reg[REG_A], sum_x / n);
        format_display(cpu);
    }
}

static void stat_mean_y(TMS1500_State *cpu) {
    BCD_Reg *bank = active_ram_bank(cpu);
    double n = bcd_to_double(&bank[3]);
    if (n > 0) {
        double sum_y = bcd_to_double(&bank[1]);
        bcd_from_double(&cpu->reg[REG_A], sum_y / n);
        format_display(cpu);
    }
}

static void stat_stddev_x(TMS1500_State *cpu) {
    BCD_Reg *bank = active_ram_bank(cpu);
    double n = bcd_to_double(&bank[3]);
    if (n < 2) { cpu->flags.error = true; return; }
    double sum_x  = bcd_to_double(&bank[4]);
    double sum_x2 = bcd_to_double(&bank[5]);
    double num = n * sum_x2 - sum_x * sum_x;
    if (num < 0) num = 0;
    bcd_from_double(&cpu->reg[REG_A], sqrt(num / (n * (n - 1))));
}

static void stat_stddev_y(TMS1500_State *cpu) {
    BCD_Reg *bank = active_ram_bank(cpu);
    double n = bcd_to_double(&bank[3]);
    if (n < 2) { cpu->flags.error = true; return; }
    double sum_y  = bcd_to_double(&bank[1]);
    double sum_y2 = bcd_to_double(&bank[2]);
    double num = n * sum_y2 - sum_y * sum_y;
    if (num < 0) num = 0;
    bcd_from_double(&cpu->reg[REG_A], sqrt(num / (n * (n - 1))));
}

static bool stat_lr_coeffs(TMS1500_State *cpu, double *a, double *b) {
    BCD_Reg *bank = active_ram_bank(cpu);
    double n = bcd_to_double(&bank[3]);
    if (n < 2) return false;
    double sum_x  = bcd_to_double(&bank[4]);
    double sum_x2 = bcd_to_double(&bank[5]);
    double sum_y  = bcd_to_double(&bank[1]);
    double sum_xy = bcd_to_double(&bank[6]);
    double denom = n * sum_x2 - sum_x * sum_x;
    if (fabs(denom) < 1e-12) return false;
    *b = (n * sum_xy - sum_x * sum_y) / denom;
    *a = (sum_y - (*b) * sum_x) / n;
    return true;
}

static void stat_lr_slope(TMS1500_State *cpu) {
    double a, b;
    if (!stat_lr_coeffs(cpu, &a, &b)) { cpu->flags.error = true; return; }
    bcd_from_double(&cpu->reg[REG_A], b);
}

static void stat_lr_intercept(TMS1500_State *cpu) {
    double a, b;
    if (!stat_lr_coeffs(cpu, &a, &b)) { cpu->flags.error = true; return; }
    bcd_from_double(&cpu->reg[REG_A], a);
}

static void stat_correlation(TMS1500_State *cpu) {
    BCD_Reg *bank = active_ram_bank(cpu);
    double n = bcd_to_double(&bank[3]);
    if (n < 2) { cpu->flags.error = true; return; }
    double sum_x  = bcd_to_double(&bank[4]);
    double sum_x2 = bcd_to_double(&bank[5]);
    double sum_y  = bcd_to_double(&bank[1]);
    double sum_y2 = bcd_to_double(&bank[2]);
    double sum_xy = bcd_to_double(&bank[6]);
    double num = n * sum_xy - sum_x * sum_y;
    double dx  = n * sum_x2 - sum_x * sum_x;
    double dy  = n * sum_y2 - sum_y * sum_y;
    if (dx <= 0 || dy <= 0) { cpu->flags.error = true; return; }
    bcd_from_double(&cpu->reg[REG_A], num / sqrt(dx * dy));
}

// ── Shared arithmetic dispatch ──────────────────────────────────────────
// Executes cpu->pending_op on (cpu->operand_x  OP  cpu->reg[REG_A]) and
// stores the result back into cpu->reg[REG_A].  Sets cpu->flags.error on
// arithmetic errors.  Returns false if pending_op is NONE (no-op).
// Callers must reset cpu->pending_op to PENDING_OP_NONE after calling.
static bool apply_pending_op(TMS1500_State *cpu) {
    if (cpu->pending_op == PENDING_OP_NONE) return false;
    BCD_Reg result;
    bcd_zero(&result);
    switch (cpu->pending_op) {
        case PENDING_OP_ADD: bcd_add(&result, &cpu->operand_x, &cpu->reg[REG_A], &cpu->flags); break;
        case PENDING_OP_SUB: bcd_sub(&result, &cpu->operand_x, &cpu->reg[REG_A], &cpu->flags); break;
        case PENDING_OP_MUL: bcd_mul(&result, &cpu->operand_x, &cpu->reg[REG_A], &cpu->flags); break;
        case PENDING_OP_DIV: bcd_div(&result, &cpu->operand_x, &cpu->reg[REG_A], &cpu->flags); break;
        case PENDING_OP_YX: {
            busy_start(BUSY_MS_YX);
            double x = bcd_to_double(&cpu->reg[REG_A]);
            double y = bcd_to_double(&cpu->operand_x);
            if (cpu->flags.inv) {
                // INV yX = y^(1/x): inverso della potenza (radice x-esima),
                // es. la routine "compute i" di ML-18 (Lbl INV: ... INV yX RCL 01 )
                // calcola (FV/PV)^(1/N). Prima di questo fix il flag INV
                // veniva perso e si eseguiva una normale potenza.
                if (fabs(x) < 1e-15) { cpu->flags.error = true; return true; }
                if (y < 0 && floor(1.0 / x) != 1.0 / x) { cpu->flags.error = true; return true; }
                bcd_from_double(&result, pow(y, 1.0 / x));
                cpu->flags.inv = false;
            } else {
                if (y < 0 && floor(x) != x) { cpu->flags.error = true; return true; }
                bcd_from_double(&result, pow(y, x));
            }
            break;
        }
        default: return false;
    }
    if (!cpu->flags.error) bcd_copy(&cpu->reg[REG_A], &result);
    return true;
}

static void hir_push(TMS1500_State *cpu) {
    if (hir_sp < HIR_STACK_SIZE) {
        bcd_copy(&hir_stack[hir_sp], &cpu->reg[REG_A]);
        bcd_copy(&hir_operand[hir_sp], &cpu->operand_x);
        hir_op[hir_sp] = cpu->pending_op;
        hir_sp++;
    }
}
static void hir_pop(TMS1500_State *cpu) {
    if (hir_sp > 0) {
        hir_sp--;
        bcd_copy(&cpu->operand_x, &hir_operand[hir_sp]);
        cpu->pending_op = hir_op[hir_sp];
        apply_pending_op(cpu);
        cpu->pending_op = PENDING_OP_NONE;
    }
}

static void rebuild_labels(TMS1500_State *cpu) {
    for (int i = 0; i < 10; i++) {
        custom_label_pc[i] = 0xFFFF;
    }
    
    for (uint16_t i = 0; i + 1 < cpu->prog_len; i++) {
        if (cpu->prog[i] == KC_LBL) {
            uint8_t lab = cpu->prog[i + 1];
            
            int idx = label_index_for_key(lab);
            if (idx >= 0) custom_label_pc[idx] = i;
        }
    }
}

// ═══════════════════════════════════════════════════════════
// GESTIONE TASTI
// ═══════════════════════════════════════════════════════════

static void input_clear(void) {
    memset(input_buf, 0, sizeof(input_buf));
    input_len = 0;
    input_has_dot = false;
    input_has_ee = false;
    input_ee_len = 0;
}

static void input_commit(TMS1500_State *cpu) {
    // Trailing dot: true if buffer ends with '.' (e.g. "5.")
    display_trailing_dp = (input_len > 0 && input_has_dot &&
                           input_buf[input_len - 1] == '.');
    if (input_len > 0 || input_has_ee) {
        bcd_from_double(&cpu->reg[REG_A], atof(input_buf));
    }
    input_clear();
}

static void exec_pending(TMS1500_State *cpu) {
    if (cpu->pending_op == PENDING_OP_NONE) return;
    apply_pending_op(cpu);
    cpu->pending_op = PENDING_OP_NONE;
}

static bool is_digit_key(uint8_t kc) { return kc <= 9; }
static int keycode_to_digit(uint8_t kc) { return (kc <= 9) ? (int)kc : -1; }


/*
 * process_keycode() — processes a single keycode.
 * 
 * KEY DESIGN PRINCIPLE FOR TI-59 COMPATIBILITY:
 * In LRN (learn) mode, keycodes are stored EXACTLY as they appear
 * on the real TI-59 keyboard. The 2nd prefix is handled by storing
 * the translated code directly — the real TI-59 does NOT store "21"
 * as a separate step for 2nd; instead, 2nd changes the code of the
 * NEXT key pressed. For example:
 *   - Pressing "sin" (2nd x²) stores code 38 directly
 *   - Pressing "2nd" then "x²" also stores code 38
 *   - Pressing "INV" then "SBR" stores code 92 (Return)
 * 
 * The INV prefix works similarly: it inverts the next operation.
 * INV + SBR = Return (code 92), INV + LNx = eˣ, etc.
 */
static void process_keycode(TMS1500_State *cpu, uint8_t kc) {

    // ── Handle INV prefix ──────────────────────────────────
    // In RUN mode, INV modifies the NEXT key pressed immediately.
    // In LRN mode, INV sets a pending flag; the NEXT key stores both
    // the keycode and the INV state as a combined step.
    if (cpu->flags.inv && kc != KC_INV) {
        // 2ND while INV is active: set pending_2nd but keep INV for the next key
        if (kc == KC_2ND) {
            cpu->pending_2nd = !cpu->pending_2nd;
            return;
        }
        if (cpu->flags.lrn) {
            // In LRN mode, set inv_pending and wait for next key
            inv_pending = true;
            cpu->flags.inv = false;
            return;
        }
        // RUN mode: process INV+key immediately
        // Apply 2nd mapping first if pending, so INV+2nd+key works correctly
        if (cpu->pending_2nd) {
            kc = keycode_2nd(kc);
            cpu->pending_2nd = false;
        }
        cpu->flags.inv = false;
        // INV + trig: arcsin, arccos, arctan
        if (kc == KC_SIN) {
            input_commit(cpu); math_sin(cpu, true); display_trailing_dp = false;
            format_display(cpu); return;
        }
        else if (kc == KC_COS) {
            input_commit(cpu); math_cos(cpu, true); display_trailing_dp = false;
            format_display(cpu); return;
        }
        else if (kc == KC_TAN) {
            input_commit(cpu); math_tan(cpu, true); display_trailing_dp = false;
            format_display(cpu); return;
        }
        // INV + log = 10ˣ, INV + LNx = eˣ
        else if (kc == KC_LOG) {
            input_commit(cpu); math_exp(cpu, true); display_trailing_dp = false;
            format_display(cpu); return;
        }
        else if (kc == KC_LNX) {
            input_commit(cpu); math_exp(cpu, false); display_trailing_dp = false;
            format_display(cpu); return;
        }
        // INV + SBR = Return (code 92)
        else if (kc == KC_SBR) {
            if (cpu->sp > 0) {
                uint8_t old_sp = cpu->sp;
                cpu->sp--;
                cpu->prog_pc = cpu->stack[cpu->sp];
                cpu->pending_op = cpu->stack_pending_op[cpu->sp]; bcd_copy(&cpu->operand_x, &cpu->stack_operand_x[cpu->sp]);
                Serial.printf("[STK] INV+SBR kbd RET sp=%u->%u pop=%u in_rom=%u\n",
                    (unsigned)old_sp, (unsigned)cpu->sp, (unsigned)cpu->stack[cpu->sp],
                    (unsigned)cpu->stack_in_rom[cpu->sp]);
                // Ripristina SEMPRE lo stato del chiamante, non solo quando
                // era "true": altrimenti un ritorno da una subroutine ROM
                // verso codice utente lascia showing_lib_prog bloccato a
                // true (bug: prima veniva toccato solo nel ramo true).
                showing_lib_prog = cpu->stack_in_rom[cpu->sp];
                if (showing_lib_prog) {
                    lib_scope_addr = cpu->stack_rom_base[cpu->sp];
                    lib_scope_len = cpu->stack_rom_len[cpu->sp];
                }
                cpu->stack_in_rom[cpu->sp] = false;
            } else {
                cpu->flags.run = false;
                cpu->flags.idle = true;
            }
            return;
        }
        // INV + P→R = R→P
        else if (kc == KC_P_R) {
            input_commit(cpu); math_r2p(cpu); display_trailing_dp = false;
            format_display(cpu); return;
        }
        // INV + EE = cancel scientific notation. Deve anche rimuovere un
        // esponente in corso di inserimento nel buffer (input_has_ee),
        // altrimenti INV EE azzerava solo il flag e il buffer restava
        // "mantissa e ±xx": il tasto cifra successivo riprendeva a scrivere
        // sull'esponente e la modalità non usciva mai davvero.
        else if (kc == KC_EE) {
            if (input_has_ee) {
                char *ep = strchr(input_buf, 'e');
                if (ep) {
                    *ep = '\0';
                    input_len = (int)strlen(input_buf);
                    input_has_ee = false;
                    input_ee_len = 0;
                    bcd_from_double(&cpu->reg[REG_A], atof(input_buf));
                }
            }
            cpu->flags.sci = false;
            cpu->flags.inv = false;
            format_display(cpu);
            return;
        }
        // INV + Fix = release Fix
        else if (kc == KC_FIX) { cpu->flags.fix = false; cpu->flags.inv = false; return; }
        // INV + ENG = release ENG
        else if (kc == KC_ENG) { cpu->flags.eng = false; cpu->flags.inv = false; return; }
        // INV + Int = frac
        else if (kc == KC_INT) {
            input_commit(cpu);
            double v = bcd_to_double(&cpu->reg[REG_A]);
            bcd_from_double(&cpu->reg[REG_A], v - trunc(v));
            display_trailing_dp = false;
            format_display(cpu); return;
        }
        // INV + |x| = signum
        else if (kc == KC_ABS) {
            input_commit(cpu);
            double v = bcd_to_double(&cpu->reg[REG_A]);
            bcd_from_double(&cpu->reg[REG_A], (v > 0) ? 1.0 : (v < 0) ? -1.0 : 0.0);
            display_trailing_dp = false;
            format_display(cpu); return;
        }
        // INV + D.MS = decimal→DMS
        else if (kc == KC_DMS) {
            input_commit(cpu);
            double v = bcd_to_double(&cpu->reg[REG_A]);
            bcd_from_double(&cpu->reg[REG_A], decimal_to_dms(v));
            display_trailing_dp = false;
            format_display(cpu); return;
        }
        // INV + Σ+ = Σ−
        else if (kc == KC_SIGP) {
            input_commit(cpu); stat_sigma_minus(cpu);
            format_display(cpu); return;
        }
        // INV + x̄ = ȳ
        else if (kc == KC_XBAR) {
            stat_mean_y(cpu); return;
        }
        // X=T e X≥T sono istruzioni di salto condizionato SOLO
        // significative dentro un programma (vedi la gestione in
        // exec_program_step, che legge l'etichetta/indirizzo di
        // destinazione dalla ROM): una pressione diretta da tastiera non
        // ha un target a cui saltare, quindi non ha effetto — come sul
        // TI-59 reale.
        else if (kc == KC_XEQ_T || kc == KC_XGE_T) {
            return;
        }
        // INV + yX = radice x-esima: y^(1/x). Qui si RI-SOSTIENE il flag
        // INV (appena azzerato in testa al ramo) senza eseguire altro:
        // il case KC_YX del main switch registra l'operazione pendente e
        // sarà apply_pending_op() a calcolare pow(y, 1/x) quando verrà
        // risolta (al "=", alla ")" o a un operatore a priorità
        // maggiore/uguale). Senza questo, INV yX diventava un yX normale
        // e la routine "compute i" di ML-18 dava un tasso errato.
        else if (kc == KC_YX) {
            cpu->flags.inv = true;
        }
        // For any other key, INV has no effect — process normally
    }

    // ── FIX pending (2nd Fix N) ────────────────────────────
    if (fix_pending) {
        int d = keycode_to_digit(kc);
        if (d >= 0 && d <= 9) {
            if (d == 9) {
                cpu->flags.fix = false;  /* FIX 9 = no FIX */
            } else {
                cpu->flags.fix = true;
                cpu->fix_digits = d;
            }
            fix_pending = false;
            format_display(cpu);
            return;
        }
        /* Non-digit cancels FIX pending */
        fix_pending = false;
    }

        // ── Op pending (2nd Op nn) ─────────────────────────────
    if (op_pending) {
        if (is_digit_key(kc)) {
            op_code = op_code * 10 + keycode_to_digit(kc);
            op_digits++;
            if (op_digits >= 2) {
                exec_op(cpu, op_code % 100);
                op_pending = false; op_code = 0; op_digits = 0;
            }
            return;
        }
        op_pending = false; op_code = 0; op_digits = 0;
    }

    // ── Pgm pending: numero di programma del modulo libreria (2 cifre) ──
    // "2nd Pgm mm" DESIGNA quale programma, senza fare altro. Il tasto
    // successivo decide cosa succede: "2nd Op 09" scarica in memoria
    // per modificarlo (v. exec_op case 9); un tasto etichetta A..E /
    // A'..E' lo esegue così com'è direttamente dalla ROM (v. gestore
    // diretto più sotto). Corretto durante la revisione: non è "2nd
    // Op 09" da solo come da tabella Sladký (quella riga si riferisce
    // a un'operazione di paginazione ROM a basso livello) — il tasto
    // fisico è Pgm (2nd LRN), seguito da 2 cifre.
    if (lib_page_pending) {
        if (is_digit_key(kc)) {
            lib_page_val = lib_page_val * 10 + keycode_to_digit(kc);
            lib_page_digits++;
            if (lib_page_digits >= 2) {
                uint8_t page = (uint8_t)(lib_page_val % 100);
                // PGM 00 esce dalla modalità libreria e torna al programma utente
                if (page == 0) {
                    showing_lib_prog = false;
                    lib_page_selected = false;
					lib_selected_page = 0;
                    lib_scope_addr = 0; lib_scope_len = 0;
                    for (int i = 0; i < 10; i++) lib_custom_label_pc[i] = 0xFFFF;
                    Serial.println("[LIB] Uscito dalla modalità ROM (PGM 00)");
                } else {
                    uint16_t addr = 0, plen = 0;
                    const char *title = nullptr;
                    if (library_find_program(page, &addr, &plen, &title)) {
                        lib_selected_page = page;
                        lib_scope_addr = addr;
                        lib_scope_len  = plen;
                        lib_page_selected = true;
                        rebuild_lib_labels(lib_scope_addr, lib_scope_len);
                        Serial.printf("[LIB] Programma %02d designato: %s (Op 09 per scaricarlo, o un tasto etichetta per eseguirlo as-is)\n",
                                      page, title ? title : "?");
                    } else {
                        cpu->flags.error = true;
                        Serial.printf("[LIB] Programma %02d non trovato (nessun modulo attivo, o numero inesistente)\n", page);
                    }
                }
                lib_page_pending = false; lib_page_digits = 0; lib_page_val = 0;
            }
            return;
        }
        lib_page_pending = false; lib_page_digits = 0; lib_page_val = 0;
    }

    // ── DSZ pending: Dsz nn LLL — registro (2 cifre) poi indirizzo ──
    // (3 cifre assolute, oppure etichetta diretta A-E / A'-E' come per GTO/SBR)
    if (dsz_phase == 1) {
        int d = keycode_to_digit(kc);
        if (d >= 0) {
            dsz_reg_val = dsz_reg_val * 10 + d;
            dsz_reg_digits++;
            if (dsz_reg_digits >= 2) dsz_phase = 2;
            return;
        }
        // tasto non numerico: annulla l'istruzione DSZ incompleta
        dsz_phase = 0; dsz_reg_val = 0; dsz_reg_digits = 0;
    } else if (dsz_phase == 2) {
        if (kc == KC_2ND) { cpu->pending_2nd = !cpu->pending_2nd; return; }
        if (kc == KC_INV) { cpu->flags.inv = !cpu->flags.inv; return; }
        if (cpu->pending_2nd) { kc = keycode_2nd(kc); cpu->pending_2nd = false; }

        int d = keycode_to_digit(kc);

        // Se non è un numero, trattalo come un'Etichetta
        if (d < 0) { 
            uint16_t target = find_label(cpu, kc);
            if (target != 0xFFFF) {
                dsz_do(cpu, dsz_reg_val, target);
            } else {
                cpu->flags.error = true;
            }
            dsz_phase = 0; dsz_reg_val = 0; dsz_reg_digits = 0;
            return;
        }
        if (d >= 0) {
            dsz_addr_val = dsz_addr_val * 10 + d;
            dsz_addr_digits++;
            if (dsz_addr_digits >= 3) {
                {
                    uint16_t plen_ = exec_prog_len(cpu);
                    uint16_t a = (uint16_t)(dsz_addr_val % 1000);
                    uint16_t target_addr = (showing_lib_prog && plen_ > 0)
                        ? (lib_scope_addr + (a % plen_))
                        : a;
                    dsz_do(cpu, dsz_reg_val, target_addr);
                }
                dsz_phase = 0; dsz_reg_val = 0; dsz_reg_digits = 0;
                dsz_addr_val = 0; dsz_addr_digits = 0;
            }
            return;
        }
        dsz_phase = 0; dsz_reg_val = 0; dsz_reg_digits = 0;
        dsz_addr_val = 0; dsz_addr_digits = 0;
    }

// ── Pending reg (STO/RCL/GTO/SBR/SUM/EXC/Prod/StFlg/IfFlg) ──
    if (cpu->pending_reg != PENDING_REG_NONE) {
        // Protezione: i tasti modificatori non devono annullare l'operazione in corso
        if (kc == KC_2ND) { cpu->pending_2nd = !cpu->pending_2nd; return; }
        if (kc == KC_INV) { cpu->flags.inv = !cpu->flags.inv; return; }
        
        // Applica il prefisso 2nd se premuto precedentemente (es. YX diventa IND)
        if (cpu->pending_2nd) { kc = keycode_2nd(kc); cpu->pending_2nd = false; }
        
        // Gestione puntatore Indiretto (IND)
        if (kc == KC_IND) { pending_indirect = true; return; }

        int d = keycode_to_digit(kc);

        // GTO (3) e SBR (4) accettano QUALSIASI tasto non numerico come Etichetta
        if ((cpu->pending_reg == PENDING_REG_GTO || cpu->pending_reg == PENDING_REG_SBR) && d < 0) {
            // Stesso attivatore già presente per il tasto etichetta
            // diretto (A..E/A'..E'): se un programma è stato appena
            // designato con "2nd Pgm mm" e non ancora scaricato con
            // Op 09, il primo GTO/SBR-con-etichetta attiva l'esecuzione
            // "as-is" dalla ROM del modulo — mancava qui, per questo
            // "SBR =" (o qualunque GTO/SBR con etichetta) restava nello
            // spazio utente (vuoto o incoerente) invece che nel modulo.
            if (lib_page_selected) {
                rebuild_lib_labels(lib_scope_addr, lib_scope_len);
                showing_lib_prog = true;
                lib_page_selected = false;
                memset(lib_ram, 0, sizeof(lib_ram));
                Serial.printf("[LIB] Esecuzione as-is dalla ROM del modulo (programma %02d)\n", lib_selected_page);
            }
            uint16_t addr = find_label(cpu, kc);
            if (addr != 0xFFFF) { // Etichetta trovata
                if (cpu->pending_reg == PENDING_REG_SBR && cpu->sp < STACK_SIZE - 1 &&
                    (cpu->prog_len > 0 || showing_lib_prog)) {
                    cpu->stack_in_rom[cpu->sp] = showing_lib_prog;
                    if (showing_lib_prog) {
                        cpu->stack_rom_base[cpu->sp] = lib_scope_addr;
                        cpu->stack_rom_len[cpu->sp] = lib_scope_len;
                    }
                    cpu->stack[cpu->sp] = cpu->prog_pc;
                    cpu->stack_pending_op[cpu->sp] = cpu->pending_op; bcd_copy(&cpu->stack_operand_x[cpu->sp], &cpu->operand_x);
                    Serial.printf("[STK] kbd SBR-label push sp=%u val=%u\n", (unsigned)cpu->sp, (unsigned)cpu->prog_pc);
                    cpu->sp++;
                }

                cpu->prog_pc = addr;
                if (cpu->pending_reg == PENDING_REG_SBR) { cpu->flags.run = true; cpu->flags.idle = false; }
            } else {
                cpu->flags.error = true; // Errore: lampeggio se l'etichetta non esiste
            }
            cpu->pending_reg = PENDING_REG_NONE; cpu->pending_digits = 0;
            pending_value = 0; pending_indirect = false;
            return;
        }

        if (d >= 0) {
            pending_value = pending_value * 10 + d;
            cpu->pending_digits++;
            uint8_t act = cpu->pending_reg;
            int target_digits;
            if (pending_indirect) {
                target_digits = 2;
            } else if (act == PENDING_REG_GTO || act == PENDING_REG_SBR) {
                target_digits = 3;  // address is 3 digits (000–999)
            } else if (act >= PENDING_REG_STO && act <= PENDING_REG_SUM) {
                target_digits = 2;  // register number is 2 digits (00–99)
            } else {
                target_digits = 1;  // flag number is 1 digit (0–9)
            }
            if (cpu->pending_digits >= target_digits) {
                uint16_t reg = pending_value;
                uint8_t action = cpu->pending_reg;
                // GTO/SBR with absolute 3-digit address: if a library program
                // was designated with "2nd Pgm mm" (but not yet downloaded via
                // Op 09), activate as-is execution from the module ROM.
                // Also handles switching between library programs in mid-flight.
                if ((action == PENDING_REG_GTO || action == PENDING_REG_SBR) && lib_page_selected) {
                    rebuild_lib_labels(lib_scope_addr, lib_scope_len);
                    showing_lib_prog = true;
                    lib_page_selected = false;
                    memset(lib_ram, 0, sizeof(lib_ram));
                    Serial.printf("[LIB] Esecuzione as-is dalla ROM del modulo (programma %02d)\n", lib_selected_page);
                }
                uint16_t effective_reg = reg;
                BCD_Reg *bank = active_ram_bank(cpu);
                if (pending_indirect && (action == PENDING_REG_GTO || action == PENDING_REG_SBR)) {
                    effective_reg = bcd_to_int_step(&bank[reg]);
                } else if (pending_indirect && (action == PENDING_REG_STO || action == PENDING_REG_RCL ||
                                                action == PENDING_REG_SUM || action == PENDING_REG_EXC ||
                                                action == PENDING_REG_PROD)) {
                    effective_reg = bcd_to_int_reg(&bank[reg]) % 100;
                }
                switch (action) {
                    case PENDING_REG_STO: bcd_copy(&bank[effective_reg], &cpu->reg[REG_A]); break;
                    case PENDING_REG_RCL: bcd_copy(&cpu->reg[REG_A], &bank[effective_reg]); break;
                    case PENDING_REG_GTO: {
                        if (exec_prog_len(cpu) > 0) cpu->prog_pc = relocate_target(cpu, effective_reg);
                        break;
                    }
                    case PENDING_REG_SBR: {
                        if (exec_prog_len(cpu) > 0) {
                            if (cpu->sp < STACK_SIZE - 1) {
                                cpu->stack_in_rom[cpu->sp] = showing_lib_prog;
                                if (showing_lib_prog) {
                                    cpu->stack_rom_base[cpu->sp] = lib_scope_addr;
                                    cpu->stack_rom_len[cpu->sp] = lib_scope_len;
                                }
                                cpu->stack[cpu->sp] = cpu->prog_pc;
                                cpu->stack_pending_op[cpu->sp] = cpu->pending_op; bcd_copy(&cpu->stack_operand_x[cpu->sp], &cpu->operand_x);
                                Serial.printf("[STK] kbd SBR-ind push sp=%u val=%u\n", (unsigned)cpu->sp, (unsigned)cpu->prog_pc);
                                cpu->sp++;
                            }
                            cpu->prog_pc = relocate_target(cpu, effective_reg);
                            cpu->flags.run = true; cpu->flags.idle = false;
                        }
                        break;
                    }
                    case PENDING_REG_SUM:  bcd_add(&bank[effective_reg], &bank[effective_reg], &cpu->reg[REG_A], &cpu->flags); break;
                    case PENDING_REG_STFL: if (reg < 10) user_flags[reg] = true; break;
                    case PENDING_REG_IFFL:
                        if (reg < 10 && !user_flags[reg]) {
                            if (cpu->flags.run && exec_prog_len(cpu) > 0)
                                cpu->prog_pc = advance_pc_by(cpu, instruction_byte_length(cpu, cpu->prog_pc));
                        }
                        break;
                    case PENDING_REG_EXC: {
                        BCD_Reg tmp;
                        bcd_copy(&tmp, &bank[effective_reg]);
                        bcd_copy(&bank[effective_reg], &cpu->reg[REG_A]);
                        bcd_copy(&cpu->reg[REG_A], &tmp);
                        break;
                    }
                    case PENDING_REG_PROD: {
                        BCD_Reg result;
                        bcd_zero(&result);
                        bcd_mul(&result, &bank[effective_reg], &cpu->reg[REG_A], &cpu->flags);
                        bcd_copy(&bank[effective_reg], &result);
                        break;
                    }
                }
                cpu->pending_reg = PENDING_REG_NONE; cpu->pending_digits = 0;
                pending_value = 0; pending_indirect = false;
                format_display(cpu);
            }
            return;
        }
        cpu->pending_reg = PENDING_REG_NONE; cpu->pending_digits = 0;
        pending_value = 0; pending_indirect = false;
    }

    // ── LRN MODE ────────────────────────────────────────────
    if (cpu->flags.lrn) {
        // In LRN mode, 2nd is handled by translating the next key
        // and storing the translated code directly.
        if (cpu->pending_2nd && kc != KC_2ND) {
            kc = keycode_2nd(kc);
            cpu->pending_2nd = false;
        }
        // In LRN mode, if INV was pressed, store the INV state with this key
        if (inv_pending) {
            // Store key with INV prefix: use high bit or special encoding
            // For simplicity, we store KC_INV (22) followed by the keycode
            // This is a 2-byte instruction in the program memory
            if (cpu->prog_pc < PROG_SIZE - 1) {
                prog_store_step(cpu, KC_INV);
                prog_store_step(cpu, kc);
            }
            inv_pending = false;
            return;
        }
        switch (kc) {
            case KC_2ND:
                cpu->pending_2nd = true;
                return;

            case KC_LRN:
                cpu->flags.lrn = false;
                return;

            case KC_SST:
                if (cpu->prog_pc < PROG_SIZE - 1) cpu->prog_pc++;
                return;

            case KC_BST:
                if (cpu->prog_pc > 0) cpu->prog_pc--;
                return;

            case KC_INS: {
                if (cpu->prog_len >= PROG_SIZE) return;
                if (cpu->prog_pc > cpu->prog_len) cpu->prog_pc = cpu->prog_len;
                for (uint16_t i = cpu->prog_len; i > cpu->prog_pc; i--) {
                    cpu->prog[i] = cpu->prog[i - 1];
                }
                cpu->prog[cpu->prog_pc] = KC_NOP;
                cpu->prog_len++;
                rebuild_labels(cpu);
                prog_dirty = true;
                return;
            }

            case KC_DEL: {
                if (cpu->prog_len == 0 || cpu->prog_pc >= cpu->prog_len) return;

                for (uint16_t i = cpu->prog_pc; i + 1 < cpu->prog_len; i++) {
                    cpu->prog[i] = cpu->prog[i + 1];
                }
                cpu->prog_len--;

                rebuild_labels(cpu);
                prog_dirty = true;
                return;
            }

            case KC_IND: {
            // Su hardware reale, STO/RCL/SUM/EXC/Prod/GTO seguiti
            // immediatamente da "2nd Ind" collassano in UN SOLO byte
			// combinato (es. 72, 73, 74, 63, 64, 83).
            if (cpu->prog_pc > 0 && cpu->prog_pc <= cpu->prog_len) {
                uint8_t *last = &cpu->prog[cpu->prog_pc - 1];
                switch (*last) {
                    case KC_STO:  *last = KC_STO_IND;  return;
                    case KC_RCL:  *last = KC_RCL_IND;  return;
                    case KC_SUM:  *last = KC_SUM_IND;  return;
                    case KC_EXC:  *last = KC_EXC_IND;  return;
                    case KC_PROD: *last = KC_PROD_IND; return;
                    case KC_GTO:  *last = KC_GTO_IND;  return;
                    default: break;   // non fondibile: registra Ind "grezzo" sotto
                }
            }
            prog_store_step(cpu, kc);
            return;
        }

            default: {
                uint16_t store_addr = cpu->prog_pc;
                prog_store_step(cpu, kc);
                
                // Label registration
                if (store_addr >= 1 && store_addr < PROG_SIZE &&
                    cpu->prog[store_addr - 1] == KC_LBL) {
                    
                    int idx = label_index_for_key(kc);
                    
                    if (idx >= 0) {
                        // Rimuove l'indirizzo precedente se sovrascritto
                        for (int j = 0; j < 10; j++) {
                            if (custom_label_pc[j] == store_addr - 1)
                                custom_label_pc[j] = 0xFFFF; // Nota: usa lo stesso flag di "vuoto" usato in rebuild_labels (0xFFFF anziché 0)
                        }
                        custom_label_pc[idx] = store_addr - 1;
                    }
                }
                return;
            }
        }
    }

    // ── 2nd mapping (RUN mode) ────────────────────────────
    if (cpu->pending_2nd && kc != KC_2ND) {
        kc = keycode_2nd(kc);
        cpu->pending_2nd = false;
    }

    // ── Special keys ──────────────────────────────────────
    switch (kc) {
        case KC_2ND:
            cpu->pending_2nd = !cpu->pending_2nd;
            return;
        case KC_INV:
            cpu->flags.inv = !cpu->flags.inv;
            return;
        case KC_CLR:
            bcd_zero(&cpu->reg[REG_A]); cpu->pending_op = PENDING_OP_NONE;
            cpu->pending_reg = PENDING_REG_NONE; cpu->pending_digits = 0;
            input_clear(); cpu->flags.error = false; cpu->flags.inv = false;
            cpu->flags.sci = false; cpu->flags.eng = false;
            cpu->stack_lift_enabled = false;
            hir_sp = 0; paren_depth = 0;
            display_trailing_dp = false;
            format_display(cpu); return;
        case KC_CE:
			cpu->flags.error = false; // Aggiungi questo per sbloccare il lampeggio
            if (input_has_ee) {
                char *ep = strchr(input_buf, 'e');
                if (ep && input_ee_len > 0) {
                    int ee_sign_offset = (int)(ep - input_buf) + 1;
                    int ee_d0 = ee_sign_offset + 1;
                    int ee_d1 = ee_sign_offset + 2;
                    if (input_ee_len == 2) {
                        input_buf[ee_d1] = '0';
                    } else if (input_ee_len == 1) {
                        input_buf[ee_d0] = '0';
                        input_buf[ee_d1] = '0';
                    }
                    input_ee_len--;
                    bcd_from_double(&cpu->reg[REG_A], atof(input_buf));
                    format_display(cpu);
                    return;
                } else if (ep) {
                    // Exit EE mode, keep mantissa
                    *ep = '\0';
                    input_len = (int)strlen(input_buf);
                    input_has_ee = false;
                    input_ee_len = 0;
                    bcd_from_double(&cpu->reg[REG_A], atof(input_buf));
                    format_display(cpu);
                    return;
                }
            }
            if (input_len > 0) {
                input_len--;
                if (input_len == 0) { input_clear(); bcd_zero(&cpu->reg[REG_A]); }
                else input_buf[input_len] = '\0';
            }
            format_display(cpu); return;
        case KC_LRN:
            // Se si entra in LRN mentre un'istruzione interattiva
            // multi-tasto è a metà (STO/RCL/GTO/DSZ/Op/Fix non ancora
            // completata), quello stato residuo intercetterebbe i
            // tasti successivi PRIMA del blocco LRN (i controlli
            // pending_reg/dsz_phase/op_pending/fix_pending girano
            // prima del "if (cpu->flags.lrn)"), facendo sembrare che
            // la digitazione in LRN non funzioni più. Si azzera tutto
            // per partire puliti, esattamente come fa già CLR altrove.
            cpu->pending_reg = PENDING_REG_NONE; cpu->pending_digits = 0;
            pending_value = 0; pending_indirect = false;
            op_pending = false; op_code = 0; op_digits = 0;
            lib_page_pending = false; lib_page_digits = 0; lib_page_val = 0;
            lib_page_selected = false;
            if (showing_lib_prog) {
                showing_lib_prog = false;
                Serial.println("[LIB] Rientrato in LRN: torno al programma utente");
				lib_page_selected = false;
				lib_selected_page = 0;
            }
            dsz_phase = 0; dsz_reg_val = 0; dsz_reg_digits = 0;
            dsz_addr_val = 0; dsz_addr_digits = 0;
            fix_pending = false;
            cpu->flags.lrn = true;
            input_clear();
            return;

        case KC_RS:
            if (!cpu->flags.run && exec_prog_len(cpu) == 0) return;
            cpu->flags.run = !cpu->flags.run;
            /* R/S mette solo in pausa/riprende da dove si era interrotta
             * l'esecuzione (comportamento reale) — non riavvolge il PC,
             * quello e' compito di RST. */
            cpu->flags.idle = !cpu->flags.run;
            return;

        case KC_SST:
            if (exec_prog_len(cpu) > 0) {
                uint8_t step = prog_read_step(cpu, cpu->prog_pc);
                process_keycode(cpu, step);
                if (exec_prog_len(cpu) > 0) cpu->prog_pc = advance_pc_wrap(cpu);
            }
            return;
        case KC_BST:
            if (cpu->prog_pc > 0) cpu->prog_pc--;
            return;
        case KC_RST:
            // RST esce dalla modalità libreria e torna al programma utente
            // (passo 000), cosí l'utente puo' premere un tasto etichetta
            // per eseguire il proprio programma invece di quello della ROM.
            // Se vuole ri-usare la libreria, deve rifare 2nd Pgm mm.
            showing_lib_prog = false;
            lib_page_selected = false;
			lib_selected_page = 0;
            cpu->prog_pc = 0;
            cpu->sp = 0;
            cpu->flags.run = false;
            for (int i = 0; i < 10; i++) user_flags[i] = false;
            return;
    }

    // ── Digits ────────────────────────────────────────────
    if (is_digit_key(kc)) {
        int digit = keycode_to_digit(kc);
        if (!input_has_ee) {
            if (input_len == 0 && cpu->stack_lift_enabled) {
                bcd_copy(&cpu->reg[REG_D], &cpu->reg[REG_C]);
                bcd_copy(&cpu->reg[REG_C], &cpu->reg[REG_B]);
                bcd_copy(&cpu->reg[REG_B], &cpu->reg[REG_A]);
                cpu->stack_lift_enabled = false;
            }
            if (input_len < 11) { input_buf[input_len++] = '0' + digit; input_buf[input_len] = '\0'; }
        } else {
            if (input_ee_len < 2) {
                char *ep = strchr(input_buf, 'e');
                if (ep) {
                    int ee_sign_offset = (int)(ep - input_buf) + 1;  // posizione del segno +/-
                    int ee_d0 = ee_sign_offset + 1;                   // prima cifra exponent
                    int ee_d1 = ee_sign_offset + 2;                   // seconda cifra exponent
                    if (input_ee_len == 0) {
                        input_buf[ee_d1] = '0' + digit;
                    } else {
                        input_buf[ee_d0] = input_buf[ee_d1];
                        input_buf[ee_d1] = '0' + digit;
                    }
                    input_ee_len++;
                }
            }
        }
        bcd_from_double(&cpu->reg[REG_A], atof(input_buf));
        format_display(cpu); return;
    }

    // All non-digit keys set stack_lift_enabled by default
    cpu->stack_lift_enabled = true;

    // ── All other keys ─────────────────────────────────────
    switch (kc) {
        case KC_DOT:
            if (input_has_ee) {
                // Dot is not allowed in exponent entry — ignore
                return;
            }
            if (!input_has_dot) {
                if (input_len == 0) input_buf[input_len++] = '0';
                input_buf[input_len++] = '.'; input_buf[input_len] = '\0';
                input_has_dot = true;
            }
            bcd_from_double(&cpu->reg[REG_A], atof(input_buf));
            format_display(cpu); return;

        case KC_PM:
            if (input_has_ee) {
                char *ep = strchr(input_buf, 'e');
                if (ep && ep[1] != '\0') {
                    ep[1] = (ep[1] == '-') ? '+' : '-';
                }
                bcd_from_double(&cpu->reg[REG_A], atof(input_buf));
            } else if (input_len > 0) {
                if (input_buf[0] == '-') {
                    memmove(input_buf, input_buf + 1, input_len);
                    input_len--;
                } else if (input_len < (int)sizeof(input_buf) - 2) {
                    memmove(input_buf + 1, input_buf, input_len + 1);
                    input_buf[0] = '-';
                    input_len++;
                }
                bcd_from_double(&cpu->reg[REG_A], atof(input_buf));
            } else {
                double v = bcd_to_double(&cpu->reg[REG_A]);
                bcd_from_double(&cpu->reg[REG_A], -v);
            }
            format_display(cpu); return;

        case KC_EE:
            cpu->flags.sci = true;
            if (input_has_ee) {
                // Toggle EE off: remove "e±xx" from buffer, keep mantissa
                char *ep = strchr(input_buf, 'e');
                if (ep) {
                    *ep = '\0';
                    input_len = (int)strlen(input_buf);
                    input_has_ee = false;
                    input_ee_len = 0;
                    bcd_from_double(&cpu->reg[REG_A], atof(input_buf));
                    format_display(cpu);
                }
                return;
            }
            // Se il buffer è vuoto, inizia con "1e+00"
            if (input_len == 0) {
                input_buf[input_len++] = '1';
            }
            // Normalizza la mantissa a "d.ddddddd" (una sola cifra prima
            // del punto) se non è già stato inserito un punto decimale.
            // Senza questo passaggio, un valore digitato come "88888888"
            // + EE + "88" verrebbe salvato come 88888888e-88 (cioè
            // 8.8888888e-81, 7 ordini di grandezza fuori da quanto
            // digitato), e l'esponente mostrato "salterebbe" non appena
            // si esce dalla modalità input.
            if (!input_has_dot) {
                int start = (input_buf[0] == '-') ? 1 : 0;
                int ndigits = input_len - start;
                /* Limita a 7 cifre significative: stessa precisione usata
                 * per la notazione scientifica in "Result mode" (%.7g).
                 * Con sign(1)+cifra(1)+punto(1)+decimali(6)=9 char e
                 * esponente su 3 char ("-88"), il totale sta esattamente
                 * nei 12 caratteri del display, senza overflow. */
                const int MAX_SIG_DIGITS = 7;
                if (ndigits > MAX_SIG_DIGITS) {
                    input_len = start + MAX_SIG_DIGITS;
                    input_buf[input_len] = '\0';
                    ndigits = MAX_SIG_DIGITS;
                }
                if (ndigits > 1) {
                    memmove(&input_buf[start + 2], &input_buf[start + 1],
                            input_len - start);      // include il '\0'
                    input_buf[start + 1] = '.';
                    input_len++;
                    input_has_dot = true;
                }
            }
            // Controlla overflow buffer (serve spazio per "e+00" = 5 chars)
            if (input_len + 5 > (int)sizeof(input_buf)) {
                return;
            }
            input_buf[input_len++] = 'e';
            input_buf[input_len++] = '+';
            input_buf[input_len++] = '0';
            input_buf[input_len++] = '0';
            input_buf[input_len] = '\0';
            input_has_ee = true;
            input_ee_len = 0;
            bcd_from_double(&cpu->reg[REG_A], atof(input_buf));
            format_display(cpu); return;

        case KC_LPAR:
            input_commit(cpu);
            if (paren_depth >= HIR_STACK_SIZE) {
                // Troppi livelli di parentesi aperti: sul TI-59 reale
                // questo fa lampeggiare il display (errore), non deve
                // scrivere fuori dai limiti di hir_paren_base[].
                cpu->flags.error = true;
                return;
            }
            hir_push(cpu);
            hir_paren_base[paren_depth] = hir_sp;   // mark depth AFTER the push
            paren_depth++;
            cpu->pending_op = PENDING_OP_NONE;
            input_clear();
            return;

        case KC_RPAR:
            input_commit(cpu);
            exec_pending(cpu);                              // evaluate pending op inside parens
            if (paren_depth <= 0) {
                // ')' senza una '(' corrispondente: errore, non
                // decrementare sotto zero (leggerebbe hir_paren_base[]
                // con indice negativo).
                cpu->flags.error = true;
                input_clear();
                format_display(cpu);
                return;
            }
            paren_depth--;                                  // pop nesting level
            // Execute precedence deferrals added inside this paren level
            while (hir_sp > hir_paren_base[paren_depth]) {
                hir_pop(cpu);
            }
            // Pop the '(' entry: restore outer pending_op/operand_x WITHOUT executing
            if (hir_sp > 0) {
                hir_sp--;
                bcd_copy(&cpu->operand_x, &hir_operand[hir_sp]);
                cpu->pending_op = hir_op[hir_sp];
            }
            input_clear();
            format_display(cpu);
            return;

        case KC_ADD: case KC_SUB: case KC_MUL: case KC_DIV:
            if (!showing_lib_prog) check_timing_toggle_combo(kc);
            input_commit(cpu);
            // AOS precedence: ×/÷ bind tighter than +/−
            if (kc == KC_MUL || kc == KC_DIV) {
                if (cpu->pending_op == PENDING_OP_ADD || cpu->pending_op == PENDING_OP_SUB) {
                    hir_push(cpu);                          // defer pending +/−
                } else {
                    exec_pending(cpu);                      // same/higher precedence: evaluate now
                }
            } else {
                // +/− : lower precedence than ×/÷/yˣ
                if (cpu->pending_op != PENDING_OP_NONE) {
                    exec_pending(cpu);
                }
            }
            bcd_copy(&cpu->operand_x, &cpu->reg[REG_A]);
            cpu->pending_op = (kc == KC_ADD) ? PENDING_OP_ADD : (kc == KC_SUB) ? PENDING_OP_SUB : (kc == KC_MUL) ? PENDING_OP_MUL : PENDING_OP_DIV;
            input_clear(); format_display(cpu); return;

        case KC_EQ:
            input_commit(cpu); exec_pending(cpu);
            while (hir_sp > 0) hir_pop(cpu);
            paren_depth = 0;
            input_clear();
            display_trailing_dp = false;
            format_display(cpu); return;

        case KC_SQRT: input_commit(cpu); math_sqrt(cpu); display_trailing_dp = false; format_display(cpu); return;
        case KC_X2:   input_commit(cpu); math_x2(cpu); display_trailing_dp = false; format_display(cpu); return;
        case KC_INV_X: input_commit(cpu); math_inv(cpu); display_trailing_dp = false; format_display(cpu); return;
        case KC_YX:
            input_commit(cpu);
            // yˣ has higher precedence than +,−,×,÷ on the real TI-59 (AOS).
            // Defer any lower-precedence pending op via the HIR stack.
            if (cpu->pending_op == PENDING_OP_ADD || cpu->pending_op == PENDING_OP_SUB ||
                cpu->pending_op == PENDING_OP_MUL || cpu->pending_op == PENDING_OP_DIV) {
                hir_push(cpu);
            }
            bcd_copy(&cpu->operand_x, &cpu->reg[REG_A]);
            cpu->pending_op = PENDING_OP_YX;
            input_clear();
            display_trailing_dp = false;
            format_display(cpu);
            return;

        case KC_SIN:
            input_commit(cpu); math_sin(cpu, cpu->flags.inv); cpu->flags.inv = false;
            display_trailing_dp = false; format_display(cpu); return;
        case KC_COS:
            input_commit(cpu); math_cos(cpu, cpu->flags.inv); cpu->flags.inv = false;
            display_trailing_dp = false; format_display(cpu); return;
        case KC_TAN:
            input_commit(cpu); math_tan(cpu, cpu->flags.inv); cpu->flags.inv = false;
            display_trailing_dp = false; format_display(cpu); return;

        case KC_LNX:
            input_commit(cpu);
            if (!cpu->flags.inv) math_log(cpu, true);
            else                 math_exp(cpu, false);
            cpu->flags.inv = false; display_trailing_dp = false; format_display(cpu); return;

        case KC_LOG:
            input_commit(cpu);
            if (!cpu->flags.inv) math_log(cpu, false);
            else                 math_exp(cpu, true);
            cpu->flags.inv = false; display_trailing_dp = false; format_display(cpu); return;

        case KC_XET: {
            input_commit(cpu);
            BCD_Reg tmp;
            bcd_copy(&tmp, &cpu->reg[REG_A]);
            bcd_copy(&cpu->reg[REG_A], &cpu->reg[REG_T]);   // A <-> T
            bcd_copy(&cpu->reg[REG_T], &tmp);
            display_trailing_dp = false; format_display(cpu); return;
        }

        case KC_STO:
            input_commit(cpu); cpu->pending_reg = PENDING_REG_STO; cpu->pending_digits = 0; pending_value = 0; return;
        case KC_RCL:
            input_commit(cpu); cpu->pending_reg = PENDING_REG_RCL; cpu->pending_digits = 0; pending_value = 0; return;
        case KC_SUM:
            input_commit(cpu); cpu->pending_reg = PENDING_REG_SUM; cpu->pending_digits = 0; pending_value = 0; return;

        // ── STO/RCL/SUM/EXC/Prod/GTO indiretti (codice combinato) ──
        // Sulla TI-59 reale "STO 2nd Ind" ecc. generano UN SOLO byte
        // di programma (72/73/74/63/64/83), non due byte separati
        // come STO(42)+Ind(40). Prima di questo fix, un dump di
        // scheda reale con questi byte veniva ignorato silenziosamente
        // (nessun case corrispondente) e i 2 byte di indirizzo che li
        // seguivano venivano interpretati come cifre digitate a caso.
        // L'indirizzamento indiretto stesso (leggere il registro
        // puntatore ed effettuare l'operazione sul registro puntato)
        // è già implementato più sopra: qui basta impostare lo stesso
        // stato (pending_reg + pending_indirect) che produce il tasto
        // base, per riusare quella logica.
        case KC_STO_IND:
            input_commit(cpu); cpu->pending_reg = PENDING_REG_STO; cpu->pending_digits = 0;
            pending_value = 0; pending_indirect = true; return;
        case KC_RCL_IND:
            input_commit(cpu); cpu->pending_reg = PENDING_REG_RCL; cpu->pending_digits = 0;
            pending_value = 0; pending_indirect = true; return;
        case KC_SUM_IND:
            input_commit(cpu); cpu->pending_reg = PENDING_REG_SUM; cpu->pending_digits = 0;
            pending_value = 0; pending_indirect = true; return;
        case KC_EXC_IND:
            input_commit(cpu); cpu->pending_reg = PENDING_REG_EXC; cpu->pending_digits = 0;
            pending_value = 0; pending_indirect = true; return;
        case KC_PROD_IND:
            input_commit(cpu); cpu->pending_reg = PENDING_REG_PROD; cpu->pending_digits = 0;
            pending_value = 0; pending_indirect = true; return;
        case KC_GTO_IND:
            cpu->pending_reg = PENDING_REG_GTO; cpu->pending_digits = 0;
            pending_value = 0; pending_indirect = true; return;

        case KC_GTO:
            cpu->pending_reg = PENDING_REG_GTO; cpu->pending_digits = 0; pending_value = 0; return;

        case KC_PGM:
            // Richiamo programma da modulo libreria: Pgm + 2 cifre.
            lib_page_pending = true; lib_page_digits = 0; lib_page_val = 0;
            return;

        case KC_SBR:
            // Comportamento normale di SBR: imposta lo stato della CPU per
            // attendere le cifre dell'indirizzo o il tasto dell'etichetta.
            // INV+SBR (RET) è già gestito dall'handler INV nel blocco
            // all'inizio di process_keycode e non arriva mai qui.
            cpu->pending_reg = PENDING_REG_SBR;
            cpu->pending_digits = 0;
            pending_value = 0;
            return;

        case KC_P_R:
            input_commit(cpu);
            if (!cpu->flags.inv) math_p2r(cpu); else math_r2p(cpu);
            cpu->flags.inv = false; display_trailing_dp = false; format_display(cpu); return;

        case KC_DEG: cpu->trig_mode = 0; return;
        case KC_RAD: cpu->trig_mode = 1; return;
        case KC_GRAD: cpu->trig_mode = 2; return;

        case KC_A: case KC_B: case KC_C: case KC_D: case KC_E:
        case KC_A_PRIME: case KC_B_PRIME: case KC_C_PRIME: case KC_D_PRIME:
        case KC_E_PRIME: {
            int idx = label_index_for_key(kc);

            if (idx < 0) {
                cpu->flags.error = true;
                return;
            }

            // Se un programma è stato appena designato con "2nd Pgm mm"
            // (e non ancora scaricato con Op 09), il primo tasto
            // etichetta attiva l'esecuzione "as-is" direttamente dalla
            // ROM del modulo: niente copie, registri dati separati
            // dall'utente (v. active_ram_bank/lib_ram), etichette
            // ricostruite dando priorità a quelle del programma
            // designato (rebuild_lib_labels). Funziona anche se già in
            // modalità as-is (passaggio a un altro programma libreria).
            if (lib_page_selected) {
                rebuild_lib_labels(lib_scope_addr, lib_scope_len);
                showing_lib_prog = true;
                lib_page_selected = false;
                // Clear library data registers for fresh execution
                memset(lib_ram, 0, sizeof(lib_ram));
                Serial.printf("[LIB] Esecuzione as-is dalla ROM del modulo (programma %02d)\n", lib_selected_page);
            }

            uint16_t addr = showing_lib_prog ? lib_custom_label_pc[idx] : custom_label_pc[idx];
            uint16_t len  = exec_prog_len(cpu);

            // Se la label non è stata trovata (0xFFFF), entra in stato di errore (lampeggio)
            // NOTA: lib_custom_label_pc memorizza indirizzi ASSOLUTI nella ROM, quindi
            // il limite deve essere lib_scope_addr + lib_scope_len, non solo len.
            if (addr == 0xFFFF || (showing_lib_prog ? (addr >= lib_scope_addr + len) : (addr >= len))) {
                cpu->flags.error = true;
                return;
            }
            
            // ═══════════════════════════════════════════════════════
            // Chiamata a subroutine tramite etichetta "nuda" durante
            // l'esecuzione di un programma (cpu->flags.run == true):
            // salva l'indirizzo di ritorno sullo stack.
            // Se invece è una pressione da tastiera a freddo (nessun
            // programma in esecuzione), NON si pusha nulla — lo stack
            // rimane vuoto, e il successivo RET si comporta come R/S
            // fermando l'esecuzione ma lasciando montato il modulo
            // libreria (showing_lib_prog), così l'utente può premere
            // altri tasti etichetta per proseguire.
            // ═══════════════════════════════════════════════════════
            if (cpu->flags.run && cpu->sp < STACK_SIZE - 1) {
                cpu->stack_in_rom[cpu->sp] = showing_lib_prog;
                cpu->stack_rom_base[cpu->sp] = lib_scope_addr;
                cpu->stack_rom_len[cpu->sp] = lib_scope_len;
                cpu->stack[cpu->sp] = cpu->prog_pc;
                cpu->stack_pending_op[cpu->sp] = cpu->pending_op; bcd_copy(&cpu->stack_operand_x[cpu->sp], &cpu->operand_x);
                Serial.printf("[STK] kbd label-key SBR push sp=%u val=%u\n", (unsigned)cpu->sp, (unsigned)cpu->prog_pc);
                cpu->sp++;
            }
            cpu->flags.inv = false;
            
            // Avvia l'esecuzione spostando il Program Counter all'indirizzo della Label
            cpu->prog_pc = addr; 
            cpu->flags.run = true; 
            cpu->flags.idle = false;
            return;
        }
		
        case KC_IND:
            // IND "nudo" (2nd yˣ) senza un'operazione registro pendente è un
            // no-op, come sull'hardware: qui si arriva SOLO quando
            // pending_reg == NONE (con un pending_reg attivo, KC_IND è già
            // intercettato nel blocco "Pending reg" sopra). Prima questo case
            // setta pending_indirect = true, che poi CONTAMINAVA
            // l'operazione successiva: un "STO 00" premuto dopo un IND
            // isolato diventava un "STO IND" e scriveva nel registro
            // puntato (bcd_to_int_reg(&bank[reg])) invece che in R00.
            return;

        case KC_LBL:
            return;

        case KC_STFL:
            cpu->pending_reg = PENDING_REG_STFL; cpu->pending_digits = 0; pending_value = 0; return;
        case KC_IFFL:
            cpu->pending_reg = PENDING_REG_IFFL; cpu->pending_digits = 0; pending_value = 0; return;

        case KC_CMS: {
            // Must clear the ACTIVE bank: lib_ram when executing a library
            // program, cpu->ram otherwise. Using cpu->ram directly would
            // corrupt the user's data registers during lib-ROM execution.
            BCD_Reg *bank = active_ram_bank(cpu);
            for (int i = 0; i < 100; i++) bcd_zero(&bank[i]);
            format_display(cpu); return;
        }
        case KC_EXC:
            input_commit(cpu); cpu->pending_reg = PENDING_REG_EXC; cpu->pending_digits = 0; pending_value = 0; return;
        case KC_PROD:
            input_commit(cpu); cpu->pending_reg = PENDING_REG_PROD; cpu->pending_digits = 0; pending_value = 0; return;

        case KC_OP:
            op_pending = true; op_code = 0; op_digits = 0;
            return;
        case KC_NOP:
            return;

        case KC_PAUSE:
            format_display(cpu);
            // Pausa NON bloccante: trattiene l'esecuzione per 500ms (come
            // l'hardware) senza vTaskDelay — il gate è in tms1500_step.
            cpu->flags.pause = true;
            pause_until_ms = millis() + 500;
            return;

        case KC_DSZ:
            dsz_phase = 1; dsz_reg_val = 0; dsz_reg_digits = 0;
            dsz_addr_val = 0; dsz_addr_digits = 0;
            return;

        // X=T e X≥T sono istruzioni di salto condizionato SOLO
        // significative dentro un programma (vedi exec_program_step,
        // che legge l'etichetta/indirizzo di destinazione dalla ROM):
        // premute direttamente da tastiera non hanno un target a cui
        // saltare, quindi non hanno effetto — come sul TI-59 reale.
        case KC_XEQ_T:
        case KC_XGE_T:
            return;

        // NOTA: qui non serve (e sarebbe irraggiungibile) un ramo
        // "if (cpu->flags.inv)": l'INV su questi due tasti è già
        // intercettato più sopra, in cima a process_keycode() (blocco
        // "if (cpu->flags.inv && kc != KC_INV)"), che per KC_SIGP e
        // KC_XBAR chiama rispettivamente stat_sigma_minus()/
        // stat_mean_y() e fa return PRIMA di arrivare qui. Quando
        // l'esecuzione raggiunge questo switch, cpu->flags.inv è
        // quindi sempre già false — un ramo INV qui sarebbe stato
        // codice morto (prima c'era, duplicato e mai eseguibile).
        case KC_SIGP:
            input_commit(cpu);
            stat_sigma_plus(cpu);
            display_trailing_dp = false;
            format_display(cpu); return;

        case KC_XBAR:
            stat_mean(cpu);
            display_trailing_dp = false;
            return;

        case KC_FIX:
            if (cpu->flags.inv) {
                cpu->flags.fix = false;
                cpu->flags.inv = false;
                fix_pending = false;
            } else {
                fix_pending = true;  /* Wait for digit 0-9 */
            }
            return;

        case KC_INT:
            input_commit(cpu);
            if (cpu->flags.inv) {
                double v = bcd_to_double(&cpu->reg[REG_A]);
                bcd_from_double(&cpu->reg[REG_A], v - trunc(v));
                cpu->flags.inv = false;
            } else {
                double v = bcd_to_double(&cpu->reg[REG_A]);
                bcd_from_double(&cpu->reg[REG_A], trunc(v));
            }
            display_trailing_dp = false;
            format_display(cpu); return;

        case KC_ABS:
            input_commit(cpu);
            if (cpu->flags.inv) {
                double v = bcd_to_double(&cpu->reg[REG_A]);
                bcd_from_double(&cpu->reg[REG_A], (v > 0) ? 1.0 : (v < 0) ? -1.0 : 0.0);
                cpu->flags.inv = false;
            } else {
                double v = bcd_to_double(&cpu->reg[REG_A]);
                bcd_from_double(&cpu->reg[REG_A], fabs(v));
            }
            display_trailing_dp = false;
            format_display(cpu); return;

        case KC_ENG:
            if (cpu->flags.inv) {
                cpu->flags.eng = false;
                cpu->flags.inv = false;
            } else {
                cpu->flags.eng = true;
            }
            return;

        case KC_DMS:
            input_commit(cpu);
            if (cpu->flags.inv) {
                double v = bcd_to_double(&cpu->reg[REG_A]);
                bcd_from_double(&cpu->reg[REG_A], decimal_to_dms(v));
                cpu->flags.inv = false;
            } else {
                double v = bcd_to_double(&cpu->reg[REG_A]);
                bcd_from_double(&cpu->reg[REG_A], dms_to_decimal(v));
            }
            display_trailing_dp = false;
            format_display(cpu); return;

        case KC_ADV:
            printer_advance(&g_printer);
            append_to_file("/print.txt", "\r\n");
            Serial.println("[ADV] Paper advance");
            return;

        case KC_PRT: {
            char buf[32];
            format_value_string(cpu, buf, sizeof(buf));
            printer_prt_register(&g_printer, buf);
            char line[64];
            snprintf(line, sizeof(line), "[%010llu] %s\r\n", cpu->total_cycles, buf);
            append_to_file("/print.txt", line);
            Serial.printf("[PRT] %s\n", buf);
            return;
        }

        case KC_LIST: {
            // Prima elencava solo byte esadecimali grezzi in righe da
            // 16, come /api/prog e la lettura scheda facevano prima
            // dei rispettivi fix — ora stesso formato "passo | hex |
            // comando" ovunque, per coerenza in tutte le visualizzazioni.
            // Buffer sull'heap: con mnemonici invece dei soli byte,
            // un programma vicino ai 960 passi può arrivare a ~15KB,
            // troppo per uno stack array su un task ESP32.
            const size_t cap = (size_t)cpu->prog_len * 20 + 256;
            char *listing = (char*)malloc(cap);
            if (!listing) {
                Serial.println("[LIST] malloc fallita, listato non salvato");
                return;
            }
            int pos = 0;
            pos += snprintf(listing + pos, cap - pos,
                "; TI-59 Program Listing\r\n; Steps: %d\r\n; Cycles: %llu\r\n"
                "; Passo | Hex | Comando\r\n; ----------------------\r\n",
                cpu->prog_len, cpu->total_cycles);
            for (uint16_t i = 0; i < cpu->prog_len && pos < (int)cap - 48; i++) {
                uint8_t codice = cpu->prog[i];
                pos += snprintf(listing + pos, cap - pos, "%03u | %02X | %s\r\n",
                                 i, codice, get_mnemonic_name(codice));
            }
            write_file("/listing.txt", listing);
            free(listing);
            Serial.println("[LIST] Saved to /listing.txt");
            return;
        }

        case KC_WRITE: {
            input_clear();
            tms1500_on_physical_write(cpu);
            return;
        }

        case KC_PI:
            input_commit(cpu); bcd_from_double(&cpu->reg[REG_A], M_PI);
            format_display(cpu); return;

        case KC_CP:
            // When executing as-is from a library ROM, CP erases the library
            // data registers (lib_ram) but NOT the user program memory — the
            // user's program in cpu->prog must survive. The library program
            // itself is read-only from the ROM and cannot be affected anyway.
            if (showing_lib_prog) {
                memset(lib_ram, 0, sizeof(lib_ram));
                for (int i = 0; i < 10; i++) user_flags[i] = false;
                // CP (2nd CE) sul TI-59 reale azzera SEMPRE il registro T
                // ("CP clears the T register", firmware ufficiale TMC0541,
                // Fast Mode docs: CP a step 313 azzera T per il test x=t a
                // step 316). ML-18 (e molti altri programmi Master Library)
                // usa CP nel tasto di INIT (E' = CP FIX 2) proprio per
                // mettere T=0, così premendo "0" + tasto A/B/C/D il test
                // x=t innesca il calcolo della variabile mancante invece
                // di memorizzare lo zero. Senza questo, T restava un
                // valore spazzatura e ML-18 si fermava mostrando 0.00.
                bcd_zero(&cpu->reg[REG_T]);
                cpu->pending_reg = PENDING_REG_NONE;
                cpu->pending_digits = 0;
                cpu->pending_op = PENDING_OP_NONE;
                cpu->pending_2nd = false;
                pending_indirect = false;
                pending_value = 0;
                op_pending = false; op_code = 0; op_digits = 0;
                dsz_phase = 0; dsz_reg_val = 0; dsz_reg_digits = 0; dsz_addr_val = 0; dsz_addr_digits = 0;
                lib_page_pending = false; lib_page_digits = 0; lib_page_val = 0;
                fix_pending = false; hir_sp = 0; paren_depth = 0;
                input_clear(); cpu->flags.error = false; cpu->flags.inv = false;
                cpu->flags.sci = false; cpu->flags.eng = false;
                cpu->stack_lift_enabled = false;
                display_trailing_dp = false;
                format_display(cpu); return;
            }
            cpu->prog_len = 0;
            cpu->prog_pc = 0;
            lib_page_selected = false;
            showing_lib_prog = false;
            memset(cpu->prog, 0, PROG_SIZE);
            for (int i = 0; i < 10; i++) custom_label_pc[i] = 0xFFFF;
            // CP (2nd CE) azzera anche il registro T (comportamento TI-59
            // reale: "CP clears the T register" — vedi commento nel ramo
            // libreria qui sopra).
            bcd_zero(&cpu->reg[REG_T]);
            cpu->pending_reg = PENDING_REG_NONE;
            cpu->pending_digits = 0;
            cpu->pending_op = PENDING_OP_NONE;
            cpu->pending_2nd = false;
            pending_indirect = false;
            pending_value = 0;
            op_pending = false;
            op_code = 0;
            op_digits = 0;
            lib_page_pending = false; lib_page_digits = 0; lib_page_val = 0;
            dsz_phase = 0; dsz_reg_val = 0; dsz_reg_digits = 0; dsz_addr_val = 0; dsz_addr_digits = 0;
            input_clear();
            cpu->flags.lrn = false;
            format_display(cpu);
            return;

        case KC_CLR_2ND:
            // Clear (2nd CLR): clears display, pending ops, but NOT program
            bcd_zero(&cpu->reg[REG_A]); cpu->pending_op = PENDING_OP_NONE;
            cpu->pending_reg = PENDING_REG_NONE; cpu->pending_digits = 0;
            input_clear(); cpu->flags.error = false;
            cpu->stack_lift_enabled = false;
            hir_sp = 0; paren_depth = 0;
            format_display(cpu);
            return;

        default:
            return;
    }
}

// ═══════════════════════════════════════════════════════════
// ESECUZIONE PROGRAMMA — decodifica diretta istruzioni da ROM
// ════════════════════════════════════════════════════════════

// Forward helpers
static bool read_2digit(TMS1500_State *cpu, uint8_t *out);
static bool read_3digit(TMS1500_State *cpu, uint16_t *out);
static bool read_label(TMS1500_State *cpu, uint8_t *out);
static bool read_next(TMS1500_State *cpu, uint8_t *out);

static void exec_program_step(TMS1500_State *cpu) {
    uint16_t plen = exec_prog_len(cpu);
    if (!cpu->flags.run || plen == 0) return;

    // Legge opcode e decodifica istruzione completa
    uint16_t *pc = &cpu->prog_pc;
    uint8_t opcode = prog_read_step(cpu, *pc);

    if (g_trace_steps) {
        double rA = bcd_to_double(&cpu->reg[REG_A]);
        double rD = bcd_to_double(&cpu->reg[REG_D]);
        double rB = bcd_to_double(&cpu->reg[REG_B]);
        if (showing_lib_prog) {
            uint16_t local = (*pc >= lib_scope_addr) ? (uint16_t)(*pc - lib_scope_addr) : 0;
            Serial.printf("[STEP] pc=%u (local=%u) op=%u A=%.10g D=%.10g B=%.10g\n",
                          (unsigned)*pc, (unsigned)local, (unsigned)opcode,
                          rA, rD, rB);
        } else {
            Serial.printf("[STEP] pc=%u op=%u A=%.10g D=%.10g B=%.10g\n",
                          (unsigned)*pc, (unsigned)opcode, rA, rD, rB);
        }
    }

    // Per programmi libreria: arresta se si raggiunge la fine della ROM fisica
    // (NON del programma designato — la ROM condivide subroutine tra programmi
    // diversi tramite SBR/GTO a etichette fuori dal proprio scopo, quindi il
    // limite va sulla ROM intera, non su lib_scope_addr+lib_scope_len).
    if (showing_lib_prog) {
        const LibraryModule *mod = library_get_active();
        if (mod && *pc >= mod->rom_size) {
            cpu->flags.run = false;
            cpu->flags.idle = true;
            showing_lib_prog = false;
            for (int i = 0; i < 10; i++) lib_custom_label_pc[i] = 0xFFFF;
            // NON azzerare lib_selected_page qui: il programma si è
            // fermato ma il modulo è ancora attivo — l'overlay deve
            // restare in vista (etichetta fisica) finché l'utente non
            // esce esplicitamente (RST / 2nd Pgm 00 / cambio modulo).
			lib_page_selected = false;
            return;
        }
    }

    // LBL xx (2 byte) - skip label e parametro
    if (opcode == KC_LBL) {
        *pc += 2;
        if (showing_lib_prog) {
            const LibraryModule *mod = library_get_active();
            if (mod && *pc >= mod->rom_size) { cpu->flags.run = false; cpu->flags.idle = true; }
        } else if (*pc >= plen) {
            *pc = 0;
        }
        return;
    }

    // INV prefix (next instruction executed with INV flag)
    if (opcode == KC_INV) {
        if (showing_lib_prog) {
            const LibraryModule *mod = library_get_active();
            if (mod && *pc + 1 >= mod->rom_size) return;
        } else if (*pc + 1 >= plen) {
            return;
        }
        cpu->flags.inv = true;
        *pc += 1;
        uint8_t next_op = prog_read_step(cpu, *pc);
        // Re-enter to execute the next opcode with INV set
        exec_program_step(cpu);
        cpu->flags.inv = false;
        return;
    }

    // Default PC advance (will be overridden by jumps/subroutines)
    *pc += 1;

    // R/S - stop program (keep lib mode active so next label press still targets library)
    if (opcode == KC_RS) {
        cpu->flags.run = false;
        cpu->flags.idle = true;
        return;
    }

    // RTN (92) - return from subroutine
    if (opcode == KC_RETURN) {
        if (cpu->sp > 0) {
            uint8_t old_sp = cpu->sp;
            cpu->sp--;
            *pc = cpu->stack[cpu->sp];
            cpu->pending_op = cpu->stack_pending_op[cpu->sp]; bcd_copy(&cpu->operand_x, &cpu->stack_operand_x[cpu->sp]);
            Serial.printf("[STK] RET sp=%u->%u pop=%u in_rom=%u\n",
                (unsigned)old_sp, (unsigned)cpu->sp, (unsigned)cpu->stack[cpu->sp],
                (unsigned)cpu->stack_in_rom[cpu->sp]);
            // Ripristina SEMPRE lo stato del chiamante (vedi commento
            // gemello nel ramo interattivo INV+SBR sopra in process_keycode).
            showing_lib_prog = cpu->stack_in_rom[cpu->sp];
            if (showing_lib_prog) {
                lib_scope_addr = cpu->stack_rom_base[cpu->sp];
                lib_scope_len = cpu->stack_rom_len[cpu->sp];
            }
            cpu->stack_in_rom[cpu->sp] = false;
            // Safety net: if stack is now empty and pc is outside the current
            // library scope, the program has no valid address to continue at.
            // Stop execution but keep library mode active so the user can
            // press another label key to restart. NON azzerare
            // lib_selected_page: l'overlay resta in vista (vedi commento
            // gemello al limite della ROM).
            if (cpu->sp == 0 && showing_lib_prog &&
				(*pc < lib_scope_addr || *pc >= lib_scope_addr + lib_scope_len)) {
				cpu->flags.run = false;
				cpu->flags.idle = true;
				showing_lib_prog = false;
				lib_page_selected = false;
				return;
			}
        } else {
            // Stack empty: RTN acts like R/S, stops program (keep lib mode active)
            Serial.printf("[STK] RET empty stack — stop\n");
            cpu->flags.run = false;
            cpu->flags.idle = true;
        }
        return;
    }

    // Helper macros
    #define READ2(d) read_2digit(cpu, d)
    #define READ3(d) read_3digit(cpu, d)
    #define READL(d) read_label(cpu, d)
    #define NEXT(c)  read_next(cpu, c)

    // DSZ nn LLL
    if (opcode == KC_DSZ) {
        uint8_t reg;
        if (READ2(&reg)) {
            uint8_t lbl;
            if (READL(&lbl)) {
                uint16_t target = find_label(cpu, lbl);
                if (target != 0xFFFF) dsz_do(cpu, reg, target);
                else cpu->flags.error = true;
            } else {
                uint16_t addr;
                if (READ3(&addr)) {
                    uint16_t target_addr = showing_lib_prog
                        ? (lib_scope_addr + (addr % plen))
                        : (addr % plen);
                    dsz_do(cpu, reg, target_addr);
                }
                else cpu->flags.error = true;
            }
        } else cpu->flags.error = true;
        return;
    }

    // ═══════════════════════════════════════════════════════════
    // IFF n LLL — "If flag n set, go to" (opcode 87). CRITICO:
    // prima di questa correzione, IFF non aveva un case dedicato qui
    // e cadeva nel ramo generico "delega a process_keycode", che usa
    // la macchina a stati pending_reg pensata per la digitazione da
    // tastiera. Quella macchina consuma UN SOLO byte (la cifra flag)
    // e poi esegue subito "se flag non impostato, salta la prossima
    // istruzione" — semantica sbagliata E non consuma il byte target,
    // corrompendo la lettura di ogni istruzione successiva. Confermato
    // dal disassemblato ufficiale TMC0541 (es. ML-11 offset 0027:
    // "87 00 97" = IFF 00, target=byte singolo 97, riusato come
    // etichetta interna 2nd-Dsz — stesso pattern di GTO/SBR/DSZ).
    // Semantica reale: se il flag n è impostato, salta a LLL
    // (etichetta o indirizzo assoluto a 3 cifre); se non impostato,
    // prosegui in sequenza. Non è uno "skip next instruction".
    // ═══════════════════════════════════════════════════════════
    if (opcode == KC_IFFL) {
        uint8_t flagnum;
        if (!NEXT(&flagnum) || flagnum > 9) { cpu->flags.error = true; return; }
        uint8_t lbl;
        if (READL(&lbl)) {
            uint16_t target = find_label(cpu, lbl);
            if (target == 0xFFFF) { cpu->flags.error = true; return; }
            if (user_flags[flagnum]) *pc = target;
            return;
        }
        uint16_t addr;
        if (READ3(&addr)) {
            uint16_t target_addr = showing_lib_prog
                ? (lib_scope_addr + (addr % plen))
                : (addr % plen);
            if (user_flags[flagnum]) *pc = target_addr;
            return;
        }
        cpu->flags.error = true;
        return;
    }

    // SBR / GTO con label (A-E, A'-E') o address 3-digit
    if (opcode == KC_SBR || opcode == KC_GTO) {
        bool is_sbr = (opcode == KC_SBR);

        // INV + SBR = Return (92). In un programma "INV SBR" è memorizzato
        // come [INV, SBR] (2 byte) e il prefisso INV sopra ci ha riportato
        // qui con cpu->flags.inv = true. Prima di questa correzione il flag
        // veniva ignorato e INV+SBR eseguiva una SBR normale (push+jump):
        // la subroutine non tornava mai al chiamante. Deve invece fare POP
        // dallo stack, esattamente come il RETURN esplicito (KC_RETURN).
        // INV+GTO resta un GTO normale: INV non è definito per GTO.
        if (cpu->flags.inv) {
            cpu->flags.inv = false;
            if (is_sbr) {
                if (cpu->sp > 0) {
                    cpu->sp--;
                    *pc = cpu->stack[cpu->sp];
                    cpu->pending_op = cpu->stack_pending_op[cpu->sp]; bcd_copy(&cpu->operand_x, &cpu->stack_operand_x[cpu->sp]);
                    Serial.printf("[STK] INV+SBR prog RET sp=%u->%u pop=%u in_rom=%u\n",
                        (unsigned)cpu->sp + 1, (unsigned)cpu->sp, (unsigned)cpu->stack[cpu->sp],
                        (unsigned)cpu->stack_in_rom[cpu->sp]);
                    showing_lib_prog = cpu->stack_in_rom[cpu->sp];
                    if (showing_lib_prog) {
                        lib_scope_addr = cpu->stack_rom_base[cpu->sp];
                        lib_scope_len = cpu->stack_rom_len[cpu->sp];
                    }
                    cpu->stack_in_rom[cpu->sp] = false;
                    // Safety net gemello del RETURN: se lo stack ora è vuoto
                    // e pc è fuori dallo scope libreria corrente, il chiamante
                    // non ha un indirizzo valido a cui continuare: ferma.
                    if (cpu->sp == 0 && showing_lib_prog &&
                        (*pc < lib_scope_addr || *pc >= lib_scope_addr + lib_scope_len)) {
                        cpu->flags.run = false;
                        cpu->flags.idle = true;
                        showing_lib_prog = false;
                        lib_page_selected = false;
                        return;
                    }
                } else {
                    cpu->flags.run = false;
                    cpu->flags.idle = true;
                }
                return;
            }
        }

        // Se una PGM ha appena prenotato un programma (lib_page_selected),
        // questa è la SBR/GTO che lo attiva davvero: salva lo scope del
        // chiamante (solo per SBR, che si aspetta un ritorno — GTO no)
        // e passa a quello del programma appena designato PRIMA di
        // risolvere l'etichetta/indirizzo, che vanno cercati nel NUOVO
        // scope, non in quello vecchio.
        bool saved_in_rom = showing_lib_prog;
        uint16_t saved_addr = lib_scope_addr, saved_len = lib_scope_len;
        if (lib_page_selected) {
            // L'overlay resta sul programma che l'utente ha scelto (come la
            // card cartacea reale): NON aggiorniamo lib_selected_page qui,
            // perché le chiamate interne (Pgm nn) cambiano solo lo scope di
            // esecuzione, non la selezione a schermo.
            rebuild_lib_labels(lib_pending_addr, lib_pending_len);
            lib_scope_addr = lib_pending_addr;
            lib_scope_len  = lib_pending_len;
            showing_lib_prog = true;
            lib_page_selected = false;
            // NON azzerare lib_ram qui: i registri servono a passare dati
            // tra chiamante e chiamato per convenzione documentata (vedi
            // "Register Contents" di ogni programma nel manuale Master
            // Library) — azzerarli romperebbe qualunque programma pensato
            // per essere richiamato come subroutine con dati preimpostati.
            Serial.printf("[LIB] (da programma) Attivato programma %02d as-is\n", lib_selected_page);
        }

        uint8_t lbl;
        if (READL(&lbl)) {
            uint16_t target = find_label(cpu, lbl);
            if (target != 0xFFFF) {
                if (is_sbr && cpu->sp < STACK_SIZE - 1) {
                    // Registra SEMPRE lo stato reale del chiamante catturato
                    // sopra (saved_*), non un "false" forzato: se questa SBR
                    // avviene già dentro la ROM (chiamata annidata a
                    // un'etichetta interna, senza una nuova PGM appena
                    // designata), il chiamante ERA in ROM e il RETURN deve
                    // poterlo ripristinare correttamente.
                    cpu->stack_rom_base[cpu->sp] = saved_addr;
                    cpu->stack_rom_len[cpu->sp]  = saved_len;
                    cpu->stack_in_rom[cpu->sp]   = saved_in_rom;
                    cpu->stack[cpu->sp] = *pc;
                    cpu->stack_pending_op[cpu->sp] = cpu->pending_op; bcd_copy(&cpu->stack_operand_x[cpu->sp], &cpu->operand_x);
                    Serial.printf("[STK] SBR-label push sp=%u val=%u\n", (unsigned)cpu->sp, (unsigned)*pc);
                    cpu->sp++;
                }
                *pc = target;
                if (is_sbr) { cpu->flags.run = true; cpu->flags.idle = false; }
            } else cpu->flags.error = true;
        } else {
            uint16_t addr;
            if (READ3(&addr)) {
                if (is_sbr && cpu->sp < STACK_SIZE - 1) {
                    cpu->stack_rom_base[cpu->sp] = saved_addr;
                    cpu->stack_rom_len[cpu->sp]  = saved_len;
                    cpu->stack_in_rom[cpu->sp]   = saved_in_rom;
                    cpu->stack[cpu->sp] = *pc;
                    cpu->stack_pending_op[cpu->sp] = cpu->pending_op; bcd_copy(&cpu->stack_operand_x[cpu->sp], &cpu->operand_x);
                    Serial.printf("[STK] SBR-3dig push sp=%u val=%u\n", (unsigned)cpu->sp, (unsigned)*pc);
                    cpu->sp++;
                }
                *pc = showing_lib_prog ? (lib_scope_addr + (addr % exec_prog_len(cpu))) : (addr % exec_prog_len(cpu));
                if (is_sbr) { cpu->flags.run = true; cpu->flags.idle = false; }
            } else cpu->flags.error = true;
        }
        return;
    }

    // PGM (36) — designa un programma del modulo libreria (operando =
    // numero programma). NON salta né chiama da sola: si limita a
    // "prenotare" lib_pending_*; è la SBR/GTO (con etichetta o indirizzo)
    // che segue subito dopo a fare davvero la chiamata, esattamente come
    // richiesto dal formato reale "2nd Pgm mm SBR label" (es. ROM ML-01:
    // "36 15 71 88" = PGM 15, SBR [DMS] — un'unica sequenza logica di 4
    // byte, non due istruzioni indipendenti).
    if (opcode == KC_PGM) {
        uint8_t pgm;
        if (showing_lib_prog) {
            uint8_t b1 = prog_read_step(cpu, *pc);
            pgm = b1;
            *pc += 1;
        } else {
            if (!READ2(&pgm)) { cpu->flags.error = true; return; }
        }
        uint16_t addr = 0, plen2 = 0;
        const char *title = nullptr;
        if (library_find_program(pgm, &addr, &plen2, &title)) {
            lib_pending_addr = addr;
            lib_pending_len  = plen2;
            lib_pending_page = pgm;
            lib_page_selected = true;
            Serial.printf("[LIB] (da programma) Pgm %02d designato: %s — in attesa della SBR/GTO che segue\n",
                pgm, title ? title : "?");
        } else {
            cpu->flags.error = true;
        }
        return;
    }

    // ═══════════════════════════════════════════════════════════
    // PGM IND (62) — come PGM, ma il numero di programma è letto
    // INDIRETTAMENTE da un registro dati (puntatore), non come
    // costante letterale a 2 cifre. Confermato dalla ROM reale: usato
    // 10 volte, sempre nella forma "62 rr" (es. ROM ML-01 offset 0103:
    // "62 00" = Pgm Ind, puntatore = registro 00), nel preambolo di
    // stampa/trace condiviso da molti dei 25 programmi Master Library
    // (v. TI Master Library QRG, "Print Routine": STO 00 mm imposta il
    // numero di programma da tracciare, letto qui indirettamente).
    // PRIMA DI QUESTA AGGIUNTA: KC_PGM_IND non aveva alcun case
    // dedicato — cadeva nel fallback verso process_keycode, che lo
    // ignora silenziosamente (default: break) SENZA consumare il byte
    // operando successivo. Quel byte veniva quindi riletto come se
    // fosse una nuova istruzione a sé stante, corrompendo il flusso in
    // tutti e 10 i punti della ROM che usano questo idioma.
    // ═══════════════════════════════════════════════════════════
    if (opcode == KC_PGM_IND) {
        uint8_t ptr_reg;
        if (!READ2(&ptr_reg)) { cpu->flags.error = true; return; }
        BCD_Reg *bank = active_ram_bank(cpu);
        uint16_t pgm = bcd_to_int_reg(&bank[ptr_reg % 100]) % 100;
        uint16_t addr = 0, plen2 = 0;
        const char *title = nullptr;
        if (library_find_program((uint8_t)pgm, &addr, &plen2, &title)) {
            lib_pending_addr = addr;
            lib_pending_len  = plen2;
            lib_pending_page = (uint8_t)pgm;
            lib_page_selected = true;
            Serial.printf("[LIB] (indiretto, da programma) Pgm %02d designato: %s — in attesa della SBR/GTO che segue\n",
                (unsigned)pgm, title ? title : "?");
        } else {
            cpu->flags.error = true;
        }
        return;
    }

    // STO/RCL/SUM/EXC/PROD rr (2-digit register)
    if (opcode == KC_STO || opcode == KC_RCL || opcode == KC_SUM ||
        opcode == KC_EXC || opcode == KC_PROD) {
        input_commit(cpu);
        uint8_t reg;
        if (!READ2(&reg)) { cpu->flags.error = true; return; }
        reg %= 100;
        BCD_Reg *bank = active_ram_bank(cpu);
        switch (opcode) {
            case KC_STO:  bcd_copy(&bank[reg], &cpu->reg[REG_A]); break;
            case KC_RCL:  bcd_copy(&cpu->reg[REG_A], &bank[reg]); cpu->stack_lift_enabled = true; break;
            case KC_SUM:  bcd_add(&bank[reg], &bank[reg], &cpu->reg[REG_A], &cpu->flags); break;
            case KC_EXC: {
                BCD_Reg tmp; bcd_copy(&tmp, &bank[reg]);
                bcd_copy(&bank[reg], &cpu->reg[REG_A]);
                bcd_copy(&cpu->reg[REG_A], &tmp);
                break;
            }
            case KC_PROD: {
                BCD_Reg res; bcd_zero(&res);
                bcd_mul(&res, &bank[reg], &cpu->reg[REG_A], &cpu->flags);
                bcd_copy(&bank[reg], &res);
                break;
            }
        }
        return;
    }

    // IND variants: STO IND, RCL IND, SUM IND, EXC IND, PROD IND, GTO IND
    if (opcode == KC_STO_IND || opcode == KC_RCL_IND || opcode == KC_SUM_IND ||
        opcode == KC_EXC_IND || opcode == KC_PROD_IND || opcode == KC_GTO_IND) {
        input_commit(cpu);
        uint8_t reg;
        if (!READ2(&reg)) { cpu->flags.error = true; return; }
        BCD_Reg *bank = active_ram_bank(cpu);
        uint8_t ptr_reg = reg % 100;
        uint16_t eff = (opcode == KC_GTO_IND) ? bcd_to_int_step(&bank[ptr_reg]) : bcd_to_int_reg(&bank[ptr_reg]);
        eff %= (opcode == KC_GTO_IND) ? 1000 : 100;
        switch (opcode) {
            case KC_STO_IND: bcd_copy(&bank[eff], &cpu->reg[REG_A]); break;
            case KC_RCL_IND: bcd_copy(&cpu->reg[REG_A], &bank[eff]); cpu->stack_lift_enabled = true; break;
            case KC_SUM_IND: bcd_add(&bank[eff], &bank[eff], &cpu->reg[REG_A], &cpu->flags); break;
            case KC_EXC_IND: {
                BCD_Reg tmp; bcd_copy(&tmp, &bank[eff]);
                bcd_copy(&bank[eff], &cpu->reg[REG_A]);
                bcd_copy(&cpu->reg[REG_A], &tmp);
                break;
            }
            case KC_PROD_IND: {
                BCD_Reg res; bcd_zero(&res);
                bcd_mul(&res, &bank[eff], &cpu->reg[REG_A], &cpu->flags);
                bcd_copy(&bank[eff], &res);
                break;
            }
            case KC_GTO_IND: *pc = showing_lib_prog ? (lib_scope_addr + (eff % plen)) : (eff % plen); break;
        }
        return;
    }

    // NOTA: la codifica dei registri STO/RCL/SUM/EXC/PROD nella ROM
    // libreria è già gestita correttamente dal blocco sopra tramite
    // READ2 (a sua volta consapevole di showing_lib_prog): 1 byte con
    // il valore diretto del registro (0-99), sempre — confermato contro
    // il dump reale TMC0541 (es. "RCL 11" = byte singolo "43 11", mai
    // due byte separati "1","1"). Un blocco precedente ipotizzava uno
    // schema alternativo a 1-o-2-byte cifra-per-cifra per i registri
    // ≥20: rimosso perché era sia irraggiungibile (il blocco sopra
    // ritorna sempre per primo) sia basato su un'ipotesi errata.

    // X=T (67) / X≥T (77) — salto CONDIZIONATO a etichetta o indirizzo a
    // 3 cifre, esattamente come GTO (confermato da decine di occorrenze
    // nel disassemblato: "67 96", "67 02 28", "77 02 97", ecc. — MAI un
    // byte singolo). NON sono "salta la prossima istruzione": se la
    // condizione è vera si salta al target: altrimenti si prosegue in
    // sequenza dopo l'operando (già consumato da READL/READ3). INV
    // inverte la condizione (visto anche nel disassemblato: "22 67 ...").
    if (opcode == KC_XEQ_T || opcode == KC_XGE_T) {
        input_commit(cpu);
        cpu->stack_lift_enabled = true;
        double a = bcd_to_double(&cpu->reg[REG_A]);
        double t = bcd_to_double(&cpu->reg[REG_T]);
        bool cond = (opcode == KC_XEQ_T) ? (fabs(a - t) < 1e-12)
                                          : (a >= t - 1e-12);
        if (cpu->flags.inv) cond = !cond;
        cpu->flags.inv = false;

        uint8_t lbl;
        uint16_t target;
        bool have_target;
        if (READL(&lbl)) {
            target = find_label(cpu, lbl);
            have_target = (target != 0xFFFF);
        } else {
            uint16_t addr;
            have_target = READ3(&addr);
            target = have_target
                ? (showing_lib_prog ? (uint16_t)(lib_scope_addr + (addr % exec_prog_len(cpu)))
                                     : (uint16_t)(addr % exec_prog_len(cpu)))
                : 0;
        }

        if (!have_target) { cpu->flags.error = true; return; }
        if (cond) *pc = target;   // altrimenti prosegue in sequenza (pc già avanzato oltre l'operando)
        return;
    }

    // FIX n (1 digit) — INV FIX clears FIX (like FIX 9)
    if (opcode == KC_FIX) {
        if (cpu->flags.inv) {
            cpu->flags.fix = false;
            cpu->flags.inv = false;
            return;
        }
        uint8_t d;
        if (NEXT(&d) && d <= 9) {
            if (d == 9) cpu->flags.fix = false;
            else { cpu->flags.fix = true; cpu->fix_digits = d; }
        } else cpu->flags.fix = false;
        return;
    }

    // OP nn (2 digits)
    if (opcode == KC_OP) {
        uint8_t op;
        if (READ2(&op)) exec_op(cpu, op);
        else cpu->flags.error = true;
        return;
    }

    // St Flg n (1 digit) — set user flag
    if (opcode == KC_STFL) {
        uint8_t flagnum;
        if (NEXT(&flagnum) && flagnum <= 9)
            user_flags[flagnum] = true;
        else
            cpu->flags.error = true;
        return;
    }

    // Single-byte math/func ops: delegate to process_keycode
    cpu->stack_lift_enabled = true;
    process_keycode(cpu, opcode);
    return;

    #undef READ2
    #undef READ3
    #undef READL
    #undef NEXT
}

// Helpers for reading program operands
static bool read_2digit(TMS1500_State *cpu, uint8_t *out) {
    if (showing_lib_prog) {
        /* La ROM Master Library non incapsula i registri a 2 cifre come
         * due keycode-cifra separati (come farebbe un programma digitato
         * a tastiera): li memorizza come UN SOLO byte con il valore
         * diretto 0-99 (es. "STO 09" = opcode + un byte di valore 9). */
        const LibraryModule *mod = library_get_active();
        uint16_t prog_end = mod ? mod->rom_size : 0;
        if (cpu->prog_pc >= prog_end) return false;
        uint8_t v = prog_read_step(cpu, cpu->prog_pc);
        cpu->prog_pc += 1;
        if (v > 99) return false;
        *out = v;
        return true;
    }
    if (cpu->prog_pc + 1 >= cpu->prog_len) return false;
    uint8_t d1 = prog_read_step(cpu, cpu->prog_pc);
    uint8_t d2 = prog_read_step(cpu, cpu->prog_pc + 1);
    cpu->prog_pc += 2;
    if (d1 > 9 || d2 > 9) return false;
    *out = d1 * 10 + d2;
    return true;
}

static bool read_3digit(TMS1500_State *cpu, uint16_t *out) {
    if (showing_lib_prog) {
        /* Indirizzi GTO/SBR/DSZ nella ROM Master Library: due byte,
         * centinaia (0-9) poi decine+unità impacchettate (00-99),
         * combinati come centinaia*100 + decine_unita. Non tre
         * byte-cifra singoli come nei programmi utente digitati. */
        const LibraryModule *mod = library_get_active();
        uint16_t prog_end = mod ? mod->rom_size : 0;
        if (cpu->prog_pc + 1 >= prog_end) return false;
        uint8_t hund = prog_read_step(cpu, cpu->prog_pc);
        uint8_t rest = prog_read_step(cpu, cpu->prog_pc + 1);
        cpu->prog_pc += 2;
        if (hund > 9 || rest > 99) return false;
        *out = (uint16_t)(hund * 100 + rest);
        return true;
    }
    if (cpu->prog_pc + 2 >= cpu->prog_len) return false;
    uint8_t d1 = prog_read_step(cpu, cpu->prog_pc);
    uint8_t d2 = prog_read_step(cpu, cpu->prog_pc + 1);
    uint8_t d3 = prog_read_step(cpu, cpu->prog_pc + 2);
    cpu->prog_pc += 3;
    if (d1 > 9 || d2 > 9 || d3 > 9) return false;
    *out = (uint16_t)(d1 * 100 + d2 * 10 + d3);
    return true;
}

static bool read_label(TMS1500_State *cpu, uint8_t *out) {
    if (showing_lib_prog) {
        const LibraryModule *mod = library_get_active();
        uint16_t prog_end = mod ? mod->rom_size : 0;
        if (cpu->prog_pc >= prog_end) return false;
    } else if (cpu->prog_pc >= cpu->prog_len) return false;
    uint8_t kc = prog_read_step(cpu, cpu->prog_pc);
    // Regola vera del TI-59: dopo GTO/SBR/DSZ, un byte 0-9 è l'inizio di
    // un indirizzo numerico letterale (000-999); QUALSIASI altro tasto è
    // un'etichetta valida. Non è una whitelist ristretta ai soli 10 tasti
    // utente (A-E/A'-E'/"="): la ROM Master Library usa moltissime altre
    // etichette "interne" (es. CE, CLR) per le subroutine condivise fra
    // programmi, mai raggiungibili da tastiera ma perfettamente valide
    // come bersaglio di SBR/GTO dentro la ROM stessa.
    if (kc <= 9) return false;
    // Eccezione confermata da fonte primaria (TI Master Library Quick
    // Reference Guide, sez. "Programming Notes / Labels"): "Any key on
    // the keyboard can be used as label except 2nd, LRN, Ins, Del, SST,
    // BST, Ind and the numbers 0-9." Questi 6 tasti extra (oltre a 0-9,
    // già escluso sopra) NON sono etichette valide quando il programma
    // gira dalla memoria utente digitata da tastiera. Dentro la ROM
    // Master Library invece questa restrizione non si applica (vedi
    // commento sopra: la ROM riusa questi byte come etichette interne
    // per subroutine condivise, mai raggiungibili da tastiera) — quindi
    // il filtro scatta solo per !showing_lib_prog.
    if (!showing_lib_prog &&
        (kc == KC_LRN || kc == KC_IND || kc == KC_SST ||
         kc == KC_INS || kc == KC_BST || kc == KC_DEL)) {
        return false;
    }
    cpu->prog_pc += 1;
    *out = kc;
    return true;
}


static bool read_next(TMS1500_State *cpu, uint8_t *out) {
    if (showing_lib_prog) {
        const LibraryModule *mod = library_get_active();
        uint16_t prog_end = mod ? mod->rom_size : 0;
        if (cpu->prog_pc >= prog_end) return false;
    } else if (cpu->prog_pc >= cpu->prog_len) return false;
    *out = prog_read_step(cpu, cpu->prog_pc);
    cpu->prog_pc += 1;
    return true;
}

// ═══════════════════════════════════════════════════════════
// INIT / RESET / STEP
// ═══════════════════════════════════════════════════════════

// Implementazione di default (no-op): sovrascritta da wifilink.cpp,
// che ha accesso al sottosistema schede (cardemu) e a un CardEmuState
// vero. Se questo file viene compilato/linkato da solo (o se il layer
// esterno non fornisce un hook), premere WRITE semplicemente non fa
// nulla invece di corrompere qualcosa o fallire in modo silenzioso.
__attribute__((weak)) void tms1500_on_physical_write(TMS1500_State *cpu) {
    (void)cpu;
}

void tms1500_init(TMS1500_State *cpu) {
    memset(cpu, 0, sizeof(TMS1500_State));
    cpu->trig_mode = 0; cpu->flags.idle = true;
    cpu->pending_reg = PENDING_REG_NONE; cpu->pending_digits = 0; cpu->pending_2nd = false;
    bcd_zero(&cpu->reg[REG_A]); format_display(cpu);
    for (int i = 0; i < 10; i++) custom_label_pc[i] = 0xFFFF;
    pending_indirect = false; pending_value = 0;
    inv_pending = false;
    for (int i = 0; i < 10; i++) user_flags[i] = false;
    op_pending = false; op_code = 0; op_digits = 0;
    hir_sp = 0; paren_depth = 0;
    dsz_phase = 0; dsz_reg_val = 0; dsz_reg_digits = 0; dsz_addr_val = 0; dsz_addr_digits = 0;
    lib_page_pending = false; lib_page_digits = 0; lib_page_val = 0;
    lib_page_selected = false; lib_selected_page = 0;
    showing_lib_prog = false; lib_scope_addr = 0; lib_scope_len = 0;
    memset(lib_ram, 0, sizeof(lib_ram));   // solo all'accensione, non su RST (v. tms1500_reset)
    printer_init(&g_printer);
}

void tms1500_reset(TMS1500_State *cpu) {
    cpu->sp = 0; cpu->prog_pc = showing_lib_prog ? lib_scope_addr : 0; cpu->flags.run = false;
    cpu->flags.error = false; cpu->flags.inv = false;
    cpu->flags.pause = false;
    cpu->flags.lrn = false; cpu->flags.idle = true;
    cpu->pending_reg = PENDING_REG_NONE; cpu->pending_digits = 0; cpu->pending_2nd = false;
    bcd_zero(&cpu->reg[REG_A]); cpu->pending_op = PENDING_OP_NONE;
    input_clear(); format_display(cpu);
    pending_indirect = false; pending_value = 0;
    inv_pending = false;
    for (int i = 0; i < 10; i++) user_flags[i] = false;
    op_pending = false; op_code = 0; op_digits = 0;
    hir_sp = 0; paren_depth = 0;
    dsz_phase = 0; dsz_reg_val = 0; dsz_reg_digits = 0; dsz_addr_val = 0; dsz_addr_digits = 0;
    lib_page_pending = false; lib_page_digits = 0; lib_page_val = 0;
    lib_page_selected = false;
	lib_selected_page = 0;
    for (int i = 0; i < 10; i++) custom_label_pc[i] = 0xFFFF;
}

void tms1500_step(TMS1500_State *cpu, KeyboardState *kbd, DisplayState *disp) {
    (void)disp;
    cpu->total_cycles++;
    if (kbd->key_ready) {
        uint8_t row = kbd->last_row, col = kbd->last_col;
        kbd->key_ready = false;
        if (row < 9 && col < 5) process_keycode(cpu, KEY_MAP[row][col]);
    }
    // PAUSE non bloccante: il programma resta "in pausa" 500ms (il pc è già
    // stato avanzato oltre l'istruzione PAUSE da exec_program_step), durante
    // i quali NON si esegue alcuna istruzione ma i tasti continuano a essere
    // processati sopra — così R/S può fermare un programma in pausa. Se il
    // programma viene fermato o il tempo scade, esci subito dalla pausa.
    if (cpu->flags.pause) {
        if (!cpu->flags.run || (long)(millis() - pause_until_ms) >= 0)
            cpu->flags.pause = false;
        else
            return;
    }
    // Esecuzione programmi pacingata a tempo reale. In modalità moderna
    // (g_realistic_timing == false) esegue subito, come prima; in Old
    // trattiene le istruzioni al ritmo dell'originale × moltiplicatore,
    // massimo un passo per chiamata (così tastiera/web restano reattivi).
    update_step_rate();
    if (cpu->flags.run) {
        if (g_realistic_timing) {
            unsigned long now = millis();
            if (g_pace_last_ms == 0) g_pace_last_ms = now;
            unsigned long elapsed = now - g_pace_last_ms;
            g_pace_last_ms = now;
            if (elapsed > 0) {
                unsigned long due = (unsigned long)((float)TI59_INSTR_PER_SEC *
                                    g_timing_multiplier * (float)elapsed / 1000.0f);
                g_inst_acc += due;
                // cap anti-deriva: mai più di 1s di istruzioni accumulate,
                // altrimenti un loop lento accumulerebbe un ritardo enorme.
                if (g_inst_acc > TI59_INSTR_PER_SEC) g_inst_acc = TI59_INSTR_PER_SEC;
            }
            if (g_inst_acc >= 1) { g_inst_acc--; exec_program_step(cpu); }
        } else if (cpu->total_cycles % CPU_CYCLES_PER_TICK == 0) {
            exec_program_step(cpu);
        }
    } else {
        g_inst_acc = 0;
    }
}

// ═══════════════════════════════════════════════════════════
// SERIALIZZAZIONE
// ═══════════════════════════════════════════════════════════

void tms1500_load_prog(TMS1500_State *cpu, const uint8_t *data, uint16_t len) {
    if (len > PROG_SIZE) len = PROG_SIZE;
    memcpy(cpu->prog, data, len); cpu->prog_len = len; cpu->prog_pc = 0;
    showing_lib_prog = false;
    lib_page_selected = false;
    lib_selected_page = 0;   // la selezione ROM si chiude: l'overlay card deve tornare in vista
    lib_scope_addr = 0; lib_scope_len = 0;
    for (int i = 0; i < 10; i++) lib_custom_label_pc[i] = 0xFFFF;
    rebuild_labels(cpu);
    prog_dirty = false;
}

void tms1500_save_prog(TMS1500_State *cpu, uint8_t *out, uint16_t *len) {
    memcpy(out, cpu->prog, cpu->prog_len); *len = cpu->prog_len;
}

void tms1500_keypress(TMS1500_State *cpu, KeyboardState *kbd, uint8_t row, uint8_t col) {
    (void)cpu;
    if (row < 9 && col < 5) {
        kbd->last_row = row; kbd->last_col = col; kbd->key_ready = true;
    }
}

// ═══════════════════════════════════════════════════════════
// GETTER
// ═══════════════════════════════════════════════════════════

bool tms1500_get_trailing_dp(void) {
    return display_trailing_dp;
}

bool tms1500_get_pending_2nd(const TMS1500_State *cpu) {
    return cpu->pending_2nd;
}

bool tms1500_get_input_ee_state(int *ee_len) {
    if (!input_has_ee) return false;
    if (ee_len) *ee_len = input_ee_len;
    return true;
}

void tms1500_get_input_buf(char *buf, unsigned int len) {
    if (!buf || len == 0) return;
    strncpy(buf, input_buf, len - 1);
    buf[len - 1] = '\0';
}

bool tms1500_get_input_has_ee(void) {
    return input_has_ee;
}

bool tms1500_get_input_has_dot(void) {
    return input_has_dot;
}

// ── Display / Program helpers (defined here to satisfy linker) ──

void format_display(TMS1500_State *cpu) {
    (void)cpu;
}

void prog_store_step(TMS1500_State *cpu, uint8_t kc) {
    if (cpu->prog_pc < PROG_SIZE) {
        cpu->prog[cpu->prog_pc] = kc;
        if (cpu->prog_pc >= cpu->prog_len)
            cpu->prog_len = cpu->prog_pc + 1;
        cpu->prog_pc++;
        prog_dirty = true;
    }
}

uint8_t prog_read_step(TMS1500_State *cpu, uint16_t addr) {
    if (showing_lib_prog) {
        const LibraryModule *m = library_get_active();
        if (m && addr < m->rom_size) return m->rom[addr];
        return 0xFF;
    }
    if (addr < PROG_SIZE) return cpu->prog[addr];
    return 0xFF;
}

// Listato completo del modulo libreria attivo: TUTTI i programmi
// (non solo quello eventualmente "mostrato"), formato a colonne
// "numero_programma passo hex comando" — così le subroutine condivise
// tra programmi restano leggibili nel loro contesto reale. Usata sia
// dal tasto fisico 2nd List (quando si guarda un programma da modulo)
// sia dall'endpoint web dei moduli. Alloca con malloc: il chiamante
// deve fare free() sul puntatore restituito (nullptr se nessun modulo
// attivo o allocazione fallita).
char* build_library_listing(size_t *out_len) {
    const LibraryModule *m = library_get_active();
    if (!m) { if (out_len) *out_len = 0; return nullptr; }

    size_t cap = 128;
    for (int p = 0; p < m->program_count; p++) cap += (size_t)m->programs[p].len * 14 + 64;
    char *buf = (char*)malloc(cap);
    if (!buf) { if (out_len) *out_len = 0; return nullptr; }

    size_t pos = 0;
    pos += snprintf(buf + pos, cap - pos,
        "; %s — listato completo (%d programmi)\r\n"
        "; Programma | Passo | Hex | Comando\r\n"
        "; --------------------------------\r\n",
        m->name, m->program_count);

    for (int p = 0; p < m->program_count && pos < cap - 64; p++) {
        const LibraryProgram *lp = &m->programs[p];
        pos += snprintf(buf + pos, cap - pos, "; --- #%02u %s ---\r\n", lp->num, lp->title);
        for (uint16_t i = 0; i < lp->len && pos < cap - 32; i++) {
            uint8_t codice = m->rom[lp->addr + i];
            pos += snprintf(buf + pos, cap - pos, "%02u %03u %02X %s\r\n",
                             lp->num, i, codice, get_mnemonic_name(codice));
        }
    }
    if (out_len) *out_len = pos;
    return buf;
}

/* Rimuove gli zeri finali dopo il punto decimale (mantiene il punto
 * se restano zero cifre frazionarie, es. "5." — stile display TI). */
 
static void trim_trailing_zeros(char *s) {
    char *dot = strchr(s, '.');
    if (!dot) return;
    char *end = s + strlen(s) - 1;
    while (end > dot && *end == '0') { *end = '\0'; end--; }
}

void tms1500_get_display_string(const TMS1500_State *cpu, char *buf, unsigned int len) {
	if (!buf || len == 0) return;

    // ═══════════════════════════════════════════════════════════
    // INDICATORE "OCCUPATO" — schermo vuoto + "C" nell'ultima
    // posizione a sinistra, come sull'hardware reale durante il
    // calcolo di funzioni lente (sqrt, trig, log, y^x...). Vedi
    // busy_start() per la logica delle due modalità (reale/moderna).
    // ═══════════════════════════════════════════════════════════
    if (busy_active) {
        if ((long)(millis() - busy_until_ms) >= 0) {
            busy_active = false;   // tempo scaduto (autentico in Old, minimo fisso in New): mostra il display normale
        } else {
            char out_str[13];
            memset(out_str, ' ', 12);
            out_str[0] = 'C';
            out_str[12] = '\0';
            strncpy(buf, out_str, len - 1);
            buf[len - 1] = '\0';
            return;
        }
    }

    format_value_string(cpu, buf, len);
}

// Nucleo di formattazione del valore vero e proprio (modalità LRN,
// immissione in corso, o risultato normale) — NON tocca mai
// l'indicatore "occupato": quello è puramente cosmetico per il display
// dal vivo (vedi tms1500_get_display_string sotto) e non deve mai
// influenzare un lettore interno/programmatico del valore attuale, come
// PRT che stampa il risultato già calcolato nel registro anche se la
// finestra cosmetica di "occupato" non è ancora scaduta.
static void format_value_string(const TMS1500_State *cpu, char *buf, unsigned int len) {
    if (!buf || len == 0) return;

	// ═══════════════════════════════════════════════════════════
    // GESTIONE MODALITÀ LRN (LEARN)
    // ═══════════════════════════════════════════════════════════
    if (cpu->flags.lrn) {
        uint16_t passo = cpu->prog_pc; 
        // Legge il comando se siamo entro i limiti della memoria, altrimenti 00
        uint8_t comando = (passo < PROG_SIZE) ? cpu->prog[passo] : 0;
        
        char tmp_lrn[16];
        snprintf(tmp_lrn, sizeof(tmp_lrn), "%03d %02d", passo, comando);

        // Allinea a destra su 12 posizioni come il resto dell'interfaccia
        char out_str[13];
        memset(out_str, ' ', 12);
        out_str[12] = '\0';
        int tlen = strlen(tmp_lrn);
        if (tlen > 12) tlen = 12;
        memcpy(out_str + 12 - tlen, tmp_lrn, tlen);
        
        strncpy(buf, out_str, len - 1);
        buf[len - 1] = '\0';
        return;
    }

    // ═══════════════════════════════════════════════════════════
    // GESTIONE ERRORI E CALCOLO NORMALE
    // ═══════════════════════════════════════════════════════════
    // Nota: in errore NON si mostra il testo "Error" (il TI-59 reale non
    // ha un display testuale: lampeggia il numero risultante). Il
    // lampeggio è gestito lato UI (wifilink/display driver) leggendo
    // cpu->flags.error; qui si prosegue con la normale formattazione.

    char tmp[32];

    /* ── Input mode: show what the user is typing ── */
    if (input_len > 0 || input_has_ee) {
        strncpy(tmp, input_buf, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';

        char *e_pos = strchr(tmp, 'e');
        if (!e_pos) e_pos = strchr(tmp, 'E');

        if (e_pos) {
            char mant[16] = {0};
            int mant_len = (int)(e_pos - tmp);
            if (mant_len > 15) mant_len = 15;
            memcpy(mant, tmp, mant_len);
            mant[mant_len] = '\0';

            char clean_mant[16];
            int ci = 0;
            for (int i = 0; i < mant_len && ci < 15; i++) {
                char c = mant[i];
                if (c != ' ' && c != 'e' && c != 'E' && c != '+') {
                    clean_mant[ci++] = c;
                }
            }
            clean_mant[ci] = '\0';
            /* If mantissa starts with '.', prepend '0' for display */
            if (clean_mant[0] == '.') {
                memmove(clean_mant + 1, clean_mant, ci + 1);
                clean_mant[0] = '0';
            }

            char *ep = e_pos + 1;
            char exp_sign = ' ';
            char exp_d1 = '0';
            char exp_d2 = '0';

            if (*ep == '+') ep++;
            else if (*ep == '-') {
                exp_sign = '-';
                ep++;
            }
            if (*ep >= '0' && *ep <= '9') exp_d1 = *ep++;
            if (*ep >= '0' && *ep <= '9') exp_d2 = *ep++;

            if (exp_sign == '-') {
                snprintf(tmp, sizeof(tmp), "%s-%c%c", clean_mant, exp_d1, exp_d2);
            } else {
                snprintf(tmp, sizeof(tmp), "%s %c%c", clean_mant, exp_d1, exp_d2);
            }
        }

        char out[13];
        memset(out, ' ', 12);
        out[12] = '\0';
        int tlen = (int)strlen(tmp);
        if (tlen > 12) tlen = 12;
        memcpy(out + 12 - tlen, tmp, tlen);
        strncpy(buf, out, len - 1);
        buf[len - 1] = '\0';
        return;
    }

    /* ── Result mode: format from BCD register ── */
    double val = bcd_to_double(&cpu->reg[REG_A]);
    int sign = (val < 0) ? 1 : 0;
    double aval = fabs(val);

    /* Zero (unless FIX/ENG/SCI overrides format) */
    if (aval < 1e-99) {
        if (cpu->flags.fix) {
            /* fall through to FIX formatting */
        } else if (cpu->flags.sci) {
            snprintf(tmp, sizeof(tmp), "%s0.0000000 00", sign ? "-" : " ");
        } else if (cpu->flags.eng) {
            snprintf(tmp, sizeof(tmp), "%s0.0000000 00", sign ? "-" : " ");
        } else {
            snprintf(tmp, sizeof(tmp), "%s0", sign ? "-" : " ");
            char out[13];
            memset(out, ' ', 12); out[12] = '\0';
            int tlen = (int)strlen(tmp);
            if (tlen > 12) tlen = 12;
            memcpy(out + 12 - tlen, tmp, tlen);
            strncpy(buf, out, len - 1);
            buf[len - 1] = '\0';
            return;
        }
    }

    /* ── FIX mode: round to fixed decimal places ── */
    if (cpu->flags.fix) {
        int fd = cpu->fix_digits;
        if (fd > 8) fd = 8;
        double scale = pow(10.0, fd);
        double rounded = round(val * scale) / scale;
        char fmt[16];
        snprintf(fmt, sizeof(fmt), "%% .%df", fd);
        snprintf(tmp, sizeof(tmp), fmt, rounded);
    }
    /* ── ENG mode: engineering notation (exp multiple of 3) ── */
    else if (cpu->flags.eng) {
        if (aval < 1e-99) {
            snprintf(tmp, sizeof(tmp), "%s0.0000000 00", sign ? "-" : " ");
        } else {
            int exp = (int)floor(log10(aval));
            int eng_exp = (exp / 3) * 3;
            if (exp < 0 && exp % 3 != 0) eng_exp -= 3;
            double mant = aval / pow(10.0, eng_exp);
            if (sign) mant = -mant;
            char exp_sign = (eng_exp < 0) ? '-' : ' ';
            int aexp = abs(eng_exp);
            if (aexp > 99) aexp = 99;
            snprintf(tmp, sizeof(tmp), "%.7g%c%02d", mant, exp_sign, aexp);
        }
    }
    /* ── EE / sci mode: scientific notation ── */
    else if (cpu->flags.sci) {
        if (aval < 1e-99) {
            snprintf(tmp, sizeof(tmp), "%s0.0000000 00", sign ? "-" : " ");
        } else {
            int exp = (int)floor(log10(aval));
            double mant = aval / pow(10.0, exp);
            if (sign) mant = -mant;
            char exp_sign = (exp < 0) ? '-' : ' ';
            int aexp = abs(exp);
            if (aexp > 99) aexp = 99;
            snprintf(tmp, sizeof(tmp), "%.7g%c%02d", mant, exp_sign, aexp);
        }
    }
    /* ── Auto format: standard if in range, scientific if out ── */
    else {
        /* Standard display range: 0.0000000001 to 9999999999 */
        if (aval >= 1e-10 && aval < 1e10) {
            /* Standard format: up to 10 significant digits */
            if (aval >= 1.0) {
                int digits = (int)floor(log10(aval)) + 1;
                int decimals = 10 - digits;
                if (decimals < 0) decimals = 0;
                double scale = pow(10.0, decimals);
                double rounded = round(val * scale) / scale;
                /* Build format string dynamically */
                if (decimals > 0) {
                    char fmt[16];
                    snprintf(fmt, sizeof(fmt), "%% .%df", decimals);
                    snprintf(tmp, sizeof(tmp), fmt, rounded);
                } else {
                    snprintf(tmp, sizeof(tmp), "%.0f", rounded);
                    if (tmp[0] == '-') tmp[0] = '-';
                    else if (strlen(tmp) < 12) {
                        /* Shift right by 1 to leave space for sign */
                        memmove(tmp + 1, tmp, strlen(tmp) + 1);
                        tmp[0] = ' ';
                    }
                }
            } else {
                /* Small number: show leading zeros after decimal */
                int leading_zeros = (int)fabs(floor(log10(aval)));
                if (leading_zeros > 9) leading_zeros = 9;
                int decimals = 10;  /* max 10 decimal digits */
                double scale = pow(10.0, decimals);
                double rounded = round(val * scale) / scale;
                char fmt[16];
                snprintf(fmt, sizeof(fmt), "%% .%df", decimals);
                snprintf(tmp, sizeof(tmp), fmt, rounded);
            }
            /* Un risultato "libero" (non FIX) non deve mostrare zeri di
             * riempimento fino a 10 cifre: es. 5 -> "5." e non "5.0000000000",
             * 5.5 -> "5.5" e non "5.5000000000". */
            trim_trailing_zeros(tmp);
        } else {
            /* Out of range: scientific notation */
            int exp = (int)floor(log10(aval));
            double mant = aval / pow(10.0, exp);
            if (sign) mant = -mant;
            char exp_sign = (exp < 0) ? '-' : ' ';
            int aexp = abs(exp);
            if (aexp > 99) aexp = 99;
            snprintf(tmp, sizeof(tmp), "%.7g%c%02d", mant, exp_sign, aexp);
        }
    }

    /* ALLINEA a destra su 12 posizioni */
    char out[13];
    memset(out, ' ', 12);
    out[12] = '\0';
    int tlen = (int)strlen(tmp);
    if (tlen > 12) tlen = 12;
    memcpy(out + 12 - tlen, tmp, tlen);
    strncpy(buf, out, len - 1);
    buf[len - 1] = '\0';
}