#!/usr/bin/env python3
"""
generate_ml_cards.py — Genera le SVG card per i programmi Master Library
(escluso pg 01, gia' esistente come ML-01.svg).

Legge i byte della ROM da rom_ml1.cpp, scansiona ogni programma per
LBL opcode (76) + tasto utente, e produce SVG nella cartella ./ml_cards/.
"""
import re, os

SRC = os.path.join(os.path.dirname(__file__), 'src', 'rom_ml1.cpp')
OUT  = os.path.join(os.path.dirname(__file__), 'ml_cards')

# Mappa keycode → nome label
KEY_NAMES = {
    11: 'A', 12: 'B', 13: 'C', 14: 'D', 15: 'E',
    16: "A'", 17: "B'", 18: "C'", 19: "D'", 10: "E'",
}
# Ordine per la griglia 5×2: riga 0 = A B C D E, riga 1 = A' B' C' D' E'
GRID_KEYS = [11, 12, 13, 14, 15, 16, 17, 18, 19, 10]

def parse_rom_bytes(path):
    """Estrae l'array ml1_rom[] dal file C."""
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()
    # Trova il contenuto tra ml1_rom[5000] = { ... };
    m = re.search(r'ml1_rom\[5000\]\s*=\s*\{(.*?)\};', text, re.DOTALL)
    if not m:
        raise ValueError("ml1_rom[] non trovato")
    body = m.group(1)
    # Estrae tutti i numeri
    nums = [int(x) for x in re.findall(r'\d+', body)]
    if len(nums) != 5000:
        print(f"Attenzione: trovati {len(nums)} byte, attesi 5000")
    return nums[:5000]

def parse_programs(path):
    """Estrae l'array ml1_programs[] e restituisce [(num,addr,len,title), ...]."""
    with open(path, 'r', encoding='utf-8') as f:
        text = f.read()
    m = re.search(r'ml1_programs\[\]\s*=\s*\{(.*?)\};', text, re.DOTALL)
    if not m:
        raise ValueError("ml1_programs[] non trovato")
    body = m.group(1)
    prog_re = re.compile(r'\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*"([^"]*)"\s*\}')
    progs = []
    for m2 in prog_re.finditer(body):
        num, addr, length, title = int(m2.group(1)), int(m2.group(2)), int(m2.group(3)), m2.group(4)
        progs.append((num, addr, length, title))
    return progs

def scan_labels(rom, addr, length):
    """Cerca LBL (76) + tasto utente nel range [addr, addr+length)."""
    labels = {}
    end = min(addr + length, len(rom) - 1)
    i = addr
    while i < end:
        if rom[i] == 76 and i + 1 < end:
            kc = rom[i + 1]
            if kc in KEY_NAMES:
                labels[kc] = True
            # LBL è 2 byte, salta
            i += 2
            continue
        # Opcode a 1 byte, ma alcuni sono 2-byte
        op = rom[i]
        # GTO (22), SBR (71), DSZ (59), x=t (67), x>=t (77),
        # IFF (87), STO (42), RCL (43), SUM (44), PROD (45),
        # EXC (46), OP (69), PGM (68), LBL (76 - gia' sopra), INV (27)
        TWO_BYTE_OPS = {22, 71, 59, 67, 77, 87, 42, 43, 44, 45, 46, 69, 68, 76}
        if op in TWO_BYTE_OPS:
            if i + 2 < end:
                i += 2
            else:
                i += 1
        elif op == 28:  # IND (28) puo' precedere STO/RCL etc — 1 byte
            i += 1
        else:
            i += 1
    return labels

def short_title(title):
    """Riduce il titolo per la card: toglie 'ML-NN ' iniziale."""
    return re.sub(r'^ML-\d+\s*', '', title)

