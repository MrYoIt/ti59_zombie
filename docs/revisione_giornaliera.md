# Revisione giornaliera TI-59 Zombie

File di controllo quotidiano. Aggiornare solo quando qualcosa cambia di stato;
la checklist del giorno va solo spuntata.

## Stato sospeso (aperto)

- [ ] **Cleanup .ino — punto 4**: rimuovere la vecchia `taskDisplay` interamente
      commentata (righe ~123-139, "AGGIORNA DISPLAY: percorso stringa SEMPRE"),
      duplicata da quella attiva. Codice morto.
- [ ] **Monitor seriale**: problemi noti, ripresi SOLO dietro richiesta esplicita.
- [ ] **God mode / wolf**: verificare che `/god_mode.txt` su SPIFFS contenga
      esattamente `ora faccio quello che voglio` (senza a-capo in più), altrimenti
      `/wolf` e il link GOD! restano fuori.
- [ ] **Log diagnostici**: rimossi dopo il fix overlay (pagina resta su
      `lib_selected_page` invariata durante le chiamate interne `Pgm nn`).

## In sospeso MA applicato (solo da ricompilare/flashare)

- [ ] **Overlay: apice/pedice** `^+...^^` / `^-...^^` in `overlays.txt`
      (FREE e GRID, ROM e schede magnetiche). Ricompilare + testare con
      `ml1|01|GRID|A|c|X^+2^^`.
- [ ] **Fix overlay run**: `lib_selected_page` non cambia più durante le chiamate
      interne → l'overlay resta sul programma lanciato.
- [ ] **Backup/restore `overlay_pos.json`** dal pannello Posizioni di `/overlays`
      (pulsanti "Apri file"/"Salva file").
- [ ] **God mode**: GOD! in `/manage` (link oro) + regolatori Old/New ed
      espulsione spostati in `/wolf` con descrizioni.
- [ ] **Cleanup .ino**: punti 1 (header moduli/endpoints), 2 (include morto),
      3 (commento mutex), 5 (`rom_init` + rimozione `MODULE_MASTER_LIBRARY`),
      6 (`crom_slot`).

## Checklist giornaliera

### Firmware / flash
- [ ] Firmware corrente flashato (tutte le modifiche "da flashare" sopra).
- [ ] Boot senza errori: `[SPIFFS] OK`, `[INIT] Completato`.
- [ ] `/god_mode.txt` presente se serve il pannello wolf.

### Web
- [ ] `/manage` → link **GOD!** dorato visibile (con god mode).
- [ ] `/wolf` → regolatori Old/New (`/api/timing`) ed espulsione (`/api/eject`)
      con le descrizioni; NON presenti in `/manage`.
- [ ] `/overlays` → pannello Posizioni: salva/carica `overlay_pos.json`.
- [ ] `/overlays` → editor: testo con `^+2^^` renderizza l'apice, `^-1^^` il pedice.

### Emulatore
- [ ] Selezionare un programma libreria che chiama altri `Pgm nn` internamente
      (es. ML-01 Diagnostic): l'overlay resta quello del programma lanciato
      durante e dopo l'esecuzione (nessuna "griglia senza testo").
- [ ] Scheda magnetica: carico/salvataggio, espulsione con la durata impostata
      in `/wolf`.
- [ ] Timing Old/New applicato correttamente.

### File su disco (sorgente)
- [ ] `ti59_zombie.ino` e `src/wifilink.cpp` senza commenti incoerenti (revisione
      visiva veloce).
