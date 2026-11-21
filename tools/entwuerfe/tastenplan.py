#!/usr/bin/env python3
# Tastenplan pro Seite im GENRA-Look: schematischer R36 Ultra mit beschrifteten Tasten.
from PIL import Image, ImageDraw
import genra_look as G
import sys

W, H = 720, 1060
S = G.S

class P2(G.P):
    def __init__(self):
        self.im = Image.new('RGBA', (W * S, H * S), G.rgb(G.BG))
        self.d = ImageDraw.Draw(self.im)
    def out(self):
        return self.im.resize((W, H), Image.LANCZOS).convert('RGB')

# Positionen der Bedienelemente auf dem schematischen Geraet
POS = {
    'L2': (232, 92), 'L1': (290, 92), 'R1': (430, 92), 'R2': (488, 92),
    'KREUZ': (282, 470), 'A': (488, 470), 'B': (450, 508), 'X': (450, 432), 'Y': (412, 470),
    'STICK L': (300, 590), 'STICK R': (420, 590), 'SELECT': (330, 672), 'START': (390, 672),
}

def device(p, col):
    p.rr(206, 104, 308, 640, 34, fill='#24262d', outline='#3a3c45', width=2)
    # Schultertasten
    for k in ('L2', 'L1', 'R1', 'R2'):
        x, y = POS[k]
        p.rr(x - 25, y - 11, 50, 22, 6, fill='#2f3139', outline='#4a4d57', width=1.2)
        p.text(x, y, k, 8, G.DIM, 800, 0.1, 'm')
    # Bildschirm
    p.rr(236, 128, 248, 248, 8, fill=G.DISP, outline='#3a3c45', width=1.5)
    p.text(360, 236, 'GENRA', 15, G.TXT, 800, 0.25, 'm')
    p.text(360, 262, 'R36 ULTRA', 7, G.DIM, 600, 0.4, 'm')
    for i, c in enumerate((G.DRUM, G.BASS, G.MEL, G.PAD)):
        p.rr(318 + i * 22, 286, 16, 4, 2, fill=c)
    # Kreuz
    x, y = POS['KREUZ']
    p.rr(x - 13, y - 40, 26, 80, 5, fill='#121317')
    p.rr(x - 40, y - 13, 80, 26, 5, fill='#121317')
    # ABXY (Nintendo-Anordnung: X oben, A rechts, B unten, Y links)
    for k in 'ABXY':
        bx, by = POS[k]
        p.circ(bx, by, 15, fill='#121317', outline='#4a4d57', width=1.2)
        p.text(bx, by, k, 10, G.DIM, 800, 0, 'm')
    for k in ('STICK L', 'STICK R'):
        sx, sy = POS[k]
        p.circ(sx, sy, 30, fill='#121317', outline='#4a4d57', width=1.5)
        p.circ(sx, sy, 19, fill='#2a2c33')
        p.text(sx, sy, 'L3' if k == 'STICK L' else 'R3', 7, G.DIM, 800, 0, 'm')
    for k in ('SELECT', 'START'):
        sx, sy = POS[k]
        p.rr(sx - 22, sy - 7, 44, 14, 7, fill='#121317', outline='#4a4d57', width=1)
        p.text(sx, sy + 17, k, 6, G.DIM, 700, 0.2, 'm')

def highlight(p, key, col):
    x, y = POS[key]
    if key in 'ABXY':
        p.circ(x, y, 15, fill=col); p.text(x, y, key, 10, '#0b0c0f', 800, 0, 'm')
    elif key.startswith('STICK'):
        p.circ(x, y, 30, outline=col, width=2.5); p.circ(x, y, 19, fill=col)
        p.text(x, y, 'L3' if key == 'STICK L' else 'R3', 7, '#0b0c0f', 800, 0, 'm')
    elif key in ('L1', 'L2', 'R1', 'R2'):
        p.rr(x - 25, y - 11, 50, 22, 6, fill=col); p.text(x, y, key, 8, '#0b0c0f', 800, 0.1, 'm')
    elif key == 'KREUZ':
        p.rr(x - 13, y - 40, 26, 80, 5, fill=col); p.rr(x - 40, y - 13, 80, 26, 5, fill=col)
    else:
        p.rr(x - 22, y - 7, 44, 14, 7, fill=col)

