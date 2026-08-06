# TI-59 Zombie — Tabella di fedeltà verso la TI-59 reale

Fonte primaria usata per la verifica: *HW programming guide for calculators TI-58/59*, Hynek Sladký, 2014 (disassemblato ROM reale, tabelle ufficiali di keycode e Op-code) — https://ti58c.phweb.me/download/system/TI_58_59-HW-manual.pdf — incrociata con *Programming my TI* (Pierre Houbert) e il manuale dell'emulatore di Claudio Larini.

Legenda: 🟢 fedele/verificato · 🟡 approssimato/parziale · 🔴 non fedele o mancante · ⚪ non applicabile in questo hardware

---

## 1. Tastiera / codici keycode (00-99)

🟢 **100% verificato.** Ogni singolo codice da 00 a 99 nella tabella `KC_*` di `tms1500.cpp` corrisponde esattamente al "Program opcode table" ufficiale (inclusi dettagli minori come il commento "TI-59 only" sul codice 96/Write, o il codice 82/HIR marcato "non accessibile direttamente" — presenti pari pari anche nella documentazione originale). Nessuna discrepanza trovata.

## 2. Formato registro (BCD)

🟢 **Verificato.** `BCD_Reg` a 16 nibble = 13 mantissa + 2 esponente + 1 segno, esattamente come da documentazione hardware ("Number format: 16..4 = mantissa, 3..2 = exponent, 1 = signs"). Le quattro operazioni aritmetiche (`bcd_add/sub/mul/div`) lavorano in BCD reale, non in floating point binario convertito.

## 3. Motore di esecuzione programma

| Elemento | Stato | Note |
|---|---|---|
| GTO/SBR con indirizzo a 3 cifre | 🟢 | |
| GTO/SBR con etichetta diretta (A-E/A'-E') | 🟢 | |
| GTO/SBR indiretti (codici 83/61+Ind) | 🟢 | |
| **DSZ (decrementa e salta)** | 🟢 | **Corretto in questa sessione** — ora `Dsz nn LLL` con salto condizionato a registro≠0 verso indirizzo a 3 cifre o etichetta, come da hardware reale (prima era una semantica "skip next" in stile TI-57) |
| **STO/RCL/SUM indiretti con codice dedicato (72/73/74)** | 🔴 | **Gap trovato durante quest'audit, non ancora corretto.** Sulla TI-59 reale, "STO 2nd Ind" genera il codice-programma singolo 72 (non 42+40 separati); idem RCL→73, SUM→74. Questo emulatore non ha case per 72/73/74: un dump di scheda reale che li contiene verrebbe eseguito come no-op silenzioso. Impatto reale: programmi con indicizzazione indiretta su array di registri (tecnica molto comune) falliscono silenziosamente. Consiglio di correggerlo con la stessa priorità di DSZ. |
| LBL come istruzione a 2 byte | 🟢 | |
| INV come prefisso runtime | 🟢 | |

## 4. "2nd Op nn" (codici 00-39) — corretto in questa sessione

Tabella ufficiale (fonte: Sladký, sezione "Code Function (69)"):

| Op | Funzione reale | Stato emulatore dopo il fix |
|---|---|---|
| 00 | Inizializza buffer stampa alfanumerica | 🟡 no-op (nessuna stampante emulata finora — vedi §6) |
| 01-04 | Riempie i 4 gruppi da 5 caratteri del buffer stampa | 🟡 no-op |
| 05 | Stampa il buffer alfanumerico | 🟡 no-op |
| 06 | Stampa il display + contenuto gruppo 4 | 🟡 no-op |
| 07 | Stampa asterisco nella colonna indicata (curva) | 🟡 no-op |
| 08 | Lista le etichette usate dal programma | 🟡 no-op |
| 09 | Scarica pagina modulo libreria | 🔴 no-op (non emulabile senza ROM dei moduli originali) |
| 10 | Signum | 🔴 no-op (non ancora implementato — segno di x) |
| 11 | Varianza | 🔴 no-op (non ancora implementato) |
| 12 | Pendenza e intercetta regressione | 🟡 spostato su Op 90/93 (numerazione non originale, vedi sotto) |
| 13 | Coefficiente di correlazione | 🟡 spostato su Op 94 |
| 14 | y' (stima y da x) | 🔴 no-op (non ancora implementato) |
| 15 | x' (stima x da y) | 🔴 no-op (non ancora implementato) |
| **16** | **Mostra partizione corrente** | 🟡 no-op — impossibile mostrare un valore reale: non esiste partizionamento (v. §5) |
| **17** | **Ripartiziona memoria** | 🟡 no-op — **era il bug più grave**: prima calcolava un coefficiente di correlazione e lo scriveva in un registro. Ora almeno non corrompe più nulla. |
| 18 | Se non errore, imposta flag 7 | 🟢 |
| 19 | Se errore, imposta flag 7 | 🟢 |
| 20-29 | Incrementa registro dati 0-9 | 🟢 (avevo scritto per errore "estensione emulatore" nel report precedente: è codice reale, confermato da fonte primaria) |
| 30-39 | Decrementa registro dati 0-9 | 🟢 (idem) |
| 40 | Test "stampante collegata" | 🟡 attualmente sempre falso (nessuna stampante) — diventerà vero quando il backend BLE è connesso, v. §6 |
| 90-94 | Deviazione std. x/y, pendenza, intercetta, correlazione | 🟡 funzioni reali ma **numerazione non autentica** — sulla TI-59 vera si richiamano con tasti dedicati (2nd s, 2nd LR...) non ancora cablati sulla tastiera fisica di questo progetto |

