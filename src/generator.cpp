#include "generator.h"
#include "presets.h"
#include <algorithm>

const char* const kGenreNames[NGENRE] = {"BOOM BAP", "LO-FI", "DEEP HOUSE", "TECHNO", "MINIMAL",
                                         "UK GARAGE", "JUNGLE/DNB", "DUB", "ELECTRO", "SYNTHWAVE"};

// Wahrscheinlichkeit pro Step: '0' = nie ... '9' = immer
struct Genre {
    int bpmLo, bpmHi, swing, scale;
    const char* drum[4];
    const char* bass;
    int bassLen;      // Laenge in Steps
    int pad;          // 0 kein Pad, 1 ein Akkord pro Takt, 2 zwei Akkorde
    const char* keys; // Akkord-Stabs
    int keysLen;
    int lead;         // Dichte 0..9
};

static const Genre G[NGENRE] = {
    {86, 94, 62, 0, {"9000000590900030", "0000900000009000", "9070907090709070", "0000000300000020"}, "9000000060900000", 2, 1, "9000000000009000", 3, 2},
    {74, 84, 60, 2, {"9000003009000000", "0000900000009003", "7050705070507050", "0002000000300000"}, "9000006000900000", 3, 2, "9000000090000000", 6, 2},
    {120, 124, 54, 2, {"9000900090009000", "0000900000009000", "0090009000900090", "0030000300300002"}, "0090000900900009", 1, 2, "0090000000900000", 1, 1},
    {128, 134, 50, 0, {"9000900090009000", "0000500000005000", "1191119111911191", "0003000300030003"}, "0090009000900090", 1, 1, "0000000000000000", 1, 1},
    {122, 126, 52, 0, {"9000900090009000", "0000000500000000", "0050005000500050", "0300030003300300"}, "0060006000600060", 1, 0, "0000003000000000", 1, 1},
    {130, 134, 64, 0, {"9000000090100000", "0000900000009000", "0090009000900990", "0030000000300030"}, "9000005000900500", 2, 1, "0000009000000900", 1, 1},
    {170, 174, 50, 0, {"9000000000900000", "0000900300009030", "5555555555555555", "0000003000000300"}, "9000000000000000", 8, 1, "0000000000000000", 1, 3},
    {72, 80, 56, 0, {"0000000090000000", "0000000090000000", "5050505050505050", "0000300000003000"}, "9000003090000000", 3, 0, "0000900000009000", 1, 1},
    {126, 130, 50, 0, {"9000000090300000", "0000900000009000", "7777777777777777", "0000000000000900"}, "9009000090090000", 1, 0, "0000000000000000", 1, 4},
    {96, 108, 50, 0, {"9000000090000000", "0000900000009000", "7070707070707070", "0000000000000357"}, "9090909090909090", 1, 2, "0000000000000000", 1, 4},
};

struct Rng {
    uint32_t s;
    float f() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s & 0xFFFFFF) / float(0x1000000); }
    int i(int n) { return std::min(n - 1, int(f() * n)); }
};

static int degNote(int root, int scale, int deg, int octBase) {
    int n;
    const int* sc = scaleSteps(scale, n);
    int o = deg >= 0 ? deg / n : -((-deg + n - 1) / n);
    int d = deg - o * n;
    return root + octBase + sc[d] + 12 * o;
}

static uint8_t lenToDecay(int steps) { return uint8_t(std::clamp((steps - 1) * 100 / 15, 0, 100)); }

void initProject(Project& p) {
    p = Project();
    for (int t = 0; t < NT; t++) p.tr[t] = Track();
    p.tr[T_HAT].vol = 62;
    p.tr[T_PERC].vol = 60;
    p.tr[T_CLAP].rev = 15;
    p.tr[T_PAD].vol = 60;
    p.tr[T_BASS].vol = 78;
    p.tr[T_LEAD].vol = 58;
    p.tr[T_SAMPLE].vol = 62;
    for (int t = T_PAD; t <= T_SAMPLE; t++) {
        int n;
        const Preset* ps = presetList(t, n);
        if (ps) applyPreset(p.tr[t], ps[0]);
        p.tr[t].preset = 0;
    }
}

