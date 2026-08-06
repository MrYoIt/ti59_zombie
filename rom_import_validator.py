#!/usr/bin/env python3
"""
rom_import_validator.py — Valida un dump ROM "Second ROM"/Library TI-58/59
(formato "ADDR: BCD DATA", lo stesso del Datamath Calculator Museum) e,
se la validazione passa, genera il file .h pronto da copiare nel progetto
firmware (ESP32-S3) per aggiungere il modulo all'emulatore.

COSA FA IN AUTOMATICO
  1. Legge il dump raw byte-per-byte del chip.
  2. Ricava l'elenco dei programmi DIRETTAMENTE dalla ROM stessa — non
     serve un file di disassemblato separato — leggendo l'header
     standard di ogni Second ROM TI (indirizzo 0000 = numero di pagine,
     0001 = codice di sicurezza, 0002+ = indirizzo di ogni pagina,
     MSB-first, terminato da un indirizzo extra "fine ultima pagina").
     Questo è documentato nella HW guide di Sladký ed è stato verificato
     byte-per-byte contro il dump reale della Master Library.
  3. Se gli fornisci ANCHE il disassemblato ufficiale (facoltativo), lo
     usa solo per recuperare i TITOLI dei programmi (la ROM non li
     contiene) e per una seconda verifica incrociata dei confini.
  4. Rivalida la grammatica di decodifica byte-per-byte su OGNI
     programma (stessa grammatica di exec_program_step() in
     tms1500.cpp — opcode compatti a 1 byte per STO/RCL/SUM/EXC/PROD/
     PGM/OP, DSZ/GTO/SBR/IFF/x=t/x≥t con etichetta-o-indirizzo, ecc.)
     e verifica che ogni branch risolva a un'etichetta reale in ROM.
  5. Controlla che lo spazio non coperto da nessun programma sia
     riempito di padding valido (opcode 92 = Return), come da spec TI.
  6. Se tutto passa, genera un header .h con l'array ROM e la tabella
     programmi, pronto per essere incluso nel firmware.
  7. Calcola quanto spazio flash/PSRAM occupa il modulo e, sommandolo
     ad altri moduli già generati in una cartella, dice se e quanti
     modelli aggiuntivi ci stanno nel budget indicato.

USO
    # Validare + generare l'header per un singolo modulo
    python3 rom_import_validator.py TMC0541.txt --name master_library \
        [--disasm TMC0541-PGMSUTF8.txt] [--out ./out]

    # Solo controllo spazio, sommando tutti gli .h già generati in ./out
    python3 rom_import_validator.py --space-check ./out [--budget-bytes N]

NOTA IMPORTANTE
    Il formato dell'header generato (struct LibraryProgramEntry, nomi
    campi, ecc.) è la mia MIGLIOR RICOSTRUZIONE di cosa si aspettano
    library_module.h/rom_ml1.cpp in base a come vengono usati in
    tms1500.cpp (library_get_active(), library_find_program(), campi
    .rom/.rom_size di LibraryModule). Non ho ancora visto il contenuto
    reale di library_module.h — se i nomi non combaciano esattamente,
    mandamelo e adatto il generatore in un minuto.
"""

import argparse
import os
import re
import sys
from dataclasses import dataclass, field

KC_LBL = 76
KC_INV = 22
KC_DSZ = 97
KC_SBR = 71
KC_GTO = 61
KC_XEQ_T = 67
KC_XGE_T = 77
KC_IFFL = 87
KC_STFL = 86
KC_FIX = 58
KC_PGM = 36
KC_PGM_IND = 62
KC_STO = 42
KC_RCL = 43
KC_SUM = 44
KC_EXC = 48
KC_PROD = 49
KC_STO_IND = 72
KC_RCL_IND = 73
KC_SUM_IND = 74
KC_EXC_IND = 63
KC_PROD_IND = 64
KC_GTO_IND = 83
KC_OP = 69
KC_OP_IND = 84
KC_RETURN = 92

STO_FAMILY = {KC_STO, KC_RCL, KC_SUM, KC_EXC, KC_PROD,
              KC_STO_IND, KC_RCL_IND, KC_SUM_IND, KC_EXC_IND, KC_PROD_IND,
              KC_GTO_IND, KC_OP, KC_OP_IND, KC_PGM_IND, KC_PGM}
GOTO_FAMILY = {KC_SBR, KC_GTO, KC_XEQ_T, KC_XGE_T}
FLAG_FAMILY = {KC_STFL}