**Nota sulla numerazione 90-94:** non è un codice Op ufficiale TI-59. È stata usata come "parcheggio" per non perdere funzionalità già presenti (dev. standard, regressione lineare) che altrimenti sarebbero rimaste raggiungibili solo tramite Op 12/13 sbagliati. La soluzione più fedele a lungo termine è cablare le vere combinazioni tasto (2nd s, 2nd LR, ecc.) sulla tastiera fisica invece di lasciarle su Op nn.

## 5. Memoria e partizionamento

🔴 **Non fedele — limite architetturale.** La TI-59 reale condivide un pool di 1920 caratteri tra passi programma e registri dati, partizionabile a blocchi di 80 passi/10 registri (default 480 passi/60 registri). Qui `PROG_SIZE=960` e `RAM_SIZE=100` sono due array fissi indipendenti: hai sempre il massimo di entrambi contemporaneamente. Più generoso in assoluto, ma incompatibile con qualunque programma che dipenda dal confine di partizione reale (v. anche l'esempio con `7 OP 17` nel manuale Larini). Ristrutturare questo richiederebbe cambiare l'architettura di memoria: fuori scope per un fix rapido.

## 6. Stampante PC-100A (Op 00-08)

🔴→🟡 **In lavorazione in questa sessione** (v. sezione implementazione sotto). Caratteristiche reali da riprodurre:
- Buffer alfanumerico a 20 caratteri, indirizzato **da destra verso sinistra**, diviso in 4 gruppi da 5
- Tabella caratteri dedicata a 64 simboli (lettere, cifre, punteggiatura) per Op 01-04
- Nomi funzione a 3 caratteri precodificati (STO, RCL, SUM, GTO, LRN, SIN, COS, TAN, ecc. — tabella "Printer function names" nel documento Sladký) per la stampa di listati di programma
- Velocità reale: 60 caratteri/secondo, 20 caratteri per riga, carta termica da 6,4 cm

## 7. Subroutine stack

🟡 8 livelli implementati contro 6 reali. Più generoso, basso rischio di rottura pratica, ma non riproduce il comportamento di overflow di un programma che lo sfrutti deliberatamente (raro).

## 8. Registri HIR (hierarchy / AOS)

🟡 Presenti internamente per la pila delle operazioni in sospeso (AOS), ma non indirizzabili come pseudo-registri extra. Tecniche di "programmazione sintetica" avanzate (uso di HIR come registri extra, descritte anche nel manuale Houbert) non funzionano.

## 9. Funzioni matematiche (sin/cos/tan/log/ln/eˣ/yˣ)

🟡 Calcolate con `libm` (`sin()`, `log()`, `pow()`...) invece del microcodice BCD originale della TMS1500. Risultati indistinguibili a schermo nella stragrande maggioranza dei casi, ma non bit-per-bit identici agli artefatti di arrotondamento dell'hardware reale.

## 10. Moduli libreria ROM (Solid State Software)

🔴 Non emulabile senza i dump ROM originali dei moduli. Op 09 resta no-op.

## 11. Lettore di schede magnetiche

⚪ Fuori dall'ambito di questo audit — gestito da `cardemu.cpp`, non incluso nei file analizzati in questa sessione.

---

## Priorità consigliate per i prossimi interventi

1. **STO/RCL/SUM indiretti (codici 72-74)** — stessa criticità di DSZ, impatto diretto sulla tua libreria di schede storiche.
2. **Stampante reale via Bluetooth** — v. proposta architetturale allegata.
3. Cablare le vere combinazioni tasto per statistica (2nd s, 2nd LR) al posto della numerazione Op 90-94 provvisoria.
4. Op 10 (Signum), 11 (Varianza), 14 (y'), 15 (x') — implementabili con confidenza alta, semplicemente non ancora fatto.
