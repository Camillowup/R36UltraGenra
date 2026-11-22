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

static App app;

static void tap(int b, int mod = -1) {
    Input x;
    x.held[b] = true;
    if (mod >= 0) x.held[mod] = true;
    if (mod >= 0) { Input m; m.held[mod] = true; app.update(m, 0.03); }
    app.update(x, 0.03);
    if (mod >= 0) { Input m; m.held[mod] = true; app.update(m, 0.03); }
    app.update(Input(), 0.03);
}

static void render(std::vector<float>& audio, int seconds) {
    const int SR = 48000;
    audio.assign(SR * seconds * 2, 0.f);
    for (int i = 0; i < SR * seconds; i += 512) app.eng.process(&audio[i * 2], std::min(512, SR * seconds - i));
}

int main(int argc, char** argv) {
    std::string out = argc > 1 ? argv[1] : ".";
    app.init(out, 48000);
    gfx::Canvas c;
    tap(B_Y, B_SELECT);                     // NEW ALL (spielt los)
    tap(B_DOWN);                            // CLAP
    for (int i = 0; i < 12; i++) tap(B_RIGHT);
    std::vector<float> audio;
    auto t0 = std::chrono::steady_clock::now();
    render(audio, 8);
    double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    saveWav(audio, out + "/test_audio.wav", 48000);
    float pk = 0, rms = 0;
    for (float v : audio) { pk = std::max(pk, std::fabs(v)); rms += v * v; }
    printf("audio peak %.3f rms %.3f bpm %d genre %d · 8 s in %.2f s (Last %.1f %%)\n", pk, std::sqrt(rms / audio.size()), app.prj.bpm,
           app.prj.genre, secs, secs / 8 * 100);
    // Stopp: nach 2 s darf nichts mehr klingen (haengende Toene)
    tap(B_START);
    render(audio, 3);
    float tail = 0;
    for (size_t i = audio.size() * 2 / 3; i < audio.size(); i++) tail = std::max(tail, std::fabs(audio[i]));
    printf("nach Stopp (2-3 s): peak %.5f %s\n", tail, tail < 0.01f ? "OK" : "HAENGT!");
    tap(B_START);
    render(audio, 1);

    auto shot = [&](const char* name) {
        auto a = std::chrono::steady_clock::now();
        app.draw(c);
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count();
        printf("draw %-10s %.2f ms\n", name, ms);
        savePPM(c, out + "/seite_" + name + ".ppm");
    };
    shot("seq");
    const char* names[] = {"song", "mix", "fx", "syn"};
    for (int p = 0; p < 4; p++) {
        tap(B_RIGHT, B_SELECT);
        if (p == 0) { for (int k = 0; k < 3; k++) tap(B_R1); tap(B_B); tap(B_RIGHT); tap(B_R1); tap(B_R2); tap(B_B); }
        if (p == 2) { tap(B_RIGHT); }
        if (p == 3) { tap(B_DOWN); tap(B_RIGHT); }
        render(audio, 1);
        shot(names[p]);
    }
    tap(B_X);  // GROOVE-Ansicht
    shot("syn_groove");
    tap(B_X);
    tap(B_L1); tap(B_L1); tap(B_L1); tap(B_L1); tap(B_L1);  // KICK
    shot("syn_drum");
    tap(B_L1);  // SAMPLE
    shot("syn_sample");
    tap(B_SELECT); tap(B_SELECT);
    app.draw(c);
    savePPM(c, out + "/seite_menu.ppm");
    // MIDI-Test
    for (int k = 0; k < 8; k++) tap(B_DOWN);
    tap(B_A);
    app.draw(c);
    savePPM(c, out + "/seite_midi.ppm");
    gfx::Canvas k;
    k.clear(0x17181d);
    for (int i = 0; i < 3; i++) drawKrabboAt(k, 120 + i * 200, 360, 2.f, i * 0.3f, -1 + i);
    savePPM(k, out + "/krabbo.ppm");
    return 0;
}
