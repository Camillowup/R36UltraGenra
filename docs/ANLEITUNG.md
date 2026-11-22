# R36 GENRA v0.2 – Anleitung

Groovebox für den R36 Ultra: 4 Drum-Spuren, 3 Plaits-Synths (PAD, BASS, LEAD), 1 SAMPLE-Spur,
Generator mit 10 Genres, Song-Modus, Live-Effekte auf den Sticks, USB-MIDI.

## Installieren / Aktualisieren

1. Bei GitHub unter **Actions** den neuesten Lauf öffnen und unten bei **Artifacts** die Datei
   **R36-GENRA-ports** herunterladen und entpacken.
2. SD-Karte mit den Spielen in den PC stecken (Laufwerk **EASYROMS**).
3. Den Inhalt des Ordners `ports` dorthin in den Ordner `ports` kopieren (`R36 GENRA.sh` und Ordner `r36genra`).
   Beim Aktualisieren vorhandene Dateien ersetzen – dein Projekt (`projekt.r36`) und deine Samples bleiben erhalten.
   Ein Projekt aus v0.1 wird beim ersten Start automatisch übernommen.
4. Im Menü **Ports** den Eintrag **R36 GENRA** starten. Erster Test leise.

Beenden: **START + SELECT**. Das Projekt wird automatisch gespeichert.

## Samples

WAV-Dateien in `r36genra/samples/kick`, `clap`, `hat`, `perc` (Drums) und `sample` (SAMPLE-Spur, Grundton C3).
Danach im Menü **SAMPLES NEU LADEN**. Ohne eigene Samples: eingebaute Klänge (SAMPLE-Spur: E-Piano).

## Überall

- **START**: Play / Stop · **SELECT + ‹ ›**: Seite (SEQ, SONG, MIX, FX, SYN) · **SELECT 2×**: Menü
- **START + SELECT**: Beenden

## SEQ

- **Kreuz**: Step und Spur · **A**: Step setzen/löschen · **L2/R2**: Sample (Drums) bzw. Note
- **L1/R1**: Pattern · **X**: Spur stumm · **Y**: NEW (Spur neu) · **SEL + Y**: NEW ALL · **SEL + X**: UNDO
- **SEL + auf/ab**: Genre · **SEL + L1/R1**: Variante A–D der Spur (leere Variante = Kopie der aktuellen)
- **B halten**: P-Lock – ‹ › wählt den Wert (Pitch/Note, Decay/Länge, Cutoff, Reverb, Delay, Repeat),
  auf/ab oder Stick L ändert. **B + A**: Locks löschen. **B + L2/R2**: Akkord an/aus (PAD, SAMPLE)
- **B kurz**: Step anhören · **R3 halten + ‹ ›**: Spur verschieben (Phrase Rotation)
- **SEL + B**: Aufnahme an/aus (MIDI-Pads schreiben in die Steps)
- **Stick links**: Filter (links Tiefpass, rechts Hochpass, loslassen = aus)
- **Stick rechts**: Stutter – hoch 1/4, rechts 1/8, links 1/16, runter Freeze; loslassen = weiter

## SONG

- **B**: zwischen Song-Liste und Pattern-Pads wechseln
- Pads: **Kreuz** wählen, **A** spielen, **X** kopieren, **Y** einfügen
- Liste: **auf/ab** Zeile, **‹ ›** Takte, **L2/R2** Teil (INTRO … OUTRO), **A** gewähltes Pad in die Zeile
- **R1**: Pad als neue Zeile einfügen · **L1**: Zeile löschen · **SEL + A**: Song ab Zeile · **SEL + Y**: Loop

## MIX

**L1 L2 R1 R2 A B X Y** = Mute der 8 Spuren · **SEL + Taste** = Solo · **SEL + auf/ab** = alles aufheben ·
Kreuz ‹ › Kanal · Stick L oder auf/ab Lautstärke · Stick R Panorama

## FX

Kreuz wählt Regler, **Stick L** ändert (B + auf/ab in 10er-Schritten). **L1/R1** Modell (Delay: Ping-Pong, Tape, Mono;
Hall: Kammer, Halle, Platte), **L2/R2** Sync bzw. Pre-Delay, **X** Ducking (Kick drückt die anderen Spuren),
**Y** Effekt an/aus, **A** Testklang. Unten die Sends pro Spur.

## SYN

- **L1/R1** Spur · **X** KLANG / GROOVE · **L2/R2** Preset (Drums: Sample) · **Y** Preset speichern
- **Kreuz** Regler oder Auswahlfeld wählen, **B halten + Stick L** ändern, **B + auf/ab** fein, **B + A** Standardwert
- **SEL + Y** würfeln · **SEL + X** UNDO · **A** anhören
- PAD/BASS/LEAD: Plaits-Engine (21 Modelle), TIMBRE, HARM, MORPH, SUB, Modus, Glide, Accent; Filter
  (Ladder 24, Tiefpass 12, Bandpass, Hochpass) mit Hüllkurve; ADSR; Chorus, Dist, Lo-Fi, Hall
- GROOVE (alle Spuren): Modell **808** (locker), **909** (straff), **MINIMAL** (Drift wächst über lange Loops),
  Swing, Velocity-Zufall, eigene Länge (z. B. 7 Steps gegen 16)

## MIDI

Controller an den **OTG**-Anschluss. Menü › **MIDI-TEST** zeigt Geräte und ankommende Noten.
Pads ab **C1 (Note 36)**: C1 KICK, C#1 CLAP, D1 HAT, D#1 PERC, E1 PAD, F1 BASS, F#1 LEAD, G1 SAMPLE.
Ab C2 spielt die auf SEQ gewählte Instrument-Spur chromatisch.

## Wenn etwas nicht klappt

- `r36genra/log.txt` enthält Meldungen (Bildschirm, Ton, Tasten, MIDI-Geräte).
- Tasten falsch: in `r36genra/einstellungen.txt` `controller=0` setzen und Nummern anpassen (`tasten_log=1`).
- Bild gedreht: `drehen=90` (oder 180/270). Menü zeigt unten die CPU-Last.