// Noten einer Spur in eine Variante schreiben
static void genTrack(Project& p, int t, int v, Rng& r, const Genre& g, const int* prog) {
    Pattern& pat = p.pat[p.curPat];
    int len = pat.len;
    auto chordAt = [&](int s) {
        if (g.pad == 2) return (s % 16) >= 8 ? prog[1] : prog[0];
        return ((s / 16) & 1) ? prog[1] : prog[0];
    };
    Step* row = pat.s[t][v];
    for (int s = 0; s < MAXSTEPS; s++) row[s] = Step();
    for (int s = 0; s < len; s++) {
        Step& st = row[s];
        int ps = s % 16;
        if (t < NDRUM) {
            float pr = (g.drum[t][ps] - '0') / 9.f;
            if (s >= 16 && pr > 0 && pr < 1) pr *= 0.6f + r.f() * 0.8f;
            if (r.f() < pr) {
                st.on = 1;
                st.vel = uint8_t(pr >= 0.99f ? 100 : 60 + r.i(40));
                if (t == T_HAT && pr < 0.5f) st.vel = uint8_t(45 + r.i(25));
                // ab und zu Hi-Hat-Ratchets bei schnellen Genres
                if (t == T_HAT && g.bpmLo >= 120 && r.f() < 0.06f) { st.repeat = uint8_t(2 + r.i(2)); st.lockMask |= 1 << P_REPEAT; }
            }
        } else if (t == T_BASS) {
            float pr = (g.bass[ps] - '0') / 9.f;
            if (r.f() < pr) {
                st.on = 1;
                int deg = chordAt(s);
                float k = r.f();
                if (k > 0.7f) deg += 4;
                else if (k > 0.55f) deg += 7;
                st.note = uint8_t(degNote(p.root, p.scale, deg, 36));
                st.vel = uint8_t(80 + r.i(21));
                if (r.f() < 0.2f) st.vel = 112;  // Akzent
            }
        } else if (t == T_PAD) {
            if (g.pad == 0) continue;
            bool two = g.pad == 2;
            if (ps == 0 || (two && ps == 8)) {
                st.on = 1;
                st.note = uint8_t(degNote(p.root, p.scale, chordAt(s), 48));
                st.lockMask |= 1 << P_DECAY;
                st.decay = lenToDecay(two ? 8 : 16);
                st.vel = 85;
            }
        } else if (t == T_SAMPLE) {
            float pr = (g.keys[ps] - '0') / 9.f;
            if (r.f() < pr) {
                st.on = 1;
                st.chord = 1;
                st.note = uint8_t(degNote(p.root, p.scale, chordAt(s), 48));
                st.vel = uint8_t(75 + r.i(20));
            }
        } else if (t == T_LEAD) {
            float pr = g.lead / 18.f * ((ps % 2) ? 0.6f : 1.2f);
            if (r.f() < pr) {
                st.on = 1;
                static const int tones[7] = {0, 2, 4, 0, 4, 6, 7};
                st.note = uint8_t(degNote(p.root, p.scale, chordAt(s) + tones[r.i(7)], 60));
                st.vel = uint8_t(70 + r.i(30));
            }
        }
    }
}

// Varianten B–D aus A ableiten: B leicht veraendert, C dichter (Fill), D ausgeduennt (Break)
static void deriveVariants(Project& p, int t, Rng& r, const Genre& g, const int* prog) {
    Pattern& pat = p.pat[p.curPat];
    Step* A = pat.s[t][0];
    Step alt[MAXSTEPS];
    Rng r2{r.s ^ 0x9E3779B9u};
    r2.f();
    // Hilfsmuster in Variante 1 erzeugen und wieder herauskopieren
    genTrack(p, t, 1, r2, g, prog);
    for (int s = 0; s < MAXSTEPS; s++) alt[s] = pat.s[t][1][s];
    for (int s = 0; s < MAXSTEPS; s++) {
        Step& B = pat.s[t][1][s];
        Step& C = pat.s[t][2][s];
        Step& D = pat.s[t][3][s];
        B = r.f() < 0.75f ? A[s] : alt[s];
        C = A[s];
        if (!C.on && alt[s].on && r.f() < 0.5f) C = alt[s];
        D = A[s];
        if (D.on && (s % 8) != 0 && r.f() < 0.5f) D.on = 0;
    }
}

void generate(Project& p, int track, uint32_t seed) {
    Rng r{seed * 2654435761u + 1};
    r.f();
    const Genre& g = G[p.genre % NGENRE];
    Pattern& pat = p.pat[p.curPat];
    static const int progs[6][2] = {{0, 5}, {0, 3}, {5, 6}, {0, 6}, {3, 4}, {0, 2}};
    if (track < 0) {
        p.bpm = uint16_t(g.bpmLo + r.i(g.bpmHi - g.bpmLo + 1));
        p.swing = uint8_t(g.swing);
        p.scale = uint8_t(g.scale);
        p.tr[T_BASS].decay = lenToDecay(g.bassLen);
        p.tr[T_SAMPLE].decay = lenToDecay(g.keysLen);
        pat.reserved[0] = uint8_t(r.i(6));  // Akkordfolge des Patterns
    }
    const int* prog = progs[pat.reserved[0] % 6];
    for (int t = 0; t < NT; t++) {
        if (track >= 0 && t != track) continue;
        if (track < 0) {
            genTrack(p, t, 0, r, g, prog);
            deriveVariants(p, t, r, g, prog);
            pat.var[t] = 0;
        } else {
            genTrack(p, t, pat.var[t] & 3, r, g, prog);
        }
    }
}