# ─────────────────────────── Parsing dump raw ───────────────────────────
def load_rom(path):
    rom = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = re.match(r"^\s*(\d{4}):\s*((?:\d+\s*)+)$", line.strip())
            if not m:
                continue
            base = int(m.group(1))
            for i, v in enumerate(m.group(2).split()):
                rom[base + i] = int(v)
    if not rom:
        raise ValueError(f"Nessun byte letto da {path}: formato non riconosciuto "
                          f"(atteso 'ADDR: BCD DATA' stile Datamath Calculator Museum)")
    return rom


# ────────────────── Ricostruzione tabella pagine dall'header ROM ──────────────────
@dataclass
class Program:
    num: int
    base: int
    length: int
    title: str = ""
    boundaries: list = field(default_factory=list)


def derive_program_table_from_header(rom):
    max_addr = max(rom.keys())
    count = rom.get(0, 0)
    security = rom.get(1, 0)
    if count == 0 or count > 40:
        raise ValueError(f"Header ROM non plausibile: numero pagine={count} "
                          f"(atteso 1-40). Il dump è nel formato giusto?")
    addrs = []
    for i in range(count + 1):
        hi = rom.get(2 + 2 * i)
        lo = rom.get(3 + 2 * i)
        if hi is None or lo is None:
            raise ValueError(f"Header ROM troncato: manca l'indirizzo pagina #{i}")
        addrs.append(hi * 100 + lo)
    programs = []
    for i in range(count):
        start = addrs[i]
        end = addrs[i + 1]
        if end <= start or end > max_addr + 1:
            raise ValueError(f"Indirizzi pagina #{i+1} incoerenti: start={start} end={end}")
        programs.append(Program(i + 1, start, end - start))
    return programs, security, addrs[count]  # addrs[count] = inizio spazio libero


def enrich_titles_from_disassembly(programs, path):
    with open(path, encoding="utf-8", errors="replace") as f:
        text = f.read()
    by_num = {p.num: p for p in programs}
    for m in re.finditer(r"#(\d+):\s+(\d{4})\s+(\d{4})\s+(.+?)\r?\n", text):
        num, addr, length, title = m.groups()
        num = int(num)
        if num in by_num:
            by_num[num].title = title.strip()
            official_len = int(length)
            if official_len != by_num[num].length:
                print(f"  ATTENZIONE: lunghezza #{num} da header ROM={by_num[num].length} "
                      f"vs disassemblato={official_len} — discrepanza, verifica il file")


# ───────────────────────── Grammatica di decodifica ─────────────────────────
def instr_len(rom, addr, prog_end):
    if addr >= prog_end:
        return 1
    op = rom.get(addr, 0)
    if op == KC_LBL:
        return 2
    if op == KC_INV:
        if addr + 1 >= prog_end:
            return 1
        return 1 + instr_len(rom, addr + 1, prog_end)
    if op == KC_DSZ:
        after_reg = addr + 2
        if after_reg < prog_end and rom.get(after_reg, 0) > 9:
            return 3
        return 4
    if op in GOTO_FAMILY or op == KC_IFFL:
        base = addr + 1
        if op == KC_IFFL:
            base += 1
        if base >= prog_end:
            return base - addr
        if rom.get(base, 0) > 9:
            return (base - addr) + 1
        return (base - addr) + 2
    if op in FLAG_FAMILY:
        return 2
    if op in STO_FAMILY:
        return 2
    if op == KC_FIX:
        return 2
    return 1


def decode_program(rom, prog):
    base, length = prog.base, prog.length
    end = base + length
    boundaries, instrs, labels, branches = [], [], {}, []
    addr = base
    while addr < end:
        boundaries.append(addr - base)
        op = rom.get(addr, 0)
        ilen = instr_len(rom, addr, end)
        instrs.append((addr - base, op, ilen))
        if op == KC_LBL:
            labels[rom.get(addr + 1, 0)] = addr - base
        if op in GOTO_FAMILY or op == KC_DSZ or op == KC_IFFL:
            off = addr + 1
            if op == KC_IFFL:
                off += 1
            if op == KC_DSZ:
                off += 1
            tgt = rom.get(off, 0) if off < end else None
            if tgt is not None and tgt > 9:
                branches.append((addr - base, "LABEL", tgt))
            else:
                b1, b2 = rom.get(off, 0), rom.get(off + 1, 0)
                branches.append((addr - base, "ADDR", b1 * 100 + b2))
        addr += ilen
    overshoot = addr - end
    pgm_exempt = {o + l for (o, op_, l) in instrs if op_ == KC_PGM}
    return boundaries, labels, branches, overshoot, pgm_exempt


