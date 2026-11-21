#!/usr/bin/env python3
# Erzeugt src/font_data.h aus Fira Sans (SIL OFL 1.1, siehe docs/FONT-OFL.txt).
# Aufruf: python3 tools/makefont.py <FiraSans-SemiBold.ttf> <FiraSans-Bold.ttf>
import sys
from PIL import Image, ImageDraw, ImageFont

semi, bold = sys.argv[1], sys.argv[2]
# (name, datei, pixelgroesse)
SIZES = [("XS", semi, 10), ("S", semi, 12), ("M", bold, 14), ("L", bold, 24)]
EXTRA = "ÄÖÜäöüß·‹›−×↑↓←→"
CHARS = [chr(c) for c in range(32, 127)] + list(EXTRA)

out = ["// Automatisch erzeugt von tools/makefont.py - nicht von Hand aendern.",
       "// Schrift: Fira Sans (SIL Open Font License 1.1)", "#pragma once", "#include <cstdint>",
       "struct Glyph { int16_t x, w, adv, ox; };",
       "static const char32_t kExtraChars[] = U\"" + EXTRA + "\";",
       "struct FontData { const char* name; int h, ascent, atlasW; const uint8_t* atlas; Glyph g[95 + %d]; };" % len(EXTRA), ""]
fonts = []
for name, path, px in SIZES:
    f = ImageFont.truetype(path, px)
    ascent, descent = f.getmetrics()
    h = ascent + descent
    glyphs = []
    x = 0
    imgs = []
    for ch in CHARS:
        l, t, r, b = f.getbbox(ch)
        adv = int(round(f.getlength(ch)))
        w = max(1, r - min(l, 0) + 1)
        ox = min(l, 0)
        im = Image.new("L", (w, h), 0)
        ImageDraw.Draw(im).text((-ox, 0), ch, font=f, fill=255)
        imgs.append((x, im))
        glyphs.append((x, w, adv, ox))
        x += w + 1
    W = x
    atlas = Image.new("L", (W, h), 0)
    for gx, im in imgs:
        atlas.paste(im, (gx, 0))
    data = atlas.tobytes()
    out.append(f"static const uint8_t atlas_{name}[] = {{")
    for i in range(0, len(data), 32):
        out.append("  " + ",".join(str(v) for v in data[i:i + 32]) + ",")
    out.append("};")
    fonts.append((name, h, ascent, W, glyphs))
for name, h, ascent, W, glyphs in fonts:
    gs = ",".join("{%d,%d,%d,%d}" % g for g in glyphs)
    out.append(f'static const FontData font_{name} = {{"{name}", {h}, {ascent}, {W}, atlas_{name}, {{{gs}}}}};')
open("src/font_data.h", "w").write("\n".join(out) + "\n")
print("ok", [(f[0], f[1], f[3]) for f in fonts])
