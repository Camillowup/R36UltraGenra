// Kleiner Software-Renderer fuer 720x720 (ARGB8888). Kein SDL noetig.
#pragma once
#include <cstdint>
#include <vector>
#include <string>

struct FontData;

namespace gfx {

constexpr int W = 720, H = 720;
using Col = uint32_t;  // 0xAARRGGBB

inline Col rgb(uint32_t hex, int a = 255) { return (uint32_t(a) << 24) | (hex & 0xFFFFFF); }
Col mix(Col a, Col b, float t);

enum FontId { XS, S, M, D, L };

struct Canvas {
    std::vector<uint32_t> px;
    int clipX0 = 0, clipY0 = 0, clipX1 = W, clipY1 = H;
    Canvas() : px(W * H, 0xFF000000) {}
    void blend(int x, int y, Col c, int cov = 255);  // cov = Deckung 0..255
    void clear(Col c);
    void fillRect(int x, int y, int w, int h, Col c);
    // Abgerundetes Rechteck, Radien: oben-links, oben-rechts, unten-rechts, unten-links
    void roundRect(float x, float y, float w, float h, const float r[4], Col top, Col bottom);
    void roundRect(float x, float y, float w, float h, float r, Col c) {
        float rr[4] = {r, r, r, r};
        roundRect(x, y, w, h, rr, c, c);
    }
    void roundRect(float x, float y, float w, float h, float r, Col top, Col bottom) {
        float rr[4] = {r, r, r, r};
        roundRect(x, y, w, h, rr, top, bottom);
    }
    void roundStroke(float x, float y, float w, float h, const float r[4], Col c, float t = 1.f);
    void roundStroke(float x, float y, float w, float h, float r, Col c, float t = 1.f) {
        float rr[4] = {r, r, r, r};
        roundStroke(x, y, w, h, rr, c, t);
    }
    // Ellipse mit radialem Verlauf (Mitte -> Rand)
    void ellipse(float cx, float cy, float rx, float ry, Col inner, Col outer, float hx = 0.5f, float hy = 0.5f);
    void glow(float x, float y, float w, float h, float r, Col c, int spread);
    void line(float x0, float y0, float x1, float y1, Col c, float t = 1.5f);
    int text(int x, int y, const std::string& s, FontId f, Col c, int spacing = 0);  // gibt Breite zurueck
    static int textW(const std::string& s, FontId f, int spacing = 0);
    static int fontH(FontId f);
};

}  // namespace gfx