def validate_rom(rom, programs, free_space_start):
    global_labels = {}
    for addr, val in rom.items():
        if val == KC_LBL:
            lbl_kc = rom.get(addr + 1)
            if lbl_kc is not None:
                global_labels.setdefault(lbl_kc, []).append(addr)

    all_ok = True
    print(f"{'PGM':<5}{'INDIRIZZO':<12}{'LUNGH.':<9}{'TITOLO':<45}{'ESITO'}")
    print("-" * 90)
    for prog in programs:
        boundaries, labels, branches, overshoot, pgm_exempt = decode_program(rom, prog)
        prog.boundaries = boundaries

        unresolved = []
        for local_off, kind, tgt in branches:
            if kind == "LABEL":
                if tgt not in labels and tgt not in global_labels:
                    unresolved.append((local_off, kind, tgt))
            else:
                if not (0 <= tgt <= max(rom.keys())):
                    unresolved.append((local_off, kind, tgt))

        ok = (overshoot == 0) and (len(unresolved) == 0)
        all_ok &= ok
        title = prog.title or f"(senza titolo)"
        print(f"#{prog.num:02d}  {prog.base:<12}{prog.length:<9}{title:<45}"
              f"{'PASS' if ok else 'FAIL'}")
        if overshoot != 0:
            print(f"      overshoot decodifica: {overshoot} byte oltre la fine dichiarata")
        for local_off, kind, tgt in unresolved[:5]:
            print(f"      branch non risolto @offset {local_off} (indirizzo {prog.base+local_off}): "
                  f"{kind}={tgt}")

    # Controllo padding nello spazio libero dichiarato dall'header
    max_addr = max(rom.keys())
    bad_padding = [a for a in range(free_space_start, max_addr + 1)
                   if rom.get(a, KC_RETURN) != KC_RETURN]
    if bad_padding:
        print(f"\nATTENZIONE: {len(bad_padding)} byte nello spazio libero dichiarato "
              f"(da {free_space_start}) NON sono opcode 92 (Return) di riempimento — "
              f"possibile dump troncato o corrotto. Primi: {bad_padding[:10]}")
        all_ok = False
    else:
        print(f"\nPadding spazio libero (da indirizzo {free_space_start} a {max_addr}): OK, "
              f"tutto opcode 92 (Return) come da spec.")

    print("-" * 90)
    if all_ok:
        print("Totale: TUTTI I PROGRAMMI OK")
    else:
        print("Totale: ALCUNI PROGRAMMI HANNO PROBLEMI — non generare l'header finché non risolvi")
    return all_ok