def label(p, side, y, key, lines, col, target):
    # Beschriftungsblock links (x 18..196) oder rechts (x 524..702), Linie zum Bedienelement
    x0 = 18 if side == 'l' else 528
    w = 174
    p.rr(x0, y - 4, w, 22 + 15 * (len(lines) - 1) + 8, 5, fill=G.PLATE)
    p.rr(x0, y - 4, 3, 22 + 15 * (len(lines) - 1) + 8, 1, fill=col)
    p.text(x0 + 12, y + 8, key, 8.5, col, 800, 0.15)
    for i, ln in enumerate(lines):
        p.text(x0 + 12, y + 8 + 15 * (i + 1) - 1, ln, 7.8, G.TXT, 600, 0.04)
    ax = x0 + w if side == 'l' else x0
    tx, ty = target
    mx = ax + (14 if side == 'l' else -14)
    p.line(ax, y + 8, mx, y + 8, '#4a4d57', 1.2)
    p.line(mx, y + 8, tx, ty, '#4a4d57', 1.2)
    p.circ(tx, ty, 2.5, fill=col)

def page(title, col, sub, left, right, combos, note=None):
    p = P2()
    p.text(22, 30, 'GENRA', 22, G.TXT, 800, 0.22)
    p.text(150, 32, 'TASTENPLAN', 8, G.DIM, 700, 0.4)
    p.rr(560, 16, 142, 30, 4, fill=col)
    p.text(631, 31, title, 11, '#0b0c0f', 800, 0.2, 'm')
    p.text(702, 58, sub, 7.5, G.DIM, 600, 0.25, 'r')
    device(p, col)
    used = set()
    for side, items in (('l', left), ('r', right)):
        for (y, key, lines, tgt) in items:
            k = tgt or key
            if lines != ['—']: used.add(k)
            label(p, side, y, key, lines, col, POS[k])
    for k in used: highlight(p, k, col)
    # Kombinationen
    G.section(p, 18, 772, 684, 200, col, 'HALTEN & KOMBIS')
    for i, (k, t) in enumerate(combos):
        cx = 36 + (i % 2) * 342; cy = 812 + (i // 2) * 30
        kw = len(k) * 6.6 + 18 if k else 0
        if k:
            p.rr(cx, cy - 11, kw, 22, 11, fill=G.DISP, outline=col, width=1.2)
            p.text(cx + kw / 2, cy, k, 8, col, 800, 0.05, 'm')
        else:
            prev = combos[i - 2][0] if i >= 2 else ''
            kw = len(prev) * 6.6 + 18
        p.text(cx + kw + 10, cy, t, 8, G.TXT, 600, 0.04)
    # Ueberall gleich
    p.rr(18, 984, 684, 58, 5, fill='#141519')
    p.text(34, 1000, 'ÜBERALL', 7, G.DIM, 800, 0.35)
    glob = [('START', 'PLAY / STOP'), ('SEL + ‹ ›', 'SEITE'), ('SEL 2×', 'MENÜ'), ('START + SEL', 'BEENDEN')]
    x = 34
    for k, t in glob:
        kw = len(k) * 6.4 + 16
        p.rr(x, 1012, kw, 20, 10, fill=G.DISP, outline='#6d7080', width=1)
        p.text(x + kw / 2, 1022, k, 7.5, G.TXT, 800, 0.05, 'm')
        p.text(x + kw + 8, 1022, t, 7.5, G.DIM, 700, 0.1)
        x += kw + 8 + len(t) * 6 + 22
    if note: p.text(702, 1000, note, 6.5, G.DIM, 600, 0.15, 'r')
    return p.out()

def seq():
    left = [(84, 'L2', ['SAMPLE (DRUMS)', 'NOTE − (INSTRUMENTE)'], None),
            (150, 'L1', ['PATTERN −'], None),
            (440, 'KREUZ', ['STEP UND SPUR WÄHLEN'], None),
            (560, 'STICK L', ['FILTER: LINKS TIEFPASS,', 'RECHTS HOCHPASS · LOS = AUS'], None),
            (652, 'SELECT', ['UMSCHALTTASTE (SEL)'], None)]
    right = [(84, 'R2', ['SAMPLE / NOTE +'], None),
             (150, 'R1', ['PATTERN +'], None),
             (370, 'X', ['SPUR STUMM'], None),
             (416, 'Y', ['NEW: SPUR NEU WÜRFELN'], None),
             (462, 'A', ['STEP SETZEN / LÖSCHEN'], None),
             (508, 'B', ['TIPPEN: ANHÖREN', 'HALTEN: P-LOCK'], None),
             (580, 'STICK R', ['STUTTER: RICHTUNG = TEMPO,', 'UNTEN = FREEZE · LOS = WEITER'], None),
             (652, 'START', ['PLAY / STOP'], None)]
    combos = [('SEL + Y', 'NEW ALL (SPIELT LOS)'), ('SEL + X', 'UNDO'),
              ('SEL + AUF/AB', 'GENRE'), ('SEL + L1/R1', 'VARIANTE A–D DER SPUR'),
              ('B + ‹ ›', 'P-LOCK-WERT WÄHLEN'), ('B + AUF/AB / STICK L', 'P-LOCK ÄNDERN'),
              ('R3 HALTEN + ‹ ›', 'PHRASE ROTATION'), ('R3 TIPPEN', 'NOTEN-GRID (INSTRUMENTE)'),
              ('B + A', 'LOCKS DES STEPS LÖSCHEN'), ('L1 + L2/R2', 'KEYS: AKKORD AN/AUS')]
    return page('SEQ', G.DRUM, 'SEQUENCER · P-LOCKS · LIVE-EFFEKTE', left, right, combos)

def song():
    left = [(84, 'L2', ['TEIL DER ZEILE −', '(INTRO … OUTRO)'], None),
            (150, 'L1', ['ZEILE AUS SONG LÖSCHEN'], None),
            (440, 'KREUZ', ['PAD BZW. ZEILE WÄHLEN'], None),
            (560, 'STICK L', ['FILTER (WIE AUF SEQ)'], None),
            (652, 'SELECT', ['UMSCHALTTASTE (SEL)'], None)]
    right = [(84, 'R2', ['TEIL DER ZEILE +'], None),
             (150, 'R1', ['PATTERN IN SONG EINFÜGEN'], None),
             (370, 'X', ['PATTERN KOPIEREN'], None),
             (416, 'Y', ['PATTERN EINFÜGEN'], None),
             (462, 'A', ['PATTERN SPIELEN', '(AB NÄCHSTEM TAKT)'], None),
             (508, 'B', ['LISTE / PADS WECHSELN'], None),
             (580, 'STICK R', ['STUTTER (WIE AUF SEQ)'], None),
             (652, 'START', ['PLAY / STOP'], None)]
    combos = [('SEL + X', 'UNDO'), ('SEL + A', 'SONG AB DIESER ZEILE'),
              ('B + AUF/AB', 'TAKTE DER ZEILE'), ('SEL + Y', 'LOOP AN/AUS')]
    return page('SONG', G.SONG, 'PATTERN-POOL · SONG-LISTE', left, right, combos)

def mix():
    left = [(84, 'L2', ['MUTE CLAP'], None),
            (150, 'L1', ['MUTE KICK'], None),
            (440, 'KREUZ', ['‹ ›: KANAL WÄHLEN', 'AUF/AB: LAUTSTÄRKE FEIN'], None),
            (560, 'STICK L', ['LAUTSTÄRKE'], None),
            (652, 'SELECT', ['HALTEN + TASTE = SOLO'], None)]
    right = [(84, 'R2', ['MUTE PERC'], None),
             (150, 'R1', ['MUTE HAT'], None),
             (370, 'X', ['MUTE LEAD'], None),
             (416, 'Y', ['MUTE KEYS'], None),
             (462, 'A', ['MUTE PAD'], None),
             (508, 'B', ['MUTE BASS'], None),
             (580, 'STICK R', ['PANORAMA'], None),
             (652, 'START', ['PLAY / STOP'], None)]
    combos = [('SEL + TASTE', 'SOLO DER SPUR'), ('SEL + AUF/AB', 'ALLE MUTES AUFHEBEN')]
    return page('MIX', '#d9dbe2', 'MUTE · SOLO · LAUTSTÄRKE · PAN', left, right, combos)

def fx():
    left = [(84, 'L2', ['SYNC-WERT −'], None),
            (150, 'L1', ['MODELL − (DELAY/HALL)'], None),
            (440, 'KREUZ', ['REGLER WÄHLEN'], None),
            (560, 'STICK L', ['WERT ÄNDERN'], None),
            (652, 'SELECT', ['UMSCHALTTASTE (SEL)'], None)]
    right = [(84, 'R2', ['SYNC-WERT +'], None),
             (150, 'R1', ['MODELL +'], None),
             (370, 'X', ['DUCKING (KICK) AN/AUS'], None),
             (416, 'Y', ['EFFEKT AN/AUS'], None),
             (462, 'A', ['TESTTON (SNARE) ANHÖREN'], None),
             (508, 'B', ['HALTEN: GROBE SCHRITTE'], None),
             (580, 'STICK R', ['—'], None),
             (652, 'START', ['PLAY / STOP'], None)]
    combos = [('B + AUF/AB', 'WERT IN 10ER-SCHRITTEN'), ('SEL + X', 'UNDO')]
    return page('FX', G.MEL, 'DELAY · HALL · DUCKING · SENDS', left, right, combos)

def syn():
    left = [(84, 'L2', ['PRESET −'], None),
            (150, 'L1', ['SPUR −'], None),
            (440, 'KREUZ', ['REGLER WÄHLEN'], None),
            (560, 'STICK L', ['MIT B: WERT ÄNDERN'], None),
            (652, 'SELECT', ['UMSCHALTTASTE (SEL)'], None)]
    right = [(84, 'R2', ['PRESET +'], None),
             (150, 'R1', ['SPUR +'], None),
             (370, 'X', ['KLANG / GROOVE WECHSELN'], None),
             (416, 'Y', ['PRESET SPEICHERN'], None),
             (462, 'A', ['ANHÖREN'], None),
             (508, 'B', ['HALTEN + STICK L / AUF/AB:', 'WERT ÄNDERN'], None),
             (580, 'STICK R', ['—'], None),
             (652, 'START', ['PLAY / STOP'], None)]
    combos = [('B + STICK L', 'REGLER DREHEN'), ('B + AUF/AB', 'FEIN IN 1ER-SCHRITTEN'),
              ('SEL + X', 'UNDO'), ('SEL + Y', 'PRESET WÜRFELN'),
              ('X (DRUM-SPUR)', 'GROOVE: 808/909/MINIMAL,'), ('B + A', 'REGLER AUF STANDARD'), ('', 'SWING, VELO-ZUFALL, LÄNGE')]
    return page('SYN', G.BASS, 'KLANG · GROOVE PRO SPUR', left, right, combos)

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else '.'
    for n, fn in (('1_seq', seq), ('2_song', song), ('3_mix', mix), ('4_fx', fx), ('5_syn', syn)):
        fn().save(f'{out}/tasten_{n}.png'); print('ok', n)
