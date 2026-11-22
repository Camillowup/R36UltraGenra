#include "gfx.h"
#include "font_data.h"
#include <cmath>
#include <algorithm>

namespace gfx {

static const FontData* fonts[] = {&font_XS, &font_S, &font_M, &font_D, &font_L};

// UTF-8 lesen und Glyph-Index bestimmen
static int nextGlyph(const std::string& s, size_t& i) {
    unsigned char c = s[i++];
    char32_t cp = c;
    if (c >= 0xF0 && i + 2 < s.size() + 1) { cp = ((c & 7) << 18) | ((s[i] & 63) << 12) | ((s[i + 1] & 63) << 6) | (s[i + 2] & 63); i += 3; }
    else if (c >= 0xE0) { cp = ((c & 15) << 12) | ((s[i] & 63) << 6) | (s[i + 1] & 63); i += 2; }
    else if (c >= 0xC0) { cp = ((c & 31) << 6) | (s[i] & 63); i += 1; }
    if (cp >= 32 && cp <= 126) return int(cp - 32);
    for (int k = 0; kExtraChars[k]; k++)
        if (kExtraChars[k] == cp) return 95 + k;
    return '?' - 32;
}

Col mix(Col a, Col b, float t) {
    t = std::clamp(t, 0.f, 1.f);
    auto ch = [&](int s) {
        float x = ((a >> s) & 255) * (1 - t) + ((b >> s) & 255) * t;
        return uint32_t(x + 0.5f) << s;
    };
    return ch(24) | ch(16) | ch(8) | ch(0);
}

void Canvas::blend(int x, int y, Col c, int cov) {
    if (x < clipX0 || y < clipY0 || x >= clipX1 || y >= clipY1) return;
    int a = ((c >> 24) * cov) / 255;
    if (a <= 0) return;
    uint32_t& d = px[y * W + x];
    if (a >= 255) { d = c | 0xFF000000; return; }
    int ia = 255 - a;
    uint32_t r = (((c >> 16) & 255) * a + ((d >> 16) & 255) * ia) / 255;
    uint32_t g = (((c >> 8) & 255) * a + ((d >> 8) & 255) * ia) / 255;
    uint32_t b = ((c & 255) * a + (d & 255) * ia) / 255;
    d = 0xFF000000 | (r << 16) | (g << 8) | b;
}

void Canvas::clear(Col c) { std::fill(px.begin(), px.end(), c | 0xFF000000); }

void Canvas::fillRect(int x, int y, int w, int h, Col c) {
    for (int j = std::max(y, clipY0); j < std::min(y + h, clipY1); j++)
        for (int i = std::max(x, clipX0); i < std::min(x + w, clipX1); i++) blend(i, j, c);
}

// Abstand eines Punktes zur Kante eines abgerundeten Rechtecks (negativ = innen)
static float rrDist(float px_, float py, float x, float y, float w, float h, const float r[4]) {
    float cx = x + w * 0.5f, cy = y + h * 0.5f;
    float dx = px_ - cx, dy = py - cy;
    float rad = dx < 0 ? (dy < 0 ? r[0] : r[3]) : (dy < 0 ? r[1] : r[2]);
    rad = std::min(rad, std::min(w, h) * 0.5f);
    float qx = std::fabs(dx) - (w * 0.5f - rad), qy = std::fabs(dy) - (h * 0.5f - rad);
    float ox = std::max(qx, 0.f), oy = std::max(qy, 0.f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.f) - rad;
}

void Canvas::roundRect(float x, float y, float w, float h, const float r[4], Col top, Col bottom) {
    int x0 = int(std::floor(x)), y0 = int(std::floor(y)), x1 = int(std::ceil(x + w)), y1 = int(std::ceil(y + h));
    for (int j = std::max(y0, clipY0); j < std::min(y1, clipY1); j++) {
        Col c = top == bottom ? top : mix(top, bottom, (j + 0.5f - y) / h);
        for (int i = std::max(x0, clipX0); i < std::min(x1, clipX1); i++) {
            float d = rrDist(i + 0.5f, j + 0.5f, x, y, w, h, r);
            if (d >= 0.5f) continue;
            int cov = d <= -0.5f ? 255 : int((0.5f - d) * 255);
            blend(i, j, c, cov);
        }
    }
}

void Canvas::roundStroke(float x, float y, float w, float h, const float r[4], Col c, float t) {
    int x0 = int(std::floor(x)) - 1, y0 = int(std::floor(y)) - 1, x1 = int(std::ceil(x + w)) + 1, y1 = int(std::ceil(y + h)) + 1;
    for (int j = std::max(y0, clipY0); j < std::min(y1, clipY1); j++)
        for (int i = std::max(x0, clipX0); i < std::min(x1, clipX1); i++) {
            float d = rrDist(i + 0.5f, j + 0.5f, x, y, w, h, r);
            float e = std::fabs(d + t * 0.5f) - t * 0.5f;  // Abstand zur Linie
            if (e >= 0.5f) continue;
            int cov = e <= -0.5f ? 255 : int((0.5f - e) * 255);
            blend(i, j, c, cov);
        }
}

void Canvas::ellipse(float cx, float cy, float rx, float ry, Col inner, Col outer, float hx, float hy) {
    float fx = cx + (hx - 0.5f) * 2 * rx, fy = cy + (hy - 0.5f) * 2 * ry;
    for (int j = std::max(int(cy - ry) - 1, clipY0); j < std::min(int(cy + ry) + 2, clipY1); j++)
        for (int i = std::max(int(cx - rx) - 1, clipX0); i < std::min(int(cx + rx) + 2, clipX1); i++) {
            float dx = (i + 0.5f - cx) / rx, dy = (j + 0.5f - cy) / ry;
            float d = std::sqrt(dx * dx + dy * dy);
            float edge = (1 - d) * std::min(rx, ry);
            if (edge <= -0.5f) continue;
            int cov = edge >= 0.5f ? 255 : int((edge + 0.5f) * 255);
            float gx = (i + 0.5f - fx) / rx, gy = (j + 0.5f - fy) / ry;
            float g = std::min(1.f, std::sqrt(gx * gx + gy * gy) / 1.3f);
            blend(i, j, inner == outer ? inner : mix(inner, outer, g), cov);
        }
}

void Canvas::glow(float x, float y, float w, float h, float r, Col c, int spread) {
    int a0 = c >> 24;
    for (int k = spread; k >= 1; k--) {
        int a = a0 * (spread - k + 1) / (spread * spread / 2 + 1);
        roundRect(x - k, y - k, w + 2 * k, h + 2 * k, r + k, (c & 0xFFFFFF) | (uint32_t(std::min(a, 255)) << 24));
    }
}

void Canvas::line(float x0, float y0, float x1, float y1, Col c, float t) {
    float len = std::hypot(x1 - x0, y1 - y0);
    int n = std::max(1, int(len * 2));
    for (int k = 0; k <= n; k++) {
        float u = float(k) / n;
        ellipse(x0 + (x1 - x0) * u, y0 + (y1 - y0) * u, t * 0.5f + 0.3f, t * 0.5f + 0.3f, c, c);
    }
}

int Canvas::text(int x, int y, const std::string& s, FontId fid, Col c, int spacing) {
    const FontData& f = *fonts[fid];
    int cx = x;
    for (size_t i = 0; i < s.size();) {
        const Glyph& g = f.g[nextGlyph(s, i)];
        for (int j = 0; j < f.h; j++)
            for (int k = 0; k < g.w; k++) {
                int a = f.atlas[j * f.atlasW + g.x + k];
                if (a) blend(cx + g.ox + k, y + j, c, a);
            }
        cx += g.adv + spacing;
    }
    return cx - x - (s.empty() ? 0 : spacing);
}

int Canvas::textW(const std::string& s, FontId fid, int spacing) {
    const FontData& f = *fonts[fid];
    int w = 0;
    for (size_t i = 0; i < s.size();) w += f.g[nextGlyph(s, i)].adv + spacing;
    return s.empty() ? 0 : w - spacing;
}

int Canvas::fontH(FontId f) { return fonts[f]->h; }

}  // namespace gfx
