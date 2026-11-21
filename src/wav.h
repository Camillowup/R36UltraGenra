// WAV-Datei laden (PCM 8/16/24/32 Bit, Float 32), auf Mono gemischt.
#pragma once
#include <vector>
#include <string>
#include <cstdio>
#include <cstdint>
#include <cstring>

inline bool loadWav(const std::string& path, std::vector<float>& out, float& sr) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    std::vector<uint8_t> d;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 44 || n > 64 * 1024 * 1024) { fclose(f); return false; }
    d.resize(n);
    size_t got = fread(d.data(), 1, n, f);
    fclose(f);
    if (got != size_t(n) || memcmp(d.data(), "RIFF", 4) || memcmp(d.data() + 8, "WAVE", 4)) return false;
    auto u16 = [&](size_t o) { return uint32_t(d[o] | (d[o + 1] << 8)); };
    auto u32 = [&](size_t o) { return uint32_t(d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | (uint32_t(d[o + 3]) << 24)); };
    int fmt = 0, ch = 0, bits = 0;
    size_t dataOff = 0, dataLen = 0;
    for (size_t o = 12; o + 8 <= d.size();) {
        uint32_t len = u32(o + 4);
        if (!memcmp(&d[o], "fmt ", 4) && o + 24 <= d.size()) {
            fmt = int(u16(o + 8)); ch = int(u16(o + 10)); sr = float(u32(o + 12)); bits = int(u16(o + 22));
            if (fmt == 0xFFFE && o + 34 <= d.size()) fmt = int(u16(o + 32));
        } else if (!memcmp(&d[o], "data", 4)) {
            dataOff = o + 8;
            dataLen = std::min<size_t>(len, d.size() - dataOff);
            break;
        }
        o += 8 + len + (len & 1);
    }
    if (!dataOff || ch < 1 || bits < 8 || sr < 1000) return false;
    int bps = bits / 8;
    size_t frames = dataLen / (bps * ch);
    if (frames > size_t(sr * 30)) frames = size_t(sr * 30);  // max. 30 Sekunden
    out.resize(frames);
    for (size_t i = 0; i < frames; i++) {
        float acc = 0;
        for (int c = 0; c < ch; c++) {
            const uint8_t* p = &d[dataOff + (i * ch + c) * bps];
            float v = 0;
            if (fmt == 3 && bits == 32) { float x; memcpy(&x, p, 4); v = x; }
            else if (bits == 8) v = (p[0] - 128) / 128.f;
            else if (bits == 16) v = int16_t(p[0] | (p[1] << 8)) / 32768.f;
            else if (bits == 24) v = (int32_t((p[0] << 8) | (p[1] << 16) | (uint32_t(p[2]) << 24)) >> 8) / 8388608.f;
            else if (bits == 32) v = int32_t(p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24)) / 2147483648.f;
            acc += v;
        }
        out[i] = acc / ch;
    }
    return !out.empty();
}
