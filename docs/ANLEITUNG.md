# R36 GENRA v0.1 – Anleitung

Step-Sequencer für den R36 Ultra: 4 Drum-Spuren, 4 Instrument-Spuren, Generator mit 10 Genres.
Erste Testversion.

## Installieren

1. Bei GitHub unter **Actions** den neuesten Lauf öffnen und unten bei **Artifacts** die Datei
   **R36-GENRA-ports** herunterladen und entpacken.
2. Die SD-Karte mit den Spielen in den PC stecken (Laufwerk **EASYROMS**).
3. Den Inhalt des Ordners `ports` dorthin in den Ordner `ports` kopieren:
   - `R36 GENRA.sh`
   - Ordner `r36genra`
4. Karte zurück in den R36, einschalten, im Menü **Ports** den Eintrag **R36 GENRA** starten.
   Falls er nicht erscheint: im Hauptmenü die Spieleliste aktualisieren oder neu starten.

**Erster Test leise!** Lautstärke am Gerät erst runter.

Beenden: **START + SELECT** gleichzeitig. Das Projekt wird automatisch gespeichert (`projekt.r36`).

## Eigene Samples

WAV-Dateien in die Ordner `r36genra/samples/kick`, `clap`, `hat`, `perc` legen
(`keys`: Samples für die Keys-Spur, Grundton C4). Danach im Menü **SAMPLES NEU LADEN**.
Ohne eigene Samples spielt das Gerät mit eingebauten Klängen (je 4 pro Drum-Spur).

## Tasten

Überall:
- **START**: Play / Stop
- **SELECT + Kreuz links/rechts**: Seite wechseln (SEQ, SONG, MIX, FX, SYN)
- **SELECT 2× antippen**: Menü (Tempo, Swing, Pattern-Länge, Tonart, Krabbo …)
- **START + SELECT**: Beenden

SEQ (Sequencer):
- **Kreuz**: Step und Spur wählen · **A**: Step setzen/löschen
- **L2 / R2**: Sample des Steps (Drums) bzw. Note (Instrumente)
- **L1 / R1**: Pattern wechseln (beim Spielen: ab dem nächsten Durchlauf)
- **Y**: NEW (neue Noten für die gewählte Spur) · **SELECT + Y**: NEW ALL (alles neu, spielt los)
- **SELECT + X**: UNDO · **SELECT + Kreuz hoch/runter**: Genre
- **X**: Spur stumm
- **B halten**: P-Lock – Kreuz links/rechts wählt den Wert (Pitch/Note, Decay/Länge, Cutoff, Hall, Delay),
  Kreuz hoch/runter oder linker Stick ändert ihn. **B + A**: Locks des Steps löschen.
  Gelockte Werte erscheinen in Messing-Farbe.
- **B kurz antippen**: Step anhören
- Keys-Spur: **L1 halten + L2/R2** schaltet Akkord an/aus

SONG: Pattern-Pool 4×4 · **A** spielen · **X** kopieren · **Y** einfügen

MIX: **L1 L2 R1 R2 A B X Y** = Mute der 8 Spuren · **SELECT + Taste** = Solo ·
Kreuz links/rechts Spur · Stick L oder Kreuz hoch/runter Lautstärke · Stick R Panorama

FX: Delay (Zeit, Feedback, Pegel) und Hall (Grösse, Dämpfung, Pegel)

SYN: Grundwerte pro Spur (Sample, Zufalls-Sample in %, Pitch, Decay, Cutoff, Sends) · **L1/R1** Spur

## Wenn etwas nicht klappt

- `r36genra/log.txt` enthält Meldungen (Bildschirm, Ton, erkannte Tasten).
- Tasten falsch belegt: in `r36genra/einstellungen.txt` `controller=0` setzen und die Nummern
  anpassen. Mit `tasten_log=1` stehen gedrückte Tastennummern in `log.txt`.
- Bild gedreht: `drehen=90` (oder 180/270) in `einstellungen.txt`.
