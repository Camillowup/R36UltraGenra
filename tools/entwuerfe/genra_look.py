#!/usr/bin/env python3
# Entwurf: R36 GENRA im Look der GENRA-VCV-Module (docs/DESIGN.md im Repo Genra-for-Vcv).
# 5 Seiten, 720x720, gerendert mit 2-facher Aufloesung fuer glatte Kanten.
from PIL import Image, ImageDraw, ImageFont
import math, sys

S = 2                      # Ueberabtastung
W = H = 720
BG, PLATE, LINE, TXT, DIM, DISP = '#17181d', '#212329', '#34363f', '#d9dbe2', '#8a8d99', '#07080a'
DRUM, BASS, MEL, PAD, KEYS, SONG = '#7b6cff', '#b8f23a', '#3dc9ff', '#e0779e', '#2fc2a8', '#ffd23c'
TCOL = [DRUM, DRUM, DRUM, DRUM, PAD, BASS, MEL, KEYS]
MONT = '/usr/share/fonts/truetype/google-fonts/ofl/montserrat/Montserrat[wght].ttf'
MONO = '/usr/share/fonts/truetype/google-fonts/ofl/anonymouspro/'

_fc = {}
def F(size, wght=700):
    k = (size, wght)
    if k not in _fc:
        f = ImageFont.truetype(MONT, int(size * S))
        try: f.set_variation_by_axes([wght])
        except Exception: pass
        _fc[k] = f
    return _fc[k]

def rgb(h, a=255):
    h = h.lstrip('#'); return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), a)

def mixc(a, b, t):
    a, b = rgb(a), rgb(b)
    return '#%02x%02x%02x' % tuple(int(a[i] * (1 - t) + b[i] * t) for i in range(3))

class P:
    def __init__(self):
        self.im = Image.new('RGBA', (W * S, H * S), rgb(BG))
        self.d = ImageDraw.Draw(self.im)
    def rr(self, x, y, w, h, r, fill=None, outline=None, width=1):
        self.d.rounded_rectangle([x * S, y * S, (x + w) * S, (y + h) * S], r * S, fill=fill, outline=outline, width=int(width * S))
    def circ(self, cx, cy, r, fill=None, outline=None, width=1):
        self.d.ellipse([(cx - r) * S, (cy - r) * S, (cx + r) * S, (cy + r) * S], fill=fill, outline=outline, width=int(width * S))
    def line(self, x0, y0, x1, y1, fill, width=1):
        self.d.line([x0 * S, y0 * S, x1 * S, y1 * S], fill=fill, width=int(width * S))
    def text(self, x, y, s, size, fill=TXT, wght=700, sp=0.0, anchor='l'):
        f = F(size, wght)
        sp_px = sp * size * S
        widths = [self.d.textlength(c, font=f) for c in s]
        tw = sum(widths) + sp_px * (len(s) - 1)
        X = x * S - (tw if anchor == 'r' else tw / 2 if anchor == 'm' else 0)
        for c, w in zip(s, widths):
            self.d.text((X, y * S), c, font=f, fill=fill, anchor='lm')
            X += w + sp_px
    def out(self):
        return self.im.resize((W, H), Image.LANCZOS).convert('RGB')

# --------------------------------------------------------------- Bauteile
def knob(p, cx, cy, r, val, ring=None, label=None, lsize=7.5):
    # gepunkteter Ring (Akzent beim Hauptregler, sonst grau)
    rc = ring or '#3a3c45'
    n = 26 if r > 14 else 20
    for k in range(n):
        a = 2 * math.pi * k / n
        p.circ(cx + math.cos(a) * (r + 4), cy + math.sin(a) * (r + 4), 0.9 if not ring else 1.1, fill=rc)
    p.circ(cx, cy, r, fill='#0e0f12')
    p.circ(cx, cy, r - 1.5, fill='#1d1f25', outline='#2a2c33', width=1)
    a = math.radians(-225 + 270 * val)
    p.line(cx + math.cos(a) * r * 0.35, cy + math.sin(a) * r * 0.35, cx + math.cos(a) * r * 0.8, cy + math.sin(a) * r * 0.8, '#f2f3f6', max(2, r * 0.16))
    if label: p.text(cx, cy + r + 13, label, lsize, TXT, 600, 0.18, 'm')

