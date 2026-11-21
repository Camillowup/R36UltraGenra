# R36 GENRA – Projektstand

## Aufbau
- `src/gfx.*` – eigener Software-Renderer 720×720 (abgerundete Formen, Verläufe, Schrift aus `font_data.h`)
- `src/project.h` – Datenmodell (Projekt, 16 Patterns, 8 Spuren, Steps mit P-Locks); Datei `projekt.r36`
- `src/engine.*` – Audio: Sequencer mit Swing, Drum-Sampler (eingebaute Klänge + WAVs), Synths
  (Pad 2× Säge, Bass Säge+Sub, Lead Puls, Keys FM-E-Piano oder Keygroup-WAV), Ping-Pong-Delay,
  Freeverb-Hall, Limiter
- `src/generator.*` – NEW ALL / NEW, 10 Genres (Wahrscheinlichkeiten pro Step, Akkordfolge für Bass/Pad/Keys/Lead)
- `src/app.*` – Seiten SEQ, SONG (Pattern-Pool), MIX, FX, SYN (Spur-Grundwerte), Menü, Krabbo
- `src/main_sdl.cpp` – SDL2 (Bild, Ton, Tasten; `einstellungen.txt`), `src/main_test.cpp` – Test ohne Bildschirm
- `port/R36 GENRA.sh` – Startskript für das Ports-Menü

## Stand
- v0.1 (21. Nov. 2026): erste Testversion, noch nicht auf dem Gerät getestet.

## Offen / nächste Schritte
- Auf dem R36 Ultra testen: Start, Bild (Drehung?), Ton, Tastenbelegung, Bildrate
- Plaits-Engines für Pad/Bass/Lead (GPL, z. B. aus fm1-dock), Presets, Synth-Seite nach Entwurf
- Noten-Grid (R3), Song-Liste und Zeitleiste, Aufnahme-Modus, USB-MIDI-In
- Effekt-Modelle erweiterbar (L1/R1 auf der FX-Seite)
