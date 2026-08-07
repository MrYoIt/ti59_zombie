/*
 * TI-59 Zombie — emulatore TI-59 su ESP32-S3 (TMS1500)
 * Copyright (C) 2026 Maurizio Petruccioli (MrYo)
 *
 * Questo programma è software libero: puoi ridistribuirlo e/o modificarlo — This program is free software: you can redistribute it and/or modify
 * sotto i termini della GNU General Public License pubblicata da — it under the terms of the GNU General Public License as published by
 * la Free Software Foundation, versione 3 della Licenza, oppure — the Free Software Foundation, either version 3 of the License, or
 * (a tua scelta) qualsiasi versione successiva. — (at your option) any later version.
 *
 * Questo programma è distribuito nella speranza che sia utile, — This program is distributed in the hope that it will be useful,
 * ma SENZA ALCUNA GARANZIA; senza nemmeno la garanzia implicita di — but WITHOUT ANY WARRANTY; without even the implied warranty of
 * COMMERCIABILITÀ o IDONEITÀ PER UNO SCOPO PARTICOLARE.  Vedi la — MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License per maggiori dettagli. — GNU General Public License for more details.
 *
 * Dovresti aver ricevuto una copia della GNU General Public License — You should have received a copy of the GNU General Public License
 * insieme a questo programma.  Se non lo è, vedi <https://www.gnu.org/licenses/>. — along with this program.  If not, see <https://www.gnu.org/licenses/>.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
/*
 * tms1500.c — Emulatore CPU TMS0500 (TI-58/59)
 * Keycodes rimappati per corrispondere alla ROM reale dell'hardware TI-59 (00-99) — Keycodes remapped to match real TI-59 hardware ROM (00-99)
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
#include <stdlib.h>   // per atof, malloc, free — for atof, malloc, free
#include <stdint.h>   // per uint8_t, uint16_t, int32_t (sicurezza extra) — for uint8_t, uint16_t, int32_t (extra safety)
#include <SPIFFS.h>
#include <FS.h>

// ═══════════════════════════════════════════════════════════
// KEYCODE TI-59 — REAL HARDWARE CODES (00-99)
// Basato sul dump della ROM TI-58/59 e sul manuale hardware — Based on TI-58/59 ROM dump and hardware manual
// ═══════════════════════════════════════════════════════════

// Row 0 — User Definable Keys (A–E, A'–E')
#define KC_E_PRIME  10   // E′ (2nd E) — codice 10 sul TI-59 reale — code 10 on real TI-59
#define KC_A        11   // A — A
#define KC_B        12   // B — B
#define KC_C        13   // C — C
#define KC_D        14   // D — D
#define KC_E        15   // E — E
#define KC_A_PRIME  16   // A′ (2nd A) — A′ (2nd A)
#define KC_B_PRIME  17   // B′ (2nd B) — B′ (2nd B)
#define KC_C_PRIME  18   // C′ (2nd C) — C′ (2nd C)
#define KC_D_PRIME  19   // D′ (2nd D) — D′ (2nd D)

// Row 1 — Modifier / Clear Keys
#define KC_CLR_2ND  20   // Clear (2nd CLR) — codice singolo 20 — single code 20
#define KC_2ND      21   // 2nd — 2nd
#define KC_INV      22   // INV — INV
#define KC_LNX      23   // LNx — LNx
#define KC_CE       24   // CE — CE
#define KC_CLR      25   // CLR — CLR
#define KC_2ND_2ND  26   // 2nd 2nd (codice 26) — 2nd 2nd (code 26)
#define KC_2ND_INV  27   // 2nd INV (codice 27) — 2nd INV (code 27)
#define KC_LOG      28   // log (2nd LNx) — log (2nd LNx)
#define KC_CP       29   // CP (2nd CE) — CP (2nd CE)
#define KC_TAN      30   // tan (2nd 1/x) — tan (2nd 1/x)

// Row 2 — LRN / Trig Primitives
#define KC_LRN      31   // LRN — LRN
#define KC_XET      32   // x↔t — x↔t
#define KC_X2       33   // x² — x²
#define KC_SQRT     34   // √x — √x
#define KC_INV_X    35   // 1/x — 1/x
#define KC_PGM      36   // PGM (2nd LRN) — PGM (2nd LRN)
#define KC_P_R      37   // P→R (2nd x↔t) — P→R (2nd x↔t)
#define KC_SIN      38   // sin (2nd x²) — sin (2nd x²)
#define KC_COS      39   // cos (2nd √x) — cos (2nd √x)
#define KC_IND      40   // IND (2nd yˣ) — IND (2nd yˣ)

// Row 3 — Memory / Data Manipulation
#define KC_SST      41   // SST — SST
#define KC_STO      42   // STO — STO
#define KC_RCL      43   // RCL — RCL
#define KC_SUM      44   // SUM — SUM
#define KC_YX       45   // yˣ — yˣ
#define KC_INS      46   // Ins (2nd SST) — Ins (2nd SST)
#define KC_CMS      47   // CMs (2nd STO) — CMs (2nd STO)
#define KC_EXC      48   // EXC (2nd RCL) — EXC (2nd RCL)
#define KC_PROD     49   // Prod (2nd SUM) — Prod (2nd SUM)
#define KC_ABS      50   // |x| (2nd ÷) — |x| (2nd ÷)

// Row 4 — Navigation / Parens / Divide
#define KC_BST      51   // BST — BST
#define KC_EE       52   // EE — EE
#define KC_LPAR     53   // (
#define KC_RPAR     54   // )
#define KC_DIV      55   // ÷ — ÷
#define KC_DEL      56   // Del (2nd BST) — Del (2nd BST)
#define KC_ENG      57   // ENG (2nd EE) — ENG (2nd EE)
#define KC_FIX      58   // Fix (2nd () — Fix (2nd ()
#define KC_INT      59   // Int (2nd )) — Int (2nd ))
#define KC_DEG      60   // Deg (2nd ×) — Deg (2nd ×)

// Row 5 — GTO / Digits 7–9 / Multiply
#define KC_GTO      61   // GTO — GTO
// 07 = 7, 08 = 8, 09 = 9 (le cifre mappano su sé stesse) — 07 = 7, 08 = 8, 09 = 9 (digits map to themselves)
#define KC_MUL      65   // × — ×
#define KC_PAUSE    66   // Pause (2nd GTO) — Pause (2nd GTO)
#define KC_XEQ_T    67   // x=t (2nd 7) — x=t (2nd 7)
#define KC_NOP      68   // Nop (2nd 8) — Nop (2nd 8)
#define KC_OP       69   // Op (2nd 9) — Op (2nd 9)
#define KC_RAD      70   // Rad (2nd −) — Rad (2nd −)

// Row 6 — SBR / Digits 4–6 / Subtract
#define KC_SBR      71   // SBR — SBR
// 04 = 4, 05 = 5, 06 = 6 (le cifre mappano su sé stesse) — 04 = 4, 05 = 5, 06 = 6 (digits map to themselves)
#define KC_SUB      75   // − — −
#define KC_LBL      76   // Lbl (2nd SBR) — Lbl (2nd SBR)
#define KC_XGE_T    77   // x≥t (2nd 4) — x≥t (2nd 4)
#define KC_SIGP     78   // Σ+ (2nd 5) — Σ+ (2nd 5)
#define KC_XBAR     79   // x̄ (2nd 6) — x̄ (2nd 6)
#define KC_GRAD     80   // Grad (2nd +) — Grad (2nd +)

// Row 7 — RST / Digits 1–3 / Add
#define KC_RST      81   // RST — RST
// 01 = 1, 02 = 2, 03 = 3 (le cifre mappano su sé stesse) — 01 = 1, 02 = 2, 03 = 3 (digits map to themselves)
#define KC_ADD      85   // + — +
#define KC_STFL     86   // St Flg (2nd RST) — St Flg (2nd RST)
#define KC_IFFL     87   // If Flg (2nd 1) — If Flg (2nd 1)
#define KC_DMS      88   // D.MS (2nd 2) — D.MS (2nd 2)
#define KC_PI       89   // π (2nd 3) — π (2nd 3)
#define KC_LIST     90   // List (2nd =) — List (2nd =)

// Row 8 — R/S / Digit 0 / Dot / Sign / Equals
#define KC_RS       91   // R/S — R/S
// 00 = 0
#define KC_DOT      93   // .
#define KC_PM       94   // +/−
#define KC_EQ       95   // =
#define KC_WRITE    96   // Write (2nd R/S, solo TI-59) — Write (2nd R/S, TI-59 only)
#define KC_DSZ      97   // DSZ (2nd 0) — DSZ (2nd 0)
#define KC_ADV      98   // Adv (2nd .) — Adv (2nd .)
#define KC_PRT      99   // Prt (2nd +/−) — Prt (2nd +/−)

// Codici speciali / indiretti (non tasti diretti ma validi nei programmi) — Special / Indirect codes (not direct keys but valid in programs)
#define KC_PGM_IND  62   // Pgm Ind (2nd PGM 2nd IND) — Pgm Ind (2nd PGM 2nd IND)
#define KC_EXC_IND  63   // EXC Ind (2nd EXC 2nd IND) — EXC Ind (2nd EXC 2nd IND)
#define KC_PROD_IND 64   // Prod Ind (2nd Prod 2nd IND) — Prod Ind (2nd Prod 2nd IND)
#define KC_STO_IND  72   // STO Ind (STO 2nd IND) — STO Ind (STO 2nd IND)
#define KC_RCL_IND  73   // RCL Ind (RCL 2nd IND) — RCL Ind (RCL 2nd IND)
#define KC_SUM_IND  74   // SUM Ind (SUM 2nd IND) — SUM Ind (SUM 2nd IND)
#define KC_GTO_IND  83   // GTO Ind — GTO Ind
#define KC_OP_IND   84   // Op Ind — Op Ind
#define KC_RETURN   92   // INV SBR = Return (singolo passo!) — INV SBR = Return (single step!)

#define KC_NONE     0xFF // Nessun tasto — No key

// ── Codici operazioni aritmetiche in attesa (cpu->pending_op) ─────────────── — ── Pending arithmetic operation codes (cpu->pending_op) ───────────────
// Consumati da exec_pending(), hir_pop() e apply_pending_op(). — Consumed by exec_pending(), hir_pop(), and apply_pending_op().
#define PENDING_OP_NONE  0   // Nessun operatore aritmetico in attesa — No pending arithmetic operator
#define PENDING_OP_ADD   1   // + — +
#define PENDING_OP_SUB   2   // − — −
#define PENDING_OP_MUL   3   // × — ×
#define PENDING_OP_DIV   4   // ÷ — ÷
#define PENDING_OP_YX    5   // yˣ — yˣ

// ── Codici azione registro in attesa (cpu->pending_reg) ─────────────────── — ── Pending register action codes (cpu->pending_reg) ───────────────────
// Impostati dagli handler di tasto STO/RCL/GTO/SBR/SUM/StFlg/IfFlg/EXC/Prod; — Set by STO/RCL/GTO/SBR/SUM/StFlg/IfFlg/EXC/Prod key handlers;
// consumati dal blocco di accumulo cifre in process_keycode(). — consumed by the digit-accumulation block in process_keycode().
#define PENDING_REG_NONE  0xFF  // Nessuna azione registro in attesa — No pending register action
#define PENDING_REG_STO   1     // Memorizza A → registro — Store A → register
#define PENDING_REG_RCL   2     // Richiama registro → A — Recall register → A
#define PENDING_REG_GTO   3     // Vai a un indirizzo o etichetta — Go to address or label
#define PENDING_REG_SBR   4     // Chiamata subroutine (inserisce l'indirizzo di ritorno) — Subroutine call (pushes return address)
#define PENDING_REG_SUM   5     // Somma A al registro — Add A to register
#define PENDING_REG_STFL  6     // Memorizza flag — Store flag
#define PENDING_REG_IFFL  7     // Testa flag — Test flag
#define PENDING_REG_EXC   8     // Scambia A con il registro — Exchange A with register
#define PENDING_REG_PROD  9     // Moltiplica A nel registro — Multiply A into register

// ═══════════════════════════════════════════════════════════
// TABELLA MNEMONICI (MAPPATURA CODICE -> STRINGA) — MNEMONIC TABLE (CODE -> STRING MAPPING)
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

// Questa funzione può vedere la tabella static perché è nello stesso file — This function can see the static table because it is in the same file
const char* get_mnemonic_name(uint8_t code) {
    if (code > 99) return "???";
    return KEYCODE_MNEMONICS[code];
}

// ═══════════════════════════════════════════════════════════
// VARIABILI STATICHE — STATIC VARIABLES
// ═══════════════════════════════════════════════════════════

static uint16_t custom_label_pc[10] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
static bool pending_indirect = false;
static int  pending_value = 0;
static bool user_flags[10] = {false};
static bool inv_pending = false;   // Prefisso INV in attesa in modalità LRN — INV prefix pending in LRN mode
static bool fix_pending = false;
static bool op_pending = false;
static int  op_code = 0;
static int  op_digits = 0;

// ─── Pgm (modulo libreria): raccolta delle 2 cifre successive ── — ─── Pgm (library module): collect the next 2 digits ──
// (il numero di programma da designare, es. "2nd Pgm 01" per ML-01). — (the program number to designate, e.g. "2nd Pgm 01" for ML-01).
// Stesso pattern a fasi già usato per DSZ. — Same phased pattern already used for DSZ.
static bool lib_page_pending = false;
static int  lib_page_digits  = 0;
static int  lib_page_val     = 0;

// ─── Modulo libreria: due modalità reali, non una ────────── — ─── Library module: two real modes, not one ──────────
// Da manuale TI-59 + chiarimento diretto: "2nd Pgm mm" DESIGNA quale — From TI-59 manual + direct clarification: "2nd Pgm mm" DESIGNATES which
// programma, senza fare altro. Cosa succede dopo dipende dal tasto — program, without doing anything else. What happens next depends on the key
// successivo: — pressed next:
//   • "2nd Op 09"      → SCARICA il programma in memoria principale — • "2nd Op 09"      → DOWNLOADS the program into main memory
//                         (passo 000, sovrascrive) per poterlo — (step 000, overwrites) so it can be
//                         modificare — memoria/etichette dell'utente, — modified — user memory/labels,
//                         nessuna area separata (v. exec_op case 9). — no separate area (see exec_op case 9).
//   • A..E / A'..E'     → esegue il programma COSÌ COM'È, direttamente — • A..E / A'..E'     → runs the program AS-IS, directly
//                         dalla ROM del modulo, SENZA toccare la — from the module ROM, WITHOUT touching the
//                         memoria LRN dell'utente né i suoi registri — user LRN memory nor its registers
//                         dati — usa un proprio banco STO/RCL — data — uses its own STO/RCL bank
//                         separato ("ecosistema della ROM"). — separate ("ROM ecosystem").
static bool     lib_page_selected = false;  // "Pgm mm" appena designato, in attesa di Op09 o di un'etichetta — "Pgm mm" just designated, waiting for Op09 or a label
static uint8_t  lib_selected_page = 0;
static uint16_t lib_scope_addr    = 0;      // indirizzo di partenza del programma designato — starting address of the designated program
static uint16_t lib_scope_len     = 0;      // sua lunghezza (per dare priorità alle SUE etichette) — its length (to give priority to ITS labels)

// Scope "in attesa": impostato da PGM (sia da tastiera sia incontrato — "Pending" scope: set by PGM (both from the keyboard and when met
// dentro un programma) quando designa un nuovo programma, ma NON ancora — inside a program) when it designates a new program, but NOT yet
// commesso a lib_scope_addr/lib_scope_len — quel commit avviene solo — committed to lib_scope_addr/lib_scope_len — that commit only happens
// quando la SBR/GTO/etichetta che segue lo attiva davvero. Necessario — when the following SBR/GTO/label actually activates it. Needed
// per le chiamate annidate ("PGM mm SBR label" dentro un programma di — for nested calls ("PGM mm SBR label" inside a program of
// libreria già in esecuzione): se PGM scrivesse subito in — library already running): if PGM wrote immediately into
// lib_scope_addr/lib_scope_len, distruggerebbe lo scope del chiamante — lib_scope_addr/lib_scope_len, it would destroy the caller's scope
// prima che la SBR possa salvarlo per il ritorno. — before the SBR can save it for the return.
static uint16_t lib_pending_addr = 0;
static uint16_t lib_pending_len  = 0;
static uint8_t  lib_pending_page = 0;

static bool     showing_lib_prog  = false;  // esecuzione "as-is" dalla ROM attiva — "as-is" execution from the module ROM active
static uint16_t lib_custom_label_pc[10] = {0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF};
static BCD_Reg  lib_ram[RAM_SIZE];          // registri dati del modulo, separati da cpu->ram[] — module data registers, separate from cpu->ram[]

// Converte un keycode etichetta (A-E / A'-E') nell'indice 0-9. — Converts a label keycode (A-E / A'-E') into the index 0-9.
static int label_index_for_key(uint8_t kc) {
    if (kc >= KC_A && kc <= KC_E) return kc - KC_A;
    if (kc >= KC_A_PRIME && kc <= KC_D_PRIME) return kc - KC_A_PRIME + 5;
    if (kc == KC_E_PRIME) return 9;
    return -1;
}

// Lunghezza del "programma" corrente ai fini del wraparound di prog_pc: — Length of the current "program" for prog_pc wraparound:
// in esecuzione "as-is" da modulo è la lunghezza LOCALE del solo — in "as-is" module execution it is the LOCAL length of only the
// programma designato (lib_scope_len, es. 189 per ML-01) — NON la — designated program (lib_scope_len, e.g. 189 for ML-01) — NOT the
// dimensione dell'intera ROM da 5000 step. cpu->prog_pc resta comunque — size of the whole 5000-step ROM. cpu->prog_pc remains anyway
// un indice ASSOLUTO nella ROM in quel caso: chi usa questa lunghezza — an ABSOLUTE index into the ROM in that case: whoever uses this length
// per calcolare un nuovo prog_pc deve ricollocare col relativo — to compute a new prog_pc must relocate with the relative
// lib_scope_addr (vedi advance_pc_wrap()/relocate_target() sotto). — lib_scope_addr (see advance_pc_wrap()/relocate_target() below).
// Altrimenti, in esecuzione utente normale, è cpu->prog_len come sempre. — Otherwise, in normal user execution, it is cpu->prog_len as always.
static inline uint16_t exec_prog_len(const TMS1500_State *cpu) {
    if (showing_lib_prog) {
        return lib_scope_len;
    }
    return cpu->prog_len;
}

// Avanza cpu->prog_pc di N byte con wraparound, restando nello spazio — Advances cpu->prog_pc by N bytes with wraparound, staying in the right
// giusto: in esecuzione "as-is" da modulo, prog_pc è un indice ASSOLUTO — space: in "as-is" module execution, prog_pc is an ABSOLUTE index
// nella ROM ma il wrap deve avvenire sulla lunghezza LOCALE del solo — into the ROM but the wrap must happen over the LOCAL length of only
// programma designato (lib_scope_len), poi va ricollocato sommando — the designated program (lib_scope_len), then it is relocated by adding
// lib_scope_addr. — lib_scope_addr.
static inline uint16_t advance_pc_by(TMS1500_State *cpu, uint16_t nbytes) {
    uint16_t len = exec_prog_len(cpu);
    if (len == 0) return cpu->prog_pc;
    if (showing_lib_prog) {
        // Fai il wrap solo degli indirizzi nell'intervallo lib_scope corrente. — Only wrap addresses within the current lib_scope range.
        // Indirizzi FUORI da lib_scope (es. dopo una chiamata/ritorno PGM, o — Addresses OUTSIDE lib_scope (e.g. after PGM call/return, or
        // SBR/GTO verso una subroutine condivisa in un'altra parte della ROM) — SBR/GTO to a shared subroutine in another part of the ROM)
        // devono avanzare senza wrap — altrimenti pc verrebbe rimappato — must advance without wrapping — otherwise pc gets remapped
        // nel programma sbagliato. — into the wrong program.
        if (cpu->prog_pc >= lib_scope_addr) {
            uint16_t local = (uint16_t)(cpu->prog_pc - lib_scope_addr);
            if (local < len)
                return (uint16_t)(lib_scope_addr + ((local + nbytes) % len));
        }
        return (uint16_t)(cpu->prog_pc + nbytes);
    }
    return (uint16_t)((cpu->prog_pc + nbytes) % len);
}

// Compatibilità: avanza di un solo byte (usata da SST, che gestisce — Compatibility: advances by a single byte (used by SST, which handles
// l'istruzione tramite process_keycode() a parte). — the instruction via process_keycode() separately).
static inline uint16_t advance_pc_wrap(TMS1500_State *cpu) {
    return advance_pc_by(cpu, 1);
}

uint8_t prog_read_step(TMS1500_State *cpu, uint16_t addr); // dichiarazione in avanti — fwd decl
static void format_value_string(const TMS1500_State *cpu, char *buf, unsigned int len); // dichiarazione in avanti — fwd decl

// Lunghezza in byte dell'istruzione che inizia a 'addr', secondo le — Length in bytes of the instruction starting at 'addr', per the
// stesse regole di decodifica di exec_program_step()/read_2digit()/ — same decoding rules as exec_program_step()/read_2digit()/
// read_3digit()/read_label() — serve per "saltare la prossima — read_3digit()/read_label() — used to "skip the next
// istruzione" (x≥t, IF flag) senza disallinearsi quando quell'istruzione — instruction" (x≥t, IF flag) without misaligning when that instruction
// occupa più di un byte (es. una GTO/STO con operando). Un +1 fisso qui — occupies more than one byte (e.g. a GTO/STO with operand). A fixed +1 here
// era il bug: dopo lo skip il decoder ripartiva a metà di un'istruzione — was the bug: after the skip the decoder restarted in the middle of an instruction
// multi-byte, leggendo byte a caso come se fossero un nuovo opcode. — multi-byte, reading random bytes as if they were a new opcode.
static uint16_t instruction_byte_length(TMS1500_State *cpu, uint16_t addr) {
    uint16_t prog_end = showing_lib_prog ? (uint16_t)(lib_scope_addr + lib_scope_len)
                                          : cpu->prog_len;
    if (addr >= prog_end) return 1;
    uint8_t op = prog_read_step(cpu, addr);
    uint16_t f2 = showing_lib_prog ? 1 : 2;   // read_2digit: 1 byte in ROM libreria, 2 da tastiera — read_2digit: 1 byte in library ROM, 2 from keyboard
    uint16_t f3 = showing_lib_prog ? 2 : 3;   // read_3digit: 2 byte in ROM libreria, 3 da tastiera — read_3digit: 2 bytes in library ROM, 3 from keyboard

    if (op == KC_LBL) return 2;               // opcode + 1 byte etichetta — opcode + 1 label byte
    if (op == KC_INV) {                       // prefisso: 1 + lunghezza dell'istruzione che segue — prefix: 1 + length of the following instruction
        if (addr + 1 >= prog_end) return 1;
        return (uint16_t)(1 + instruction_byte_length(cpu, (uint16_t)(addr + 1)));
    }
    if (op == KC_DSZ) {
        // In libreria ROM la codifica dopo il registro è sempre indirizzo — In the library ROM the encoding after the register is always address
        // (mai etichetta come keycode singolo, perchè i registri ≥ 10 sono — (never a label as a single keycode, because registers ≥ 10 are
        // valori validi e > 9, indistinguibili da un'etichetta col test >9). — valid values and > 9, indistinguishable from a label with the >9 test).
        if (showing_lib_prog)
            return (uint16_t)(1 + f2 + f3);    // DSZ reg, indirizzo letterale — DSZ reg, literal address
        uint16_t after_reg = (uint16_t)(addr + 1 + f2);
        if (after_reg < prog_end && prog_read_step(cpu, after_reg) > 9)
            return (uint16_t)(1 + f2 + 1);     // DSZ reg, LBL — DSZ reg, LBL
        return (uint16_t)(1 + f2 + f3);        // DSZ reg, indirizzo letterale — DSZ reg, literal address
    }
    if (op == KC_SBR || op == KC_GTO || op == KC_XEQ_T || op == KC_XGE_T) {
        uint16_t after_op = (uint16_t)(addr + 1);
        if (after_op < prog_end && prog_read_step(cpu, after_op) > 9)
            return 2;                          // opcode + LBL — opcode + LBL
        return (uint16_t)(1 + f3);             // opcode + indirizzo letterale — opcode + literal address
    }
    if (op == KC_STO || op == KC_RCL || op == KC_SUM || op == KC_EXC || op == KC_PROD ||
        op == KC_STO_IND || op == KC_RCL_IND || op == KC_SUM_IND || op == KC_EXC_IND ||
        op == KC_PROD_IND || op == KC_GTO_IND || op == KC_OP) {
        return (uint16_t)(1 + f2);             // opcode + registro/parametro a 2 cifre — opcode + 2-digit register/parameter
    }
    if (op == KC_FIX || op == KC_STFL) return 2;   // opcode + 1 cifra — opcode + 1 digit
    if (op == KC_PGM || op == KC_PGM_IND) return (uint16_t)(1 + f2);  // Pgm + 2 cifre — Pgm + 2 digits
    if (op == KC_IFFL) {
        // IFFL flagnum + etichetta (1 byte) o + indirizzo (f3 byte) — IFFL flagnum + label (1 byte) or + address (f3 bytes)
        uint16_t after_flag = (uint16_t)(addr + 2);
        if (after_flag < prog_end && prog_read_step(cpu, after_flag) > 9)
            return 3;                          // opcode + flag + LBL — opcode + flag + LBL
        return (uint16_t)(2 + f3);             // opcode + flag + indirizzo — opcode + flag + address
    }
    return 1;                                  // default: opcode a singolo byte — default: single-byte opcode
}

// Ricolloca un indirizzo di destinazione "grezzo" (es. il valore letto da — Relocates a "raw" target address (e.g. the value read from
// un registro per GTO/SBR indiretto) nello spazio di esecuzione corrente: — a register for indirect GTO/SBR) into the current execution space:
// locale al programma designato + lib_scope_addr in modalità libreria, — local to the designated program + lib_scope_addr in library mode,
// assoluto altrimenti. Stesso principio di advance_pc_wrap() ma per un — absolute otherwise. Same principle as advance_pc_wrap() but for an
// indirizzo arbitrario invece che "prog_pc + 1". — arbitrary address instead of "prog_pc + 1".
static inline uint16_t relocate_target(TMS1500_State *cpu, uint16_t raw_local) {
    uint16_t len = exec_prog_len(cpu);
    if (len == 0) return raw_local;
    uint16_t wrapped = raw_local % len;
    return showing_lib_prog ? (uint16_t)(lib_scope_addr + wrapped) : wrapped;
}

// Banco registri dati attivo: quello del modulo se in esecuzione — Active data-register bank: the module's one if running
// "as-is", altrimenti quello dell'utente come sempre. Così STO/RCL/SUM — "as-is", otherwise the user's as always. Thus STO/RCL/SUM
// e DSZ durante un programma da modulo non toccano mai cpu->ram[]. — and DSZ during a module program never touch cpu->ram[].
static inline BCD_Reg* active_ram_bank(TMS1500_State *cpu) {
    return showing_lib_prog ? lib_ram : cpu->ram;
}

// Ricostruisce la tabella etichette per l'esecuzione "as-is": prima — Rebuilds the label table for "as-is" execution: first
// scandisce SOLO l'intervallo del programma designato (così "LBL A" — it scans ONLY the designated program's range (so "LBL A"
// di QUESTO programma ha la precedenza — più programmi nello stesso — of THIS program takes precedence — several programs in the same
// modulo possono definire ciascuno una propria "LBL A"), poi riempie — module can each define their own "LBL A"), then it fills
// gli indici ancora mancanti scandendo il resto della ROM (per le — the still-missing indexes by scanning the rest of the ROM (for the
// subroutine condivise richiamate per etichetta da fuori del proprio — shared subroutines called by label from outside its own
// intervallo). — range).
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
// Collega questo modulo al cpu attivo, così library_on_module_changed() — Binds this module to the active CPU, so library_on_module_changed()
// (sotto) può intervenire sull'esecuzione in corso quando l'utente — (below) can act on the running execution when the user
// cambia modulo da web. Va chiamata una volta sola, subito dopo che — changes the module from the web. Must be called once, right after
// g_cpu viene creato/assegnato (g_cpu vive in wifilink.cpp — vedi — g_cpu is created/assigned (g_cpu lives in wifilink.cpp — see
// "TMS1500_State *g_cpu = ..."). — "TMS1500_State *g_cpu = ...").
//   IMPORTANTE: aggiungere in tms1500.h la dichiarazione — IMPORTANT: add to tms1500.h the declaration
//     void tms1500_bind_cpu(TMS1500_State *cpu); — void tms1500_bind_cpu(TMS1500_State *cpu);
//   e chiamare tms1500_bind_cpu(g_cpu); in wifilink.cpp subito dopo — and call tms1500_bind_cpu(g_cpu); in wifilink.cpp right after
//   l'assegnazione di g_cpu. — the g_cpu assignment.
// ═══════════════════════════════════════════════════════════════
static TMS1500_State *g_bound_cpu = nullptr;
void tms1500_bind_cpu(TMS1500_State *cpu) { g_bound_cpu = cpu; }

// ═══════════════════════════════════════════════════════════════
// Hook chiamato da library_module.cpp ogni volta che l'utente — Hook called by library_module.cpp every time the user
// cambia modulo attivo (endpoint /api/modules in wifilink.cpp). — changes the active module (/api/modules endpoint in wifilink.cpp).
// Oltre a ripulire lo stato di scope/etichette (già fatto sopra), — Besides clearing the scope/label state (already done above),
// simula l'estrazione FISICA del modulo: se in questo momento — simulates the PHYSICAL removal of the module: if at this moment
// l'esecuzione dipende davvero dalla ROM che sta per sparire — o — execution really depends on the ROM that is about to disappear — or
// perché ci si trova dentro di essa (showing_lib_prog era true), o — because we are inside it (showing_lib_prog was true), or
// perché lo stack di ritorno ha almeno una chiamata pendente verso — because the return stack has at least one pending call towards
// di essa (stack_in_rom[i]==true per qualche frame) — il programma — it (stack_in_rom[i]==true for some frame) — the program
// viene interrotto sul colpo, esattamente come farebbe la calcolatrice — is stopped on the spot, exactly as the calculator would do
// reale se le si strappasse via il modulo dallo slot a metà — real one if the module were yanked from the slot in the middle
// esecuzione: RUN si ferma, il flag errore si accende, l'eventuale — of execution: RUN stops, the error flag lights up, any
// operazione/cifra a metà inserimento viene scartata, e lo stack di — operation/digit mid-entry is discarded, and the return stack is
// ritorno viene svuotato (non si può "srotolare" in modo pulito un — emptied (you cannot cleanly "unwind" a
// mix di frame utente/ROM quando la ROM stessa non c'è più). — mix of user/ROM frames when the ROM itself is gone).
// Se invece il programma in corso è interamente in memoria utente e — If instead the running program is entirely in user memory and
// non ha alcuna chiamata pendente verso la ROM, NON viene toccato: — has no pending call towards the ROM, it is NOT touched:
// esattamente come sull'hardware reale, il modulo può essere cambiato — exactly like on real hardware, the module can be changed
// senza conseguenze finché non viene davvero richiamato. — without consequences until it is really called.
// ═══════════════════════════════════════════════════════════════
// NOTA: NON si chiama "library_on_module_changed" — quel nome è — NOTE: it is NOT called "library_on_module_changed" — that name is
// l'hook debole dichiarato in library_module.h, e wifilink.cpp ne — the weak hook declared in library_module.h, and wifilink.cpp
// fornisce già la propria implementazione forte (persistenza su NVS — already provides its own strong implementation (NVS persistence
// dell'ultimo modulo innestato). Due implementazioni forti dello — of the last inserted module). Two strong implementations of the
// stesso simbolo darebbero "multiple definition" in fase di link. — same symbol would give "multiple definition" at link time.
// Questa funzione va quindi richiamata ESPLICITAMENTE da dentro — This function must therefore be called EXPLICITLY from inside
// l'implementazione di wifilink.cpp, non sovrascrive nulla da sola. — the wifilink.cpp implementation, it does not overwrite anything by itself.
//   Aggiungere in wifilink.cpp, dentro il suo library_on_module_changed — Add in wifilink.cpp, inside its library_on_module_changed
//   già esistente:  tms1500_on_library_module_changed(id); — already existing: tms1500_on_library_module_changed(id);
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
        g_bound_cpu->sp = 0;          // niente ritorni puliti verso una ROM sparita — no clean returns towards a vanished ROM
        g_bound_cpu->prog_pc = 0;     // scarta un indirizzo assoluto ora privo di senso — discard an absolute address now meaningless
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
// Tracker di annidamento parentesi: ogni '(' registra la profondità HIR così ')' può ripristinare senza — Paren nesting tracker: each '(' records HIR depth so ')' can restore without
// eseguire l'operazione pendente esterna. Deve essere >= HIR_STACK_SIZE per la sicurezza dell'annidamento. — executing the outer pending op. Must be >= HIR_STACK_SIZE for nesting safety.
static int paren_depth = 0;
static int hir_paren_base[HIR_STACK_SIZE];

// DSZ (Decrement and Skip if Zero) — sulla TI-59 reale è un'istruzione a
// DUE operandi: "Dsz nn LLL" decrementa il registro dati nn e, se il — TWO operands: "Dsz nn LLL" decrements data register nn and, if the
// risultato NON è zero, salta all'indirizzo/etichetta LLL; se il risultato — result is NOT zero, jumps to address/label LLL; if the result
// è zero, l'esecuzione prosegue in sequenza (fine ciclo). Servono quindi — is zero, execution continues in sequence (end of loop). We therefore need
// due fasi di raccolta cifre, esattamente come per GTO/SBR. — two digit-collection phases, exactly like for GTO/SBR.
static int dsz_phase       = 0;   // 0=inattivo, 1=raccolta registro, 2=raccolta indirizzo — 0=inactive, 1=register collection, 2=address collection
static int dsz_reg_val     = 0;
static int dsz_reg_digits  = 0;
static int dsz_addr_val    = 0;
static int dsz_addr_digits = 0;

// ─── Indicatore "occupato" (funzioni lente: sqrt, trig, log, y^x...) — Busy indicator (slow functions: sqrt, trig, log, y^x...) ──
// Sulla TMS1500 reale, durante il calcolo di queste funzioni lo — On the real TMS1500, during the calculation of these functions the
// schermo si svuotava e mostrava solo "C" nell'ultima posizione a — screen emptied and showed only "C" in the last position at
// sinistra finché il risultato non era pronto — l'hardware era — left until the result was ready — the hardware was
// letteralmente occupato per centinaia di millisecondi/qualche — literally busy for hundreds of milliseconds/a few
// secondo. Due modalità: — seconds. Two modes:
//   - "reale": blocca per la durata autentica (approssimata), proprio — "real": blocks for the authentic (approximate) duration, just
//     come l'originale — il "C" resta visibile per tutto quel tempo. — like the original — the "C" stays visible for that whole time.
//   - "moderna" (default): il calcolo sottostante resta istantaneo — "modern" (default): the underlying calculation stays instantaneous
//     (nessun blocco di tastiera/WiFi), ma "C" resta comunque visibile — (no keyboard/WiFi blocking), but "C" still stays visible
//     per una durata minima fissa, solo per il feeling tattile. — for a fixed minimum duration, just for the tactile feel.
//
// NOTA SULLE DURATE: valori approssimativi presi dalla memoria — NOTE ON DURATIONS: approximate values taken from the memory
// collettiva della comunità di appassionati TI-58/59, NON misurati su — collective memory of the TI-58/59 enthusiast community, NOT measured on
// una fonte primaria in questa sessione. Se hai ancora una macchina — a primary source in this session. If you still have a machine
// reale funzionante, cronometrala con un cellulare e aggiorna pure — working real one, time it with a phone and feel free to update
// queste costanti di conseguenza. — these constants accordingly.
#define BUSY_MS_SQRT        150   // radice quadrata — square root
#define BUSY_MS_TRIG        700   // sin/cos/tan — sin/cos/tan
#define BUSY_MS_ATRIG      1000   // asin/acos/atan (INV sin/cos/tan) — asin/acos/atan (INV sin/cos/tan)
#define BUSY_MS_LOG         500   // ln/log10 — ln/log10
#define BUSY_MS_EXP         600   // e^x/10^x — e^x/10^x
#define BUSY_MS_YX         1000   // y^x — y^x
#define BUSY_MODERN_MIN_MS  650   // durata minima "C" in modalità moderna — minimum "C" duration in modern mode
                                   // (>500ms: il polling della pagina web — (>500ms: the web page polling
                                   // interroga lo stato ogni 500ms — sotto — polls the state every 500ms — below
                                   // quella soglia la finestra "occupato" — below that threshold the "busy" window
                                   // rischia di cadere sempre tra due poll — risks always falling between two polls
                                   // e non venire mai vista dall'interfaccia) — and never being seen by the interface)

static bool          g_realistic_timing = false;   // default: modalità moderna — default: modern mode
static float         g_timing_multiplier = 1.0f;   // 1.0 = timing autentico — 1.0 = authentic timing
static volatile bool busy_active   = false;
static unsigned long busy_until_ms = 0;

// ─── Pacing a tempo reale dell'esecuzione programmi — Real-time pacing of program execution ──────────
// Il TI-59 originale esegue ~TI59_INSTR_PER_SEC passi/s (v. config.h). — The original TI-59 runs ~TI59_INSTR_PER_SEC steps/s (see config.h).
// In modalità Old (g_realistic_timing) l'esecuzione dei programmi viene — In Old mode (g_realistic_timing) program execution is
// rallentata a tempo reale per un feel autentico: si accumulano le — slowed down to real time for an authentic feel: we accumulate the
// istruzioni "dovute" in base al tempo reale trascorso e il rate target è — "due" instructions based on elapsed real time and the target rate is
// originale × g_timing_multiplier (100% = stesso ritmo del hardware reale). — original × g_timing_multiplier (100% = same pace as real hardware).
// Così la velocità NON dipende dal carico del device. g_step_rate misura — Thus speed does NOT depend on device load. g_step_rate measures
// il loop reale (passi/s) = soffitto pratico: se < target, l'emulatore — the real loop (steps/s) = practical ceiling: if < target, the emulator
// gira comunque al massimo che può (e /api/sysinfo lo riporta). — still runs as fast as it can (and /api/sysinfo reports it).
static unsigned long g_inst_acc      = 0;   // istruzioni "in arretrato" — "backlogged" instructions
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

// PAUSE (66, 2nd GTO) non bloccante: la vecchia vTaskDelay(500) congelava — non-blocking PAUSE (66, 2nd GTO): the old vTaskDelay(500) froze
// il task che esegue l'emulazione (e che serve anche display/web), quindi — the task running the emulation (which also serves display/web), so
// durante la pausa non si poteva nemmeno fermare il programma con R/S. — during the pause you couldn't even stop the program with R/S.
// Ora si imposta solo un timestamp e tms1500_step trattiene l'esecuzione; — Now only a timestamp is set and tms1500_step holds back execution;
// tastiera, display e web restano reattivi durante i 500ms. — keyboard, display and web stay responsive during the 500ms.
static unsigned long pause_until_ms = 0;

// ─── Tracer passo-passo (debug) — Step-by-step tracer (debug) ──────────────────────────────────
// Se attivo, ogni istruzione eseguita da exec_program_step() viene — When enabled, every instruction executed by exec_program_step() is
// stampata su Serial come "[STEP] pc=.... (local=....) op=NN (mnemonico)" — printed on Serial as "[STEP] pc=.... (local=....) op=NN (mnemonic)"
// prima di essere decodificata/eseguita — utile per seguire dal vivo — before being decoded/executed — useful to follow live
// dove va un programma (anche "as-is" da modulo libreria) senza dover — where a program goes (also "as-is" from a library module) without having to
// aggiungere printf sparsi ogni volta. Di default spento: su un — add scattered printf's every time. Off by default: on a
// programma che gira per migliaia di cicli stamperebbe moltissimo e — program running for thousands of cycles it would print a lot and
// rallenterebbe l'esecuzione; va acceso solo mentre si sta — slow down execution; it should only be turned on while
// diagnosticando un problema. — diagnosing a problem.
static bool g_trace_steps = false;
void tms1500_set_trace_steps(bool enable) { g_trace_steps = enable; }
bool tms1500_get_trace_steps(void) { return g_trace_steps; }

// ─── Indicatore Old/New (programma modificato dall'ultimo salvataggio) — Old/New indicator (program modified since last save) ──
// "New" = ci sono modifiche non salvate su scheda dall'ultima — "New" = there are unsaved changes on card since the last
// operazione di caricamento/salvataggio; "Old" = combacia con quanto — load/save operation; "Old" = matches what
// già persistito. Diventa "New" ad ogni scrittura nel buffer — is already persisted. Becomes "New" on every write to the buffer
// programma durante LRN (prog_store_step, DEL, INS); torna "Old" — program during LRN (prog_store_step, DEL, INS); goes back to "Old"
// quando un programma viene caricato da fonte esterna — when a program is loaded from an external source
// (tms1500_load_prog) o quando un livello esterno con accesso al — (tms1500_load_prog) or when an external layer with access to the
// sottosistema schede conferma un salvataggio riuscito — card subsystem confirms a successful save
// (tms1500_mark_prog_saved(), da chiamare da wifilink.cpp dopo — (tms1500_mark_prog_saved(), to be called from wifilink.cpp after
// cardemu_write()/handle_prog_post() o dal WRITE fisico). — cardemu_write()/handle_prog_post() or from the physical WRITE).
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
    if (enable == g_realistic_timing) return;   // nessun cambiamento reale — no real change
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
    // Prima, in modalità Old, questa funzione bloccava con vTaskDelay() — Previously, in Old mode, this function blocked with vTaskDelay()
    // per la durata autentica: busy_active passava a true e tornava a — for the authentic duration: busy_active went true and went back to
    // false PRIMA che la chiamata HTTP che ha originato la pressione — false BEFORE the HTTP call that originated the keypress
    // tasto potesse anche solo rispondere — nessun poll da web poteva — key could even respond — no web poll could
    // mai osservare lo stato "occupato", perché avveniva interamente — ever observe the "busy" state, because it happened entirely
    // dentro una singola chiamata sincrona sullo stesso task che serve — inside a single synchronous call on the same task that serves
    // anche il server web. Ora entrambe le modalità sono non — also the web server. Now both modes are non-
    // bloccanti: il calcolo resta istantaneo, ma "C" resta visibile — blocking: the calculation stays instantaneous, but "C" stays visible
    // (controllato pigramente in tms1500_get_display_string) per la — (checked lazily in tms1500_get_display_string) for the
    // durata autentica in Old, o per il minimo fisso in New — così è — authentic duration in Old, or the fixed minimum in New — so it is
    // finalmente osservabile da un poll web in entrambi i casi. — finally observable from a web poll in both cases.
    busy_active = true;
    unsigned long duration = g_realistic_timing
        ? (unsigned long)((float)real_ms * g_timing_multiplier)
        : BUSY_MODERN_MIN_MS;
    busy_until_ms = millis() + duration;
}

// ─── Combo +,-,×,÷ "premuti insieme": alterna timing reale/moderno — "+,-,×,÷ pressed together" combo: toggles real/modern timing ──
// Con una tastiera a matrice scandita in sequenza, la "simultaneità" — With a matrix keyboard scanned in sequence, true "simultaneity"
// vera non esiste: si approssima registrando l'istante di arrivo di — does not exist: it is approximated by recording the arrival instant of
// ciascuno dei 4 tasti operatore e controllando che tutti e 4 siano — each of the 4 operator keys and checking that all 4 are
// arrivati entro una finestra breve (400ms, ragionevole per una — within a short window (400ms, reasonable for a
// pressione a mano "a ventaglio" con più dita). Finché non arrivano — hand "fan" press with several fingers). Until all
// tutti e 4 nella finestra, il normale comportamento aritmetico dei — 4 arrive in the window, the normal arithmetic behavior of the
// tasti +,-,×,÷ resta invariato — il controllo si limita ad — +,-,×,÷ keys stays unchanged — the check only
// aggiungersi PRIMA di esso, senza toglierlo: premere in sequenza — adds itself BEFORE it, without removing it: pressing
// questi tasti senza cifre in mezzo è già di per sé innocuo sul — these keys with no digits in between is already harmless on the
// risultato (nessuna cifra nuova = exec_pending() non ha nulla da — result (no new digit = exec_pending() has nothing to
// risolvere), quindi non serve "annullare" nulla se il combo scatta. — resolve), so nothing needs to be "cancelled" if the combo fires.
#define TIMING_TOGGLE_WINDOW_MS 400
static unsigned long op_combo_ms[4] = {0, 0, 0, 0};   // +, -, ×, ÷ — +, -, ×, ÷

static void check_timing_toggle_combo(uint8_t kc) {
    int idx = (kc == KC_ADD) ? 0 : (kc == KC_SUB) ? 1 : (kc == KC_MUL) ? 2 : 3;
    unsigned long now = millis();
    op_combo_ms[idx] = now;

    unsigned long oldest = now;
    for (int i = 0; i < 4; i++) {
        if (op_combo_ms[i] == 0) return;   // non tutti e 4 ancora premuti — not all 4 pressed yet
        if (op_combo_ms[i] < oldest) oldest = op_combo_ms[i];
    }
    if (now - oldest <= TIMING_TOGGLE_WINDOW_MS) {
        bool new_mode = !tms1500_get_realistic_timing();
        tms1500_set_realistic_timing(new_mode);
        Serial.printf("[TIMING] Combo +,-,x,/ rilevato -> modalita' %s\n",
                      new_mode ? "REALE (timing autentico)" : "MODERNA (istantanea)");
        for (int i = 0; i < 4; i++) op_combo_ms[i] = 0;   // consuma il combo — consume the combo
    }
}

static char input_buf[16];
static int  input_len = 0;

static PrinterState g_printer;
static bool input_has_dot = false;
static bool input_has_ee  = false;
static int  input_ee_len  = 0;

// KEY_MAP[row][col] — maps physical keyboard to TI-59 keycodes
// Riga 0: A, B, C, D, E — Row 0: A, B, C, D, E
// Riga 1: 2nd, INV, LNx, CE, CLR — Row 1: 2nd, INV, LNx, CE, CLR
// Riga 2: LRN, x↔t, x², √x, 1/x — Row 2: LRN, x↔t, x², √x, 1/x
// Riga 3: SST, STO, RCL, SUM, yˣ — Row 3: SST, STO, RCL, SUM, yˣ
// Riga 4: BST, EE, (, ), ÷ — Row 4: BST, EE, (, ), ÷
// Riga 5: GTO, 7, 8, 9, × — Row 5: GTO, 7, 8, 9, ×
// Riga 6: SBR, 4, 5, 6, − — Row 6: SBR, 4, 5, 6, −
// Riga 7: RST, 1, 2, 3, + — Row 7: RST, 1, 2, 3, +
// Riga 8: R/S, 0, ., +/−, = — Row 8: R/S, 0, ., +/−, =
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
// Viene usato a RUNTIME quando 2nd viene premuto prima di un tasto. — This is used at RUNTIME when 2nd is pressed before a key.
// In modalità LRN, il keycode grezzo + il prefisso 2nd vengono memorizzati separatamente. — In LRN mode, the raw keycode + 2nd prefix are stored separately.
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
        case KC_CLR:     return KC_CLR_2ND;   // 2nd CLR = Clear (codice 20) — 2nd CLR = Clear (code 20)
        case KC_CE:      return KC_CP;        // 2nd CE = CP (codice 29) — 2nd CE = CP (code 29)
        default:         return kc;
    }
}

// ═══════════════════════════════════════════════════════════
// HELPER I/O FILE — HELPER FILE I/O
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
// ARITMETICA BCD — BCD ARITHMETIC
// ═══════════════════════════════════════════════════════════

// ── Dichiarazioni in avanti — Forward declarations ───────────────────────────────────

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
// Dichiarazioni in avanti per funzioni esterne — Forward declarations for external functions
void format_display(TMS1500_State *cpu);
static void rebuild_labels(TMS1500_State *cpu);
void prog_store_step(TMS1500_State *cpu, uint8_t kc);
uint8_t prog_read_step(TMS1500_State *cpu, uint16_t addr);
char* build_library_listing(size_t *out_len);

// ── Interfaccia pubblica — Public interface ───────────────────────────────────

void bcd_zero(BCD_Reg *r) { memset(r->n, 0, REG_WIDTH); }
void bcd_copy(BCD_Reg *dst, const BCD_Reg *src) { memcpy(dst->n, src->n, REG_WIDTH); }

bool bcd_is_zero(const BCD_Reg *r) {
    for (int i = 2; i < REG_WIDTH; i++) if (r->n[i] != 0) return false;
    return true;
}

void bcd_from_int(BCD_Reg *r, int32_t v) {
    bcd_from_double(r, (double)v);
}

// Precisione interna della mantissa: 13 cifre, come il TI-59 reale — Internal mantissa precision: 13 digits, like the real TI-59
// (che internamente calcola con 13 cifre pur mostrandone solo 10-11 a — (which internally computes with 13 digits while showing only 10-11 on
// display, per arrotondamenti fedeli). REG_WIDTH (18, in tms1500.h) ha — display, for faithful rounding). REG_WIDTH (18, in tms1500.h) has
// spazio a sufficienza: n[0]=segno valore, n[1]=segno esponente, — enough room: n[0]=value sign, n[1]=exponent sign,
// n[2..3]=esponente (2 cifre), n[4..4+MANT_DIGITS-1]=mantissa, il resto — n[2..3]=exponent (2 digits), n[4..4+MANT_DIGITS-1]=mantissa, the rest
// resta di riserva. — stays as reserve.
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
        // Overflow/invalido: segnala l'errore come fanno bcd_clamp/bcd_div, — Overflow/invalid: signals the error as bcd_clamp/bcd_div do,
        // altrimenti l'overflow restava silenzioso e il calcolo proseguiva — otherwise the overflow stayed silent and the calculation continued
        // con 9.9999999 99 come se nulla fosse. La CPU mostrerà il — with 9.9999999 99 as if nothing happened. The CPU will show the
        // lampeggio di errore (display.cpp) e il programma si fermerà. — error blinking (display.cpp) and the program will stop.
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
        // Underflow: valore troppo piccolo per essere rappresentato; restituisci +0. — Underflow: value is too small to represent; return +0.
        // (Il ripristino del segno che c'era qui era codice morto: bcd_zero() aveva — (The sign-restore that was here was dead code: bcd_zero() had
        // già azzerato n[0], quindi la condizionale veniva sempre valutata a 0.) — already cleared n[0], so the conditional always evaluated to 0.)
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

// ── Helper BCD interni — Internal BCD helpers ─────────────────────────────────────

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

// ── Divisione lunga BCD — Long BCD division ────────────────────────────────────
// cmp_mant_ext/sub_mant_ext operano su buffer larghi MANT_DIGITS+1 (una — cmp_mant_ext/sub_mant_ext operate on buffers MANT_DIGITS+1 wide (one
// cifra in più per l'allineamento durante la divisione lunga) — il nome — extra digit for alignment during long division) — the name
// storico "12" viene da quando MANT_DIGITS era 11; la dimensione reale — historical "12" comes from when MANT_DIGITS was 11; the real size
// ora è MANT_DIGITS+1. — now is MANT_DIGITS+1.
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
    w[0] = 0; // Spazio per l'allineamento e il carry — Space for alignment and carry
    for (int i = 0; i < MANT_DIGITS; i++) w[i + 1] = a[i];

    memset(quot, 0, 2 * MANT_DIGITS);

    // MANT_DIGITS+2 iterazioni per ottenere MANT_DIGITS cifre valide + — MANT_DIGITS+2 iterations to get MANT_DIGITS valid digits +
    // eventuale shift + una cifra per l'arrotondamento. — possible shift + one digit for rounding.
    for (int i = 0; i < MANT_DIGITS + 2; i++) {
        int d = 0;

        // Trova la cifra massima 'd' (0..9) tale che d * b <= w — Find the maximum digit 'd' (0..9) such that d * b <= w
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

        // w = w - d * b — w = w - d * b
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

        // w = w * 10 (Shift a sinistra per la prossima iterazione) — w = w * 10 (Shift left for the next iteration)
        for (int j = 0; j < MANT_DIGITS; j++) w[j] = w[j + 1];
        w[MANT_DIGITS] = 0;
    }

    if (rem) {
        for (int i = 0; i < MANT_DIGITS + 1; i++) rem[i] = w[i];
    }
}

// ── Overflow / Underflow — Overflow / Underflow ───────────────────────────────────

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

// ── Operazioni fondamentali BCD — Basic BCD operations ──────────────────────────────

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
        tmp.n[0] = (tmp.n[0] == 0) ? -1 : 0;   // inverti segno: 0 -> -1, -1 -> 0 — toggle sign: 0 -> -1, -1 -> 0
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
        tmp.n[0] = (tmp.n[0] == 0) ? -1 : 0;   // inverti segno: 0 -> -1, -1 -> 0 — toggle sign: 0 -> -1, -1 -> 0
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
        // Prodotto in [10.0, 100.0): le prime MANT_DIGITS cifre sono la — Product in [10.0, 100.0): the first MANT_DIGITS digits are the
        // mantissa (rappresentano un valore in [1.0,10.0) dopo il bump — mantissa (they represent a value in [1.0,10.0) after the bump
        // di esponente sotto), prod[MANT_DIGITS] è la cifra di guardia. — of exponent below), prod[MANT_DIGITS] is the guard digit.
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
        // Prodotto in [1.0, 10.0): salta lo zero iniziale in prod[0] e — Product in [1.0, 10.0): skip the leading zero in prod[0] and
        // prendi le cifre 1..MANT_DIGITS come mantissa, prod[MANT_DIGITS+1] — take digits 1..MANT_DIGITS as the mantissa, prod[MANT_DIGITS+1]
        // come cifra di guardia. — as the guard digit.
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

    // CORRETTO: Dividendo (mant_a) e poi Divisore (mant_b) — CORRECT: Dividend (mant_a) and then Divisor (mant_b)
    div_mant(mant_a, mant_b, quot, rem);

    int res_exp = exp_a - exp_b;
    uint8_t res_mant[MANT_DIGITS];

    // Se quot[0] è != 0 (es. 4/2 = 2.0), mantissa è già posizionata — If quot[0] is != 0 (e.g. 4/2 = 2.0), mantissa is already positioned
    if (quot[0] != 0) {
        for (int i = 0; i < MANT_DIGITS; i++) res_mant[i] = quot[i];

        // Arrotondamento (cifra di guardia in quot[MANT_DIGITS]) — Rounding (guard digit in quot[MANT_DIGITS])
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
    // Se quot[0] == 0 (es. 2/4 = 0.5), scaliamo di una posizione — If quot[0] == 0 (e.g. 2/4 = 0.5), we scale by one position
    else {
        for (int i = 0; i < MANT_DIGITS; i++) res_mant[i] = quot[i + 1];
        res_exp--;

        // Arrotondamento (cifra di guardia in quot[MANT_DIGITS+1] dopo lo shift) — Rounding (guard digit in quot[MANT_DIGITS+1] after the shift)
        if (quot[MANT_DIGITS + 1] >= 5) {
            uint8_t one[MANT_DIGITS] = {0};
            one[MANT_DIGITS - 1] = 1;
            int c = add_mant(res_mant, res_mant, one);
            if (c) {
                for (int i = MANT_DIGITS - 1; i > 0; i--) res_mant[i] = res_mant[i - 1];
                res_mant[0] = 1;
                res_exp++; // Gestisce casi come 0.9999... -> 1.0000... — Handles cases like 0.9999... -> 1.0000...
            }
        }
    }

    bcd_zero(result);
    result->n[0] = (sign_a * sign_b < 0) ? -1 : 0;
    bcd_set_mantissa(result, res_mant);
    bcd_set_exp(result, res_exp);
    bcd_clamp(result, flags);
}

// ── Operazioni BCD aggiuntive — Additional BCD operations ──────────────────────────────

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
            /* tan(90°), tan(270°), ecc. non sono definiti — tan(90°), tan(270°), etc. are undefined */
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
    // P→R (TI-59 reale): si digita prima θ (l'ENTER/stack-lift lo sposta — P→R (real TI-59): you enter θ first (the ENTER/stack-lift moves it
    // in B), poi r che resta in display/A. Risultato: x→display(A), y→B. — into B), then r which stays in display/A. Result: x→display(A), y→B.
    double r     = bcd_to_double(&cpu->reg[REG_A]);   // A = r (ultimo digitato) — A = r (last entered)
    double theta = bcd_to_double(&cpu->reg[REG_B]);   // B = θ (spinto su dallo stack-lift) — B = θ (pushed up by the stack-lift)
    double rad = theta;
    if (cpu->trig_mode == 0) rad = theta * M_PI / 180.0;
    else if (cpu->trig_mode == 2) rad = theta * M_PI / 200.0;
    double x = r * cos(rad);
    double y = r * sin(rad);
    bcd_from_double(&cpu->reg[REG_A], x);   // x → display — x → display
    bcd_from_double(&cpu->reg[REG_B], y);   // y → B — y → B
}
static void math_r2p(TMS1500_State *cpu) {
    // INV P→R = R→P: si digita prima y (spinto in B), poi x che resta in — INV P→R = R→P: you enter y first (pushed into B), then x which stays in
    // display/A. Risultato: r→display(A), θ→B. — display/A. Result: r→display(A), θ→B.
    double x = bcd_to_double(&cpu->reg[REG_A]);   // A = x (ultimo digitato) — A = x (last entered)
    double y = bcd_to_double(&cpu->reg[REG_B]);   // B = y (spinto su dallo stack-lift) — B = y (pushed up by the stack-lift)
    double r = sqrt(x*x + y*y);
    double theta = atan2(y, x);
    if (cpu->trig_mode == 0) theta = theta * 180.0 / M_PI;
    else if (cpu->trig_mode == 2) theta = theta * 200.0 / M_PI;
    bcd_from_double(&cpu->reg[REG_A], r);   // r → display — r → display
    bcd_from_double(&cpu->reg[REG_B], theta);  // θ → B — θ → B
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
// UTILITY — HELPER
// ═══════════════════════════════════════════════════════════

static uint16_t find_label(TMS1500_State *cpu, uint8_t label_kc) {
    if (showing_lib_prog) {
        int idx = label_index_for_key(label_kc);
        if (idx >= 0) return lib_custom_label_pc[idx];

        // L'etichetta non è A-E/A'-E' (es. etichette ROM interne come "=" = 95, — Label is not A-E/A'-E' (e.g. internal ROM labels like "=" = 95,
        // o "CE" = 24, o "CLR" = 25). La tabella precalcolata lib_custom_label_pc — or "CE" = 24, or "CLR" = 25). The precomputed lib_custom_label_pc
        // copre solo le 10 etichette accessibili all'utente; per qualsiasi altro — table only covers the 10 user-accessible labels; for any other
        // keycode usato come etichetta dentro la ROM (target SBR/GTO che sono — keycode used as a label inside the ROM (SBR/GTO targets that are
        // interni al firmware della libreria), facciamo una scansione lineare consapevole dello scope. — internal to the library firmware), we do a scope-aware linear scan.
        //
        // La priorità rispecchia rebuild_lib_labels: — Priority mirrors rebuild_lib_labels:
        //   1. Dentro lo scope del programma designato (priorità più alta) — 1. Within the designated program's own scope (highest priority)
        //   2. Prima dello scope (routine ROM condivise prima nell'immagine) — 2. Before scope (shared ROM routines earlier in the image)
        //   3. Dopo lo scope (routine ROM condivise dopo nell'immagine) — 3. After scope  (shared ROM routines later in the image)
        const LibraryModule *m = library_get_active();
        if (!m) return 0xFFFF;

        uint16_t scope_end = lib_scope_addr + lib_scope_len;
        if (scope_end > m->rom_size) scope_end = m->rom_size;

        // Passo 1: dentro lo scope del programma designato — Pass 1: within the designated program's scope
        for (uint16_t i = lib_scope_addr; i + 1 < scope_end; i++) {
            if (m->rom[i] == KC_LBL && m->rom[i + 1] == label_kc) return i;
        }
        // Passo 2: prima dello scope (subroutine condivise che precedono il programma) — Pass 2: before the scope (shared subroutines preceding the program)
        for (uint16_t i = 0; i + 1 < lib_scope_addr; i++) {
            if (m->rom[i] == KC_LBL && m->rom[i + 1] == label_kc) return i;
        }
        // Passo 3: dopo lo scope (subroutine condivise che seguono il programma) — Pass 3: after the scope (shared subroutines following the program)
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

/* Esegue il DSZ vero e proprio: decrementa il VALORE ASSOLUTO del registro — Executes the actual DSZ: decrements the ABSOLUTE VALUE of the register
 * verso zero (senza mai superarlo, comportamento documentato della TI-59 — toward zero (without ever exceeding it, documented TI-59 behavior
 * reale — un contatore negativo va verso 0, non verso -N). Se il risultato — real — a negative counter goes toward 0, not toward -N). If the result
 * non è zero, salta a target_pc (il loop continua); se è zero, non salta — is not zero, jumps to target_pc (the loop continues); if it is zero, it does not jump
 * (l'esecuzione prosegue con l'istruzione successiva, il loop finisce). — (execution continues with the next instruction, the loop ends). */
static void dsz_do(TMS1500_State *cpu, int reg, uint16_t target_pc) {
    reg %= 100;
    BCD_Reg *bank = active_ram_bank(cpu);
    double v = bcd_to_double(&bank[reg]);
    if (v > 0)      { v -= 1.0; if (v < 0) v = 0; }
    else if (v < 0) { v += 1.0; if (v > 0) v = 0; }
    bcd_from_double(&bank[reg], v);
    if (fabs(v) > 1e-9) {
        /* target_pc arriva già risolto e assoluto dal chiamante (label — target_pc arrives already resolved and absolute from the caller (label
         * via find_label(), oppure indirizzo a 3 cifre già rilocato con — via find_label(), or a 3-digit address already relocated with
         * lib_scope_addr se in esecuzione "as-is" da modulo — vedi — lib_scope_addr if running "as-is" from a module — see
         * chiamate in process_keycode()/exec_program_step()). Non va — calls in process_keycode()/exec_program_step()). It must not be
         * più ridotto qui con "% exec_prog_len(cpu)": in modalità — further reduced here with "% exec_prog_len(cpu)": in library
         * libreria exec_prog_len() restituisce la lunghezza del solo — mode exec_prog_len() returns the length of only the
         * programma designato (es. 189 per ML-01), non l'indirizzo — designated program (e.g. 189 for ML-01), not the absolute
         * assoluto nella ROM — wrappare qui perdeva l'offset — address in the ROM — wrapping here lost the
         * lib_scope_addr e faceva atterrare il salto fuori dal — lib_scope_addr offset and made the jump land outside the
         * programma (spesso nell'area prima del suo inizio). — program (often in the area before its start). */
        cpu->prog_pc = target_pc;
    }
    /* v == 0: nessun salto, si prosegue in sequenza — v == 0: no jump, execution continues in sequence */
}

static uint8_t bcd_to_int_reg(const BCD_Reg *r) {
    // REG_WIDTH è 18: il loop accumula 14 cifre (i da 4 a 17). Un "int" a — REG_WIDTH is 18: the loop accumulates 14 digits (i from 4 to 17). A 32-bit "int"
    // 32 bit overflow oltre 10 cifre (2147483647), quindi i registri con — overflows beyond 10 digits (2147483647), so registers with
    // moltiplicando/indice grande (es. 99999999999999) producevano un — large multiplicand/index (e.g. 99999999999999) produced a
    // valore negativo e l'operazione indiretta puntava al registro sbagliato. — negative value and the indirect operation pointed to the wrong register.
    // Accumulo in uint64_t: 14 cifre (max ~1e14) ci stanno senza problemi. — Accumulation in uint64_t: 14 digits (max ~1e14) fit without problems.
    uint64_t v = 0;
    for (int i = 4; i < REG_WIDTH; i++) {
        int d = r->n[i]; if (d < 0) d = 0; if (d > 9) d = 9;
        v = v * 10 + (uint64_t)d;
    }
    return (uint8_t)(v % 100);
}

/* Variante per GTO/SBR indiretto: il registro puntato contiene uno step — Variant for indirect GTO/SBR: the pointed register contains a program
   di programma (000-959), non un numero di registro dati (00-99). — step (000-959), not a data-register number (00-99). */
static uint16_t bcd_to_int_step(const BCD_Reg *r) {
    uint64_t v = 0;
    for (int i = 4; i < REG_WIDTH; i++) {
        int d = r->n[i]; if (d < 0) d = 0; if (d > 9) d = 9;
        v = v * 10 + (uint64_t)d;
    }
    return (uint16_t)(v % 1000);
}

// Dichiarazioni in avanti: exec_op usa queste funzioni statistiche — Forward declarations: exec_op uses these statistical functions
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
        // Op 00-08: stampante/plotter PC-100A. Op 00, 05, 06, 07 — Op 00-08: PC-100A printer/plotter. Op 00, 05, 06, 07
        // sono ora collegati alla vera emulazione del buffer di — are now wired to the real emulation of the print buffer
        // stampa (vedi printer.h/.cpp) e al backend BLE, quando — (see printer.h/.cpp) and to the BLE backend, when
        // collegato — nessun effetto sui registri di calcolo, come — connected — no effect on the calculation registers, as
        // sull'hardware reale. Op 01-04 (riempimento gruppi — on the real hardware. Op 01-04 (alphanumeric group
        // alfanumerici) restano no-op finché non è disponibile la — filling) remain no-op until the complete
        // tabella completa a 64 simboli (Table VII, brevetto — 64-symbol table (Table VII, patent
        // US4153937) usata per decodificare i codici carattere — US4153937) used to decode the character codes
        // digitati: senza quella, decodificare in modo scorretto — entered: without it, decoding incorrectly
        // sarebbe peggio che non stampare nulla. Op 08 (lista — would be worse than printing nothing. Op 08 (label
        // etichette) richiede la stessa tabella per i mnemonici a 3 — listing) requires the same table for the 3-character mnemonics
        // caratteri — no-op per lo stesso motivo. — no-op for the same reason.
        // ═══════════════════════════════════════════════════════
        case 0:
            printer_op00_init(&g_printer);
            break;
        case 1: case 2: case 3: case 4: {
            // Legge 10 cifre di mantissa da REG_A (n[4]..n[13]), — Reads 10 mantissa digits from REG_A (n[4]..n[13]),
            // le interpreta come 5 coppie di codici carattere Table VII, — interprets them as 5 pairs of Table VII character codes,
            // e le spedisce al gruppo stampante corrispondente. — and sends them to the corresponding printer group.
            char pairs[6]; // 5 caratteri + NUL — 5 characters + NUL
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
            // Lista etichette del programma corrente — Label list of the current program
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
        // Op 09: scarica il programma designato con "2nd Pgm mm" in — Op 09: downloads the program designated with "2nd Pgm mm" into
        // memoria principale, a partire dal passo 000, sovrascrivendo — main memory, starting at step 000, overwriting
        // quanto c'era prima — esattamente come da manuale TI-59 — what was there before — exactly as per the TI-59 manual
        // (Programmazione Personale): "Questa procedura colloca il richiesto — (Personal Programming): "This procedure places the requested
        // programma nella memoria programmi a partire dalla posizione — program into program memory beginning at program location
        // 000. Il programma scaricato sovrascrive qualsiasi istruzione — 000. The downloaded program writes over any instructions
        // memorizzata in precedenza in quella parte di memoria." Da quel — previously stored in that part of program memory." From that
        // momento è un programma come un altro: stessa memoria, stesse — moment it is a program like any other: same memory, same
        // LBL/GTO/SBR di sempre. Percorso alternativo a quello "as-is" — LBL/GTO/SBR as always. Alternative path to the "as-is" one
        // (un tasto etichetta dopo Pgm mm, senza passare da qui): qui — (a label key after Pgm mm, without going through here): here
        // invece la modalità as-is va sempre chiusa, se per caso era — instead the as-is mode must always be closed, if it happened to be
        // attiva, dato che si sta passando alla memoria normale. — active, since we are switching to normal memory.
        // ═══════════════════════════════════════════════════════
        case 9: {
            if (!lib_page_selected) {
                cpu->flags.error = true;   // Op 09 senza un Pgm mm precedente — Op 09 without a preceding Pgm mm
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
                tms1500_mark_prog_saved();   // combacia con la sorgente (il modulo), non "modificato" — matches the source (the module), not "modified"
                Serial.printf("[LIB] Programma %02d scaricato: %s (%u passi, da modulo %s)\n",
                              lib_selected_page, title ? title : "?", plen, m->name);
            } else {
                cpu->flags.error = true;   // programma inesistente, o nessun modulo attivo — non-existent program, or no active module
                Serial.printf("[LIB] Scarico fallito per il programma %02d (nessun modulo attivo, o numero inesistente)\n",
                              lib_selected_page);
            }
            lib_page_selected = false;
            showing_lib_prog  = false;   // torna comunque alla memoria normale — goes back to normal memory anyway
            break;
        }

        // ═══════════════════════════════════════════════════════
        // Op 10: funzione segno — restituisce il segno del valore — Op 10: sign function — returns the sign of the value
        // nel registro A: +1, 0, -1. — in register A: +1, 0, -1.
        // Op 11: varianza campionaria. — Op 11: sample variance.
        // Op 12: pendenza e intercetta della regressione lineare. — Op 12: slope and intercept of the linear regression.
        // Op 13: coefficiente di correlazione. — Op 13: correlation coefficient.
        // Op 14: stima di y (y') per x in A. — Op 14: estimate of y (y') for x in A.
        // Op 15: stima di x (x') per y in A. — Op 15: estimate of x (x') for y in A.
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
        // Op 16: mostra la partizione corrente memoria/passi. — Op 16: shows the current memory/steps partition.
        // Op 17: imposta la partizione (uno dei codici Op più usati — Op 17: sets the partition (one of the most used Op codes
        // nei programmi reali — confermato da due fonti indipendenti, — in real programs — confirmed by two independent sources,
        // inclusa la documentazione dell'emulatore TI59C). — including the TI59C emulator documentation).
        // Questo firmware NON implementa un pool di memoria condiviso — This firmware does NOT implement a shared
        // e ripartizionabile: PROG_SIZE (960 passi) e RAM_SIZE (100 — and re-partitionable memory pool: PROG_SIZE (960 steps) and RAM_SIZE (100
        // registri) sono sempre entrambi al massimo contemporaneamente, — registers) are always both at maximum simultaneously,
        // quindi non possiamo davvero cambiare la partizione. Il punto — so we cannot really change the partition. The critical
        // critico è che ora Op 17 non produce PIÙ un effetto collaterale — point is that now Op 17 no longer produces a wrong side effect
        // sbagliato (prima calcolava un coefficiente di correlazione — (previously it computed a statistical correlation
        // statistica e lo scriveva in un registro!). I programmi che la — coefficient and wrote it into a register!). Programs that
        // usano solo per fissare la partizione continuano a funzionare — use it only to fix the partition keep working
        // correttamente: la richiesta viene semplicemente ignorata, — correctly: the request is simply ignored,
        // dato che qui c'è comunque sempre la capacità massima. — since maximum capacity is always available here anyway.
        // ═══════════════════════════════════════════════════════
        case 16: case 17:
            break;

        // ═══════════════════════════════════════════════════════
        // Op 18-19: test flag di errore. CONFERMATO da fonte primaria — Op 18-19: error flag test. CONFIRMED by a primary source
        // (TI Master Library Quick Reference Guide, "Special Control — (TI Master Library Quick Reference Guide, "Special Control
        // Operations"): "18 Se in un programma non esiste una condizione di errore, — Operations"): "18 If no error condition exists in a program,
        // imposta il flag 7" / "19 Se in un programma esiste una condizione di errore, — set flag 7" / "19 If an error condition exists in a program,
        // set flag 7". Numerazione e semantica esatte, nessuna ipotesi. — set flag 7". Exact numbering and semantics, no guesswork.
        // ═══════════════════════════════════════════════════════
        case 18: if (!cpu->flags.error) user_flags[7] = true; break;
        case 19: if (cpu->flags.error)  user_flags[7] = true; break;

        // ═══════════════════════════════════════════════════════
        // Op 20-39: incrementa/decrementa registri dati 00-09. — Op 20-39: increment/decrement data registers 00-09.
        // CONFERMATO da fonte primaria (TI Master Library Quick — CONFIRMED by a primary source (TI Master Library Quick
        // Reference Guide, "Special Control Operations"): "20-29 — Reference Guide, "Special Control Operations"): "20-29
        // Incrementa un registro dati 0-9 di 1" / "30-39 Decrementa un — Increment a data register 0-9 by 1" / "30-39 Decrement a
        // registro dati 0-9 di 1". Codici Op reali della TI-59, non — data register 0-9 by 1". Real TI-59 Op codes, not
        // un'estensione dell'emulatore come si pensava in precedenza. — an emulator extension as previously thought.
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
        // Op 40: test "stampante collegata". ATTENZIONE: la fonte — Op 40: "printer connected" test. WARNING: the primary
        // primaria (TI Master Library QRG) documenta ufficialmente — source (TI Master Library QRG) officially documents
        // solo i codici Op 00-39; Op 40 resta un'estensione non — only Op codes 00-39; Op 40 remains an unverified
        // verificata di questo emulatore e non un codice Op reale — emulator extension and not a real Op code
        // confermato — possibile collisione futura se emergesse un — confirmed — possible future collision if a different
        // significato diverso per questo codice su hardware originale. — meaning for this code emerged on original hardware.
        // Nel frattempo riflette lo stato reale del backend BLE — Meanwhile it reflects the real state of the BLE backend
        // (v. printer.h/ble_thermal_printer.h): flag 7 impostato solo — (see printer.h/ble_thermal_printer.h): flag 7 set only
        // se una stampante è davvero connessa, così un programma che — if a printer is really connected, so a program that
        // dirama su questo test salta correttamente le sezioni di — branches on this test correctly skips the
        // stampa quando non c'è nulla collegato, invece di crederle — printing sections when nothing is connected, instead of believing them
        // eseguite con successo. — successfully executed.
        // ═══════════════════════════════════════════════════════
        case 40: if (printer_is_connected(&g_printer)) user_flags[7] = true; break;

        // ═══════════════════════════════════════════════════════
        // Op 90-94: funzioni statistiche dell'emulatore (deviazione — Op 90-94: emulator statistical functions (standard
        // standard x/y, pendenza e intercetta della regressione — deviation x/y, slope and intercept of the linear
        // lineare, coefficiente di correlazione). NON corrispondono a — regression, correlation coefficient). They do NOT correspond to
        // codici Op reali della TI-59 — su hardware originale queste — real TI-59 Op codes — on original hardware these
        // funzioni si richiamano con tasti dedicati (2nd s, 2nd LR...) — functions are called with dedicated keys (2nd s, 2nd LR...)
        // non ancora cablati sulla tastiera fisica di questo progetto. — not yet wired on the physical keyboard of this project.
        // Le ho spostate qui, fuori dalla fascia 00-19 potenzialmente — I moved them here, outside the potentially real 00-19 range,
        // reale, per non rischiare più collisioni con la numerazione — to avoid further collisions with the original
        // originale mentre restano comunque disponibili. — numbering while they remain available anyway.
        // ═══════════════════════════════════════════════════════
        case 90: stat_stddev_x(cpu);     format_display(cpu); break;
        case 91: stat_stddev_y(cpu);     format_display(cpu); break;
        case 92: stat_lr_slope(cpu);     format_display(cpu); break;
        case 93: stat_lr_intercept(cpu); format_display(cpu); break;
        case 94: stat_correlation(cpu);  format_display(cpu); break;

        // ═══════════════════════════════════════════════════════
        // Resto: non implementato / riservato. — Rest: not implemented / reserved.
        // ═══════════════════════════════════════════════════════
        default: break;
    }
}

// Registri statistici standard TI-59: R01=Σy, R02=Σy², R03=N, R04=Σx, — Standard TI-59 statistical registers: R01=Σy, R02=Σy², R03=N, R04=Σx,
// R05=Σx², R06=Σxy — mappatura documentata sia da ML-01 ("Linear — R05=Σx², R06=Σxy — mapping documented both by ML-01 ("Linear
// Regression Init... clears registers R01 through R06") sia dalla — Regression Init... clears registers R01 through R06") and by the
// tabella Register Contents di ML-15 (Random Number Generator, che — Register Contents table of ML-15 (Random Number Generator, which
// riusa questi stessi registri per calcolare media e deviazione — reuses these same registers to compute mean and standard
// standard dei numeri generati). Nessun accumulatore nascosto separato: — deviation of the generated numbers). No separate hidden accumulator:
// i registri STESSI sono l'accumulatore, esattamente come sull'hardware — the registers THEMSELVES are the accumulator, exactly as on the
// reale — così un programma che scrive/legge R01-R06 direttamente resta — real hardware — so a program that writes/reads R01-R06 directly stays
// sempre sincronizzato con Σ+/Σ-/x̄/deviazione standard/regressione. — always in sync with Σ+/Σ-/x̄/standard deviation/regression.
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

// ── Dispatch aritmetico condiviso — Shared arithmetic dispatch ──────────────────────────────────────────
// Esegue cpu->pending_op su (cpu->operand_x  OP  cpu->reg[REG_A]) e — Executes cpu->pending_op on (cpu->operand_x  OP  cpu->reg[REG_A]) and
// memorizza il risultato di nuovo in cpu->reg[REG_A].  Imposta cpu->flags.error su — stores the result back into cpu->reg[REG_A].  Sets cpu->flags.error on
// errori aritmetici.  Restituisce false se pending_op è NONE (no-op). — arithmetic errors.  Returns false if pending_op is NONE (no-op).
// I chiamanti devono resettare cpu->pending_op a PENDING_OP_NONE dopo la chiamata. — Callers must reset cpu->pending_op to PENDING_OP_NONE after calling.
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
                // INV yX = y^(1/x): inverso della potenza (radice x-esima), — INV yX = y^(1/x): inverse of the power (x-th root),
                // es. la routine "compute i" di ML-18 (Lbl INV: ... INV yX RCL 01 ) — e.g. the "compute i" routine of ML-18 (Lbl INV: ... INV yX RCL 01 )
                // calcola (FV/PV)^(1/N). Prima di questo fix il flag INV — computes (FV/PV)^(1/N). Before this fix the INV flag
                // veniva perso e si eseguiva una normale potenza. — was lost and a normal power was executed.
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
// GESTIONE TASTI — KEY HANDLING
// ═══════════════════════════════════════════════════════════

static void input_clear(void) {
    memset(input_buf, 0, sizeof(input_buf));
    input_len = 0;
    input_has_dot = false;
    input_has_ee = false;
    input_ee_len = 0;
}

static void input_commit(TMS1500_State *cpu) {
    // Punto finale: true se il buffer termina con '.' (es. "5.") — Trailing dot: true if buffer ends with '.' (e.g. "5.")
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
 * process_keycode() — elabora un singolo keycode — processes a single keycode.
 * 
 * PRINCIPIO DI PROGETTAZIONE DEI TASTI PER LA COMPATIBILITÀ TI-59: — KEY DESIGN PRINCIPLE FOR TI-59 COMPATIBILITY:
 * In modalità LRN (learn), i keycode vengono memorizzati ESATTAMENTE come appaiono — In LRN (learn) mode, keycodes are stored EXACTLY as they appear
 * sulla tastiera reale del TI-59. Il prefisso 2nd viene gestito memorizzando — on the real TI-59 keyboard. The 2nd prefix is handled by storing
 * direttamente il codice tradotto — il TI-59 reale NON memorizza "21" — the translated code directly — the real TI-59 does NOT store "21"
 * come passo separato per 2nd; invece, 2nd cambia il codice del — as a separate step for 2nd; instead, 2nd changes the code of the
 * tasto SUCCESSIVO premuto. Per esempio: — NEXT key pressed. For example:
 *   - Premere "sin" (2nd x²) memorizza direttamente il codice 38 — Pressing "sin" (2nd x²) stores code 38 directly
 *   - Premere "2nd" e poi "x²" memorizza anch'esso il codice 38 — Pressing "2nd" then "x²" also stores code 38
 *   - Premere "INV" e poi "SBR" memorizza il codice 92 (Return) — Pressing "INV" then "SBR" stores code 92 (Return)
 * 
 * Il prefisso INV funziona in modo simile: inverte l'operazione successiva. — The INV prefix works similarly: it inverts the next operation.
 * INV + SBR = Return (codice 92), INV + LNx = eˣ, ecc. — INV + SBR = Return (code 92), INV + LNx = eˣ, etc.
 */
static void process_keycode(TMS1500_State *cpu, uint8_t kc) {

    // ── Gestione prefisso INV — Handle INV prefix ──────────────────────────────────
    // In modalità RUN, INV modifica IMMEDIATAMENTE il tasto successivo premuto. — In RUN mode, INV modifies the NEXT key pressed immediately.
    // In modalità LRN, INV imposta un flag pendente; il tasto successivo memorizza sia — In LRN mode, INV sets a pending flag; the NEXT key stores both
    // il keycode sia lo stato INV come passo combinato. — the keycode and the INV state as a combined step.
    if (cpu->flags.inv && kc != KC_INV) {
        // 2ND mentre INV è attivo: imposta pending_2nd ma tieni INV per il tasto successivo — 2ND while INV is active: set pending_2nd but keep INV for the next key
        if (kc == KC_2ND) {
            cpu->pending_2nd = !cpu->pending_2nd;
            return;
        }
        if (cpu->flags.lrn) {
            // In modalità LRN, imposta inv_pending e attendi il tasto successivo — In LRN mode, set inv_pending and wait for next key
            inv_pending = true;
            cpu->flags.inv = false;
            return;
        }
        // Modalità RUN: elabora INV+tasto immediatamente — RUN mode: process INV+key immediately
        // Applica prima la mappatura 2nd se pendente, così INV+2nd+tasto funziona correttamente — Apply 2nd mapping first if pending, so INV+2nd+key works correctly
        if (cpu->pending_2nd) {
            kc = keycode_2nd(kc);
            cpu->pending_2nd = false;
        }
        cpu->flags.inv = false;
        // INV + trig: arcoseno, arcocoseno, arcotangente — INV + trig: arcsin, arccos, arctan
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
        // INV + log = 10ˣ, INV + LNx = eˣ — INV + log = 10ˣ, INV + LNx = eˣ
        else if (kc == KC_LOG) {
            input_commit(cpu); math_exp(cpu, true); display_trailing_dp = false;
            format_display(cpu); return;
        }
        else if (kc == KC_LNX) {
            input_commit(cpu); math_exp(cpu, false); display_trailing_dp = false;
            format_display(cpu); return;
        }
        // INV + SBR = Return (codice 92) — INV + SBR = Return (code 92)
        else if (kc == KC_SBR) {
            if (cpu->sp > 0) {
                uint8_t old_sp = cpu->sp;
                cpu->sp--;
                cpu->prog_pc = cpu->stack[cpu->sp];
                cpu->pending_op = cpu->stack_pending_op[cpu->sp]; bcd_copy(&cpu->operand_x, &cpu->stack_operand_x[cpu->sp]);
                Serial.printf("[STK] INV+SBR kbd RET sp=%u->%u pop=%u in_rom=%u\n",
                    (unsigned)old_sp, (unsigned)cpu->sp, (unsigned)cpu->stack[cpu->sp],
                    (unsigned)cpu->stack_in_rom[cpu->sp]);
                // Ripristina SEMPRE lo stato del chiamante, non solo quando — ALWAYS restores the caller's state, not only when
                // era "true": altrimenti un ritorno da una subroutine ROM — it was "true": otherwise a return from a ROM subroutine
                // verso codice utente lascia showing_lib_prog bloccato a — to user code leaves showing_lib_prog stuck at
                // true (bug: prima veniva toccato solo nel ramo true). — true (bug: previously it was only touched in the true branch).
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
        // INV + P→R = R→P — INV + P→R = R→P
        else if (kc == KC_P_R) {
            input_commit(cpu); math_r2p(cpu); display_trailing_dp = false;
            format_display(cpu); return;
        }
        // INV + EE = cancella la notazione scientifica. Deve anche rimuovere un — INV + EE = cancel scientific notation. Must also remove an
        // esponente in corso di inserimento nel buffer (input_has_ee), — exponent being entered in the buffer (input_has_ee),
        // altrimenti INV EE azzerava solo il flag e il buffer restava — otherwise INV EE only cleared the flag and the buffer stayed
        // "mantissa e ±xx": il tasto cifra successivo riprendeva a scrivere — "mantissa and ±xx": the next digit key resumed writing
        // sull'esponente e la modalità non usciva mai davvero. — on the exponent and the mode never really exited.
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
        // INV + Fix = rilascia Fix — INV + Fix = release Fix
        else if (kc == KC_FIX) { cpu->flags.fix = false; cpu->flags.inv = false; return; }
        // INV + ENG = rilascia ENG — INV + ENG = release ENG
        else if (kc == KC_ENG) { cpu->flags.eng = false; cpu->flags.inv = false; return; }
        // INV + Int = parte frazionaria — INV + Int = frac
        else if (kc == KC_INT) {
            input_commit(cpu);
            double v = bcd_to_double(&cpu->reg[REG_A]);
            bcd_from_double(&cpu->reg[REG_A], v - trunc(v));
            display_trailing_dp = false;
            format_display(cpu); return;
        }
        // INV + |x| = segno — INV + |x| = signum
        else if (kc == KC_ABS) {
            input_commit(cpu);
            double v = bcd_to_double(&cpu->reg[REG_A]);
            bcd_from_double(&cpu->reg[REG_A], (v > 0) ? 1.0 : (v < 0) ? -1.0 : 0.0);
            display_trailing_dp = false;
            format_display(cpu); return;
        }
        // INV + D.MS = decimale→DMS — INV + D.MS = decimal→DMS
        else if (kc == KC_DMS) {
            input_commit(cpu);
            double v = bcd_to_double(&cpu->reg[REG_A]);
            bcd_from_double(&cpu->reg[REG_A], decimal_to_dms(v));
            display_trailing_dp = false;
            format_display(cpu); return;
        }
        // INV + Σ+ = Σ− — INV + Σ+ = Σ−
        else if (kc == KC_SIGP) {
            input_commit(cpu); stat_sigma_minus(cpu);
            format_display(cpu); return;
        }
        // INV + x̄ = ȳ — INV + x̄ = ȳ
        else if (kc == KC_XBAR) {
            stat_mean_y(cpu); return;
        }
        // X=T e X≥T sono istruzioni di salto condizionato SOLO — X=T and X≥T are conditional jump instructions ONLY
        // significative dentro un programma (vedi la gestione in — meaningful inside a program (see the handling in
        // exec_program_step, che legge l'etichetta/indirizzo di — exec_program_step, which reads the destination label/address
        // destinazione dalla ROM): una pressione diretta da tastiera non — from the ROM): a direct keyboard press does not
        // ha un target a cui saltare, quindi non ha effetto — come sul — have a target to jump to, so it has no effect — as on the
        // TI-59 reale. — real TI-59.
        else if (kc == KC_XEQ_T || kc == KC_XGE_T) {
            return;
        }
        // INV + yX = radice x-esima: y^(1/x). Qui si RI-SOSTIENE il flag — INV + yX = x-th root: y^(1/x). Here the INV flag is RE-SUPPORTED
        // INV (appena azzerato in testa al ramo) senza eseguire altro: — (just cleared at the top of the branch) without doing anything else:
        // il case KC_YX del main switch registra l'operazione pendente e — the KC_YX case of the main switch records the pending operation and
        // sarà apply_pending_op() a calcolare pow(y, 1/x) quando verrà — apply_pending_op() will compute pow(y, 1/x) when it is
        // risolta (al "=", alla ")" o a un operatore a priorità — resolved (at "=", at ")" or at an operator with higher/equal
        // maggiore/uguale). Senza questo, INV yX diventava un yX normale — precedence). Without this, INV yX became a normal yX
        // e la routine "compute i" di ML-18 dava un tasso errato. — and the "compute i" routine of ML-18 gave an incorrect rate.
        else if (kc == KC_YX) {
            cpu->flags.inv = true;
        }
        // Per qualsiasi altro tasto, INV non ha effetto — elabora normalmente — For any other key, INV has no effect — process normally
    }

    // ── FIX pendente (2nd Fix N) — FIX pending (2nd Fix N) ────────────────────────────
    if (fix_pending) {
        int d = keycode_to_digit(kc);
        if (d >= 0 && d <= 9) {
            if (d == 9) {
                cpu->flags.fix = false;  /* FIX 9 = nessun FIX — FIX 9 = no FIX */
            } else {
                cpu->flags.fix = true;
                cpu->fix_digits = d;
            }
            fix_pending = false;
            format_display(cpu);
            return;
        }
        /* Un non-cifra annulla il FIX pendente — Non-digit cancels FIX pending */
        fix_pending = false;
    }

        // ── Op pendente (2nd Op nn) — Op pending (2nd Op nn) ─────────────────────────────
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

    // ── Pgm pendente: numero di programma del modulo libreria (2 cifre) — Pgm pending: library module program number (2 digits) ──
    // "2nd Pgm mm" DESIGNA quale programma, senza fare altro. Il tasto — "2nd Pgm mm" DESIGNATES which program, without doing anything else. The key
    // successivo decide cosa succede: "2nd Op 09" scarica in memoria — next one decides what happens: "2nd Op 09" downloads it into memory
    // per modificarlo (v. exec_op case 9); un tasto etichetta A..E / — to modify it (see exec_op case 9); a label key A..E /
    // A'..E' lo esegue così com'è direttamente dalla ROM (v. gestore — A'..E' runs it as-is directly from the ROM (see handler
    // diretto più sotto). Corretto durante la revisione: non è "2nd — further down). Fixed during review: it is not "2nd
    // Op 09" da solo come da tabella Sladký (quella riga si riferisce — Op 09" alone as per the Sladký table (that row refers
    // a un'operazione di paginazione ROM a basso livello) — il tasto
    // fisico è Pgm (2nd LRN), seguito da 2 cifre. — physical key is Pgm (2nd LRN), followed by 2 digits.
    if (lib_page_pending) {
        if (is_digit_key(kc)) {
            lib_page_val = lib_page_val * 10 + keycode_to_digit(kc);
            lib_page_digits++;
            if (lib_page_digits >= 2) {
                uint8_t page = (uint8_t)(lib_page_val % 100);
                // PGM 00 esce dalla modalità libreria e torna al programma utente — PGM 00 exits library mode and returns to the user program
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
    // (3 cifre assolute, oppure etichetta diretta A-E / A'-E' come per GTO/SBR) — (3 absolute digits, or a direct label A-E / A'-E' as for GTO/SBR)
    if (dsz_phase == 1) {
        int d = keycode_to_digit(kc);
        if (d >= 0) {
            dsz_reg_val = dsz_reg_val * 10 + d;
            dsz_reg_digits++;
            if (dsz_reg_digits >= 2) dsz_phase = 2;
            return;
        }
        // tasto non numerico: annulla l'istruzione DSZ incompleta — non-numeric key: cancels the incomplete DSZ instruction
        dsz_phase = 0; dsz_reg_val = 0; dsz_reg_digits = 0;
    } else if (dsz_phase == 2) {
        if (kc == KC_2ND) { cpu->pending_2nd = !cpu->pending_2nd; return; }
        if (kc == KC_INV) { cpu->flags.inv = !cpu->flags.inv; return; }
        if (cpu->pending_2nd) { kc = keycode_2nd(kc); cpu->pending_2nd = false; }

        int d = keycode_to_digit(kc);

        // Se non è un numero, trattalo come un'Etichetta — If it is not a number, treat it as a Label
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

// ── Registro in attesa (STO/RCL/GTO/SBR/SUM/EXC/Prod/StFlg/IfFlg) — Pending reg (STO/RCL/GTO/SBR/SUM/EXC/Prod/StFlg/IfFlg) ──
    if (cpu->pending_reg != PENDING_REG_NONE) {
        // Protezione: i tasti modificatori non devono annullare l'operazione in corso — Protection: modifier keys must not cancel the current operation
        if (kc == KC_2ND) { cpu->pending_2nd = !cpu->pending_2nd; return; }
        if (kc == KC_INV) { cpu->flags.inv = !cpu->flags.inv; return; }
        
        // Applica il prefisso 2nd se premuto precedentemente (es. YX diventa IND) — Apply the 2nd prefix if pressed earlier (e.g. YX becomes IND)
        if (cpu->pending_2nd) { kc = keycode_2nd(kc); cpu->pending_2nd = false; }
        
        // Gestione puntatore Indiretto (IND) — Indirect pointer handling (IND)
        if (kc == KC_IND) { pending_indirect = true; return; }

        int d = keycode_to_digit(kc);

        // GTO (3) e SBR (4) accettano QUALSIASI tasto non numerico come Etichetta — GTO (3) and SBR (4) accept ANY non-numeric key as a Label
        if ((cpu->pending_reg == PENDING_REG_GTO || cpu->pending_reg == PENDING_REG_SBR) && d < 0) {
            // Stesso attivatore già presente per il tasto etichetta — Same trigger already present for the label key
            // diretto (A..E/A'..E'): se un programma è stato appena — direct (A..E/A'..E'): if a program has just been
            // designato con "2nd Pgm mm" e non ancora scaricato con — designated with "2nd Pgm mm" and not yet downloaded with
            // Op 09, il primo GTO/SBR-con-etichetta attiva l'esecuzione — Op 09, the first GTO/SBR-with-label activates the as-is
            // "as-is" dalla ROM del modulo — mancava qui, per questo
            // "SBR =" (o qualunque GTO/SBR con etichetta) restava nello — "SBR =" (or any GTO/SBR with a label) remained in the
            // spazio utente (vuoto o incoerente) invece che nel modulo. — user space (empty or inconsistent) instead of in the module.
            if (lib_page_selected) {
                rebuild_lib_labels(lib_scope_addr, lib_scope_len);
                showing_lib_prog = true;
                lib_page_selected = false;
                memset(lib_ram, 0, sizeof(lib_ram));
                Serial.printf("[LIB] Esecuzione as-is dalla ROM del modulo (programma %02d)\n", lib_selected_page);
            }
            uint16_t addr = find_label(cpu, kc);
            if (addr != 0xFFFF) { // Etichetta trovata — Label found
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
                cpu->flags.error = true; // Errore: lampeggio se l'etichetta non esiste — Error: flashes if the label does not exist
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
                target_digits = 3;  // indirizzo a 3 cifre (000–999) — address is 3 digits (000–999)
            } else if (act >= PENDING_REG_STO && act <= PENDING_REG_SUM) {
                target_digits = 2;  // numero di registro a 2 cifre (00–99) — register number is 2 digits (00–99)
            } else {
                target_digits = 1;  // numero di flag a 1 cifra (0–9) — flag number is 1 digit (0–9)
            }
            if (cpu->pending_digits >= target_digits) {
                uint16_t reg = pending_value;
                uint8_t action = cpu->pending_reg;
                // GTO/SBR con indirizzo assoluto a 3 cifre: se un programma libreria — GTO/SBR with absolute 3-digit address: if a library program
                // è stato designato con "2nd Pgm mm" (ma non ancora scaricato via — was designated with "2nd Pgm mm" (but not yet downloaded via
                // Op 09), attiva l'esecuzione as-is dalla ROM del modulo. — Op 09), activate as-is execution from the module ROM.
                // Gestisce anche il passaggio tra programmi libreria a metà volo. — Also handles switching between library programs in mid-flight.
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

    // ── Modalità LRN — LRN MODE ────────────────────────────────────────────
    if (cpu->flags.lrn) {
        // In modalità LRN, il 2nd è gestito traducendo il tasto successivo — In LRN mode, 2nd is handled by translating the next key
        // e memorizzando direttamente il codice tradotto. — and storing the translated code directly.
        if (cpu->pending_2nd && kc != KC_2ND) {
            kc = keycode_2nd(kc);
            cpu->pending_2nd = false;
        }
        // In LRN mode, if INV was pressed, store the INV state with this key — In modalità LRN, se INV è stato premuto, memorizza lo stato INV con questo tasto
        if (inv_pending) {
            // Store key with INV prefix: use high bit or special encoding — Memorizza il tasto con prefisso INV: usa il bit alto o una codifica speciale
            // For simplicity, we store KC_INV (22) followed by the keycode — Per semplicità, memorizziamo KC_INV (22) seguito dal keycode
            // This is a 2-byte instruction in the program memory — Questa è un'istruzione a 2 byte nella memoria di programma
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
            // Su hardware reale, STO/RCL/SUM/EXC/Prod/GTO seguiti — On real hardware, STO/RCL/SUM/EXC/Prod/GTO followed
            // immediatamente da "2nd Ind" collassano in UN SOLO byte — immediately by "2nd Ind" collapse into A SINGLE byte
			// combinato (es. 72, 73, 74, 63, 64, 83). — combined (e.g. 72, 73, 74, 63, 64, 83).
            if (cpu->prog_pc > 0 && cpu->prog_pc <= cpu->prog_len) {
                uint8_t *last = &cpu->prog[cpu->prog_pc - 1];
                switch (*last) {
                    case KC_STO:  *last = KC_STO_IND;  return;
                    case KC_RCL:  *last = KC_RCL_IND;  return;
                    case KC_SUM:  *last = KC_SUM_IND;  return;
                    case KC_EXC:  *last = KC_EXC_IND;  return;
                    case KC_PROD: *last = KC_PROD_IND; return;
                    case KC_GTO:  *last = KC_GTO_IND;  return;
                    default: break;   // non fondibile: registra Ind "grezzo" sotto — not fusible: store raw Ind below
                }
            }
            prog_store_step(cpu, kc);
            return;
        }

            default: {
                uint16_t store_addr = cpu->prog_pc;
                prog_store_step(cpu, kc);
                
                // Registrazione etichetta — Label registration
                if (store_addr >= 1 && store_addr < PROG_SIZE &&
                    cpu->prog[store_addr - 1] == KC_LBL) {
                    
                    int idx = label_index_for_key(kc);
                    
                    if (idx >= 0) {
                        // Rimuove l'indirizzo precedente se sovrascritto — Remove the previous address if overwritten
                        for (int j = 0; j < 10; j++) {
                            if (custom_label_pc[j] == store_addr - 1)
                                custom_label_pc[j] = 0xFFFF; // Nota: usa lo stesso flag di "vuoto" usato in rebuild_labels (0xFFFF anziché 0) — Note: uses the same "empty" flag as rebuild_labels (0xFFFF instead of 0)
                        }
                        custom_label_pc[idx] = store_addr - 1;
                    }
                }
                return;
            }
        }
    }

    // ── Mapping 2nd (modalità RUN) — 2nd mapping (RUN mode) ────────────────────────────
    if (cpu->pending_2nd && kc != KC_2ND) {
        kc = keycode_2nd(kc);
        cpu->pending_2nd = false;
    }

    // ── Tasti speciali — Special keys ──────────────────────────────────────
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
			cpu->flags.error = false; // Aggiungi questo per sbloccare il lampeggio — Add this to unlock the blinking
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
                    // Esce dalla modalità EE, mantiene la mantissa — Exit EE mode, keep mantissa
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
            // Se si entra in LRN mentre un'istruzione interattiva — If you enter LRN while an interactive
            // multi-tasto è a metà (STO/RCL/GTO/DSZ/Op/Fix non ancora — multi-key instruction is mid-way (STO/RCL/GTO/DSZ/Op/Fix not yet
            // completata), quello stato residuo intercetterebbe i — complete), that residual state would intercept the
            // tasti successivi PRIMA del blocco LRN (i controlli — next keys BEFORE the LRN block (the checks
            // pending_reg/dsz_phase/op_pending/fix_pending girano — pending_reg/dsz_phase/op_pending/fix_pending run
            // prima del "if (cpu->flags.lrn)"), facendo sembrare che — before the "if (cpu->flags.lrn)"), making it seem like
            // la digitazione in LRN non funzioni più. Si azzera tutto — typing in LRN no longer works. Everything is reset
            // per partire puliti, esattamente come fa già CLR altrove. — to start clean, exactly as CLR already does elsewhere.
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
            /* R/S mette solo in pausa/riprende da dove si era interrotta — R/S only pauses/resumes from where it stopped
             * l'esecuzione (comportamento reale) — non riavvolge il PC,
             * quello e' compito di RST. — that is RST's job. */
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
            // RST esce dalla modalità libreria e torna al programma utente — RST exits library mode and returns to the user program
            // (passo 000), cosí l'utente puo' premere un tasto etichetta — (step 000), so the user can press a label key
            // per eseguire il proprio programma invece di quello della ROM. — to run their own program instead of the ROM one.
            // Se vuole ri-usare la libreria, deve rifare 2nd Pgm mm. — If they want to reuse the library, they must redo 2nd Pgm mm.
            showing_lib_prog = false;
            lib_page_selected = false;
			lib_selected_page = 0;
            cpu->prog_pc = 0;
            cpu->sp = 0;
            cpu->flags.run = false;
            for (int i = 0; i < 10; i++) user_flags[i] = false;
            return;
    }

    // ── Cifre — Digits ────────────────────────────────────────────
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
                    int ee_sign_offset = (int)(ep - input_buf) + 1;  // posizione del segno +/- — position of the +/- sign
                    int ee_d0 = ee_sign_offset + 1;                   // prima cifra exponent — first exponent digit
                    int ee_d1 = ee_sign_offset + 2;                   // seconda cifra exponent — second exponent digit
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

    // Tutti i tasti non numerici attivano stack_lift_enabled per impostazione predefinita — All non-digit keys set stack_lift_enabled by default
    cpu->stack_lift_enabled = true;

    // ── Tutti gli altri tasti — All other keys ─────────────────────────────────────
    switch (kc) {
        case KC_DOT:
            if (input_has_ee) {
                // Il punto non è consentito nell'inserimento dell'esponente — ignora — Dot is not allowed in exponent entry — ignore
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
                // Disattiva EE: rimuove "e±xx" dal buffer, mantiene la mantissa — Toggle EE off: remove "e±xx" from buffer, keep mantissa
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
            // Se il buffer è vuoto, inizia con "1e+00" — If the buffer is empty, start with "1e+00"
            if (input_len == 0) {
                input_buf[input_len++] = '1';
            }
            // Normalizza la mantissa a "d.ddddddd" (una sola cifra prima — Normalize the mantissa to "d.ddddddd" (a single digit before
            // del punto) se non è già stato inserito un punto decimale. — the decimal point) if a decimal point has not been entered yet.
            // Senza questo passaggio, un valore digitato come "88888888" — Without this step, a value typed like "88888888"
            // + EE + "88" verrebbe salvato come 88888888e-88 (cioè — + EE + "88" would be saved as 88888888e-88 (i.e.
            // 8.8888888e-81, 7 ordini di grandezza fuori da quanto — 8.8888888e-81, 7 orders of magnitude off from what was
            // digitato), e l'esponente mostrato "salterebbe" non appena — typed), and the shown exponent would "jump" as soon as
            // si esce dalla modalità input. — one exits input mode.
            if (!input_has_dot) {
                int start = (input_buf[0] == '-') ? 1 : 0;
                int ndigits = input_len - start;
                /* Limita a 7 cifre significative: stessa precisione usata — Limit to 7 significant digits: the same precision used
                 * per la notazione scientifica in "Result mode" (%.7g). — for scientific notation in "Result mode" (%.7g).
                 * Con sign(1)+cifra(1)+punto(1)+decimali(6)=9 char e — With sign(1)+digit(1)+dot(1)+decimals(6)=9 chars and
                 * esponente su 3 char ("-88"), il totale sta esattamente — an exponent on 3 chars ("-88"), the total fits exactly
                 * nei 12 caratteri del display, senza overflow. — in the 12 display characters, without overflow. */
                const int MAX_SIG_DIGITS = 7;
                if (ndigits > MAX_SIG_DIGITS) {
                    input_len = start + MAX_SIG_DIGITS;
                    input_buf[input_len] = '\0';
                    ndigits = MAX_SIG_DIGITS;
                }
                if (ndigits > 1) {
                    memmove(&input_buf[start + 2], &input_buf[start + 1],
                            input_len - start);      // include il '\0' — includes the '\0'
                    input_buf[start + 1] = '.';
                    input_len++;
                    input_has_dot = true;
                }
            }
            // Controlla overflow buffer (serve spazio per "e+00" = 5 chars) — Check buffer overflow (room needed for "e+00" = 5 chars)
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
                // Troppi livelli di parentesi aperti: sul TI-59 reale — Too many open paren levels: on the real TI-59
                // questo fa lampeggiare il display (errore), non deve — this makes the display blink (error), it must not
                // scrivere fuori dai limiti di hir_paren_base[]. — write out of the bounds of hir_paren_base[].
                cpu->flags.error = true;
                return;
            }
            hir_push(cpu);
            hir_paren_base[paren_depth] = hir_sp;   // segna la profondità DOPO il push — mark depth AFTER the push
            paren_depth++;
            cpu->pending_op = PENDING_OP_NONE;
            input_clear();
            return;

        case KC_RPAR:
            input_commit(cpu);
            exec_pending(cpu);                              // valuta l'op pendente dentro le parentesi — evaluate pending op inside parens
            if (paren_depth <= 0) {
                // ')' senza una '(' corrispondente: errore, non — ')' without a matching '(': error, do not
                // decrementare sotto zero (leggerebbe hir_paren_base[] — decrement below zero (it would read hir_paren_base[]
                // con indice negativo). — with a negative index).
                cpu->flags.error = true;
                input_clear();
                format_display(cpu);
                return;
            }
            paren_depth--;                                  // pop del livello di annidamento — pop nesting level
            // Esegue i rinvii di precedenza aggiunti dentro questo livello di parentesi — Execute precedence deferrals added inside this paren level
            while (hir_sp > hir_paren_base[paren_depth]) {
                hir_pop(cpu);
            }
            // Estrae la voce '(': ripristina pending_op/operand_x esterni SENZA eseguire — Pop the '(' entry: restore outer pending_op/operand_x WITHOUT executing
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
            // Precedenza AOS: ×/÷ legano più forte di +/− — AOS precedence: ×/÷ bind tighter than +/−
            if (kc == KC_MUL || kc == KC_DIV) {
                if (cpu->pending_op == PENDING_OP_ADD || cpu->pending_op == PENDING_OP_SUB) {
                    hir_push(cpu);                          // rinvia il +/− pendente — defer pending +/−
                } else {
                    exec_pending(cpu);                      // stessa/precedenza superiore: valuta ora — same/higher precedence: evaluate now
                }
            } else {
                // +/− : precedenza inferiore rispetto a ×/÷/yˣ — +/− : lower precedence than ×/÷/yˣ
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
            // yˣ ha precedenza maggiore di +,−,×,÷ sul TI-59 reale (AOS). — yˣ has higher precedence than +,−,×,÷ on the real TI-59 (AOS).
            // Rimanda qualsiasi op pendente a precedenza inferiore tramite lo stack HIR. — Defer any lower-precedence pending op via the HIR stack.
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
            bcd_copy(&cpu->reg[REG_A], &cpu->reg[REG_T]);   // A <-> T — scambia A con T
            bcd_copy(&cpu->reg[REG_T], &tmp);
            display_trailing_dp = false; format_display(cpu); return;
        }

        case KC_STO:
            input_commit(cpu); cpu->pending_reg = PENDING_REG_STO; cpu->pending_digits = 0; pending_value = 0; return;
        case KC_RCL:
            input_commit(cpu); cpu->pending_reg = PENDING_REG_RCL; cpu->pending_digits = 0; pending_value = 0; return;
        case KC_SUM:
            input_commit(cpu); cpu->pending_reg = PENDING_REG_SUM; cpu->pending_digits = 0; pending_value = 0; return;

        // ── STO/RCL/SUM/EXC/Prod/GTO indiretti (codice combinato) — Indirect STO/RCL/SUM/EXC/Prod/GTO (combined code) ──
        // Sulla TI-59 reale "STO 2nd Ind" ecc. generano UN SOLO byte — On the real TI-59 "STO 2nd Ind" etc. generate A SINGLE byte
        // di programma (72/73/74/63/64/83), non due byte separati — of program (72/73/74/63/64/83), not two separate bytes
        // come STO(42)+Ind(40). Prima di questo fix, un dump di — like STO(42)+Ind(40). Before this fix, a real card dump
        // scheda reale con questi byte veniva ignorato silenziosamente — with these bytes was silently ignored
        // (nessun case corrispondente) e i 2 byte di indirizzo che li — (no matching case) and the 2 address bytes that
        // seguivano venivano interpretati come cifre digitate a caso. — followed were interpreted as randomly typed digits.
        // L'indirizzamento indiretto stesso (leggere il registro — The indirect addressing itself (reading the pointer
        // puntatore ed effettuare l'operazione sul registro puntato) — register and performing the operation on the pointed register)
        // è già implementato più sopra: qui basta impostare lo stesso — is already implemented above: here it's enough to set the same
        // stato (pending_reg + pending_indirect) che produce il tasto — state (pending_reg + pending_indirect) produced by the base key
        // base, per riusare quella logica. — to reuse that logic.
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
            // Richiamo programma da modulo libreria: Pgm + 2 cifre. — Recall program from library module: Pgm + 2 digits.
            lib_page_pending = true; lib_page_digits = 0; lib_page_val = 0;
            return;

        case KC_SBR:
            // Comportamento normale di SBR: imposta lo stato della CPU per — Normal SBR behavior: sets the CPU state to
            // attendere le cifre dell'indirizzo o il tasto dell'etichetta. — wait for the address digits or the label key.
            // INV+SBR (RET) è già gestito dall'handler INV nel blocco — INV+SBR (RET) is already handled by the INV handler in the block
            // all'inizio di process_keycode e non arriva mai qui. — at the start of process_keycode and never gets here.
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

            // Se un programma è stato appena designato con "2nd Pgm mm" — If a program has just been designated with "2nd Pgm mm"
            // (e non ancora scaricato con Op 09), il primo tasto — (and not yet downloaded with Op 09), the first
            // etichetta attiva l'esecuzione "as-is" direttamente dalla — label key activates the "as-is" execution directly from the
            // ROM del modulo: niente copie, registri dati separati — module ROM: no copies, data registers separate
            // dall'utente (v. active_ram_bank/lib_ram), etichette — from the user's (see active_ram_bank/lib_ram), labels
            // ricostruite dando priorità a quelle del programma — rebuilt giving priority to those of the designated
            // designato (rebuild_lib_labels). Funziona anche se già in — program (rebuild_lib_labels). It also works if already in
            // modalità as-is (passaggio a un altro programma libreria). — as-is mode (switching to another library program).
            if (lib_page_selected) {
                rebuild_lib_labels(lib_scope_addr, lib_scope_len);
                showing_lib_prog = true;
                lib_page_selected = false;
                // Azzera i registri dati della libreria per un'esecuzione pulita — Clear library data registers for fresh execution
                memset(lib_ram, 0, sizeof(lib_ram));
                Serial.printf("[LIB] Esecuzione as-is dalla ROM del modulo (programma %02d)\n", lib_selected_page);
            }

            uint16_t addr = showing_lib_prog ? lib_custom_label_pc[idx] : custom_label_pc[idx];
            uint16_t len  = exec_prog_len(cpu);

            // Se la label non è stata trovata (0xFFFF), entra in stato di errore (lampeggio) — If the label was not found (0xFFFF), enter error state (blinking)
            // NOTA: lib_custom_label_pc memorizza indirizzi ASSOLUTI nella ROM, quindi — NOTE: lib_custom_label_pc stores ABSOLUTE addresses in the ROM, so
            // il limite deve essere lib_scope_addr + lib_scope_len, non solo len. — the limit must be lib_scope_addr + lib_scope_len, not just len.
            if (addr == 0xFFFF || (showing_lib_prog ? (addr >= lib_scope_addr + len) : (addr >= len))) {
                cpu->flags.error = true;
                return;
            }
            
            // ═══════════════════════════════════════════════════════
            // Chiamata a subroutine tramite etichetta "nuda" durante — Subroutine call via a "bare" label during
            // l'esecuzione di un programma (cpu->flags.run == true): — program execution (cpu->flags.run == true):
            // salva l'indirizzo di ritorno sullo stack. — saves the return address on the stack.
            // Se invece è una pressione da tastiera a freddo (nessun — If instead it is a cold keyboard press (no
            // programma in esecuzione), NON si pusha nulla — lo stack — program running), NOTHING is pushed — the stack
            // rimane vuoto, e il successivo RET si comporta come R/S — stays empty, and the following RET behaves like R/S
            // fermando l'esecuzione ma lasciando montato il modulo — stopping execution but leaving the library module
            // libreria (showing_lib_prog), così l'utente può premere — mounted (showing_lib_prog), so the user can press
            // altri tasti etichetta per proseguire. — other label keys to continue.
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
            
            // Avvia l'esecuzione spostando il Program Counter all'indirizzo della Label — Start execution by moving the Program Counter to the Label's address
            cpu->prog_pc = addr; 
            cpu->flags.run = true; 
            cpu->flags.idle = false;
            return;
        }
		
        case KC_IND:
            // IND "nudo" (2nd yˣ) senza un'operazione registro pendente è un — A "bare" IND (2nd yˣ) with no pending register operation is a
            // no-op, come sull'hardware: qui si arriva SOLO quando — no-op, as on the hardware: here you arrive ONLY when
            // pending_reg == NONE (con un pending_reg attivo, KC_IND è già — pending_reg == NONE (with an active pending_reg, KC_IND is already
            // intercettato nel blocco "Pending reg" sopra). Prima questo case — intercepted in the "Pending reg" block above). Before, this case
            // setta pending_indirect = true, che poi CONTAMINAVA — set pending_indirect = true, which then CONTAMINATED
            // l'operazione successiva: un "STO 00" premuto dopo un IND — the next operation: a "STO 00" pressed after an isolated
            // isolato diventava un "STO IND" e scriveva nel registro — IND became a "STO IND" and wrote to the pointed
            // puntato (bcd_to_int_reg(&bank[reg])) invece che in R00. — register (bcd_to_int_reg(&bank[reg])) instead of R00.
            return;

        case KC_LBL:
            return;

        case KC_STFL:
            cpu->pending_reg = PENDING_REG_STFL; cpu->pending_digits = 0; pending_value = 0; return;
        case KC_IFFL:
            cpu->pending_reg = PENDING_REG_IFFL; cpu->pending_digits = 0; pending_value = 0; return;

        case KC_CMS: {
            // Deve azzerare il bank ATTIVO: lib_ram quando si esegue un — Must clear the ACTIVE bank: lib_ram when executing a library
            // programma libreria, cpu->ram altrimenti. Usare direttamente cpu->ram — program, cpu->ram otherwise. Using cpu->ram directly would
            // corromperebbe i registri dati dell'utente durante l'esecuzione lib-ROM. — corrupt the user's data registers during lib-ROM execution.
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
            // Pausa NON bloccante: trattiene l'esecuzione per 500ms (come — Non-blocking pause: holds execution for 500ms (as
            // l'hardware) senza vTaskDelay — il gate è in tms1500_step. — the hardware) without vTaskDelay — the gate is in tms1500_step.
            cpu->flags.pause = true;
            pause_until_ms = millis() + 500;
            return;

        case KC_DSZ:
            dsz_phase = 1; dsz_reg_val = 0; dsz_reg_digits = 0;
            dsz_addr_val = 0; dsz_addr_digits = 0;
            return;

        // X=T e X≥T sono istruzioni di salto condizionato SOLO — X=T and X≥T are conditional jump instructions that are ONLY
        // significative dentro un programma (vedi exec_program_step, — meaningful inside a program (see exec_program_step,
        // che legge l'etichetta/indirizzo di destinazione dalla ROM): — which reads the destination label/address from the ROM):
        // premute direttamente da tastiera non hanno un target a cui — pressed directly from the keyboard they have no target to
        // saltare, quindi non hanno effetto — come sul TI-59 reale. — jump to, so they have no effect — as on the real TI-59.
        case KC_XEQ_T:
        case KC_XGE_T:
            return;

        // NOTA: qui non serve (e sarebbe irraggiungibile) un ramo — NOTE: here a branch is not needed (and would be unreachable)
        // "if (cpu->flags.inv)": l'INV su questi due tasti è già — "if (cpu->flags.inv)": the INV on these two keys is already
        // intercettato più sopra, in cima a process_keycode() (blocco — intercepted above, at the top of process_keycode() (block
        // "if (cpu->flags.inv && kc != KC_INV)"), che per KC_SIGP e — "if (cpu->flags.inv && kc != KC_INV)"), which for KC_SIGP and
        // KC_XBAR chiama rispettivamente stat_sigma_minus()/ — KC_XBAR calls stat_sigma_minus()/respectively
        // stat_mean_y() e fa return PRIMA di arrivare qui. Quando — stat_mean_y() and returns BEFORE getting here. When
        // l'esecuzione raggiunge questo switch, cpu->flags.inv è — execution reaches this switch, cpu->flags.inv is
        // quindi sempre già false — un ramo INV qui sarebbe stato — therefore always already false — an INV branch here would have been
        // codice morto (prima c'era, duplicato e mai eseguibile). — dead code (there used to be one, duplicated and never executable).
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
                fix_pending = true;  /* In attesa della cifra 0-9 — Wait for digit 0-9 */
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
            // Prima elencava solo byte esadecimali grezzi in righe da — Previously it listed only raw hexadecimal bytes in rows of
            // 16, come /api/prog e la lettura scheda facevano prima — 16, as /api/prog and the card reader did before
            // dei rispettivi fix — ora stesso formato "passo | hex | — their respective fixes — now the same "step | hex |
            // comando" ovunque, per coerenza in tutte le visualizzazioni. — command" format everywhere, for consistency in all views.
            // Buffer sull'heap: con mnemonici invece dei soli byte, — Buffer on the heap: with mnemonics instead of raw bytes,
            // un programma vicino ai 960 passi può arrivare a ~15KB, — a program near 960 steps can reach ~15KB,
            // troppo per uno stack array su un task ESP32. — too much for a stack array on an ESP32 task.
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
            // Quando si esegue as-is da una ROM libreria, CP cancella i registri — When executing as-is from a library ROM, CP erases the library
            // dati della libreria (lib_ram) ma NON la memoria del programma utente — — data registers (lib_ram) but NOT the user program memory — the
            // il programma dell'utente in cpu->prog deve sopravvivere. Il programma — user's program in cpu->prog must survive. The library program
            // libreria stesso è di sola lettura dalla ROM e comunque non può essere toccato. — itself is read-only from the ROM and cannot be affected anyway.
            if (showing_lib_prog) {
                memset(lib_ram, 0, sizeof(lib_ram));
                for (int i = 0; i < 10; i++) user_flags[i] = false;
                // CP (2nd CE) sul TI-59 reale azzera SEMPRE il registro T — CP (2nd CE) on the real TI-59 ALWAYS clears the T register
                // ("CP clears the T register", firmware ufficiale TMC0541, — ("CP clears the T register", official TMC0541 firmware,
                // Fast Mode docs: CP a step 313 azzera T per il test x=t a — Fast Mode docs: CP at step 313 clears T for the x=t test at
                // step 316). ML-18 (e molti altri programmi Master Library) — step 316). ML-18 (and many other Master Library programs)
                // usa CP nel tasto di INIT (E' = CP FIX 2) proprio per — uses CP in the INIT key (E' = CP FIX 2) precisely to
                // mettere T=0, così premendo "0" + tasto A/B/C/D il test — set T=0, so pressing "0" + key A/B/C/D the
                // x=t innesca il calcolo della variabile mancante invece — x=t test triggers the calculation of the missing variable instead
                // di memorizzare lo zero. Senza questo, T restava un — of storing the zero. Without this, T remained a
                // valore spazzatura e ML-18 si fermava mostrando 0.00. — garbage value and ML-18 stopped showing 0.00.
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
            // CP (2nd CE) azzera anche il registro T (comportamento TI-59 — CP (2nd CE) also clears the T register (TI-59 behavior
            // reale: "CP clears the T register" — vedi commento nel ramo
            // libreria qui sopra). — library branch above).
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
            // Azzera (2nd CLR): azzera display e op pendenti, ma NON il programma — Clear (2nd CLR): clears display, pending ops, but NOT program
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

// Helper in avanti — Forward helpers
static bool read_2digit(TMS1500_State *cpu, uint8_t *out);
static bool read_3digit(TMS1500_State *cpu, uint16_t *out);
static bool read_label(TMS1500_State *cpu, uint8_t *out);
static bool read_next(TMS1500_State *cpu, uint8_t *out);

static void exec_program_step(TMS1500_State *cpu) {
    uint16_t plen = exec_prog_len(cpu);
    if (!cpu->flags.run || plen == 0) return;

    // Legge opcode e decodifica istruzione completa — Reads the opcode and decodes the full instruction
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

    // Per programmi libreria: arresta se si raggiunge la fine della ROM fisica — For library programs: stop if the end of the physical ROM is reached
    // (NON del programma designato — la ROM condivide subroutine tra programmi — (NOT of the designated program — the ROM shares subroutines between programs
    // diversi tramite SBR/GTO a etichette fuori dal proprio scopo, quindi il — via SBR/GTO to labels outside their own scope, so the
    // limite va sulla ROM intera, non su lib_scope_addr+lib_scope_len). — limit must apply to the whole ROM, not lib_scope_addr+lib_scope_len).
    if (showing_lib_prog) {
        const LibraryModule *mod = library_get_active();
        if (mod && *pc >= mod->rom_size) {
            cpu->flags.run = false;
            cpu->flags.idle = true;
            showing_lib_prog = false;
            for (int i = 0; i < 10; i++) lib_custom_label_pc[i] = 0xFFFF;
            // NON azzerare lib_selected_page qui: il programma si è — Do NOT clear lib_selected_page here: the program has
            // fermato ma il modulo è ancora attivo — l'overlay deve — stopped but the module is still active — the overlay must
            // restare in vista (etichetta fisica) finché l'utente non — stay in view (physical label) until the user
            // esce esplicitamente (RST / 2nd Pgm 00 / cambio modulo). — explicitly exits (RST / 2nd Pgm 00 / module change).
			lib_page_selected = false;
            return;
        }
    }

    // LBL xx (2 byte) - salta l'etichetta e il parametro — skip label and parameter
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

    // Prefisso INV (l'istruzione successiva viene eseguita con il flag INV) — INV prefix (next instruction executed with INV flag)
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
        // Rientra per eseguire il prossimo opcode con INV impostato — Re-enter to execute the next opcode with INV set
        exec_program_step(cpu);
        cpu->flags.inv = false;
        return;
    }

    // Avanzamento PC predefinito (sarà sovrascritto da salti/subroutine) — Default PC advance (will be overridden by jumps/subroutines)
    *pc += 1;

    // R/S - ferma il programma (mantiene attiva la modalità lib così la prossima pressione etichetta colpisce ancora la libreria) — R/S - stop program (keep lib mode active so next label press still targets library)
    if (opcode == KC_RS) {
        cpu->flags.run = false;
        cpu->flags.idle = true;
        return;
    }

    // RTN (92) - ritorno da subroutine — return from subroutine
    if (opcode == KC_RETURN) {
        if (cpu->sp > 0) {
            uint8_t old_sp = cpu->sp;
            cpu->sp--;
            *pc = cpu->stack[cpu->sp];
            cpu->pending_op = cpu->stack_pending_op[cpu->sp]; bcd_copy(&cpu->operand_x, &cpu->stack_operand_x[cpu->sp]);
            Serial.printf("[STK] RET sp=%u->%u pop=%u in_rom=%u\n",
                (unsigned)old_sp, (unsigned)cpu->sp, (unsigned)cpu->stack[cpu->sp],
                (unsigned)cpu->stack_in_rom[cpu->sp]);
            // Ripristina SEMPRE lo stato del chiamante (vedi commento — Always restore the caller's state (see the twin comment
            // gemello nel ramo interattivo INV+SBR sopra in process_keycode). — in the interactive INV+SBR branch above in process_keycode).
            showing_lib_prog = cpu->stack_in_rom[cpu->sp];
            if (showing_lib_prog) {
                lib_scope_addr = cpu->stack_rom_base[cpu->sp];
                lib_scope_len = cpu->stack_rom_len[cpu->sp];
            }
            cpu->stack_in_rom[cpu->sp] = false;
            // Safety net: se lo stack ora è vuoto e pc è fuori dal corrente — Safety net: if stack is now empty and pc is outside the current
            // scope libreria, il programma non ha un indirizzo valido dove continuare. — library scope, the program has no valid address to continue at.
            // Ferma l'esecuzione ma mantieni la modalità libreria attiva così l'utente può — Stop execution but keep library mode active so the user can
            // premere un altro tasto etichetta per riavviare. NON azzerare — press another label key to restart. Do NOT clear
            // lib_selected_page: l'overlay resta in vista (vedi commento — lib_selected_page: the overlay stays in view (see the twin comment
            // gemello al limite della ROM). — at the ROM limit).
            if (cpu->sp == 0 && showing_lib_prog &&
				(*pc < lib_scope_addr || *pc >= lib_scope_addr + lib_scope_len)) {
				cpu->flags.run = false;
				cpu->flags.idle = true;
				showing_lib_prog = false;
				lib_page_selected = false;
				return;
			}
        } else {
            // Stack vuoto: RTN si comporta come R/S, ferma il programma (mantiene attiva la modalità lib) — Stack empty: RTN acts like R/S, stops program (keep lib mode active)
            Serial.printf("[STK] RET empty stack — stop\n");
            cpu->flags.run = false;
            cpu->flags.idle = true;
        }
        return;
    }

    // Macro helper — Helper macros
    #define READ2(d) read_2digit(cpu, d)
    #define READ3(d) read_3digit(cpu, d)
    #define READL(d) read_label(cpu, d)
    #define NEXT(c)  read_next(cpu, c)

    // DSZ nn LLL — decrementa e salta se zero: registro (nn) poi indirizzo (LLL)
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
    // prima di questa correzione, IFF non aveva un case dedicato qui — before this fix, IFF had no dedicated case here
    // e cadeva nel ramo generico "delega a process_keycode", che usa — and fell into the generic "delegate to process_keycode" branch, which uses
    // la macchina a stati pending_reg pensata per la digitazione da — the pending_reg state machine designed for keyboard
    // tastiera. Quella macchina consuma UN SOLO byte (la cifra flag) — entry. That machine consumes A SINGLE byte (the flag digit)
    // e poi esegue subito "se flag non impostato, salta la prossima — and then immediately executes "if flag not set, skip the next
    // istruzione" — semantica sbagliata E non consuma il byte target,
    // corrompendo la lettura di ogni istruzione successiva. Confermato — corrupting the reading of every subsequent instruction. Confirmed
    // dal disassemblato ufficiale TMC0541 (es. ML-11 offset 0027: — by the official TMC0541 disassembly (e.g. ML-11 offset 0027:
    // "87 00 97" = IFF 00, target=byte singolo 97, riusato come — "87 00 97" = IFF 00, target=single byte 97, reused as
    // etichetta interna 2nd-Dsz — stesso pattern di GTO/SBR/DSZ).
    // Semantica reale: se il flag n è impostato, salta a LLL — Real semantics: if flag n is set, jump to LLL
    // (etichetta o indirizzo assoluto a 3 cifre); se non impostato, — (label or absolute 3-digit address); if not set,
    // prosegui in sequenza. Non è uno "skip next instruction". — continue in sequence. It is not a "skip next instruction".
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

    // SBR / GTO con label (A-E, A'-E') o indirizzo a 3 cifre — SBR / GTO with label (A-E, A'-E') or 3-digit address
    if (opcode == KC_SBR || opcode == KC_GTO) {
        bool is_sbr = (opcode == KC_SBR);

        // INV + SBR = Return (92). In un programma "INV SBR" è memorizzato — INV + SBR = Return (92). In a program "INV SBR" is stored
        // come [INV, SBR] (2 byte) e il prefisso INV sopra ci ha riportato — as [INV, SBR] (2 bytes) and the INV prefix above brought us
        // qui con cpu->flags.inv = true. Prima di questa correzione il flag — here with cpu->flags.inv = true. Before this fix the flag
        // veniva ignorato e INV+SBR eseguiva una SBR normale (push+jump): — was ignored and INV+SBR executed a normal SBR (push+jump):
        // la subroutine non tornava mai al chiamante. Deve invece fare POP — the subroutine never returned to the caller. It must instead POP
        // dallo stack, esattamente come il RETURN esplicito (KC_RETURN). — from the stack, exactly like the explicit RETURN (KC_RETURN).
        // INV+GTO resta un GTO normale: INV non è definito per GTO. — INV+GTO stays a normal GTO: INV is not defined for GTO.
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
                    // Safety net gemello del RETURN: se lo stack ora è vuoto — Twin safety net of the RETURN: if the stack is now empty
                    // e pc è fuori dallo scope libreria corrente, il chiamante — and pc is outside the current library scope, the caller
                    // non ha un indirizzo valido a cui continuare: ferma. — has no valid address to continue at: stop.
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

        // Se una PGM ha appena prenotato un programma (lib_page_selected), — If a PGM has just reserved a program (lib_page_selected),
        // questa è la SBR/GTO che lo attiva davvero: salva lo scope del — this is the SBR/GTO that really activates it: save the scope of the
        // chiamante (solo per SBR, che si aspetta un ritorno — GTO no) — caller (only for SBR, which expects a return — GTO doesn't)
        // e passa a quello del programma appena designato PRIMA di — and switch to the just-designated program BEFORE
        // risolvere l'etichetta/indirizzo, che vanno cercati nel NUOVO — resolving the label/address, which must be looked up in the NEW
        // scope, non in quello vecchio. — scope, not the old one.
        bool saved_in_rom = showing_lib_prog;
        uint16_t saved_addr = lib_scope_addr, saved_len = lib_scope_len;
        if (lib_page_selected) {
            // L'overlay resta sul programma che l'utente ha scelto (come la — The overlay stays on the program the user chose (like the real
            // card cartacea reale): NON aggiorniamo lib_selected_page qui, — paper card): we do NOT update lib_selected_page here,
            // perché le chiamate interne (Pgm nn) cambiano solo lo scope di — because internal calls (Pgm nn) only change the execution
            // esecuzione, non la selezione a schermo. — scope, not the on-screen selection.
            rebuild_lib_labels(lib_pending_addr, lib_pending_len);
            lib_scope_addr = lib_pending_addr;
            lib_scope_len  = lib_pending_len;
            showing_lib_prog = true;
            lib_page_selected = false;
            // NON azzerare lib_ram qui: i registri servono a passare dati — Do NOT clear lib_ram here: the registers are used to pass data
            // tra chiamante e chiamato per convenzione documentata (vedi — between caller and callee per documented convention (see
            // "Register Contents" di ogni programma nel manuale Master — "Register Contents" of each program in the Master Library
            // Library) — azzerarli romperebbe qualunque programma pensato — manual) — clearing them would break any program designed
            // per essere richiamato come subroutine con dati preimpostati. — to be called as a subroutine with pre-set data.
            Serial.printf("[LIB] (da programma) Attivato programma %02d as-is\n", lib_selected_page);
        }

        uint8_t lbl;
        if (READL(&lbl)) {
            uint16_t target = find_label(cpu, lbl);
            if (target != 0xFFFF) {
                if (is_sbr && cpu->sp < STACK_SIZE - 1) {
                    // Registra SEMPRE lo stato reale del chiamante catturato — Always record the caller's real state captured
                    // sopra (saved_*), non un "false" forzato: se questa SBR — above (saved_*), not a forced "false": if this SBR
                    // avviene già dentro la ROM (chiamata annidata a — happens already inside the ROM (nested call to
                    // un'etichetta interna, senza una nuova PGM appena — an internal label, without a new PGM just
                    // designata), il chiamante ERA in ROM e il RETURN deve — designated), the caller WAS in ROM and the RETURN must
                    // poterlo ripristinare correttamente. — be able to restore it correctly.
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
    // numero programma). NON salta né chiama da sola: si limita a — program number). It does NOT jump or call on its own: it only
    // "prenotare" lib_pending_*; è la SBR/GTO (con etichetta o indirizzo) — "reserves" lib_pending_*; it is the SBR/GTO (with label or address)
    // che segue subito dopo a fare davvero la chiamata, esattamente come — that immediately follows to actually make the call, exactly as
    // richiesto dal formato reale "2nd Pgm mm SBR label" (es. ROM ML-01: — required by the real format "2nd Pgm mm SBR label" (e.g. ROM ML-01:
    // "36 15 71 88" = PGM 15, SBR [DMS] — un'unica sequenza logica di 4
    // byte, non due istruzioni indipendenti). — bytes, not two independent instructions).
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
    // INDIRETTAMENTE da un registro dati (puntatore), non come — INDIRECTLY from a data register (pointer), not as
    // costante letterale a 2 cifre. Confermato dalla ROM reale: usato — a 2-digit literal constant. Confirmed by the real ROM: used
    // 10 volte, sempre nella forma "62 rr" (es. ROM ML-01 offset 0103: — 10 times, always in the form "62 rr" (e.g. ROM ML-01 offset 0103:
    // "62 00" = Pgm Ind, puntatore = registro 00), nel preambolo di — "62 00" = Pgm Ind, pointer = register 00), in the preamble of
    // stampa/trace condiviso da molti dei 25 programmi Master Library — print/trace shared by many of the 25 Master Library programs
    // (v. TI Master Library QRG, "Print Routine": STO 00 mm imposta il — (see TI Master Library QRG, "Print Routine": STO 00 mm sets the
    // numero di programma da tracciare, letto qui indirettamente). — program number to trace, read here indirectly).
    // PRIMA DI QUESTA AGGIUNTA: KC_PGM_IND non aveva alcun case — BEFORE THIS ADDITION: KC_PGM_IND had no case
    // dedicato — cadeva nel fallback verso process_keycode, che lo
    // ignora silenziosamente (default: break) SENZA consumare il byte — silently ignores it (default: break) WITHOUT consuming the next
    // operando successivo. Quel byte veniva quindi riletto come se — operand byte. That byte was then re-read as if
    // fosse una nuova istruzione a sé stante, corrompendo il flusso in — it were a new standalone instruction, corrupting the flow at
    // tutti e 10 i punti della ROM che usano questo idioma. — all 10 points in the ROM that use this idiom.
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

    // STO/RCL/SUM/EXC/PROD rr (registro a 2 cifre) — STO/RCL/SUM/EXC/PROD rr (2-digit register)
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

    // Varianti IND: STO IND, RCL IND, SUM IND, EXC IND, PROD IND, GTO IND — IND variants: STO IND, RCL IND, SUM IND, EXC IND, PROD IND, GTO IND
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

    // NOTA: la codifica dei registri STO/RCL/SUM/EXC/PROD nella ROM — NOTE: the encoding of STO/RCL/SUM/EXC/PROD registers in the library ROM
    // libreria è già gestita correttamente dal blocco sopra tramite — is already handled correctly by the block above via
    // READ2 (a sua volta consapevole di showing_lib_prog): 1 byte con — READ2 (itself aware of showing_lib_prog): 1 byte with
    // il valore diretto del registro (0-99), sempre — confermato contro — the register's direct value (0-99), always — confirmed against
    // il dump reale TMC0541 (es. "RCL 11" = byte singolo "43 11", mai — the real TMC0541 dump (e.g. "RCL 11" = single byte "43 11", never
    // due byte separati "1","1"). Un blocco precedente ipotizzava uno — two separate bytes "1","1"). A previous block hypothesized a
    // schema alternativo a 1-o-2-byte cifra-per-cifra per i registri — 1-or-2-byte digit-by-digit alternative scheme for registers
    // ≥20: rimosso perché era sia irraggiungibile (il blocco sopra — ≥20: removed because it was both unreachable (the block above
    // ritorna sempre per primo) sia basato su un'ipotesi errata. — always returns first) and based on a wrong hypothesis.

    // X=T (67) / X≥T (77) — salto CONDIZIONATO a etichetta o indirizzo a
    // 3 cifre, esattamente come GTO (confermato da decine di occorrenze — 3 digits, exactly like GTO (confirmed by dozens of occurrences
    // nel disassemblato: "67 96", "67 02 28", "77 02 97", ecc. — MAI un
    // byte singolo). NON sono "salta la prossima istruzione": se la — single byte). They are NOT "skip the next instruction": if the
    // condizione è vera si salta al target: altrimenti si prosegue in — condition is true it jumps to the target: otherwise it continues in
    // sequenza dopo l'operando (già consumato da READL/READ3). INV — sequence after the operand (already consumed by READL/READ3). INV
    // inverte la condizione (visto anche nel disassemblato: "22 67 ..."). — inverts the condition (also seen in the disassembly: "22 67 ...").
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
        if (cond) *pc = target;   // altrimenti prosegue in sequenza (pc già avanzato oltre l'operando) — otherwise continues in sequence (pc already advanced past the operand)
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

    // OP nn (2 cifre) — OP nn (2 digits)
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

    // Operazioni matematiche/funzionali a byte singolo: delega a process_keycode — Single-byte math/func ops: delegate to process_keycode
    cpu->stack_lift_enabled = true;
    process_keycode(cpu, opcode);
    return;

    #undef READ2
    #undef READ3
    #undef READL
    #undef NEXT
}

// Helper per la lettura degli operandi di programma — Helpers for reading program operands
static bool read_2digit(TMS1500_State *cpu, uint8_t *out) {
    if (showing_lib_prog) {
        /* La ROM Master Library non incapsula i registri a 2 cifre come — The Master Library ROM does not wrap 2-digit registers as
         * due keycode-cifra separati (come farebbe un programma digitato — two separate keycode-digits (as a keyboard-typed program would
         * a tastiera): li memorizza come UN SOLO byte con il valore — do): it stores them as A SINGLE byte with the direct
         * diretto 0-99 (es. "STO 09" = opcode + un byte di valore 9). — value 0-99 (e.g. "STO 09" = opcode + a byte of value 9). */
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
        /* Indirizzi GTO/SBR/DSZ nella ROM Master Library: due byte, — GTO/SBR/DSZ addresses in the Master Library ROM: two bytes,
         * centinaia (0-9) poi decine+unità impacchettate (00-99), — hundreds (0-9) then packed tens+units (00-99),
         * combinati come centinaia*100 + decine_unita. Non tre — combined as hundreds*100 + tens_units. Not three
         * byte-cifra singoli come nei programmi utente digitati. — single digit-bytes as in typed user programs. */
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
    // Regola vera del TI-59: dopo GTO/SBR/DSZ, un byte 0-9 è l'inizio di — The real TI-59 rule: after GTO/SBR/DSZ, a byte 0-9 is the start of
    // un indirizzo numerico letterale (000-999); QUALSIASI altro tasto è — a literal numeric address (000-999); ANY other key is
    // un'etichetta valida. Non è una whitelist ristretta ai soli 10 tasti — a valid label. It is not a whitelist restricted to the 10 user
    // utente (A-E/A'-E'/"="): la ROM Master Library usa moltissime altre — keys (A-E/A'-E'/"="): the Master Library ROM uses many other
    // etichette "interne" (es. CE, CLR) per le subroutine condivise fra — "internal" labels (e.g. CE, CLR) for subroutines shared among
    // programmi, mai raggiungibili da tastiera ma perfettamente valide — programs, never reachable from the keyboard but perfectly valid
    // come bersaglio di SBR/GTO dentro la ROM stessa. — as targets of SBR/GTO inside the ROM itself.
    if (kc <= 9) return false;
    // Eccezione confermata da fonte primaria (TI Master Library Quick — Exception confirmed by a primary source (TI Master Library Quick
    // Reference Guide, sez. "Programming Notes / Labels"): "Any key on — Reference Guide, sect. "Programming Notes / Labels"): "Any key on
    // the keyboard can be used as label except 2nd, LRN, Ins, Del, SST, — the keyboard can be used as label except 2nd, LRN, Ins, Del, SST,
    // BST, Ind and the numbers 0-9." Questi 6 tasti extra (oltre a 0-9, — BST, Ind and the numbers 0-9." These 6 extra keys (besides 0-9,
    // già escluso sopra) NON sono etichette valide quando il programma — already excluded above) are NOT valid labels when the program
    // gira dalla memoria utente digitata da tastiera. Dentro la ROM — runs from the user memory typed on the keyboard. Inside the
    // Master Library invece questa restrizione non si applica (vedi — Master Library ROM, however, this restriction does not apply (see
    // commento sopra: la ROM riusa questi byte come etichette interne — comment above: the ROM reuses these bytes as internal labels
    // per subroutine condivise, mai raggiungibili da tastiera) — quindi
    // il filtro scatta solo per !showing_lib_prog. — the filter only applies for !showing_lib_prog.
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
// INIT / RESET / STEP — inizializzazione / reset / passo
// ═══════════════════════════════════════════════════════════

// Implementazione di default (no-op): sovrascritta da wifilink.cpp, — Default implementation (no-op): overridden by wifilink.cpp,
// che ha accesso al sottosistema schede (cardemu) e a un CardEmuState — which has access to the card subsystem (cardemu) and a real CardEmuState
// vero. Se questo file viene compilato/linkato da solo (o se il layer — . If this file is compiled/linked alone (or if the external layer
// esterno non fornisce un hook), premere WRITE semplicemente non fa — provides no hook), pressing WRITE simply does
// nulla invece di corrompere qualcosa o fallire in modo silenzioso. — nothing instead of corrupting something or failing silently.
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
    memset(lib_ram, 0, sizeof(lib_ram));   // solo all'accensione, non su RST (v. tms1500_reset) — only at power-on, not on RST (see tms1500_reset)
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
    // PAUSE non bloccante: il programma resta "in pausa" 500ms (il pc è già — Non-blocking PAUSE: the program stays "paused" for 500ms (the pc has already
    // stato avanzato oltre l'istruzione PAUSE da exec_program_step), durante — been advanced past the PAUSE instruction by exec_program_step), during which
    // i quali NON si esegue alcuna istruzione ma i tasti continuano a essere — NO instruction is executed but keys keep being
    // processati sopra — così R/S può fermare un programma in pausa. Se il
    // programma viene fermato o il tempo scade, esci subito dalla pausa. — program is stopped or the time expires, exit the pause immediately.
    if (cpu->flags.pause) {
        if (!cpu->flags.run || (long)(millis() - pause_until_ms) >= 0)
            cpu->flags.pause = false;
        else
            return;
    }
    // Esecuzione programmi pacingata a tempo reale. In modalità moderna — Real-time paced program execution. In modern mode
    // (g_realistic_timing == false) esegue subito, come prima; in Old — (g_realistic_timing == false) it runs immediately, as before; in Old
    // trattiene le istruzioni al ritmo dell'originale × moltiplicatore, — it holds instructions at the original pace × multiplier,
    // massimo un passo per chiamata (così tastiera/web restano reattivi). — at most one step per call (so keyboard/web stay responsive).
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
                // cap anti-deriva: mai più di 1s di istruzioni accumulate, — anti-drift cap: never more than 1s of accumulated instructions,
                // altrimenti un loop lento accumulerebbe un ritardo enorme. — otherwise a slow loop would accumulate an enormous delay.
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
// SERIALIZZAZIONE — SERIALIZATION
// ═══════════════════════════════════════════════════════════

void tms1500_load_prog(TMS1500_State *cpu, const uint8_t *data, uint16_t len) {
    if (len > PROG_SIZE) len = PROG_SIZE;
    memcpy(cpu->prog, data, len); cpu->prog_len = len; cpu->prog_pc = 0;
    showing_lib_prog = false;
    lib_page_selected = false;
    lib_selected_page = 0;   // la selezione ROM si chiude: l'overlay card deve tornare in vista — the ROM selection closes: the card overlay must come back into view
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
// GETTER — accessori (funzioni getter)
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

// ── Helper Display / Programma (definiti qui per soddisfare il linker) — Display / Program helpers (defined here to satisfy linker) ──

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

// Listato completo del modulo libreria attivo: TUTTI i programmi — Full listing of the active library module: ALL programs
// (non solo quello eventualmente "mostrato"), formato a colonne — (not just the possibly "shown" one), formatted in columns
// "numero_programma passo hex comando" — così le subroutine condivise
// tra programmi restano leggibili nel loro contesto reale. Usata sia — between programs remain readable in their real context. Used both
// dal tasto fisico 2nd List (quando si guarda un programma da modulo) — by the physical 2nd List key (when viewing a module program)
// sia dall'endpoint web dei moduli. Alloca con malloc: il chiamante — and by the web module endpoint. Allocates with malloc: the caller
// deve fare free() sul puntatore restituito (nullptr se nessun modulo — must free() the returned pointer (nullptr if no module
// attivo o allocazione fallita). — is active or allocation failed).
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

/* Rimuove gli zeri finali dopo il punto decimale (mantiene il punto — Removes trailing zeros after the decimal point (keeps the point
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
    // posizione a sinistra, come sull'hardware reale durante il — position on the left, as on real hardware during the
    // calcolo di funzioni lente (sqrt, trig, log, y^x...). Vedi — computation of slow functions (sqrt, trig, log, y^x...). See
    // busy_start() per la logica delle due modalità (reale/moderna). — busy_start() for the logic of the two modes (real/modern).
    // ═══════════════════════════════════════════════════════════
    if (busy_active) {
        if ((long)(millis() - busy_until_ms) >= 0) {
            busy_active = false;   // tempo scaduto (autentico in Old, minimo fisso in New): mostra il display normale — time expired (genuine in Old, fixed minimum in New): show the normal display
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

// Nucleo di formattazione del valore vero e proprio (modalità LRN, — Core of the actual value formatting (LRN mode,
// immissione in corso, o risultato normale) — NON tocca mai
// l'indicatore "occupato": quello è puramente cosmetico per il display — the "busy" indicator: that is purely cosmetic for the live
// dal vivo (vedi tms1500_get_display_string sotto) e non deve mai — display (see tms1500_get_display_string below) and must never
// influenzare un lettore interno/programmatico del valore attuale, come — influence an internal/programmatic reader of the current value, like
// PRT che stampa il risultato già calcolato nel registro anche se la — PRT which prints the already computed register result even if the
// finestra cosmetica di "occupato" non è ancora scaduta. — cosmetic "busy" window has not expired yet.
static void format_value_string(const TMS1500_State *cpu, char *buf, unsigned int len) {
    if (!buf || len == 0) return;

	// ═══════════════════════════════════════════════════════════
    // GESTIONE MODALITÀ LRN (LEARN) — LRN MODE HANDLING (LEARN)
    // ═══════════════════════════════════════════════════════════
    if (cpu->flags.lrn) {
        uint16_t passo = cpu->prog_pc; 
        // Legge il comando se siamo entro i limiti della memoria, altrimenti 00 — Reads the command if within memory limits, otherwise 00
        uint8_t comando = (passo < PROG_SIZE) ? cpu->prog[passo] : 0;
        
        char tmp_lrn[16];
        snprintf(tmp_lrn, sizeof(tmp_lrn), "%03d %02d", passo, comando);

        // Allinea a destra su 12 posizioni come il resto dell'interfaccia — Align right to 12 positions like the rest of the interface
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
    // GESTIONE ERRORI E CALCOLO NORMALE — ERROR HANDLING AND NORMAL CALCULATION
    // ═══════════════════════════════════════════════════════════
    // Nota: in errore NON si mostra il testo "Error" (il TI-59 reale non — Note: in error the text "Error" is NOT shown (the real TI-59 does not
    // ha un display testuale: lampeggia il numero risultante). Il — have a text display: it blinks the resulting number). The
    // lampeggio è gestito lato UI (wifilink/display driver) leggendo — blinking is handled on the UI side (wifilink/display driver) by reading
    // cpu->flags.error; qui si prosegue con la normale formattazione. — cpu->flags.error; here we continue with normal formatting.

    char tmp[32];

    /* ── Modalità input: mostra cosa sta digitando l'utente — Input mode: show what the user is typing ── */
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
            /* Se la mantissa inizia con '.', anteponi '0' per la visualizzazione — If mantissa starts with '.', prepend '0' for display */
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

    /* ── Modalità risultato: formatta dal registro BCD — Result mode: format from BCD register ── */
    double val = bcd_to_double(&cpu->reg[REG_A]);
    int sign = (val < 0) ? 1 : 0;
    double aval = fabs(val);

    /* Zero (a meno che FIX/ENG/SCI non sovrascriva il formato) — Zero (unless FIX/ENG/SCI overrides format) */
    if (aval < 1e-99) {
        if (cpu->flags.fix) {
            /* passa alla formattazione FIX — fall through to FIX formatting */
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

    /* ── Modalità FIX: arrotonda a un numero fisso di decimali — FIX mode: round to fixed decimal places ── */
    if (cpu->flags.fix) {
        int fd = cpu->fix_digits;
        if (fd > 8) fd = 8;
        double scale = pow(10.0, fd);
        double rounded = round(val * scale) / scale;
        char fmt[16];
        snprintf(fmt, sizeof(fmt), "%% .%df", fd);
        snprintf(tmp, sizeof(tmp), fmt, rounded);
    }
    /* ── Modalità ENG: notazione ingegneristica (esponente multiplo di 3) — ENG mode: engineering notation (exp multiple of 3) ── */
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
    /* ── Modalità EE / sci: notazione scientifica — EE / sci mode: scientific notation ── */
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
    /* ── Formato automatico: standard se in range, scientifico se fuori — Auto format: standard if in range, scientific if out ── */
    else {
        /* Intervallo di visualizzazione standard: da 0.0000000001 a 9999999999 — Standard display range: 0.0000000001 to 9999999999 */
        if (aval >= 1e-10 && aval < 1e10) {
            /* Formato standard: fino a 10 cifre significative — Standard format: up to 10 significant digits */
            if (aval >= 1.0) {
                int digits = (int)floor(log10(aval)) + 1;
                int decimals = 10 - digits;
                if (decimals < 0) decimals = 0;
                double scale = pow(10.0, decimals);
                double rounded = round(val * scale) / scale;
                /* Costruisce la stringa di formato dinamicamente — Build format string dynamically */
                if (decimals > 0) {
                    char fmt[16];
                    snprintf(fmt, sizeof(fmt), "%% .%df", decimals);
                    snprintf(tmp, sizeof(tmp), fmt, rounded);
                } else {
                    snprintf(tmp, sizeof(tmp), "%.0f", rounded);
                    if (tmp[0] == '-') tmp[0] = '-';
                    else if (strlen(tmp) < 12) {
                        /* Scosta a destra di 1 per lasciare spazio al segno — Shift right by 1 to leave space for sign */
                        memmove(tmp + 1, tmp, strlen(tmp) + 1);
                        tmp[0] = ' ';
                    }
                }
            } else {
                /* Numero piccolo: mostra zeri iniziali dopo la virgola — Small number: show leading zeros after decimal */
                int leading_zeros = (int)fabs(floor(log10(aval)));
                if (leading_zeros > 9) leading_zeros = 9;
                int decimals = 10;  /* massimo 10 cifre decimali — max 10 decimal digits */
                double scale = pow(10.0, decimals);
                double rounded = round(val * scale) / scale;
                char fmt[16];
                snprintf(fmt, sizeof(fmt), "%% .%df", decimals);
                snprintf(tmp, sizeof(tmp), fmt, rounded);
            }
            /* Un risultato "libero" (non FIX) non deve mostrare zeri di — A "free" (non-FIX) result must not show padding zeros up to
             * riempimento fino a 10 cifre: es. 5 -> "5." e non "5.0000000000", — 10 digits: e.g. 5 -> "5." and not "5.0000000000",
             * 5.5 -> "5.5" e non "5.5000000000". — 5.5 -> "5.5" and not "5.5000000000". */
            trim_trailing_zeros(tmp);
        } else {
            /* Fuori range: notazione scientifica — Out of range: scientific notation */
            int exp = (int)floor(log10(aval));
            double mant = aval / pow(10.0, exp);
            if (sign) mant = -mant;
            char exp_sign = (exp < 0) ? '-' : ' ';
            int aexp = abs(exp);
            if (aexp > 99) aexp = 99;
            snprintf(tmp, sizeof(tmp), "%.7g%c%02d", mant, exp_sign, aexp);
        }
    }

    /* ALLINEA a destra su 12 posizioni — Align right to 12 positions */
    char out[13];
    memset(out, ' ', 12);
    out[12] = '\0';
    int tlen = (int)strlen(tmp);
    if (tlen > 12) tlen = 12;
    memcpy(out + 12 - tlen, tmp, tlen);
    strncpy(buf, out, len - 1);
    buf[len - 1] = '\0';
}