def button(p, cx, cy, r, on=False, col=None, label=None):
    p.circ(cx, cy, r + 2, fill='#0b0c0f')
    p.circ(cx, cy, r, fill=col if on else '#3b3d45')
    if label: p.text(cx, cy + r + 10, label, 7, TXT, 600, 0.18, 'm')

def section(p, x, y, w, h, col, title=None):
    p.rr(x, y + 3, w, h - 3, 5, fill=PLATE)
    p.rr(x, y, w, 4, 2, fill=col)
    if title: p.text(x + w / 2, y + 18, title, 9.5, col, 800, 0.5, 'm')

def dropdown(p, x, y, w, h, text, col, sub=None):
    p.rr(x, y, w, h, 4, fill=DISP, outline=col, width=1.2)
    p.text(x + 9, y + h / 2, text, 8.5, col, 800, 0.16)
    if sub: p.text(x + w - 24, y + h / 2, sub, 6.5, DIM, 600, 0.2, 'r')
    cx, cy = x + w - 12, y + h / 2
    p.line(cx - 4, cy - 2, cx, cy + 2, col, 1.6); p.line(cx, cy + 2, cx + 4, cy - 2, col, 1.6)

PAGES = ['SEQ', 'SONG', 'MIX', 'FX', 'SYN']
PCOL = [DRUM, SONG, TXT, MEL, BASS]

def head(p, page, sub):
    p.text(22, 26, 'GENRA', 22, TXT, 800, 0.22)
    p.text(150, 28, sub, 7.5, DIM, 600, 0.35)
    # Seitenreiter wie GENRAs Akkordtasten
    x = 372
    for i, t in enumerate(PAGES):
        w = 62
        if i == page: p.rr(x, 12, w, 28, 3, fill=PCOL[i] if PCOL[i] != TXT else '#d9dbe2')
        else: p.rr(x, 12, w, 28, 3, fill='#2a2c33')
        p.text(x + w / 2, 26.5, t, 9, '#0b0c0f' if i == page else DIM, 800, 0.15, 'm')
        x += w + 6

def display(p, x, y, w, h):
    p.rr(x, y, w, h, 7, fill=DISP, outline=LINE, width=1.5)

def footer(p, l, r):
    p.text(22, 704, l, 7, DIM, 600, 0.12)
    p.text(698, 704, r, 7, DIM, 600, 0.12, 'r')

