// Test ohne Bildschirm: rendert Seiten als PPM-Bilder und einige Sekunden Audio als WAV.
// Aufruf: r36genra_test <ausgabeordner>
#include "app.h"
#include <cstdio>
#include <chrono>
#include <cmath>
#include <string>

static void savePPM(const gfx::Canvas& c, const std::string& p) {
    FILE* f = fopen(p.c_str(), "wb");
    fprintf(f, "P6 %d %d 255\n", gfx::W, gfx::H);
    for (uint32_t v : c.px) { unsigned char b[3] = {uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)}; fwrite(b, 1, 3, f); }
    fclose(f);
}

static void saveWav(const std::vector<float>& d, const std::string& p, int sr) {
    FILE* f = fopen(p.c_str(), "wb");
    uint32_t n = uint32_t(d.size() * 2), v32;
    uint16_t v16;
    fwrite("RIFF", 1, 4, f); v32 = 36 + n; fwrite(&v32, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v32 = 16; fwrite(&v32, 4, 1, f); v16 = 1; fwrite(&v16, 2, 1, f); v16 = 2; fwrite(&v16, 2, 1, f);
    v32 = sr; fwrite(&v32, 4, 1, f); v32 = sr * 4; fwrite(&v32, 4, 1, f); v16 = 4; fwrite(&v16, 2, 1, f); v16 = 16; fwrite(&v16, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f);
    for (float x : d) { int16_t s = int16_t(std::max(-1.f, std::min(1.f, x)) * 32000); fwrite(&s, 2, 1, f); }
    fclose(f);
}

int main(int argc, char** argv) {
    std::string out = argc > 1 ? argv[1] : ".";
    static App app;
    app.init(out, 44100);
    gfx::Canvas c;
    Input in;
    auto tap = [&](int b, bool sel = false) {
        Input x; x.held[b] = true; if (sel) x.held[B_SELECT] = true;
        app.update(x, 0.03); app.update(Input(), 0.03);
    };
    // NEW ALL
    tap(B_Y, true);
    // Cursor auf CLAP, Step 13, P-Lock-Feld
    tap(B_DOWN); for (int i = 0; i < 12; i++) tap(B_RIGHT);
    // Audio 8 Sekunden rendern (spielt nach NEW ALL)
    std::vector<float> audio(44100 * 8 * 2);
    for (int i = 0; i < 44100 * 8; i += 512) app.eng.process(&audio[i * 2], std::min(512, 44100 * 8 - i));
    saveWav(audio, out + "/test_audio.wav", 44100);
    float pk = 0, rms = 0;
    for (float v : audio) { pk = std::max(pk, std::fabs(v)); rms += v * v; }
    printf("audio peak %.3f rms %.3f bpm %d genre %d\n", pk, std::sqrt(rms / audio.size()), app.prj.bpm, app.prj.genre);

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 20; i++) app.draw(c);
    auto t1 = std::chrono::steady_clock::now();
    printf("draw SEQ: %.2f ms\n", std::chrono::duration<double, std::milli>(t1 - t0).count() / 20);
    savePPM(c, out + "/seite_seq.ppm");
    const char* names[] = {"song", "mix", "fx", "syn"};
    for (int p = 0; p < 4; p++) {
        Input x; x.held[B_SELECT] = true; x.held[B_RIGHT] = true;
        app.update(x, 0.03); app.update(Input(), 0.5);
        t0 = std::chrono::steady_clock::now();
        app.draw(c);
        t1 = std::chrono::steady_clock::now();
        printf("draw %s: %.2f ms\n", names[p], std::chrono::duration<double, std::milli>(t1 - t0).count());
        savePPM(c, out + "/seite_" + names[p] + ".ppm");
    }
    // Menue (Select doppelt)
    app.update(Input(), 1.0);
    tap(B_SELECT); tap(B_SELECT);
    app.draw(c);
    savePPM(c, out + "/seite_menu.ppm");
    // Krabbo
    gfx::Canvas k;
    for (int i = 0; i < 3; i++) drawKrabboAt(k, 120 + i * 200, 360, 2.f, i * 0.3f, -1 + i);
    savePPM(k, out + "/krabbo.ppm");
    return 0;
}