# ───────────────────────────── Generazione modulo ─────────────────────────────
def emit_module(rom, programs, name, module_id, security_code, out_dir):
    """Genera rom_<id>.cpp + rom_<id>.h conformi a library_module.h reale:
    struct LibraryProgram {num,addr,len,title} e LibraryModule
    {id,name,rom,rom_size,programs,program_count}, con un oggetto
    'extern const LibraryModule <ID>_MODULE' pronto per essere aggiunto
    a LIBRARY_REGISTRY[] in library_module.cpp. I nomi di file/variabili
    si basano sull'id breve (es. 'ml1'), non sul nome descrittivo esteso,
    coerentemente con l'esempio rom_ml1.cpp citato in library_module.h."""
    slug = re.sub(r"[^a-zA-Z0-9_]", "_", module_id).lower()
    max_addr = max(rom.keys())
    size = max_addr + 1
    guard = f"ROM_{slug.upper()}_H"
    module_var = f"{slug.upper()}_MODULE"

    # ---- header (.h): solo la dichiarazione extern, da includere in
    # library_module.cpp per registrare il modulo ----
    h_lines = [
        f"// rom_{slug}.h — generato automaticamente da rom_import_validator.py",
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        '#include "library_module.h"',
        "",
        f"extern const LibraryModule {module_var};",
        "",
        f"#endif // {guard}",
    ]

    # ---- implementazione (.cpp): array ROM + tabella programmi + oggetto modulo ----
    c_lines = []
    c_lines.append(f"// rom_{slug}.cpp — generato automaticamente da rom_import_validator.py")
    c_lines.append(f"// Modulo: {name}  |  id=\"{module_id}\"  |  {len(programs)} programmi  |  ROM_SIZE={size}")
    c_lines.append(f"// Codice di sicurezza ROM: {security_code:02d}  "
                   f"({'non protetto' if security_code == 0 else 'protetto — verifica compatibilità'})")
    c_lines.append("// Validazione: TUTTI i programmi PASS (confini istruzione + risoluzione branch,")
    c_lines.append("// vedi rom_import_validator.py) prima della generazione di questo file.")
    c_lines.append(f'#include "rom_{slug}.h"')
    c_lines.append("")
    c_lines.append(f"static const uint8_t {slug}_rom[{size}] = {{")
    row = []
    for addr in range(size):
        row.append(f"{rom.get(addr, 0):3d}")
        if len(row) == 20:
            c_lines.append("    " + ",".join(row) + ",")
            row = []
    if row:
        c_lines.append("    " + ",".join(row) + ",")
    c_lines.append("};")
    c_lines.append("")
    c_lines.append(f"static const LibraryProgram {slug}_programs[] = {{")
    for p in programs:
        title = (p.title or f"PGM {p.num:02d}").replace('"', '\\"')
        c_lines.append(f'    {{ {p.num}, {p.base}, {p.length}, "{title}" }},')
    c_lines.append("};")
    c_lines.append("")
    c_lines.append(f"const LibraryModule {module_var} = {{")
    c_lines.append(f'    "{module_id}",')
    c_lines.append(f'    "{name}",')
    c_lines.append(f"    {slug}_rom,")
    c_lines.append(f"    {size},")
    c_lines.append(f"    {slug}_programs,")
    c_lines.append(f"    {len(programs)}")
    c_lines.append("};")

    os.makedirs(out_dir, exist_ok=True)
    h_path = os.path.join(out_dir, f"rom_{slug}.h")
    c_path = os.path.join(out_dir, f"rom_{slug}.cpp")
    with open(h_path, "w", encoding="utf-8") as f:
        f.write("\n".join(h_lines) + "\n")
    with open(c_path, "w", encoding="utf-8") as f:
        f.write("\n".join(c_lines) + "\n")
    return h_path, c_path, size, module_var