# --------------------------------------------------------------- SEQ
def page_seq():
    p = P(); head(p, 0, 'R36 · STEP SEQUENCER')
    display(p, 18, 50, 684, 66)
    p.text(32, 72, 'TECHNO', 19, TXT, 800, 0.08)
    p.text(130, 74, '· MINIMAL', 10, DIM, 700, 0.15)
    p.text(32, 98, 'A MINOR  124 BPM  SWING 52%', 8.5, DIM, 500, 0.1)
    # Pattern- und Takt-Anzeige
    p.rr(300, 62, 64, 24, 3, fill='#2a2c33'); p.text(332, 74, 'PAT 03', 8.5, TXT, 800, 0.1, 'm')
    p.rr(370, 62, 64, 24, 3, fill=DRUM); p.text(402, 74, 'BAR 1/2', 8.5, '#0b0c0f', 800, 0.06, 'm')
    p.text(300, 100, 'NEXT › PAT 04', 8, KEYS, 700, 0.12)
    # Generator rechts: NEW ALL / NEW / UNDO
    for i, (lab, key, col) in enumerate((('NEW ALL', 'SEL+Y', '#5cf08a'), ('NEW', 'Y', None), ('UNDO', 'SEL+X', None))):
        cx = 520 + i * 62
        button(p, cx, 76, 13, col is not None, col)
        p.text(cx, 100, lab, 7, TXT, 700, 0.15, 'm')
        p.text(cx, 110, key, 6, DIM, 600, 0.15, 'm')
    # Spuren
    section(p, 18, 126, 684, 380, DRUM)
    p.rr(18, 126, 336, 4, 2, fill=DRUM)
    p.rr(358, 126, 344, 4, 2, fill=PAD)
    names = ['KICK', 'CLAP', 'HAT', 'PERC', 'PAD', 'BASS', 'LEAD', 'KEYS']
    pats = ['x...x...x...x...', '....x.......x...', '..xx..x...x..x..', '...x.......x....',
            'x...............', '..x...x.....x..x', '......x....x....', '....x.......x...']
    notes = {(5, 2): 'A1', (5, 6): 'A1', (5, 12): 'E2', (5, 15): 'G1', (6, 6): 'C4', (6, 11): 'E4', (4, 0): 'Am', (7, 4): 'Am', (7, 12): 'Fm'}
    play, selT, selS = 6, 1, 12
    gx0, gx1 = 92, 690
    gap, grp = 4, 6
    sw = (gx1 - gx0 - 15 * gap - 3 * grp) / 16
    for t in range(8):
        y = 142 + t * 43 + (8 if t >= 4 else 0)
        col = TCOL[t]
        p.rr(28, y + 6, 4, 22, 2, fill=col)
        p.text(38, y + 17, names[t], 9.5, col if t == selT else TXT, 800, 0.15)
        for i in range(16):
            x = gx0 + i * (sw + gap) + (i // 4) * grp
            on = pats[t][i] == 'x'
            if i == play:
                p.rr(x - 1, y - 1, sw + 2, 36, 4, fill='#2c2f38')
            if on:
                p.rr(x, y, sw, 34, 3, fill=col)
                p.rr(x, y, sw, 5, 2, fill=mixc(col, '#ffffff', 0.35))
                if (t, i) in notes: p.text(x + sw / 2, y + 21, notes[(t, i)], 7.5, '#0b0c0f', 800, 0, 'm')
            else:
                p.rr(x, y, sw, 34, 3, fill=DISP if i % 4 else '#0c0d10', outline='#26282f', width=1)
            if t == selT and i == selS:
                p.rr(x - 3, y - 3, sw + 6, 40, 5, outline='#ffffff', width=2)
    p.line(28, 142 + 4 * 43 - 2, 692, 142 + 4 * 43 - 2, LINE, 1)
    # P-Lock-Sektion mit Reglern
    section(p, 18, 516, 684, 172, DRUM, 'P-LOCK')
    p.text(32, 541, 'CLAP · STEP 13', 8.5, TXT, 800, 0.15)
    p.text(688, 541, 'clap_dry_02.wav', 8, DIM, 600, 0.05, 'r')
    vals = [(0.46, 'PITCH', '−2'), (0.38, 'DECAY', '38'), (0.7, 'CUTOFF', '70'), (0.55, 'REVERB', '55'), (0.25, 'DELAY', '25')]
    for k, (v, lab, num) in enumerate(vals):
        cx = 90 + k * 135
        big = k == 1
        knob(p, cx, 600, 27 if big else 21, v, DRUM if big else None)
        p.text(cx, 645, lab, 8, DRUM if big else TXT, 700, 0.2, 'm')
        p.text(cx, 662, num, 9, '#ffffff' if big else DIM, 800, 0.05, 'm')
    footer(p, 'KREUZ: STEP · A: SETZEN · L2/R2: SAMPLE · L1/R1: PATTERN · X: STUMM', 'B HALTEN: P-LOCK')
    return p.out()

# --------------------------------------------------------------- SONG
def page_song():
    p = P(); head(p, 1, 'R36 · SONG ARRANGER')
    section(p, 18, 52, 300, 444, SONG, 'SONG')
    display(p, 30, 80, 276, 292)
    p.text(42, 96, '#', 6.5, DIM, 600, 0.3); p.text(70, 96, 'PATTERN', 6.5, DIM, 600, 0.3); p.text(250, 96, 'BARS', 6.5, DIM, 600, 0.3)
    rows = [('01', 'P01', 'INTRO', 8, '#6f86c9'), ('02', 'P02', 'VERSE', 16, '#9b7bdc'), ('03', 'P03', 'BUILD', 8, '#e0a72a'),
            ('04', 'P04', 'DROP', 16, '#f0564d'), ('05', 'P06', 'BREAK', 8, '#3aaf9c'), ('06', 'P03', 'BUILD', 8, '#e0a72a'),
            ('07', 'P05', 'DROP', 16, '#f0564d'), ('08', 'P01', 'OUTRO', 8, '#8a8d99')]
    for i, (n, pt, part, bars, col) in enumerate(rows):
        y = 118 + i * 30
        cur = i == 3
        if cur: p.rr(34, y - 12, 268, 24, 2, fill='#3a3216'); p.rr(34, y - 12, 3, 24, 1, fill=SONG)
        c = SONG if cur else TXT
        p.text(42, y, n, 8, c if cur else DIM, 700, 0.1)
        p.text(70, y, pt, 8.5, c, 800, 0.1)
        p.rr(110, y - 7, 4, 14, 1, fill=col)
        p.text(120, y, part, 8.5, col if cur else TXT, 700, 0.15)
        p.text(262, y, str(bars), 8.5, c, 700, 0, 'm')
    p.text(294, 360, 'BAR 37 / 92 · ROW 4 / 8', 6.5, DIM, 600, 0.2, 'r')
    for i, (lab, col, on) in enumerate((('PLAY', '#5cf08a', True), ('STOP', None, False), ('LOOP', SONG, True))):
        button(p, 70 + i * 92, 410, 15, on, col, lab)
    for i, lab in enumerate(('INSERT', 'DELETE', 'UNDO')):
        button(p, 70 + i * 92, 462, 12, False, None, lab)
    # Pattern-Pool 4x4 wie ABLAUF
    section(p, 330, 52, 372, 444, SONG, 'PATTERNS')
    parts = {0: ('INTRO 1', '#6f86c9'), 1: ('VERSE 1', '#9b7bdc'), 2: ('BUILD 1', '#e0a72a'), 3: ('DROP 1', '#f0564d'),
             4: ('DROP 2', '#f0564d'), 5: ('BREAK 1', '#3aaf9c'), 8: ('GENRA 1', '#8a8d99')}
    for i in range(16):
        x = 344 + (i % 4) * 88; y = 82 + (i // 4) * 88
        if i in parts:
            name, col = parts[i]
            p.rr(x, y, 80, 80, 4, fill=mixc(PLATE, col, 0.35))
            p.rr(x + 6, y + 6, 68, 5, 2, fill=SONG)
            p.text(x + 8, y + 24, f'P{i + 1:02d}', 7, DIM if i != 3 else TXT, 700, 0.1)
            p.text(x + 8, y + 42, name, 8.5, TXT, 800, 0.08)
            for k, c in enumerate((DRUM, BASS, MEL, PAD)):
                p.rr(x + 8 + k * 11, y + 64, 8, 4, 2, fill=c if (i + k) % 5 else '#2a2c33')
            if i == 3: p.rr(x - 2, y - 2, 84, 84, 5, outline='#ffffff', width=2.2)
            if i == 4:
                for k in range(10): p.rr(x - 2 + k * 8.6, y - 3, 4.5, 2, 1, fill='#ffffff'); p.rr(x - 2 + k * 8.6, y + 81, 4.5, 2, 1, fill='#ffffff')
        else:
            p.rr(x, y, 80, 80, 4, fill='#1b1c21')
            p.text(x + 8, y + 24, f'P{i + 1:02d}', 7, '#4a4c55', 700, 0.1)
    # Info-Zeile wie ABLAUF
    display(p, 18, 508, 684, 40)
    p.text(32, 528, 'P04  DROP 1', 10, TXT, 800, 0.15); p.text(170, 528, 'DROP', 10, '#f0564d', 800, 0.15)
    p.text(688, 528, 'NEXT  › P06', 9, TXT, 800, 0.12, 'r')
    section(p, 18, 558, 684, 130, SONG, 'PARTS')
    for i, (lab, col) in enumerate((('INTRO', '#6f86c9'), ('VERSE', '#9b7bdc'), ('BUILD', '#e0a72a'), ('DROP', '#f0564d'), ('BREAK', '#3aaf9c'), ('OUTRO', '#8a8d99'))):
        cx = 80 + i * 112
        p.circ(cx, 622, 21, fill='#0b0c0f'); p.circ(cx, 622, 19, fill=col)
        p.text(cx, 656, lab, 8, col, 800, 0.18, 'm')
    footer(p, 'KREUZ: PATTERN · A: SPIELEN · X: KOPIEREN · Y: EINFÜGEN', 'R1: IN SONG EINFÜGEN')
    return p.out()

# --------------------------------------------------------------- MIX
def page_mix():
    p = P(); head(p, 2, 'R36 · MIXER')
    names = ['KICK', 'CLAP', 'HAT', 'PERC', 'PAD', 'BASS', 'LEAD', 'KEYS', 'MAIN']
    keys = ['L1', 'L2', 'R1', 'R2', 'A', 'B', 'X', 'Y', '']
    vols = [84, 70, 58, 62, 52, 80, 60, 55, 75]
    lv = [0.78, 0.55, 0.4, 0, 0.35, 0.7, 0.22, 0.3, 0.72]
    pans = [0.5, 0.42, 0.62, 0.38, 0.5, 0.5, 0.6, 0.4, 0.5]
    sends = [(5, 0), (35, 15), (10, 25), (30, 40), (60, 20), (0, 0), (30, 45), (40, 30), (0, 0)]
    muted, solo, sel = 3, -1, 5
    x = 18
    for i in range(9):
        w = 70 if i < 8 else 84
        if i == 4: x += 6
        if i == 8: x += 6
        col = TCOL[i] if i < 8 else '#d9dbe2'
        p.rr(x, 55, w, 633, 5, fill=PLATE if i != sel else '#262932')
        p.rr(x, 52, w, 4, 2, fill=col)
        if i == sel: p.rr(x, 52, w, 636, 5, outline=col, width=1.2)
        cx = x + w / 2
        p.text(cx, 74, names[i], 8.5, col, 800, 0.15, 'm')
        if i < 8:
            m = i == muted
            p.rr(x + 8, 88, w - 16, 22, 3, fill='#f0564d' if m else '#2a2c33')
            p.text(cx, 99, keys[i], 8.5, '#0b0c0f' if m else DIM, 800, 0.1, 'm')
            p.text(cx, 120, 'MUTE' if m else '', 6, '#f0564d', 700, 0.2, 'm')
        knob(p, cx, 150, 13, pans[i], None)
        p.text(cx, 178, 'C' if abs(pans[i] - 0.5) < 0.01 else ('L%d' % round((0.5 - pans[i]) * 100) if pans[i] < 0.5 else 'R%d' % round((pans[i] - 0.5) * 100)), 6.5, DIM, 600, 0.15, 'm')
        if i < 8:
            for k, (lab, v, c) in enumerate((('REV', sends[i][0], col), ('DLY', sends[i][1], col))):
                yy = 196 + k * 16
                p.text(x + 7, yy, lab, 5.5, DIM, 700, 0.1)
                p.rr(x + 26, yy - 2, w - 34, 4, 2, fill=DISP)
                if v: p.rr(x + 26, yy - 2, (w - 34) * v / 100, 4, 2, fill=c if k == 0 else mixc(c, '#ffffff', 0.4))
        # Fader + Pegel
        fy0, fy1 = 236, 630
        fx = cx - 9
        p.rr(fx - 3, fy0, 6, fy1 - fy0, 3, fill=DISP)
        for k in range(11):
            ty = fy0 + (fy1 - fy0) * k / 10
            p.line(fx - 12, ty, fx - 7, ty, '#3a3c45', 1)
        cy = fy1 - (fy1 - fy0) * vols[i] / 100
        p.rr(fx - 12, cy - 9, 24, 18, 3, fill='#d9dbe2')
        p.line(fx - 9, cy, fx + 9, cy, '#0b0c0f', 1.5)
        mx = cx + 15
        p.rr(mx - 3, fy0, 6, fy1 - fy0, 3, fill=DISP)
        L = 0 if i == muted else (fy1 - fy0) * lv[i]
        if L > 0:
            p.rr(mx - 3, fy1 - L, 6, L, 3, fill=col if i < 8 else '#5cf08a')
        p.text(cx, 656, str(vols[i]), 11, '#ffffff' if i == sel else TXT, 800, 0, 'm')
        p.text(cx, 674, 'LIMIT' if i == 8 else ('VOL'), 6, DIM, 700, 0.2, 'm')
        x += w + 4
    footer(p, 'L1 L2 R1 R2 A B X Y: MUTE · SEL + TASTE: SOLO · KREUZ: SPUR', 'STICK L: LAUT · R: PAN')
    return p.out()

# --------------------------------------------------------------- FX
def page_fx():
    p = P(); head(p, 3, 'R36 · SEND EFFECTS')
    display(p, 18, 52, 684, 110)
    # Echo-Anzeige: abklingende Impulse links/rechts
    p.text(32, 70, 'SEND 1', 7, DIM, 700, 0.3); p.text(32, 92, 'PING-PONG', 16, TXT, 800, 0.08)
    p.text(32, 116, 'DELAY  3/16  ·  FB 45', 7.5, MEL, 700, 0.25)
    for k in range(7):
        h = 46 * (0.62 ** k)
        x = 250 + k * 30
        y0 = 108
        p.rr(x, y0 - h, 8, h, 2, fill=mixc(MEL, DISP, k * 0.1) if k % 2 == 0 else mixc(PAD, DISP, k * 0.1))
    p.text(688, 70, 'SEND 2', 7, DIM, 700, 0.3, 'r'); p.text(688, 92, 'KAMMER', 16, TXT, 800, 0.08, 'r')
    p.text(688, 116, 'HALL  ·  SIZE 70', 7.5, PAD, 700, 0.25, 'r')
    for k in range(40):
        h = 44 * math.exp(-k / 13) * (0.7 + 0.3 * math.sin(k * 1.7))
        p.rr(470 + k * 3.4, 108 - h, 2, h, 1, fill=mixc(PAD, DISP, k / 45))
    for blk in range(2):
        x = 18 + blk * 348
        col = MEL if blk == 0 else PAD
        section(p, x, 176, 336, 290, col, 'DELAY' if blk == 0 else 'HALL')
        dropdown(p, x + 16, 204, 304, 26, 'PING-PONG' if blk == 0 else 'KAMMER', col, 'STEREO' if blk == 0 else 'DARK ROOM')
        knob(p, x + 76, 300, 32, 0.45 if blk == 0 else 0.7, col)
        p.text(x + 76, 350, 'TIME' if blk == 0 else 'SIZE', 8.5, TXT, 700, 0.25, 'm')
        for k, (lab, v) in enumerate((('FDBK', 0.45), ('LEVEL', 0.7)) if blk == 0 else (('DAMP', 0.5), ('LEVEL', 0.7))):
            cx = x + 182 + k * 82
            knob(p, cx, 300, 24, v)
            p.text(cx, 342, lab, 8.5, TXT, 700, 0.25, 'm')
        # Sync-Tasten / Vorverzoegerung
        opts = ('1/16', '1/8', '3/16', '1/4') if blk == 0 else ('S', 'M', 'L', 'XL')
        for k, o in enumerate(opts):
            cx = x + 60 + k * 72
            on = (k == 2) if blk == 0 else (k == 2)
            p.circ(cx, 400, 7, fill=col if on else '#0e0f12')
            p.text(cx, 420, o, 7.5, TXT if on else DIM, 700, 0.15, 'm')
        p.text(x + 168, 446, 'SYNC' if blk == 0 else 'ROOM', 7, DIM, 700, 0.35, 'm')
    # Sends pro Spur
    section(p, 18, 478, 684, 210, '#d9dbe2', 'SENDS PRO SPUR')
    names = ['KICK', 'CLAP', 'HAT', 'PERC', 'PAD', 'BASS', 'LEAD', 'KEYS']
    sends = [(5, 0), (35, 15), (10, 25), (30, 40), (60, 20), (0, 0), (30, 45), (40, 30)]
    for i in range(8):
        cx = 62 + i * 83
        p.text(cx, 518, names[i], 8, TCOL[i], 800, 0.15, 'm')
        knob(p, cx, 556, 15, sends[i][1] / 100, MEL if i == 6 else None)
        p.text(cx, 588, 'DLY', 6.5, DIM, 700, 0.2, 'm')
        knob(p, cx, 624, 15, sends[i][0] / 100, None)
        p.text(cx, 656, 'REV', 6.5, DIM, 700, 0.2, 'm')
    footer(p, 'KREUZ: REGLER · STICK L / ‹›: WERT · L1/R1: MODELL', 'B HALTEN: GROB')
    return p.out()

# --------------------------------------------------------------- SYN (wie FUNKE, Bass-Spur)
def page_syn():
    p = P(); head(p, 4, 'R36 · SYNTH VOICE')
    # Reiter der Instrument-Spuren und Preset-Feld
    for i, (t, c) in enumerate((('PAD', PAD), ('BASS', BASS), ('LEAD', MEL), ('KEYS', KEYS))):
        x = 18 + i * 72
        p.rr(x, 52, 66, 26, 3, fill=c if i == 1 else '#2a2c33')
        p.text(x + 33, 65, t, 9, '#0b0c0f' if i == 1 else c, 800, 0.15, 'm')
    dropdown(p, 318, 52, 384, 26, '07  ACID WURM', BASS)
    display(p, 18, 88, 684, 112)
    p.text(32, 106, 'PLAITS ENGINE 1 / 8', 7, DIM, 700, 0.3)
    p.text(32, 134, 'VIRTUAL ANALOG', 17, TXT, 800, 0.05)
    p.text(32, 184, 'SAW + SUB', 7.5, BASS, 800, 0.3)
    pts = []
    for k in range(200):
        t = k / 199
        ph = (t * 3.0) % 1
        y = 144 - (ph * 2 - 1) * 26 * (0.8 + 0.2 * math.sin(t * 8))
        pts.append((300 + t * 250, y))
    for a, b in zip(pts, pts[1:]):
        p.line(a[0], a[1], b[0], b[1], BASS, 2.2)
    p.text(425, 184, 'LADDER 24 · DRIVE · MONO', 7, DIM, 700, 0.25, 'm')
    p.text(688, 106, '124 BPM', 7, DIM, 700, 0.3, 'r')
    p.text(688, 136, 'A1', 22, TXT, 800, 0.02, 'r')
    p.text(688, 184, 'SLIDE · ACC', 7.5, BASS, 800, 0.3, 'r')
    # Sektionen wie FUNKE
    section(p, 18, 212, 336, 226, BASS, 'OSC')
    dropdown(p, 32, 240, 308, 24, 'VIRTUAL ANALOG', BASS, 'PLAITS')
    knob(p, 82, 318, 30, 0.55, BASS, None)
    p.text(82, 366, 'TIMBRE', 8.5, TXT, 700, 0.25, 'm')
    for k, (lab, v) in enumerate((('HARM', 0.4), ('MORPH', 0.62), ('SUB', 0.7))):
        cx = 170 + k * 64
        knob(p, cx, 310, 20, v); p.text(cx, 344, lab, 8, TXT, 700, 0.22, 'm')
    for k, (lab, on) in enumerate((('MONO', True), ('LEGATO', False), ('POLY', False))):
        cx = 52 + k * 62
        p.circ(cx, 400, 7, fill=BASS if on else '#0e0f12')
        p.text(cx, 420, lab, 7, TXT, 700, 0.2, 'm')
    for k, (lab, v) in enumerate((('GLIDE', 0.3), ('ACCENT', 0.5))):
        cx = 250 + k * 64
        knob(p, cx, 396, 17, v); p.text(cx, 424, lab, 7.5, TXT, 700, 0.22, 'm')
    section(p, 366, 212, 336, 226, BASS, 'FILTER')
    dropdown(p, 380, 240, 308, 24, 'LADDER 24', BASS, 'MOOG-STYLE LP')
    knob(p, 430, 318, 30, 0.42, BASS)
    p.text(430, 366, 'CUTOFF', 8.5, TXT, 700, 0.25, 'm')
    for k, (lab, v) in enumerate((('RES', 0.68), ('ENV', 0.6), ('DRIVE', 0.55))):
        cx = 518 + k * 64
        knob(p, cx, 310, 20, v); p.text(cx, 344, lab, 8, TXT, 700, 0.22, 'm')
    for k, (lab, v) in enumerate((('F.ATK', 0.05), ('F.DEC', 0.35), ('KEY', 0.5))):
        cx = 518 + k * 64
        knob(p, cx, 396, 17, v); p.text(cx, 424, lab, 7.5, TXT, 700, 0.22, 'm')
    section(p, 18, 450, 336, 156, BASS, 'ENVELOPE')
    for k, (lab, v) in enumerate((('A', 0.02), ('D', 0.4), ('S', 0.55), ('R', 0.15))):
        cx = 62 + k * 82
        knob(p, cx, 518, 20, v); p.text(cx, 556, lab, 8.5, TXT, 700, 0.2, 'm')
    section(p, 366, 450, 336, 156, BASS, 'FX')
    for k, (lab, v) in enumerate((('CHORUS', 0.1), ('DIST', 0.45), ('LO-FI', 0.0), ('VERB', 0.2))):
        cx = 410 + k * 82
        knob(p, cx, 518, 20, v); p.text(cx, 556, lab, 8, TXT, 700, 0.2, 'm')
    # Statt Buchsen: Tastenhilfe-Leiste im Stil der GENRA-Leiste
    p.rr(18, 618, 684, 70, 5, fill='#141519')
    p.rr(28, 626, 300, 3, 1, fill=mixc(BASS, '#141519', 0.65))
    for k, (key, lab) in enumerate((('A', 'ANHÖREN'), ('B+STICK', 'WERT'), ('L1/R1', 'SPUR'), ('L2/R2', 'PRESET'))):
        cx = 70 + k * 82
        kw = 22 + 6 * len(key)
        p.rr(cx - kw / 2, 640, kw, 24, 12, fill='#0b0c0f', outline='#6d7080', width=1.5)
        p.text(cx, 652, key, 7.5, TXT, 800, 0.05, 'm')
        p.text(cx, 676, lab, 6.5, DIM, 700, 0.2, 'm')
    p.rr(384, 626, 140, 3, 1, fill='#5a5d68')
    p.text(454, 646, 'R3', 9, TXT, 800, 0.1, 'm'); p.text(454, 672, 'NOTEN-GRID', 6.5, DIM, 700, 0.2, 'm')
    for k, (lab, c) in enumerate((('LEVEL', BASS), ('PAN', BASS))):
        cx = 590 + k * 72
        p.rr(cx - 30, 632, 60, 50, 4, fill=mixc(c, '#141519', 0.72))
        knob(p, cx, 652, 11, 0.7 if k == 0 else 0.5)
        p.text(cx, 674, lab, 6.5, TXT, 700, 0.2, 'm')
    footer(p, '', '')
    return p.out()

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else '.'
    for name, fn in (('1_seq', page_seq), ('2_song', page_song), ('3_mix', page_mix), ('4_fx', page_fx), ('5_syn', page_syn)):
        fn().save(f'{out}/genra_{name}.png')
        print('ok', name)