def make_svg(num, title, labels, path):
    """Genera il file SVG."""
    prog_id = f"ML-{num:02d}"
    short = short_title(title)

    # Determina quali celle sono attive
    active = {kc: (kc in labels) for kc in GRID_KEYS}

    # Layout
    W, H = 544, 120
    # Colori
    C_BG    = "#0d0d0b"
    C_STRIP = "#f58827"  # arancio per Solid State Software
    C_NAME  = "#de7411"
    C_CELL  = "#d36910"
    C_GRID  = "#d36910"
    C_ACT   = "#d36910"
    C_INACT = "#2a2a28"

    strips = []
    def txt(content, x, y, size=18, color=C_NAME, bold=False, align="start"):
        fw = " font-weight=\"bold\"" if bold else ""
        return f'<text x="{x}" y="{y}" font-size="{size}" fill="{color}" font-family="Arial" text-anchor="{align}"{fw}>{content}</text>'

    # --- Strip 1: Solid State Software (fissa) ---
    strips.append(txt("Solid State Software", W/2, 22, 17, C_NAME, True, "middle"))

    # --- Strip 2: codice programma + nome ---
    strips.append(txt(prog_id, 15, 50, 20, C_STRIP, True, "start"))
    strips.append(txt(short, 100, 50, 18, C_NAME, False, "start"))

    # --- Linea orizzontale superiore della griglia ---
    grid_y0 = 60
    grid_y1 = 115
    grid_cell_w = W / 5
    grid_cell_h = (grid_y1 - grid_y0) / 2

    # Rettangolo sfondo griglia
    strips.append(f'<rect x="0" y="{grid_y0}" width="{W}" height="{grid_y1 - grid_y0}" fill="{C_BG}" stroke="{C_GRID}" stroke-width="1.5"/>')

    # Linee verticali
    for col in range(1, 5):
        x = col * grid_cell_w
        strips.append(f'<line x1="{x}" y1="{grid_y0}" x2="{x}" y2="{grid_y1}" stroke="{C_GRID}" stroke-width="1.5"/>')

    # Linea orizzontale centrale
    mid_y = grid_y0 + grid_cell_h
    strips.append(f'<line x1="0" y1="{mid_y}" x2="{W}" y2="{mid_y}" stroke="{C_GRID}" stroke-width="1.5"/>')

    # Celle
    for idx, kc in enumerate(GRID_KEYS):
        col = idx % 5
        row = idx // 5
        cx = col * grid_cell_w + grid_cell_w / 2
        cy = grid_y0 + row * grid_cell_h + grid_cell_h / 2
        label = KEY_NAMES[kc]
        is_active = active[kc]
        colr = C_ACT if is_active else C_INACT
        strips.append(txt(label, cx, cy - 5, 14, colr, True, "middle"))
        if not is_active:
            strips.append(f'<line x1="{cx - 10}" y1="{cy + 2}" x2="{cx + 10}" y2="{cy + 2}" stroke="{C_INACT}" stroke-width="1"/>')

    # Linea verticale destra (bordo)
    strips.append(f'<line x1="{W}" y1="0" x2="{W}" y2="{H}" stroke="{C_CELL}" stroke-width="1.5"/>')

    # Linee orizzontali extra (stile card originale)
    strips.append(f'<line x1="0" y1="{28}" x2="{W}" y2="{28}" stroke="{C_CELL}" stroke-width="1.5"/>')
    strips.append(f'<line x1="0" y1="{58}" x2="{W}" y2="{58}" stroke="{C_CELL}" stroke-width="1"/>')

    svg = f'''<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<svg version="1.1" viewBox="0 0 {W} {H}" xmlns="http://www.w3.org/2000/svg">
  <rect width="{W}" height="{H}" fill="{C_BG}"/>
  {chr(10).join(strips)}
</svg>'''

    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(svg)
    print(f"  -> {os.path.basename(path)}")

def generate_overlays(progs, labels_by_num, data_dir):
    """Genera overlays.txt nel formato MOD|PROG|TYPE|KEY|ATTR|TESTO."""
    lines = []
    # Programma 01 — diagnostic (FREE)
    # ATTR: c=centro, s=sinistra, e=destra
    lines.append("ml1|01|FREE|1|c|Solid State Software")
    lines.append("ml1|01|FREE|2|s|M1 - MASTER LIBRARY DIAGNOSTIC")
    lines.append("ml1|01|FREE|3|c|DIAGNOSTIC: SBR =")
    lines.append("ml1|01|FREE|4|s|L.R. INIT:SBR CLR")
    lines.append("ml1|01|FREE|4|e|PRINT:mm STO 00")

    for num, addr, length, title in progs:
        if num == 1:
            continue
        labels = labels_by_num.get(num, {})
        for kc in GRID_KEYS:
            if kc not in labels:
                continue  # salta tasti non usati dal programma
            kn = KEY_NAMES[kc]
            # ATTR = m (mezzo=centrato nella cella).
            # Testo placeholder = nome tasto — da editare via web
            lines.append(f"ml1|{num:02d}|GRID|{kn}|m|{kn}")

    path = os.path.join(data_dir, "overlays.txt")
    with open(path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines) + '\n')
    print(f"\nGenerato {path} ({len(lines)} righe)")

def main():
    print("Leggo rom_ml1.cpp...")
    rom = parse_rom_bytes(SRC)
    print(f"  {len(rom)} bytes letti")
    progs = parse_programs(SRC)
    print(f"  {len(progs)} programmi trovati\n")

    data_dir = os.path.join(os.path.dirname(__file__), 'data')
    labels_by_num = {}

    for num, addr, length, title in progs:
        if num == 1:
            continue  # ML-01 gia' esistente
        labels = scan_labels(rom, addr, length)
        labels_by_num[num] = labels
        active_keys = [KEY_NAMES[kc] for kc in GRID_KEYS if kc in labels]
        print(f"  ML-{num:02d} ({title}): labels={active_keys}")
        fname = f"romcard_ml1_{num:02d}.svg"
        make_svg(num, title, labels, os.path.join(OUT, fname))

    print(f"\nFatto. {len(progs) - 1} SVG generati in '{OUT}'")

    generate_overlays(progs, labels_by_num, data_dir)

if __name__ == '__main__':
    main()