# ───────────────────────────── Controllo spazio ─────────────────────────────
def space_check(out_dir, budget_bytes):
    if not os.path.isdir(out_dir):
        print(f"Cartella {out_dir} non trovata.")
        return
    total = 0
    modules = []
    for fname in sorted(os.listdir(out_dir)):
        if not fname.endswith(".cpp"):
            continue
        path = os.path.join(out_dir, fname)
        with open(path, encoding="utf-8", errors="replace") as f:
            text = f.read()
        m = re.search(r"ROM_SIZE=(\d+)", text)
        if not m:
            continue
        size = int(m.group(1))
        modules.append((fname, size))
        total += size

    print(f"{'FILE':<40}{'BYTE':>10}")
    print("-" * 50)
    for fname, size in modules:
        print(f"{fname:<40}{size:>10,}")
    print("-" * 50)
    print(f"{'TOTALE':<40}{total:>10,}")
    print()
    remaining = budget_bytes - total
    avg = (total / len(modules)) if modules else 5000  # 5000 B: dimensione tipica Second ROM TI
    extra = int(remaining // avg) if remaining > 0 else 0
    print(f"Budget indicato: {budget_bytes:,} byte")
    print(f"Spazio residuo:  {remaining:,} byte")
    if remaining >= 0:
        print(f"→ Ci stanno ancora circa {extra} moduli aggiuntivi da ~{int(avg):,} byte l'uno "
              f"(stima sulla dimensione media dei moduli già presenti).")
    else:
        print(f"→ BUDGET SUPERATO di {-remaining:,} byte.")


# ───────────────────────────── Auto-registrazione ─────────────────────────────
def update_registry(registry_path, module_var, out_dir):
    """Inserisce automaticamente 'extern const LibraryModule <var>;' e
    '&<var>,' in LIBRARY_REGISTRY[] dentro library_module.cpp. Idempotente:
    se il modulo è già registrato non tocca nulla. Scrive il risultato in
    out_dir (NON sovrascrive l'originale) così puoi controllare il diff
    prima di sostituirlo nel progetto."""
    with open(registry_path, encoding="utf-8") as f:
        text = f.read()

    if re.search(rf"\b{re.escape(module_var)}\b", text):
        print(f"  {module_var} è già presente in {os.path.basename(registry_path)} — nessuna modifica.")
        return None

    # 1) aggiunge la dichiarazione extern, subito dopo l'ultima esistente
    extern_re = re.compile(r"^extern const LibraryModule \w+;[ \t]*$", re.MULTILINE)
    matches = list(extern_re.finditer(text))
    new_extern = f"extern const LibraryModule {module_var};"
    if matches:
        pos = matches[-1].end()
        text = text[:pos] + "\n" + new_extern + text[pos:]
    else:
        # nessuna extern preesistente: la mette dopo l'ultimo #include
        inc_re = re.compile(r'^#include.*$', re.MULTILINE)
        inc_matches = list(inc_re.finditer(text))
        pos = inc_matches[-1].end() if inc_matches else 0
        text = text[:pos] + "\n\n" + new_extern + text[pos:]

    # 2) aggiunge "&<var>," dentro LIBRARY_REGISTRY[] prima della "};" di chiusura
    arr_re = re.compile(
        r"(static const LibraryModule\* const LIBRARY_REGISTRY\[\] = \{)(.*?)(\n\};)",
        re.DOTALL)
    m = arr_re.search(text)
    if not m:
        raise ValueError("Non trovo l'array LIBRARY_REGISTRY[] in "
                          f"{registry_path} — formato inatteso, inserimento manuale necessario.")
    body = m.group(2).rstrip()
    if not body.endswith(","):
        body += ","
    new_body = body + f"\n    &{module_var},"
    text = text[:m.start()] + m.group(1) + new_body + m.group(3) + text[m.end():]

    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, os.path.basename(registry_path))
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(text)
    return out_path


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dump", nargs="?", help="File dump ROM 'ADDR: BCD DATA'")
    ap.add_argument("--name", help="Nome leggibile del modulo (es. 'Master Library -1-')")
    ap.add_argument("--id", help="Identificatore breve per LibraryModule.id (es. 'ml1'); "
                                  "default: derivato da --name")
    ap.add_argument("--disasm", help="Disassemblato ufficiale opzionale, solo per i titoli")
    ap.add_argument("--out", default="./out", help="Cartella output (default ./out)")
    ap.add_argument("--registry", help="Percorso di library_module.cpp: se indicato, genera "
                                        "automaticamente anche la versione aggiornata con "
                                        "extern + entry in LIBRARY_REGISTRY[] (scritta in --out, "
                                        "l'originale non viene toccato)")
    ap.add_argument("--space-check", metavar="DIR",
                     help="Solo controllo spazio sugli rom_*.cpp già generati in DIR")
    ap.add_argument("--budget-bytes", type=int, default=8 * 1024 * 1024,
                     help="Budget totale in byte da confrontare (default 8MB, tipico Flash/PSRAM ESP32-S3)")
    args = ap.parse_args()

    if args.space_check:
        space_check(args.space_check, args.budget_bytes)
        return

    if not args.dump or not args.name:
        ap.error("servono 'dump' e --name (oppure usa --space-check DIR da solo)")

    rom = load_rom(args.dump)
    programs, security, free_start = derive_program_table_from_header(rom)
    print(f"Header ROM: {len(programs)} programmi dichiarati, codice sicurezza={security:02d}, "
          f"spazio libero da indirizzo {free_start}\n")

    if args.disasm:
        enrich_titles_from_disassembly(programs, args.disasm)

    ok = validate_rom(rom, programs, free_start)
    if not ok:
        print("\nFile NON generati: risolvi i problemi sopra e rilancia.")
        sys.exit(1)

    module_id = args.id or re.sub(r"[^a-zA-Z0-9]", "", args.name.lower())[:8]
    h_path, c_path, size, module_var = emit_module(rom, programs, args.name, module_id, security, args.out)
    slug = re.sub(r"[^a-zA-Z0-9_]", "_", module_id).lower()
    print(f"\nGenerati:\n  {h_path}\n  {c_path}\n({size:,} byte, id=\"{module_id}\")")

    if args.registry:
        print(f"\nAggiornamento registro ({args.registry}):")
        reg_out = update_registry(args.registry, module_var, args.out)
        if reg_out:
            print(f"  Scritto: {reg_out}")
            print(f"  Controlla il diff e poi sostituisci il tuo library_module.cpp con questo.")
    else:
        print(f"\nPer registrarlo a mano in library_module.cpp:")
        print(f'  1. Aggiungi: extern const LibraryModule {module_var};')
        print(f"  2. Aggiungi &{module_var} all'array LIBRARY_REGISTRY[]")
        print(f"  (oppure rilancia con --registry path/a/library_module.cpp per farlo in automatico)")


if __name__ == "__main__":
    main()
