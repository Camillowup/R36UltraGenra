# R36 GENRA – Projektstand

## Aufbau
- `src/gfx.*` – Software-Renderer 720×720 (abgerundete Formen, Kreise, Linien, Schrift aus `font_data.h`, Montserrat)
- `src/project.h` – Datenmodell v0.2 (16 Patterns, Varianten A–D pro Spur, Synth- und Groove-Werte, Song-Liste);
  `project_v1.h` nur zum Einlesen alter Dateien. Datei `projekt.r36`, eigene Presets `presets_eigen.bin`
- `src/engine.*` – Audio (48 kHz): Sequencer mit Swing, Groove 808/909/MINIMAL, Ratchets, Spurlängen, Song-Modus;
  Drum-Sampler; Plaits-Stimmen (PAD 3, BASS 2, LEAD 3; zusammen 8) mit Ladder/SVF-Filter, ADSR, Sub;
  SAMPLE-Spur (Keygroup-WAV oder E-Piano); Spur-FX Chorus/Dist/Lo-Fi; Delay (Ping-Pong/Tape/Mono),
  Hall (Kammer/Halle/Platte, Pre-Delay); Ducking; Live-Filter und Stutter/Freeze; CPU-Messung
- `src/presets.h` – Plaits-Engines und Werks-Presets (je 8 für PAD/BASS/LEAD, 4 für SAMPLE)
- `src/generator.*` – NEW ALL / NEW, 10 Genres, Varianten B–D aus A abgeleitet (leicht anders, Fill, Break)
- `src/midi.*` – USB-MIDI über `/dev/snd/midiC*D*` (ohne Zusatzbibliothek)
- `src/app.*` – Seiten SEQ, SONG, MIX, FX, SYN, Menü, MIDI-Test, Krabbo (GENRA-Look)
- `src/main_sdl.cpp` – SDL2, `src/main_test.cpp` – Test ohne Bildschirm (`make test`)
- `third_party/eurorack` – Plaits + stmlib (MIT), gebaut mit `-DTEST`

## Stand
- v0.1 (21. Nov. 2026): erste Testversion, auf dem Gerät getestet (gut).
- v0.2 (22. Nov. 2026): GENRA-Look, Plaits, Groove, Varianten, Song-Liste, Live-Effekte, FX-Modelle, MIDI. Noch nicht
  auf dem Gerät getestet.

## Offen / nächste Schritte
- v0.2 auf dem R36 testen: CPU-Last (Menü unten), MIDI-Controller, Tasten, Bildrate
- Noten-Grid (R3), Live-Einspielen über die 8 Tasten, Loop-Spur, Drum-Kit-Modus für MIDI
