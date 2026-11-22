#include "app.h"
#include "generator.h"
#include "presets.h"
#include "project_v1.h"
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <algorithm>
#include <cstring>

using namespace gfx;

// ---------------------------------------------------------------- Farben (GENRA-Look, docs/DESIGN.md)
static const Col C_BG = rgb(0x17181d), C_PLATE = rgb(0x212329), C_LINE = rgb(0x34363f), C_TXT = rgb(0xd9dbe2),
                 C_DIM = rgb(0x8a8d99), C_DISP = rgb(0x07080a), C_DRUM = rgb(0x7b6cff), C_BASS = rgb(0xb8f23a),
                 C_MEL = rgb(0x3dc9ff), C_PAD = rgb(0xe0779e), C_SMP = rgb(0x2fc2a8), C_SONG = rgb(0xffd23c),
                 C_GO = rgb(0x5cf08a), C_RED = rgb(0xf0564d), C_BTN = rgb(0x3b3d45), C_TAB = rgb(0x2a2c33),
                 C_INK = rgb(0x0b0c0f), C_WHITE = rgb(0xffffff), C_DOT = rgb(0x3a3c45), C_DARK = rgb(0x0e0f12);
static const Col kTrackCol[NT] = {C_DRUM, C_DRUM, C_DRUM, C_DRUM, C_PAD, C_BASS, C_MEL, C_SMP};
static const char* kPageNames[PG_COUNT] = {"SEQ", "SONG", "MIX", "FX", "SYN"};
static const Col kPageCol[PG_COUNT] = {C_DRUM, C_SONG, C_TXT, C_MEL, C_BASS};

static std::string fmt(const char* f, ...) {
    char b[200];
    va_list a;
    va_start(a, f);
    vsnprintf(b, sizeof b, f, a);
    va_end(a);
    return b;
}

// 36 = C1 (wie bei den meisten Controllern, Ableton und der MPC)
std::string noteName(int n) { return fmt("%s%d", kNoteNames[((n % 12) + 12) % 12], n / 12 - 2); }

static Col dim(Col c, float t) { return mix(c, C_BG, t); }

// Text vertikal mittig: a = 0 links, 1 Mitte, 2 rechts
static void tx(Canvas& c, float x, float yc, const std::string& s, FontId f, Col col, int sp = 0, int a = 0) {
    int w = Canvas::textW(s, f, sp);
    int xx = int(a == 1 ? x - w / 2.f : a == 2 ? x - w : x);
    c.text(xx, int(yc - Canvas::fontH(f) / 2.f), s, f, col, sp);
}

static void circle(Canvas& c, float cx, float cy, float r, Col fill) { c.ellipse(cx, cy, r, r, fill, fill); }
static void ring(Canvas& c, float cx, float cy, float r, Col col, float w = 1.5f) { c.roundStroke(cx - r, cy - r, 2 * r, 2 * r, r, col, w); }

static void section(Canvas& c, float x, float y, float w, float h, Col col, const char* title) {
    c.roundRect(x, y + 3, w, h - 3, 5, C_PLATE);
    c.roundRect(x, y, w, 4, 2, col);
    if (title && *title) tx(c, x + w / 2, y + 18, title, S, col, 4, 1);
}

static void display(Canvas& c, float x, float y, float w, float h) {
    c.roundRect(x, y, w, h, 7, C_DISP);
    c.roundStroke(x, y, w, h, 7, C_LINE, 1.5f);
}

static void knob(Canvas& c, float cx, float cy, float r, float val, Col accent, bool focus) {
    int n = r > 18 ? 26 : 20;
    Col dc = focus ? (accent ? accent : C_WHITE) : (accent != 0 ? dim(accent, 0.25f) : C_DOT);
    for (int k = 0; k < n; k++) {
        float a = 2 * 3.14159f * k / n;
        float rr = focus ? 1.4f : 1.0f;
        circle(c, cx + std::cos(a) * (r + 5), cy + std::sin(a) * (r + 5), rr, dc);
    }
    circle(c, cx, cy, r, C_DARK);
    circle(c, cx, cy, r - 1.5f, rgb(0x1d1f25));
    ring(c, cx, cy, r - 1.5f, rgb(0x2a2c33), 1.f);
    float a = (-225.f + 270.f * std::clamp(val, 0.f, 1.f)) * 3.14159f / 180.f;
    c.line(cx + std::cos(a) * r * 0.32f, cy + std::sin(a) * r * 0.32f, cx + std::cos(a) * r * 0.78f, cy + std::sin(a) * r * 0.78f,
           C_WHITE, std::max(2.f, r * 0.15f));
    if (focus) ring(c, cx, cy, r + 10, C_WHITE, 1.5f);
}

static void dropdown(Canvas& c, float x, float y, float w, float h, const std::string& text, Col col, const std::string& sub, bool focus) {
    c.roundRect(x, y, w, h, 4, C_DISP);
    c.roundStroke(x, y, w, h, 4, focus ? C_WHITE : col, focus ? 2.f : 1.2f);
    tx(c, x + 10, y + h / 2, text, S, col, 1);
    if (!sub.empty()) tx(c, x + w - 26, y + h / 2, sub, XS, C_DIM, 1, 2);
    float cx = x + w - 13, cy = y + h / 2;
    c.line(cx - 4, cy - 2, cx, cy + 2, col, 1.6f);
    c.line(cx, cy + 2, cx + 4, cy - 2, col, 1.6f);
}

static void chip(Canvas& c, float x, float yc, const std::string& key, const std::string& txt, Col col) {
    int kw = Canvas::textW(key, XS, 1) + 14;
    c.roundRect(x, yc - 9, kw, 18, 9, C_DISP);
    c.roundStroke(x, yc - 9, kw, 18, 9, col, 1.f);
    tx(c, x + kw / 2.f, yc, key, XS, col, 1, 1);
    tx(c, x + kw + 6, yc, txt, XS, C_TXT, 1);
}

// ================================================================ Start, Laden, Speichern
void App::init(const std::string& dir, float sampleRate) {
    dataDir = dir;
    if (!load()) {
        initProject(prj);
        prj.genre = 3;
        seed = 7;
        generate(prj, -1, seed);
    }
    eng.init(&prj, sampleRate);
    samplesLoaded = eng.loadSamples(dataDir + "/samples");
    loadUserPresets();
    if (toast.empty()) msg(samplesLoaded ? fmt("%d SAMPLES GELADEN", samplesLoaded) : std::string("EINGEBAUTE KLÄNGE AKTIV"));
    for (int t = NDRUM; t < NT; t++) lastNote[t] = uint8_t(prj.root + (t == T_BASS ? 36 : t == T_LEAD ? 60 : 48));
    undoPat = prj.pat[prj.curPat];
}

void App::save() {
    std::string p = dataDir + "/projekt.r36";
    std::string tmp = p + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) { msg("SPEICHERN FEHLGESCHLAGEN"); return; }
    bool ok = fwrite(&prj, sizeof prj, 1, f) == 1;
    fclose(f);
    if (ok) rename(tmp.c_str(), p.c_str());
    dirty = false;
    lastSave = now;
}

// Altes Format (v0.1) in das neue umwandeln
static void convertV1(const v1::Project& o, Project& p) {
    initProject(p);
    p.bpm = o.bpm; p.swing = o.swing; p.genre = o.genre; p.curPat = o.curPat % NPAT; p.root = o.root; p.scale = o.scale;
    p.krabbo = o.krabbo; p.dlyFb = o.dlyFb; p.dlyLvl = o.dlyLvl; p.revSize = o.revSize; p.revDamp = o.revDamp;
    p.revLvl = o.revLvl; p.master = o.master; p.limiter = o.limiter;
    static const uint8_t dmap[4] = {1, 2, 3, 0};
    p.dlyTime = dmap[o.dlyTime & 3];
    for (int t = 0; t < NT; t++) {
        const v1::Track& a = o.tr[t];
        Track& b = p.tr[t];
        b.vol = a.vol; b.pan = a.pan; b.mute = a.mute; b.solo = a.solo; b.sample = a.sample; b.pitch = a.pitch;
        if (t < NDRUM) { b.decay = a.decay; b.cutoff = a.cutoff; }
        b.rev = a.rev; b.dly = a.dly; b.randPct = a.randPct;
    }
    for (int i = 0; i < NPAT; i++) {
        p.pat[i].len = o.pat[i].len;
        for (int t = 0; t < NT; t++)
            for (int s = 0; s < MAXSTEPS; s++) {
                const v1::Step& a = o.pat[i].s[t][s];
                Step& b = p.pat[i].s[t][0][s];
                b.on = a.on; b.sample = a.sample; b.note = a.note; b.lockMask = a.lockMask & 31; b.pitch = a.pitch;
                b.decay = a.decay; b.cutoff = a.cutoff; b.rev = a.rev; b.dly = a.dly; b.vel = a.vel; b.chord = a.chord;
            }
    }
}

bool App::load() {
    FILE* f = fopen((dataDir + "/projekt.r36").c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    bool ok = false;
    if (n == long(sizeof(Project))) {
        static Project tmp;
        ok = fread(&tmp, sizeof tmp, 1, f) == 1 && !memcmp(tmp.magic, "R36GEN02", 8);
        if (ok) prj = tmp;
    } else if (n == long(sizeof(v1::Project))) {
        static v1::Project old;
        ok = fread(&old, sizeof old, 1, f) == 1 && !memcmp(old.magic, "R36GEN01", 8);
        if (ok) { convertV1(old, prj); msg("PROJEKT AUS V0.1 ÜBERNOMMEN"); }
    }
    fclose(f);
    if (ok) {
        prj.curPat %= NPAT;
        prj.songLen = uint8_t(std::min<int>(prj.songLen, NSONG));
        for (auto& p : prj.pat) p.len = uint8_t(std::clamp<int>(p.len, 16, MAXSTEPS) / 16 * 16);
    }
    return ok;
}

// ---------------------------------------------------------------- Eigene Presets
int App::presetCount(int t) const { int n; presetList(t, n); return n + int(userPresets[t].size()); }

std::string App::presetName(int t, int idx) const {
    int n;
    const Preset* ps = presetList(t, n);
    if (idx < n) return ps[idx].name;
    idx -= n;
    if (idx < int(userPresets[t].size())) return userPresets[t][idx].name;
    return "-";
}

void App::loadPreset(int t, int idx) {
    int n;
    const Preset* ps = presetList(t, n);
    int tot = presetCount(t);
    if (tot == 0) return;
    idx = (idx % tot + tot) % tot;
    undoTrack = prj.tr[t];
    undoTrackIdx = t;
    Track& tr = prj.tr[t];
    if (idx < n) applyPreset(tr, ps[idx]);
    else {
        const Track& u = userPresets[t][idx - n].tr;
        tr.syn = u.syn; tr.cutoff = u.cutoff; tr.rev = u.rev; tr.dly = u.dly; tr.decay = u.decay;
    }
    tr.preset = uint8_t(idx);
    dirty = true;
}

void App::loadUserPresets() {
    FILE* f = fopen((dataDir + "/presets_eigen.bin").c_str(), "rb");
    if (!f) return;
    int32_t t;
    UserPreset up;
    while (fread(&t, 4, 1, f) == 1 && fread(&up, sizeof up, 1, f) == 1)
        if (t >= NDRUM && t < NT && userPresets[t].size() < 64) userPresets[t].push_back(up);
    fclose(f);
}

void App::writeUserPresets() {
    FILE* f = fopen((dataDir + "/presets_eigen.bin").c_str(), "wb");
    if (!f) return;
    for (int32_t t = 0; t < NT; t++)
        for (auto& up : userPresets[t]) { fwrite(&t, 4, 1, f); fwrite(&up, sizeof up, 1, f); }
    fclose(f);
}

void App::saveUserPreset(int t) {
    if (t < NDRUM) { msg("PRESETS NUR FÜR PAD, BASS, LEAD, SAMPLE"); return; }
    UserPreset up{};
    snprintf(up.name, sizeof up.name, "EIGEN %d", int(userPresets[t].size()) + 1);
    up.tr = prj.tr[t];
    userPresets[t].push_back(up);
    writeUserPresets();
    int n;
    presetList(t, n);
    prj.tr[t].preset = uint8_t(n + userPresets[t].size() - 1);
    msg(fmt("GESPEICHERT ALS %s", up.name));
}

void App::randomizeTrack(int t) {
    undoTrack = prj.tr[t];
    undoTrackIdx = t;
    seed = seed * 1103515245u + 12345u + uint32_t(now * 1000);
    uint32_t r = seed | 1;
    auto rnd = [&](int lo, int hi) { r ^= r << 13; r ^= r >> 17; r ^= r << 5; return lo + int(r % uint32_t(hi - lo + 1)); };
    Synth& s = prj.tr[t].syn;
    if (isSynthTrack(t)) {
        static const int good[] = {0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 16, 19, 20};
        s.engine = uint8_t(good[rnd(0, 15)]);
        s.timbre = uint8_t(rnd(15, 85)); s.harm = uint8_t(rnd(10, 90)); s.morph = uint8_t(rnd(10, 90));
    }
    s.ftype = uint8_t(rnd(0, 2)); s.res = uint8_t(rnd(0, 70)); s.fenv = uint8_t(rnd(0, 60));
    prj.tr[t].cutoff = uint8_t(rnd(35, 90));
    s.drive = uint8_t(rnd(0, 50)); s.chorus = uint8_t(rnd(0, 50));
    dirty = true;
    msg(fmt("%s GEWÜRFELT", kTrackNames[t]));
}

// ================================================================ Eingabe
void App::update(const Input& in, double dt) {
    now += dt;
    for (int b = 0; b < B_COUNT; b++) {
        pressed[b] = in.held[b] && !prev.held[b];
        released[b] = !in.held[b] && prev.held[b];
        rep[b] = pressed[b];
        if (pressed[b]) { heldSince[b] = now; nextRepeat[b] = now + 0.32; }
        else if (in.held[b] && b <= B_RIGHT && now >= nextRepeat[b]) { rep[b] = true; nextRepeat[b] = now + 0.07; }
    }
    if (in.held[B_START] && in.held[B_SELECT]) { quit = true; prev = in; return; }

    handleMidi();

    if (pressed[B_SELECT]) selUsed = false;
    if (in.held[B_SELECT])
        for (int b = 0; b < B_COUNT; b++) if (b != B_SELECT && pressed[b]) selUsed = true;
    if (released[B_SELECT] && !selUsed && now - heldSince[B_SELECT] < 0.35) {
        if (now - lastSelTap < 0.4) {
            if (midiTest) midiTest = false;
            else { menuOpen = !menuOpen; menuSel = 0; confirmNew = false; }
            lastSelTap = -10;
        } else lastSelTap = now;
    }

    if (midiTest) { if (pressed[B_B]) midiTest = false; }
    else if (menuOpen) handleMenu(in);
    else {
        if (in.held[B_SELECT] && (pressed[B_LEFT] || pressed[B_RIGHT])) {
            page = (page + (pressed[B_RIGHT] ? 1 : PG_COUNT - 1)) % PG_COUNT;
        } else if (pressed[B_START] && !in.held[B_SELECT]) {
            if (eng.playing) eng.stop(); else eng.start();
        } else {
            switch (page) {
                case PG_SEQ: handleSeq(in, dt); break;
                case PG_SONG: handleSong(in); break;
                case PG_MIX: handleMix(in, dt); break;
                case PG_FX: handleFx(in, dt); break;
                case PG_SYN: handleSyn(in, dt); break;
            }
        }
    }
    livePerf(in);
    krabboTick();
    if (dirty && now - lastSave > 30) save();
    prev = in;
}

// Live-Effekte auf den Sticks (nur SEQ und SONG, nicht im Menue)
void App::livePerf(const Input& in) {
    bool on = !menuOpen && !midiTest && (page == PG_SEQ || page == PG_SONG) && !in.held[B_B];
    float x = in.lx;
    if (!on || std::fabs(x) < 0.15f) eng.perfFilter = 0;
    else eng.perfFilter = (x - (x > 0 ? 0.15f : -0.15f)) / 0.85f;
    int st = 0;
    if (on && std::hypot(in.rx, in.ry) > 0.6f) {
        if (std::fabs(in.ry) > std::fabs(in.rx)) st = in.ry < 0 ? 1 : 4;   // hoch 1/4, runter Freeze
        else st = in.rx > 0 ? 2 : 3;                                      // rechts 1/8, links 1/16
    }
    eng.perfStutter = st;
}

void App::handleMidi() {
    midiEv.clear();
    midi.poll(now, midiEv);
    for (auto& e : midiEv) {
        uint8_t hi = e.status & 0xF0;
        bool on = hi == 0x90 && e.d2 > 0, off = hi == 0x80 || (hi == 0x90 && e.d2 == 0);
        if (!on && !off) continue;
        int n = e.d1, t = -1, note = n;
        if (n >= 36 && n <= 43) t = n - 36;                // C1..G1 = Spuren
        else if (n >= 48 && curT >= NDRUM) t = curT;       // ab C2: gewaehlte Instrument-Spur chromatisch
        if (on) midi.addLog(fmt("%s  VEL %d  > %s", noteName(n).c_str(), e.d2, t >= 0 ? kTrackNames[t] : "-"));
        if (t < 0) continue;
        if (t >= NDRUM && n <= 43) note = lastNote[t];
        if (on) {
            eng.liveOn(t, note, e.d2);
            if (rec && eng.playing) {
                Pattern& pat = prj.pat[prj.curPat];
                int len = trackLen(prj, pat, t);
                int s = eng.playStepT[t];
                if (s < 0) s = 0;
                if (eng.stepPhase() > 0.5f) s = (s + 1) % len;
                Step& st = pat.s[t][pat.var[t] & 3][s];
                st.on = 1;
                st.vel = uint8_t(std::clamp(e.d2 * 100 / 127, 1, 127));
                if (t >= NDRUM) st.note = uint8_t(note);
                dirty = true;
            }
        } else eng.liveOff(t, note);
    }
}

void App::storeUndo() {
    undoPat = prj.pat[prj.curPat];
    undoBpm = uint8_t(std::min<int>(prj.bpm, 255));
    undoSwing = prj.swing;
    hasUndo = true;
}

void App::doUndo() {
    if (!hasUndo) { msg("NICHTS ZUM RÜCKGÄNGIG MACHEN"); return; }
    std::swap(undoPat, prj.pat[prj.curPat]);
    uint8_t b = undoBpm, s = undoSwing;
    undoBpm = uint8_t(std::min<int>(prj.bpm, 255));
    undoSwing = prj.swing;
    prj.bpm = b;
    prj.swing = s;
    dirty = true;
    msg("UNDO");
}

void App::newAll() {
    storeUndo();
    seed = seed * 1103515245u + 12345u + uint32_t(now * 1000);
    generate(prj, -1, seed);
    dirty = true;
    if (!eng.playing) eng.start();
    msg(fmt("NEW ALL · %s · %d BPM", kGenreNames[prj.genre], prj.bpm));
}

void App::newTrack() {
    storeUndo();
    seed = seed * 1103515245u + 12345u + uint32_t(now * 1000);
    generate(prj, curT, seed);
    dirty = true;
    msg(fmt("NEW · %s · VARIANTE %c", kTrackNames[curT], 'A' + (prj.pat[prj.curPat].var[curT] & 3)));
}

void App::rotateTrack(int dir) {
    Pattern& pat = prj.pat[prj.curPat];
    int len = trackLen(prj, pat, curT);
    Step* row = pat.s[curT][pat.var[curT] & 3];
    if (dir > 0) std::rotate(row, row + len - 1, row + len);
    else std::rotate(row, row + 1, row + len);
    dirty = true;
}

void App::switchVariant(int dir) {
    Pattern& pat = prj.pat[prj.curPat];
    int cur = pat.var[curT] & 3, nv = (cur + dir + NVAR) % NVAR;
    bool empty = true;
    for (int s = 0; s < MAXSTEPS; s++) if (pat.s[curT][nv][s].on) { empty = false; break; }
    if (empty) {
        for (int s = 0; s < MAXSTEPS; s++) pat.s[curT][nv][s] = pat.s[curT][cur][s];
        msg(fmt("%s: VARIANTE %c (KOPIE VON %c)", kTrackNames[curT], 'A' + nv, 'A' + cur));
    } else msg(fmt("%s: VARIANTE %c", kTrackNames[curT], 'A' + nv));
    pat.var[curT] = uint8_t(nv);
    dirty = true;
}

void App::changeLock(int delta) {
    Pattern& pat = prj.pat[prj.curPat];
    Step& st = pat.s[curT][pat.var[curT] & 3][curS];
    const Track& tr = prj.tr[curT];
    if (!st.on) { msg("ERST STEP SETZEN (A)"); return; }
    bool inst = curT >= NDRUM;
    if (lockSel == P_PITCH && inst) {
        st.note = uint8_t(std::clamp(st.note + delta, 12, 108));
        lastNote[curT] = st.note;
    } else {
        int v = stepParam(st, tr, lockSel);
        int lo = lockSel == P_PITCH ? -24 : lockSel == P_REPEAT ? 1 : 0;
        int hi = lockSel == P_PITCH ? 24 : lockSel == P_REPEAT ? 4 : 100;
        v = std::clamp(v + delta, lo, hi);
        st.lockMask |= 1 << lockSel;
        switch (lockSel) {
            case P_PITCH: st.pitch = int8_t(v); break;
            case P_DECAY: st.decay = uint8_t(v); break;
            case P_CUTOFF: st.cutoff = uint8_t(v); break;
            case P_REVERB: st.rev = uint8_t(v); break;
            case P_DELAY: st.dly = uint8_t(v); break;
            default: st.repeat = uint8_t(v); break;
        }
    }
    dirty = true;
}

void App::handleSeq(const Input& in, double dt) {
    Pattern& pat = prj.pat[prj.curPat];
    int len = pat.len;
    bool sel = in.held[B_SELECT], bh = in.held[B_B];
    if (pressed[B_B]) bUsed = false;
    if (pressed[B_R3]) r3Used = false;

    if (sel) {
        if (pressed[B_UP] || pressed[B_DOWN]) {
            prj.genre = uint8_t((prj.genre + (pressed[B_UP] ? NGENRE - 1 : 1)) % NGENRE);
            msg(fmt("GENRE: %s", kGenreNames[prj.genre]));
            dirty = true;
        }
        if (pressed[B_Y]) newAll();
        if (pressed[B_X]) doUndo();
        if (pressed[B_L1]) switchVariant(-1);
        if (pressed[B_R1]) switchVariant(+1);
        if (pressed[B_B]) { rec = !rec; msg(rec ? "AUFNAHME AN (MIDI-PADS)" : "AUFNAHME AUS"); bUsed = true; }
        return;
    }
    if (in.held[B_R3]) {
        if (rep[B_LEFT]) { rotateTrack(-1); r3Used = true; }
        if (rep[B_RIGHT]) { rotateTrack(+1); r3Used = true; }
        return;
    }
    if (released[B_R3] && !r3Used) msg("NOTEN-GRID KOMMT IN V0.3");
    Step& st = pat.s[curT][pat.var[curT] & 3][curS];
    if (bh) {
        if (rep[B_LEFT]) { lockSel = (lockSel + NPARAM - 1) % NPARAM; bUsed = true; }
        if (rep[B_RIGHT]) { lockSel = (lockSel + 1) % NPARAM; bUsed = true; }
        if (rep[B_UP]) { changeLock(+1); bUsed = true; }
        if (rep[B_DOWN]) { changeLock(-1); bUsed = true; }
        if (std::fabs(in.ly) > 0.25f) {
            float speed = (lockSel == P_PITCH ? 14.f : lockSel == P_REPEAT ? 4.f : 60.f) * (std::fabs(in.ly) - 0.2f) / 0.8f;
            stickAcc += -in.ly / std::fabs(in.ly) * speed * dt;
            int d = int(stickAcc);
            if (d) { stickAcc -= d; changeLock(d); }
            bUsed = true;
        } else stickAcc = 0;
        if (pressed[B_A]) { st.lockMask = 0; st.repeat = 1; bUsed = true; msg("LOCKS GELÖSCHT"); dirty = true; }
        if ((pressed[B_L2] || pressed[B_R2]) && st.on && (curT == T_SAMPLE || curT == T_PAD)) {
            st.chord = !st.chord;
            bUsed = true;
            msg(st.chord ? "AKKORD AN" : "AKKORD AUS");
            dirty = true;
        }
        return;
    }
    if (released[B_B] && !bUsed && st.on) eng.preview(curT, st);

    if (rep[B_UP]) curT = (curT + NT - 1) % NT;
    if (rep[B_DOWN]) curT = (curT + 1) % NT;
    if (rep[B_LEFT]) curS = (curS + len - 1) % len;
    if (rep[B_RIGHT]) curS = (curS + 1) % len;
    curS = std::min(curS, len - 1);

    Step& s2 = pat.s[curT][pat.var[curT] & 3][curS];
    if (pressed[B_A]) {
        s2.on = !s2.on;
        if (s2.on) {
            if (curT >= NDRUM) s2.note = lastNote[curT];
            if (s2.vel == 0) s2.vel = 100;
            if (!eng.playing) eng.preview(curT, s2);
        }
        dirty = true;
    }
    if ((rep[B_L2] || rep[B_R2]) && s2.on) {
        int d = rep[B_R2] ? 1 : -1;
        if (curT < NDRUM) {
            int n = int(eng.drums[curT].size());
            int v = s2.sample + d;
            if (v < -1) v = n - 1;
            if (v >= n) v = -1;
            s2.sample = int8_t(v);
        } else {
            s2.note = uint8_t(std::clamp(s2.note + d, 12, 108));
            lastNote[curT] = s2.note;
        }
        eng.preview(curT, s2);
        dirty = true;
    }
    if (pressed[B_L1] || pressed[B_R1]) {
        int np = (prj.curPat + (pressed[B_R1] ? 1 : NPAT - 1)) % NPAT;
        if (eng.playing) { eng.queuedPat = np; msg(fmt("NÄCHSTES PATTERN: %02d", np + 1)); }
        else prj.curPat = uint8_t(np);
        hasUndo = false;
    }
    if (pressed[B_Y]) newTrack();
    if (pressed[B_X]) {
        prj.tr[curT].mute = !prj.tr[curT].mute;
        msg(fmt("%s %s", kTrackNames[curT], prj.tr[curT].mute ? "STUMM" : "AN"));
    }
}

void App::handleSong(const Input& in) {
    if (in.held[B_SELECT]) {
        if (pressed[B_A]) {
            if (prj.songLen == 0) msg("SONG IST LEER (R1: PATTERN EINFÜGEN)");
            else { eng.startSong(songSel); msg(fmt("SONG AB ZEILE %d", songSel + 1)); }
        }
        if (pressed[B_Y]) { prj.songLoop = !prj.songLoop; msg(prj.songLoop ? "SONG-LOOP AN" : "SONG-LOOP AUS"); dirty = true; }
        if (pressed[B_X]) doUndo();
        return;
    }
    if (pressed[B_B]) songFocusList = !songFocusList;
    if (songFocusList) {
        int n = std::max<int>(1, prj.songLen);
        if (rep[B_UP]) songSel = (songSel + n - 1) % n;
        if (rep[B_DOWN]) songSel = (songSel + 1) % n;
        if (prj.songLen > 0) {
            SongRow& r = prj.song[songSel];
            if (rep[B_LEFT]) { r.bars = uint8_t(std::max(1, r.bars - 1)); dirty = true; }
            if (rep[B_RIGHT]) { r.bars = uint8_t(std::min(64, r.bars + 1)); dirty = true; }
            if (pressed[B_L2]) { r.part = uint8_t((r.part + NPART - 1) % NPART); dirty = true; }
            if (pressed[B_R2]) { r.part = uint8_t((r.part + 1) % NPART); dirty = true; }
            if (pressed[B_A]) { r.pat = uint8_t(songPad); dirty = true; msg(fmt("ZEILE %d = P%02d", songSel + 1, songPad + 1)); }
        }
    } else {
        if (rep[B_LEFT]) songPad = (songPad + NPAT - 1) % NPAT;
        if (rep[B_RIGHT]) songPad = (songPad + 1) % NPAT;
        if (rep[B_UP]) songPad = (songPad + NPAT - 4) % NPAT;
        if (rep[B_DOWN]) songPad = (songPad + 4) % NPAT;
        if (pressed[B_A]) {
            if (eng.playing && !eng.songMode) { eng.queuedPat = songPad; msg(fmt("NÄCHSTES PATTERN: %02d", songPad + 1)); }
            else if (!eng.playing) prj.curPat = uint8_t(songPad);
            hasUndo = false;
        }
        if (pressed[B_X]) { clipPat = songPad; msg(fmt("PATTERN %02d KOPIERT", songPad + 1)); }
        if (pressed[B_Y]) {
            if (clipPat < 0) msg("ERST KOPIEREN (X)");
            else if (clipPat != songPad) { prj.pat[songPad] = prj.pat[clipPat]; dirty = true; msg(fmt("EINGEFÜGT IN %02d", songPad + 1)); }
        }
    }
    if (pressed[B_R1]) {
        if (prj.songLen >= NSONG) msg("SONG IST VOLL");
        else {
            int at = prj.songLen == 0 ? 0 : songSel + 1;
            for (int i = prj.songLen; i > at; i--) prj.song[i] = prj.song[i - 1];
            SongRow r;
            r.pat = uint8_t(songPad);
            r.bars = uint8_t(std::max(1, prj.pat[songPad].len / 16) * 2);
            r.part = at > 0 ? prj.song[at - 1].part : 0;
            prj.song[at] = r;
            prj.songLen++;
            songSel = at;
            dirty = true;
            msg(fmt("P%02d IN ZEILE %d", songPad + 1, at + 1));
        }
    }
    if (pressed[B_L1] && prj.songLen > 0) {
        for (int i = songSel; i < prj.songLen - 1; i++) prj.song[i] = prj.song[i + 1];
        prj.songLen--;
        songSel = std::min(songSel, std::max(0, prj.songLen - 1));
        dirty = true;
        msg("ZEILE GELÖSCHT");
    }
}

void App::handleMix(const Input& in, double dt) {
    static const int keys[NT] = {B_L1, B_L2, B_R1, B_R2, B_A, B_B, B_X, B_Y};
    for (int t = 0; t < NT; t++) {
        if (!pressed[keys[t]]) continue;
        if (in.held[B_SELECT]) prj.tr[t].solo = !prj.tr[t].solo;
        else prj.tr[t].mute = !prj.tr[t].mute;
        dirty = true;
    }
    if (in.held[B_SELECT]) {
        if (pressed[B_UP] || pressed[B_DOWN]) {
            for (auto& t : prj.tr) { t.mute = 0; t.solo = 0; }
            msg("ALLE MUTES UND SOLOS AUFGEHOBEN");
        }
        return;
    }
    if (rep[B_LEFT]) mixSel = (mixSel + NT) % (NT + 1);
    if (rep[B_RIGHT]) mixSel = (mixSel + 1) % (NT + 1);
    uint8_t* vol = mixSel < NT ? &prj.tr[mixSel].vol : &prj.master;
    if (rep[B_UP]) { *vol = uint8_t(std::min(100, *vol + 2)); dirty = true; }
    if (rep[B_DOWN]) { *vol = uint8_t(std::max(0, *vol - 2)); dirty = true; }
    if (std::fabs(in.ly) > 0.25f) {
        stickAcc += -in.ly * 70.0 * dt;
        int d = int(stickAcc);
        if (d) { stickAcc -= d; *vol = uint8_t(std::clamp(*vol + d, 0, 100)); dirty = true; }
    } else stickAcc = 0;
    if (mixSel < NT && std::fabs(in.rx) > 0.25f) {
        stickAccR += in.rx * 70.0 * dt;
        int d = int(stickAccR);
        if (d) { stickAccR -= d; prj.tr[mixSel].pan = uint8_t(std::clamp(prj.tr[mixSel].pan + d, 0, 100)); dirty = true; }
    } else stickAccR = 0;
}

// Fokus per Kreuz auf das naechste Bedienelement in der Richtung; Werte per Stick (mit oder ohne B)
void App::handleCtls(const Input& in, double dt, std::vector<Ctl>& cs, int& sel, bool needB) {
    if (cs.empty()) return;
    sel = std::clamp(sel, 0, int(cs.size()) - 1);
    bool bh = in.held[B_B];
    if (!bh) {
        int dx = rep[B_RIGHT] - rep[B_LEFT], dy = rep[B_DOWN] - rep[B_UP];
        if (dx || dy) {
            const Ctl& a = cs[sel];
            int best = -1;
            float bs = 1e9f;
            for (int i = 0; i < int(cs.size()); i++) {
                if (i == sel) continue;
                float ex = cs[i].x - a.x, ey = cs[i].y - a.y;
                float along = dx ? ex * dx : ey * dy, across = dx ? std::fabs(ey) : std::fabs(ex);
                if (along <= 4) continue;
                float score = along + across * 2.2f;
                if (score < bs) { bs = score; best = i; }
            }
            if (best >= 0) sel = best;
        }
    }
    const Ctl& k = cs[sel];
    int range = k.hi - k.lo;
    int v = k.get();
    int step = 0;
    if (bh) {
        if (rep[B_UP]) step = needB ? 1 : 10;
        if (rep[B_DOWN]) step = needB ? -1 : -10;
        if (step) bUsed = true;
    }
    if (std::fabs(in.ly) > 0.25f && (bh || !needB)) {
        float speed = range <= 24 ? 6.f : 70.f;
        stickAcc += -in.ly / std::fabs(in.ly) * speed * (std::fabs(in.ly) - 0.2f) / 0.8f * dt;
        int d = int(stickAcc);
        if (d) { stickAcc -= d; step += d; }
        bUsed = true;
    } else stickAcc = 0;
    if (step) {
        if (page == PG_SYN && undoTrackIdx != synT) { undoTrack = prj.tr[synT]; undoTrackIdx = synT; }
        k.set(std::clamp(v + step, k.lo, k.hi));
        dirty = true;
    }
}

static const char* kSync[4] = {"1/16", "1/8", "3/16", "1/4"};

void App::buildFxCtls(std::vector<Ctl>& cs) {
    auto pct = [](int v) { return fmt("%d", v); };
    cs.push_back({94, 298, 30, false, "TIME", &prj.dlyTime, nullptr, 0, 3, C_MEL, [](int v) { return std::string(kSync[v & 3]); }});
    cs.push_back({200, 290, 22, false, "FDBK", &prj.dlyFb, nullptr, 0, 100, 0, pct});
    cs.push_back({282, 290, 22, false, "LEVEL", &prj.dlyLvl, nullptr, 0, 100, 0, pct});
    cs.push_back({442, 298, 30, false, "SIZE", &prj.revSize, nullptr, 0, 100, C_PAD, pct});
    cs.push_back({548, 290, 22, false, "DAMP", &prj.revDamp, nullptr, 0, 100, 0, pct});
    cs.push_back({630, 290, 22, false, "LEVEL", &prj.revLvl, nullptr, 0, 100, 0, pct});
    for (int t = 0; t < NT; t++) cs.push_back({62.f + t * 74, 538, 14, false, "DLY", &prj.tr[t].dly, nullptr, 0, 100, 0, pct});
    cs.push_back({664, 538, 14, false, "TIEFE", &prj.duckDepth, nullptr, 0, 100, prj.duck ? C_GO : 0, pct});
    for (int t = 0; t < NT; t++) cs.push_back({62.f + t * 74, 606, 14, false, "REV", &prj.tr[t].rev, nullptr, 0, 100, 0, pct});
}

void App::handleFx(const Input& in, double dt) {
    std::vector<Ctl> cs;
    buildFxCtls(cs);
    if (pressed[B_B]) bUsed = false;
    handleCtls(in, dt, cs, fxSel, false);
    bool hall = (fxSel >= 3 && fxSel <= 5) || fxSel >= 15;
    if (pressed[B_L1] || pressed[B_R1]) {
        int d = pressed[B_R1] ? 1 : 2;
        uint8_t& m = hall ? prj.revModel : prj.dlyModel;
        m = uint8_t((m + d) % 3);
        static const char* dn[3] = {"PING-PONG", "TAPE", "MONO"};
        static const char* hn[3] = {"KAMMER", "HALLE", "PLATTE"};
        msg(hall ? fmt("HALL: %s", hn[m]) : fmt("DELAY: %s", dn[m]));
        dirty = true;
    }
    if (pressed[B_L2] || pressed[B_R2]) {
        int d = pressed[B_R2] ? 1 : 3;
        if (hall) prj.revPre = uint8_t((prj.revPre + d) % 4);
        else prj.dlyTime = uint8_t((prj.dlyTime + d) % 4);
        dirty = true;
    }
    if (pressed[B_X]) { prj.duck = !prj.duck; msg(prj.duck ? "DUCKING AN (KICK)" : "DUCKING AUS"); dirty = true; }
    if (pressed[B_Y]) {
        uint8_t& o = hall ? prj.revOn : prj.dlyOn;
        o = !o;
        msg(fmt("%s %s", hall ? "HALL" : "DELAY", o ? "AN" : "AUS"));
        dirty = true;
    }
    if (pressed[B_A] && !in.held[B_B]) {
        Step st;
        st.on = 1; st.rev = 60; st.dly = 60;
        st.lockMask = (1 << P_REVERB) | (1 << P_DELAY);
        eng.preview(T_CLAP, st);
    }
}

void App::buildSynCtls(std::vector<Ctl>& cs) {
    Track& tr = prj.tr[synT];
    Synth& s = tr.syn;
    auto pct = [](int v) { return fmt("%d", v); };
    Col col = kTrackCol[synT];
    float L = 18, R = 366;          // linke / rechte Sektion
    float yA = 206, yB = 436;       // obere / untere Sektionsreihe
    if (synGroove) {
        cs.push_back({L + 170, yA + 46, 150, true, "MODELL", &tr.groove, nullptr, 0, 3, col, [](int v) { return std::string(kGrooveNames[v & 3]); }});
        cs.push_back({L + 70, yA + 130, 30, false, "SWING", &tr.swing, nullptr, 50, 75, col, [](int v) { return fmt("%d %%", v); }});
        cs.push_back({L + 200, yA + 124, 22, false, "VELO-ZUFALL", &tr.veloRnd, nullptr, 0, 100, 0, [](int v) { return fmt("%d %%", v); }});
        cs.push_back({L + 300, yA + 124, 22, false, "LÄNGE", &tr.len, nullptr, 0, 64, 0, [](int v) { return v ? fmt("%d ST", v) : std::string("PATTERN"); }});
        return;
    }
    if (synT < NDRUM) {
        int n = int(eng.drums[synT].size());
        Engine* e = &eng;
        int t = synT;
        cs.push_back({L + 168, yA + 46, 150, true, "SAMPLE", nullptr, &tr.sample, 0, std::max(0, n - 1), col,
                      [e, t](int v) { const SampleBuf* b = e->drumSample(t, v); return b ? b->name : std::string("-"); }});
        cs.push_back({L + 64, yA + 130, 30, false, "PITCH", nullptr, &tr.pitch, -24, 24, col, [](int v) { return fmt("%+d", v); }});
        cs.push_back({L + 160, yA + 124, 20, false, "DECAY", &tr.decay, nullptr, 0, 100, 0, pct});
        cs.push_back({L + 232, yA + 124, 20, false, "CUTOFF", &tr.cutoff, nullptr, 0, 100, 0, pct});
        cs.push_back({L + 302, yA + 124, 20, false, "ZUFALL", &tr.randPct, nullptr, 0, 100, 0, [](int v) { return fmt("%d %%", v); }});
        cs.push_back({R + 56, yA + 100, 22, false, "HALL", &tr.rev, nullptr, 0, 100, 0, pct});
        cs.push_back({R + 130, yA + 100, 22, false, "DELAY", &tr.dly, nullptr, 0, 100, 0, pct});
        cs.push_back({R + 204, yA + 100, 22, false, "LAUT", &tr.vol, nullptr, 0, 100, 0, pct});
        cs.push_back({R + 278, yA + 100, 22, false, "PAN", &tr.pan, nullptr, 0, 100, 0, [](int v) { return v == 50 ? std::string("C") : v < 50 ? fmt("L%d", 50 - v) : fmt("R%d", v - 50); }});
        return;
    }
    if (synT == T_SAMPLE) {
        int n = int(eng.keys.size());
        Engine* e = &eng;
        cs.push_back({L + 168, yA + 46, 150, true, "SAMPLE", nullptr, &tr.sample, 0, std::max(0, n - 1), col,
                      [e](int v) { return e->keys.empty() ? std::string("E-PIANO (EINGEBAUT)") : e->keys[std::clamp(v, 0, int(e->keys.size()) - 1)].name; }});
        cs.push_back({L + 64, yA + 130, 30, false, "LÄNGE", &tr.decay, nullptr, 0, 100, col, [](int v) { return fmt("%d ST", 1 + (v * 15 + 50) / 100); }});
        cs.push_back({L + 160, yA + 124, 20, false, "ACCENT", &s.accent, nullptr, 0, 100, 0, pct});
        cs.push_back({L + 232, yA + 124, 20, false, "DELAY", &tr.dly, nullptr, 0, 100, 0, pct});
        cs.push_back({L + 302, yA + 124, 20, false, "LAUT", &tr.vol, nullptr, 0, 100, 0, pct});
    } else {
        cs.push_back({L + 168, yA + 46, 150, true, "ENGINE", &s.engine, nullptr, 0, 20, col, [](int v) { return std::string(kEngineNames[std::clamp(v, 0, 20)]); }});
        cs.push_back({L + 60, yA + 124, 28, false, "TIMBRE", &s.timbre, nullptr, 0, 100, col, pct});
        cs.push_back({L + 160, yA + 92, 17, false, "HARM", &s.harm, nullptr, 0, 100, 0, pct});
        cs.push_back({L + 232, yA + 92, 17, false, "MORPH", &s.morph, nullptr, 0, 100, 0, pct});
        cs.push_back({L + 302, yA + 92, 17, false, "SUB", &s.sub, nullptr, 0, 100, 0, pct});
        cs.push_back({L + 160, yA + 170, 14, false, "MODUS", &s.mode, nullptr, 0, 2, 0, [](int v) { return std::string(kModeNames[v % 3]); }});
        cs.push_back({L + 232, yA + 170, 14, false, "GLIDE", &s.glide, nullptr, 0, 100, 0, pct});
        cs.push_back({L + 302, yA + 170, 14, false, "ACCENT", &s.accent, nullptr, 0, 100, 0, pct});
    }
    cs.push_back({R + 168, yA + 46, 150, true, "FILTER", &s.ftype, nullptr, 0, 3, col, [](int v) { return std::string(kFilterNames[v & 3]); }});
    cs.push_back({R + 60, yA + 124, 28, false, "CUTOFF", &tr.cutoff, nullptr, 0, 100, col, pct});
    cs.push_back({R + 160, yA + 92, 17, false, "RES", &s.res, nullptr, 0, 100, 0, pct});
    cs.push_back({R + 232, yA + 92, 17, false, "ENV", &s.fenv, nullptr, 0, 100, 0, pct});
    cs.push_back({R + 302, yA + 92, 17, false, "DRIVE", &s.drive, nullptr, 0, 100, 0, pct});
    cs.push_back({R + 160, yA + 170, 14, false, "F.ATK", &s.fatk, nullptr, 0, 100, 0, pct});
    cs.push_back({R + 232, yA + 170, 14, false, "F.DEC", &s.fdec, nullptr, 0, 100, 0, pct});
    cs.push_back({R + 302, yA + 170, 14, false, "KEY", &s.key, nullptr, 0, 100, 0, pct});
    const char* el[4] = {"A", "D", "S", "R"};
    uint8_t* ev[4] = {&s.a, &s.d, &s.s, &s.r};
    for (int k = 0; k < 4; k++) cs.push_back({L + 50 + k * 78.f, yB + 66, 19, false, el[k], ev[k], nullptr, 0, 100, 0, pct});
    const char* fl[4] = {"CHORUS", "DIST", "LO-FI", "VERB"};
    uint8_t* fv[4] = {&s.chorus, &s.dist, &s.lofi, &tr.rev};
    for (int k = 0; k < 4; k++) cs.push_back({R + 50 + k * 78.f, yB + 66, 19, false, fl[k], fv[k], nullptr, 0, 100, 0, pct});
}

void App::handleSyn(const Input& in, double dt) {
    if (pressed[B_B]) bUsed = false;
    if (in.held[B_SELECT]) {
        if (pressed[B_Y]) randomizeTrack(synT);
        if (pressed[B_X]) {
            if (undoTrackIdx >= 0) { std::swap(prj.tr[undoTrackIdx], undoTrack); synT = undoTrackIdx; msg("UNDO"); dirty = true; }
            else msg("NICHTS ZUM RÜCKGÄNGIG MACHEN");
        }
        return;
    }
    if (pressed[B_L1]) { synT = (synT + NT - 1) % NT; synSel = 0; }
    if (pressed[B_R1]) { synT = (synT + 1) % NT; synSel = 0; }
    if (pressed[B_X]) { synGroove = !synGroove; synSel = 0; }
    std::vector<Ctl> cs;
    buildSynCtls(cs);
    if (in.held[B_B] && pressed[B_A]) {
        // Regler auf Standardwert
        const Ctl& k = cs[std::clamp(synSel, 0, int(cs.size()) - 1)];
        std::string lb = k.label;
        int v = k.lo;
        if (lb == "SWING" || lb == "PAN" || lb == "DECAY" || lb == "TIMBRE" || lb == "HARM" || lb == "MORPH" || lb == "KEY") v = 50;
        else if (lb == "CUTOFF") v = 100;
        else if (lb == "PITCH") v = 0;
        k.set(std::clamp(v, k.lo, k.hi));
        bUsed = true;
        dirty = true;
        return;
    }
    handleCtls(in, dt, cs, synSel, true);
    if (pressed[B_A] && !in.held[B_B]) {
        Step st;
        st.on = 1;
        st.note = lastNote[synT];
        eng.preview(synT, st);
    }
    if (pressed[B_L2] || pressed[B_R2]) {
        int d = pressed[B_R2] ? 1 : -1;
        if (synT < NDRUM) {
            int n = int(eng.drums[synT].size());
            if (n) prj.tr[synT].sample = int8_t((prj.tr[synT].sample + d + n) % n);
            Step st;
            st.on = 1;
            eng.preview(synT, st);
        } else {
            loadPreset(synT, prj.tr[synT].preset + d);
            msg(presetName(synT, prj.tr[synT].preset));
            Step st;
            st.on = 1;
            st.note = lastNote[synT];
            if (!eng.playing) eng.preview(synT, st);
        }
        dirty = true;
    }
    if (pressed[B_Y]) saveUserPreset(synT);
}

enum MenuItem { MI_TEMPO, MI_SWING, MI_LEN, MI_ROOT, MI_SCALE, MI_KRABBO, MI_LIMIT, MI_MASTER, MI_MIDI, MI_RELOAD, MI_SAVE, MI_NEW, MI_COUNT };
static const char* kMenuLabels[MI_COUNT] = {"TEMPO", "SWING", "PATTERN-LÄNGE", "GRUNDTON", "TONLEITER", "KRABBO", "LIMITER MAIN",
                                           "MAIN-PEGEL", "MIDI-TEST", "SAMPLES NEU LADEN", "JETZT SPEICHERN", "NEUES PROJEKT"};

void App::handleMenu(const Input& in) {
    if (pressed[B_B]) { menuOpen = false; return; }
    if (rep[B_UP]) { menuSel = (menuSel + MI_COUNT - 1) % MI_COUNT; confirmNew = false; }
    if (rep[B_DOWN]) { menuSel = (menuSel + 1) % MI_COUNT; confirmNew = false; }
    int d = rep[B_RIGHT] ? 1 : rep[B_LEFT] ? -1 : 0;
    Pattern& pat = prj.pat[prj.curPat];
    if (d) {
        dirty = true;
        switch (menuSel) {
            case MI_TEMPO: prj.bpm = uint16_t(std::clamp(prj.bpm + d * (in.held[B_A] ? 10 : 1), 40, 220)); break;
            case MI_SWING: prj.swing = uint8_t(std::clamp(prj.swing + d, 50, 75)); break;
            case MI_LEN: {
                int nl = std::clamp(pat.len + d * 16, 16, MAXSTEPS);
                for (int s = pat.len; s < nl; s++)
                    for (int t = 0; t < NT; t++)
                        for (int v = 0; v < NVAR; v++) pat.s[t][v][s] = pat.s[t][v][s % 16];
                pat.len = uint8_t(nl);
                curS = std::min(curS, nl - 1);
                break;
            }
            case MI_ROOT: prj.root = uint8_t((prj.root + d + 12) % 12); break;
            case MI_SCALE: prj.scale = uint8_t((prj.scale + d + 4) % 4); break;
            case MI_KRABBO: prj.krabbo = uint8_t((prj.krabbo + d + 3) % 3); break;
            case MI_LIMIT: prj.limiter = !prj.limiter; break;
            case MI_MASTER: prj.master = uint8_t(std::clamp(prj.master + d * 2, 0, 100)); break;
        }
    }
    if (pressed[B_A]) {
        if (menuSel == MI_MIDI) { midiTest = true; menuOpen = false; }
        else if (menuSel == MI_RELOAD) {
            eng.stop();
            eng.allNotesOff();
            samplesLoaded = eng.loadSamples(dataDir + "/samples");
            msg(fmt("%d SAMPLES GELADEN", samplesLoaded));
        } else if (menuSel == MI_SAVE) {
            save();
            msg("GESPEICHERT");
        } else if (menuSel == MI_NEW) {
            if (!confirmNew) { confirmNew = true; msg("NOCHMAL A = ALLES LÖSCHEN"); }
            else {
                eng.stop();
                eng.allNotesOff();
                uint8_t kr = prj.krabbo;
                initProject(prj);
                prj.krabbo = kr;
                confirmNew = false;
                hasUndo = false;
                dirty = true;
                menuOpen = false;
                msg("NEUES PROJEKT");
            }
        }
    }
}

// ================================================================ Krabbo
void App::krabboTick() {
    if (prj.krabbo == 0 || menuOpen || midiTest) { krMode = -1; return; }
    if (krMode < 0) {
        if (now >= krNext) {
            uint32_t r = uint32_t(now * 1000) * 2654435761u;
            krMode = int((r >> 8) % 3);
            krStart = now;
            krY = 180.f + float((r >> 16) % 260);
        }
        return;
    }
    if (now - krStart > 4.5) {
        krMode = -1;
        uint32_t r = uint32_t(now * 777) * 2246822519u;
        double lo = prj.krabbo == 2 ? 12 : 30, hi = prj.krabbo == 2 ? 40 : 120;
        krNext = now + lo + (r % 1000) / 1000.0 * (hi - lo);
    }
}

void drawKrabboAt(Canvas& c, float x, float y, float s, float t, float look) {
    Col shell = rgb(0x8a7bff), shellD = rgb(0x3b3290), leg = rgb(0x5a4fd0);
    for (int side = -1; side <= 1; side += 2)
        for (int k = 0; k < 3; k++) {
            float a = std::sin(t * 14.f + k * 2.1f + (side > 0 ? 1.f : 0.f)) * 3.f * s;
            float bx = x + side * (10 + k * 6) * s, by = y + 6 * s;
            c.line(bx, by, bx + side * 9 * s, by + (10 + a) * s, leg, 2.5f * s);
            c.line(bx + side * 9 * s, by + (10 + a) * s, bx + side * 13 * s, by + (17 + a) * s, shellD, 2.f * s);
        }
    for (int side = -1; side <= 1; side += 2) {
        float cx = x + side * 30 * s, cy = y - 4 * s + std::sin(t * 6.f) * 2 * s;
        c.line(x + side * 18 * s, y, cx, cy, leg, 3.f * s);
        c.ellipse(cx + side * 3 * s, cy - 3 * s, 7 * s, 5 * s, rgb(0xb8f23a), rgb(0x5f8a1a), 0.35f, 0.3f);
        c.ellipse(cx + side * 8 * s, cy - 7 * s, 2.5f * s, 2.5f * s, C_BG, C_BG);
    }
    c.ellipse(x, y, 24 * s, 14 * s, shell, shellD, 0.4f, 0.25f);
    for (int k = -1; k <= 1; k++) c.ellipse(x + k * 9 * s, y - 3 * s, 3 * s, 2 * s, rgb(0x3dc9ff, 200), rgb(0x3dc9ff, 120));
    for (int side = -1; side <= 1; side += 2) {
        float ex = x + side * 7 * s + std::sin(t * 3.f + side) * 1.5f * s, ey = y - 24 * s;
        c.line(x + side * 5 * s, y - 10 * s, ex, ey + 4 * s, leg, 2.5f * s);
        c.ellipse(ex, ey, 5.5f * s, 5.5f * s, rgb(0xffffff), rgb(0xd9dbe2));
        c.ellipse(ex + look * 2 * s, ey + 0.5f * s, 2.4f * s, 2.4f * s, C_INK, C_INK);
    }
}

void App::drawKrabbo(Canvas& c) {
    if (krMode < 0 || menuOpen || midiTest) return;
    float t = float(now - krStart), dur = 4.5f;
    float x, y, look = std::sin(t * 2.f);
    if (krMode == 0) {
        float in = std::min(1.f, t / 0.6f), out = std::min(1.f, std::max(0.f, (dur - t) / 0.6f));
        x = 742 - 50 * std::min(in, out);
        y = krY;
        look = -1;
    } else if (krMode == 1) {
        x = -40 + 800.f * (t / dur);
        y = 702;
    } else {
        // turnt an der BASS-Spur (auf der SEQ-Seite), sonst unten am Rand
        x = 650 + 8 * std::sin(t * 2.5f);
        float in = std::min(1.f, t / 0.5f), out = std::min(1.f, std::max(0.f, (dur - t) / 0.5f));
        float base = page == PG_SEQ ? 136 + 5 * 42 + 8 - 16 : 700;
        y = base + 40 * (1 - std::min(in, out)) - 6 * std::fabs(std::sin(t * 5.f));
    }
    drawKrabboAt(c, x, y, 0.9f, t, look);
}

// ================================================================ Zeichnen: gemeinsame Teile
void App::drawHeader(Canvas& c, const std::string& sub) {
    tx(c, 22, 25, "GENRA", L, C_TXT, 4);
    tx(c, 150, 27, sub, XS, C_DIM, 2);
    float x = 384;
    for (int i = 0; i < PG_COUNT; i++) {
        float w = 60;
        bool a = i == page && !menuOpen && !midiTest;
        c.roundRect(x, 12, w, 28, 3, a ? kPageCol[i] : C_TAB);
        tx(c, x + w / 2, 26, kPageNames[i], S, a ? C_INK : C_DIM, 1, 1);
        x += w + 4;
    }
}

void App::drawFooter(Canvas& c, const std::string& l1, const std::string& l2) {
    tx(c, 22, 680, l1, XS, C_DIM, 1);
    tx(c, 22, 700, l2, XS, dim(C_DIM, 0.3f), 1);
}

void App::drawCtl(Canvas& c, const Ctl& k, bool focus) {
    int v = k.get();
    Col col = k.col ? k.col : kTrackCol[page == PG_SYN ? synT : 0];
    if (page != PG_SYN && !k.col) col = C_MEL;
    if (k.drop) {
        dropdown(c, k.x - k.r, k.y - 13, 2 * k.r, 26, k.fmt ? k.fmt(v) : fmt("%d", v), col, k.label, focus);
        return;
    }
    float val = float(v - k.lo) / std::max(1, k.hi - k.lo);
    knob(c, k.x, k.y, k.r, val, k.col, focus);
    FontId lf = k.r >= 25 ? S : XS;
    tx(c, k.x, k.y + k.r + 16, k.label, lf, focus ? col : C_TXT, 1, 1);
    tx(c, k.x, k.y + k.r + 31, k.fmt ? k.fmt(v) : fmt("%d", v), lf, focus ? C_WHITE : C_DIM, 0, 1);
}

// ================================================================ SEQ
void App::drawSeq(Canvas& c) {
    Pattern& pat = prj.pat[prj.curPat];
    int len = pat.len;
    int view = (curS / 16) * 16;
    drawHeader(c, "R36 · STEP SEQUENCER");

    display(c, 18, 50, 684, 70);
    tx(c, 32, 74, kGenreNames[prj.genre], D, C_TXT, 1);
    tx(c, 32, 103, fmt("%s %s · %d BPM · SWING %d%%", kNoteNames[prj.root], kScaleNames[prj.scale], prj.bpm, prj.swing), XS, C_DIM, 1);
    float bx = 262;
    c.roundRect(bx, 60, 70, 24, 3, C_TAB);
    tx(c, bx + 35, 72, fmt("PAT %02d", prj.curPat + 1), S, C_TXT, 0, 1);
    c.roundRect(bx + 76, 60, 84, 24, 3, C_DRUM);
    tx(c, bx + 76 + 42, 72, len > 16 ? fmt("TAKT %d/%d", view / 16 + 1, len / 16) : std::string("16 STEPS"), S, C_INK, 0, 1);
    std::string info;
    Col ic = C_SMP;
    if (eng.queuedPat >= 0) info = fmt("NÄCHSTES › P%02d", eng.queuedPat + 1);
    if (eng.songMode) { info = fmt("SONG · ZEILE %d/%d", eng.songRow + 1, prj.songLen); ic = C_SONG; }
    if (eng.perfStutter) { static const char* sn[5] = {"", "1/4", "1/8", "1/16", "FREEZE"}; info = fmt("STUTTER %s", sn[eng.perfStutter]); ic = C_PAD; }
    else if (std::fabs(eng.perfFilter) > 0.02f) { info = fmt("FILTER %s %d%%", eng.perfFilter < 0 ? "LP" : "HP", int(std::fabs(eng.perfFilter) * 100)); ic = C_MEL; }
    if (!info.empty()) tx(c, bx, 103, info, XS, ic, 1);
    float rx = bx + 168;
    if (rec) { c.roundRect(rx, 60, 50, 24, 3, C_RED); tx(c, rx + 25, 72, "REC", S, C_INK, 1, 1); }
    if (!midi.devs.empty()) { circle(c, rx + 6, 103, 4, C_GO); tx(c, rx + 14, 103, "MIDI", XS, C_GO, 1); }
    struct B_ { const char* l; const char* k; Col on; };
    B_ bs[3] = {{"NEW ALL", "SEL+Y", C_GO}, {"NEW", "Y", 0}, {"UNDO", "SEL+X", 0}};
    for (int i = 0; i < 3; i++) {
        float cx = 556 + i * 60;
        circle(c, cx, 69, 13, C_INK);
        circle(c, cx, 69, 11, bs[i].on ? bs[i].on : C_BTN);
        tx(c, cx, 93, bs[i].l, XS, C_TXT, 0, 1);
        tx(c, cx, 108, bs[i].k, XS, C_DIM, 0, 1);
    }

    c.roundRect(18, 129, 684, 351, 5, C_PLATE);
    c.roundRect(18, 126, 336, 4, 2, C_DRUM);
    c.roundRect(358, 126, 344, 4, 2, C_PAD);
    const float gx0 = 104, gx1 = 692, gap = 4, grp = 6;
    float sw = (gx1 - gx0 - 15 * gap - 3 * grp) / 16.f;
    bool anySolo = false;
    for (auto& t : prj.tr) anySolo |= t.solo != 0;
    for (int t = 0; t < NT; t++) {
        float y = 136 + t * 42 + (t >= NDRUM ? 8 : 0);
        Col col = kTrackCol[t];
        bool aud = anySolo ? prj.tr[t].solo : !prj.tr[t].mute;
        c.roundRect(26, y + 2, 4, 30, 2, aud ? col : C_LINE);
        tx(c, 36, y + 10, kTrackNames[t], S, t == curT ? col : (aud ? C_TXT : C_DIM), 1);
        int var = pat.var[t] & 3;
        for (int v = 0; v < NVAR; v++) {
            bool used = false;
            for (int s = 0; s < len && !used; s++) used = pat.s[t][v][s].on;
            Col vc = v == var ? col : (used ? C_DIM : dim(C_DIM, 0.6f));
            tx(c, 36 + v * 12, y + 26, std::string(1, char('A' + v)), XS, vc, 0);
        }
        if (prj.tr[t].mute) tx(c, 90, y + 26, "M", XS, C_RED, 0, 1);
        else if (prj.tr[t].solo) tx(c, 90, y + 26, "S", XS, C_GO, 0, 1);
        int tl = trackLen(prj, pat, t);
        int ps = eng.playing ? eng.playStepT[t] : -1;
        for (int i = 0; i < 16; i++) {
            int s = view + i;
            float x = gx0 + i * (sw + gap) + (i / 4) * grp;
            const Step& st = pat.s[t][var][s];
            bool out = s >= tl;
            if (s == ps) c.roundRect(x - 2, y - 2, sw + 4, 38, 4, rgb(0x3a3e4a));
            if (out) c.roundRect(x, y, sw, 34, 3, rgb(0x121318));
            else if (st.on) {
                Col fc = aud ? col : dim(col, 0.6f);
                if (s == ps) fc = mix(fc, C_WHITE, 0.3f);
                c.roundRect(x, y, sw, 34, 3, fc);
                c.roundRect(x, y, sw, 5, 2, mix(fc, C_WHITE, 0.35f));
                if (t >= NDRUM) {
                    std::string nn = (t == T_PAD || st.chord) ? std::string(kNoteNames[st.note % 12]) : noteName(st.note);
                    tx(c, x + sw / 2, y + 20, nn, XS, C_INK, 0, 1);
                } else if (st.sample >= 0) circle(c, x + sw / 2, y + 26, 2.5f, C_INK);
                if (st.lockMask & ~(1 << P_REPEAT)) circle(c, x + sw - 5, y + 10, 2.5f, C_WHITE);
                int rp = stepParam(st, prj.tr[t], P_REPEAT);
                for (int k = 1; k < rp; k++) c.fillRect(int(x + 4 + (k - 1) * 5), int(y + 28), 3, 3, C_INK);
            } else {
                c.roundRect(x, y, sw, 34, 3, (i % 4) ? C_DISP : rgb(0x0c0d10));
                c.roundStroke(x, y, sw, 34, 3, rgb(0x26282f), 1.f);
            }
            if (t == curT && s == curS) c.roundStroke(x - 3, y - 3, sw + 6, 40, 5, C_WHITE, 2.f);
        }
    }
    c.fillRect(26, 136 + 4 * 42 - 3, 666, 1, C_LINE);

    const Step& st = pat.s[curT][pat.var[curT] & 3][curS];
    const Track& tr = prj.tr[curT];
    bool inst = curT >= NDRUM;
    Col col = kTrackCol[curT];
    section(c, 18, 488, 684, 172, col, "P-LOCK");
    tx(c, 32, 518, fmt("%s · STEP %d · VARIANTE %c", kTrackNames[curT], curS + 1, 'A' + (pat.var[curT] & 3)), S, C_TXT, 1);
    std::string right;
    if (!st.on) right = "STEP AUS";
    else if (!inst) {
        int idx = st.sample >= 0 ? st.sample : tr.sample;
        const SampleBuf* b = eng.drumSample(curT, idx);
        right = (st.sample < 0 ? std::string("SPUR: ") : std::string("")) + (b ? b->name : "-");
    } else right = isSynthTrack(curT) ? presetName(curT, tr.preset) : std::string(st.chord ? "AKKORD" : "EINZELNOTE");
    tx(c, 688, 518, right, S, C_DIM, 0, 2);
    for (int p = 0; p < NPARAM; p++) {
        float cx = 78 + p * 113;
        bool selp = p == lockSel;
        int v = stepParam(st, tr, p);
        std::string val;
        float pct;
        if (p == P_PITCH && inst) { val = noteName(st.note); pct = (st.note - 24) / 72.f; }
        else if (p == P_PITCH) { val = v > 0 ? fmt("+%d", v) : fmt("%d", v); pct = (v + 24) / 48.f; }
        else if (p == P_DECAY && inst) { val = fmt("%d ST", 1 + (v * 15 + 50) / 100); pct = v / 100.f; }
        else if (p == P_REPEAT) { val = fmt("%d×", v); pct = (v - 1) / 3.f; }
        else { val = fmt("%d", v); pct = v / 100.f; }
        bool locked = st.lockMask & (1 << p);
        knob(c, cx, 572, selp ? 25 : 20, pct, selp ? col : 0, false);
        if (selp && prev.held[B_B]) ring(c, cx, 572, 35, C_WHITE, 1.5f);
        tx(c, cx, 614, (inst ? kParamNamesInst : kParamNamesDrum)[p], XS, selp ? col : C_TXT, 1, 1);
        tx(c, cx, 634, val, S, locked ? C_WHITE : C_DIM, 0, 1);
        if (locked) circle(c, cx + 24, 550, 3, C_WHITE);
    }
    drawFooter(c, "KREUZ: STEP · A: SETZEN · L2/R2: " + std::string(inst ? "NOTE" : "SAMPLE") + " · L1/R1: PATTERN · X: STUMM · Y: NEW",
               "B: P-LOCK · R3+‹›: ROTATION · SEL+L1/R1: VARIANTE · SEL+B: REC · STICKS: FILTER/STUTTER");
}

// ================================================================ SONG
void App::drawSong(Canvas& c) {
    drawHeader(c, "R36 · SONG ARRANGER");
    section(c, 18, 52, 300, 444, C_SONG, "SONG");
    display(c, 30, 80, 276, 300);
    if (songFocusList) c.roundStroke(27, 77, 282, 306, 8, C_SONG, 1.5f);
    tx(c, 42, 94, "#", XS, C_DIM, 2);
    tx(c, 72, 94, "PATTERN", XS, C_DIM, 2);
    tx(c, 294, 94, "TAKTE", XS, C_DIM, 2, 2);
    int first = std::clamp(songSel - 4, 0, std::max(0, prj.songLen - 8));
    for (int i = 0; i < 8; i++) {
        int r = first + i;
        if (r >= prj.songLen) break;
        const SongRow& row = prj.song[r];
        float y = 116 + i * 32;
        bool cur = eng.songMode && eng.songRow == r;
        bool sel = r == songSel;
        if (cur) { c.roundRect(34, y - 13, 268, 26, 2, rgb(0x3a3216)); c.roundRect(34, y - 13, 3, 26, 1, C_SONG); }
        if (sel && songFocusList) c.roundStroke(33, y - 14, 270, 28, 3, C_WHITE, 1.5f);
        Col pc = rgb(kPartColors[row.part % NPART]);
        tx(c, 42, y, fmt("%02d", r + 1), S, cur ? C_SONG : C_DIM, 0);
        tx(c, 72, y, fmt("P%02d", row.pat + 1), S, cur ? C_SONG : C_TXT, 0);
        c.roundRect(118, y - 8, 4, 16, 1, pc);
        tx(c, 128, y, kPartNames[row.part % NPART], S, cur ? pc : C_TXT, 1);
        tx(c, 292, y, fmt("%d", row.bars), S, cur ? C_SONG : C_TXT, 0, 2);
    }
    if (prj.songLen == 0) {
        tx(c, 168, 200, "SONG IST LEER", S, C_DIM, 1, 1);
        tx(c, 168, 222, "PAD WÄHLEN, R1: EINFÜGEN", XS, C_DIM, 1, 1);
    }
    tx(c, 296, 368, fmt("ZEILE %d / %d · LOOP %s", prj.songLen ? songSel + 1 : 0, prj.songLen, prj.songLoop ? "AN" : "AUS"), XS, C_DIM, 1, 2);
    chip(c, 32, 402, "SEL+A", "SONG AB ZEILE", C_SONG);
    chip(c, 32, 426, "R1", "PAD EINFÜGEN", C_SONG);
    chip(c, 32, 450, "L1", "ZEILE LÖSCHEN", C_SONG);
    chip(c, 32, 474, "‹ ›", "TAKTE · L2/R2: TEIL", C_SONG);

    section(c, 330, 52, 372, 444, C_SONG, "PATTERNS");
    if (!songFocusList) c.roundStroke(327, 49, 378, 450, 7, C_SONG, 1.5f);
    for (int i = 0; i < NPAT; i++) {
        float x = 344 + (i % 4) * 88, y = 80 + (i / 4) * 102;
        int cnt = 0;
        for (int t = 0; t < NT; t++)
            for (int s = 0; s < prj.pat[i].len; s++) cnt += prj.pat[i].s[t][prj.pat[i].var[t] & 3][s].on;
        int part = NPART - 1;
        for (int r = 0; r < prj.songLen; r++) if (prj.song[r].pat == i) { part = prj.song[r].part; break; }
        bool cur = i == prj.curPat;
        if (cnt) {
            Col pc = rgb(kPartColors[part]);
            c.roundRect(x, y, 80, 92, 4, mix(C_PLATE, pc, 0.35f));
            c.roundRect(x + 6, y + 6, 68, 5, 2, cur ? C_SONG : dim(C_SONG, 0.55f));
            tx(c, x + 8, y + 26, fmt("P%02d", i + 1), XS, C_TXT, 1);
            tx(c, x + 8, y + 46, kPartNames[part], S, C_TXT, 0);
            tx(c, x + 8, y + 64, fmt("%d TAKT", prj.pat[i].len / 16), XS, C_DIM, 1);
            static const int tk[4] = {T_KICK, T_BASS, T_LEAD, T_PAD};
            for (int k = 0; k < 4; k++) {
                bool any = false;
                for (int s = 0; s < prj.pat[i].len && !any; s++) any = prj.pat[i].s[tk[k]][prj.pat[i].var[tk[k]] & 3][s].on;
                c.roundRect(x + 8 + k * 12, y + 80, 9, 4, 2, any ? kTrackCol[tk[k]] : C_TAB);
            }
        } else {
            c.roundRect(x, y, 80, 92, 4, rgb(0x1b1c21));
            tx(c, x + 8, y + 26, fmt("P%02d", i + 1), XS, rgb(0x4a4c55), 1);
        }
        if (eng.queuedPat == i)
            for (int k = 0; k < 10; k++) { c.roundRect(x - 2 + k * 8.6f, y - 4, 4.5f, 2, 1, C_WHITE); c.roundRect(x - 2 + k * 8.6f, y + 94, 4.5f, 2, 1, C_WHITE); }
        if (cur && eng.playing) circle(c, x + 70, y + 26, 4, C_GO);
        if (i == songPad && !songFocusList) c.roundStroke(x - 3, y - 3, 86, 98, 5, C_WHITE, 2.f);
        else if (i == songPad) c.roundStroke(x - 3, y - 3, 86, 98, 5, C_DIM, 1.f);
        if (clipPat == i) tx(c, x + 72, y + 46, "C", XS, C_MEL, 0, 2);
    }
    display(c, 18, 506, 684, 40);
    if (prj.songLen) {
        const SongRow& r = prj.song[songSel];
        tx(c, 32, 526, fmt("ZEILE %d  ·  P%02d  ·  %d TAKTE", songSel + 1, r.pat + 1, r.bars), S, C_TXT, 1);
        tx(c, 300, 526, kPartNames[r.part % NPART], S, rgb(kPartColors[r.part % NPART]), 2);
    }
    tx(c, 688, 526, eng.songMode ? fmt("SONG LÄUFT · ZEILE %d", eng.songRow + 1) : std::string("PATTERN-LOOP"), S, eng.songMode ? C_SONG : C_DIM, 1, 2);
    section(c, 18, 554, 684, 106, C_SONG, "TEILE");
    int curPart = prj.songLen ? prj.song[songSel].part : -1;
    for (int i = 0; i < 6; i++) {
        float cx = 78 + i * 112;
        Col pc = rgb(kPartColors[i]);
        circle(c, cx, 604, 18, C_INK);
        circle(c, cx, 604, 16, i == curPart ? pc : dim(pc, 0.55f));
        if (i == curPart) ring(c, cx, 604, 22, C_WHITE, 1.5f);
        tx(c, cx, 638, kPartNames[i], XS, pc, 1, 1);
    }
    drawFooter(c, "B: LISTE/PADS · A: SPIELEN (PADS) / PATTERN SETZEN (LISTE) · X/Y: KOPIEREN/EINFÜGEN",
               "SEL+Y: LOOP · SEL+X: UNDO · STICKS: FILTER/STUTTER");
}

// ================================================================ MIX
void App::drawMix(Canvas& c) {
    drawHeader(c, "R36 · MIXER");
    static const char* keys[NT] = {"L1", "L2", "R1", "R2", "A", "B", "X", "Y"};
    float x = 18;
    for (int i = 0; i <= NT; i++) {
        bool master = i == NT;
        float w = master ? 80 : 70;
        if (i == NDRUM) x += 6;
        if (master) x += 6;
        Col col = master ? C_TXT : kTrackCol[i];
        bool sel = i == mixSel;
        c.roundRect(x, 55, w, 608, 5, sel ? rgb(0x262932) : C_PLATE);
        c.roundRect(x, 52, w, 4, 2, col);
        if (sel) c.roundStroke(x, 52, w, 611, 5, col, 1.2f);
        float cx = x + w / 2;
        tx(c, cx, 74, master ? "MAIN" : kTrackNames[i], S, col, 1, 1);
        if (!master) {
            bool m = prj.tr[i].mute, so = prj.tr[i].solo;
            c.roundRect(x + 8, 88, w - 16, 24, 3, m ? C_RED : so ? C_GO : C_TAB);
            tx(c, cx, 100, keys[i], S, (m || so) ? C_INK : C_DIM, 0, 1);
            tx(c, cx, 124, m ? "MUTE" : so ? "SOLO" : "", XS, m ? C_RED : C_GO, 1, 1);
        }
        float pan = master ? 0.5f : prj.tr[i].pan / 100.f;
        knob(c, cx, 156, 13, pan, 0, false);
        std::string ps = master ? std::string(prj.limiter ? "LIMIT" : "-") : (prj.tr[i].pan == 50 ? std::string("C") : prj.tr[i].pan < 50 ? fmt("L%d", 50 - prj.tr[i].pan) : fmt("R%d", prj.tr[i].pan - 50));
        tx(c, cx, 184, ps, XS, C_DIM, 0, 1);
        if (!master) {
            const char* sl[2] = {"REV", "DLY"};
            int sv[2] = {prj.tr[i].rev, prj.tr[i].dly};
            for (int k = 0; k < 2; k++) {
                float yy = 202 + k * 16;
                tx(c, x + 6, yy, sl[k], XS, C_DIM, 0);
                c.roundRect(x + 32, yy - 2, w - 38, 4, 2, C_DISP);
                if (sv[k]) c.roundRect(x + 32, yy - 2, (w - 38) * sv[k] / 100.f, 4, 2, k ? mix(col, C_WHITE, 0.4f) : col);
            }
        }
        float fy0 = 240, fy1 = 614;
        float fx = cx - 9;
        c.roundRect(fx - 3, fy0, 6, fy1 - fy0, 3, C_DISP);
        for (int k = 0; k <= 10; k++) c.fillRect(int(fx - 12), int(fy0 + (fy1 - fy0) * k / 10), 5, 1, C_DOT);
        int vol = master ? prj.master : prj.tr[i].vol;
        float cy = fy1 - (fy1 - fy0) * vol / 100.f;
        c.roundRect(fx - 12, cy - 9, 24, 18, 3, C_TXT);
        c.fillRect(int(fx - 9), int(cy), 18, 2, C_INK);
        float mx = cx + 15;
        c.roundRect(mx - 3, fy0, 6, fy1 - fy0, 3, C_DISP);
        float lv = master ? std::max(eng.meterL, eng.meterR) : std::min(1.f, eng.meter[i] * 1.8f);
        if (!master && prj.tr[i].mute) lv = 0;
        float mh = (fy1 - fy0) * std::min(1.f, lv);
        if (mh > 2) c.roundRect(mx - 3, fy1 - mh, 6, mh, 3, master ? C_GO : col);
        tx(c, cx, 640, fmt("%d", vol), M, sel ? C_WHITE : C_TXT, 0, 1);
        x += w + 4;
    }
    drawFooter(c, "L1 L2 R1 R2 A B X Y: MUTE · SEL + TASTE: SOLO · SEL + AUF/AB: ALLES AUFHEBEN", "KREUZ ‹›: KANAL · STICK L / AUF/AB: LAUTSTÄRKE · STICK R: PAN");
}

// ================================================================ FX
void App::drawFx(Canvas& c) {
    drawHeader(c, "R36 · SEND EFFECTS");
    static const char* dn[3] = {"PING-PONG", "TAPE", "MONO"};
    static const char* hn[3] = {"KAMMER", "HALLE", "PLATTE"};
    static const char* pre[4] = {"0", "10", "30", "60"};
    display(c, 18, 52, 684, 100);
    tx(c, 32, 70, "SEND 1", XS, C_DIM, 3);
    tx(c, 32, 96, dn[prj.dlyModel % 3], D, prj.dlyOn ? C_TXT : C_DIM, 1);
    tx(c, 32, 126, prj.dlyOn ? fmt("DELAY %s · FB %d", kSync[prj.dlyTime & 3], prj.dlyFb) : std::string("AUS"), XS, C_MEL, 2);
    for (int k = 0; k < 7; k++) {
        float h = 50 * std::pow(0.62f, float(k)) * (0.4f + prj.dlyFb / 100.f);
        c.roundRect(240 + k * 26, 136 - h, 8, h, 2, mix(k % 2 ? C_PAD : C_MEL, C_DISP, k * 0.1f));
    }
    tx(c, 688, 70, "SEND 2", XS, C_DIM, 3, 2);
    tx(c, 688, 96, hn[prj.revModel % 3], D, prj.revOn ? C_TXT : C_DIM, 1, 2);
    tx(c, 688, 126, prj.revOn ? fmt("HALL · SIZE %d · PRE %s MS", prj.revSize, pre[prj.revPre & 3]) : std::string("AUS"), XS, C_PAD, 2, 2);
    for (int k = 0; k < 36; k++) {
        float h = 48 * std::exp(-k / (6.f + prj.revSize / 8.f)) * (0.7f + 0.3f * std::sin(k * 1.7f));
        c.roundRect(380 + k * 3.2f, 136 - h, 2, h, 1, mix(C_PAD, C_DISP, k / 40.f));
    }
    if (prj.duck) { c.roundRect(338, 60, 64, 20, 3, C_GO); tx(c, 370, 70, "DUCK", XS, C_INK, 1, 1); }

    for (int blk = 0; blk < 2; blk++) {
        float x = 18 + blk * 348;
        Col col = blk ? C_PAD : C_MEL;
        section(c, x, 164, 336, 290, col, blk ? "HALL" : "DELAY");
        dropdown(c, x + 16, 194, 304, 26, blk ? hn[prj.revModel % 3] : dn[prj.dlyModel % 3], col, "L1/R1", false);
        const char* const* opts = blk ? pre : kSync;
        int cur = blk ? prj.revPre & 3 : prj.dlyTime & 3;
        for (int k = 0; k < 4; k++) {
            float cx = x + 60 + k * 72;
            circle(c, cx, 398, 7, k == cur ? col : C_DARK);
            tx(c, cx, 418, opts[k], XS, k == cur ? C_TXT : C_DIM, 0, 1);
        }
        tx(c, x + 168, 440, blk ? "PRE-DELAY MS · L2/R2" : "SYNC · L2/R2", XS, C_DIM, 2, 1);
    }
    section(c, 18, 464, 684, 198, C_TXT, "SENDS PRO SPUR");
    for (int t = 0; t < NT; t++) tx(c, 62 + t * 74, 500, kTrackNames[t], XS, kTrackCol[t], 1, 1);
    tx(c, 664, 500, "DUCK", XS, prj.duck ? C_GO : C_DIM, 1, 1);
    std::vector<Ctl> cs;
    buildFxCtls(cs);
    for (int i = 0; i < int(cs.size()); i++) drawCtl(c, cs[i], i == fxSel);
    drawFooter(c, "KREUZ: REGLER · STICK L: WERT · B + AUF/AB: 10ER · L1/R1: MODELL · L2/R2: SYNC/PRE", "X: DUCKING · Y: EFFEKT AN/AUS · A: TESTKLANG");
}

// ================================================================ SYN
void App::drawSyn(Canvas& c) {
    drawHeader(c, "R36 · SPUR & SYNTH");
    for (int t = 0; t < NT; t++) {
        float x = 18 + t * 86.f;
        Col col = kTrackCol[t];
        c.roundRect(x, 50, 82, 26, 3, t == synT ? col : C_TAB);
        tx(c, x + 41, 63, kTrackNames[t], S, t == synT ? C_INK : col, 0, 1);
    }
    Track& tr = prj.tr[synT];
    Col col = kTrackCol[synT];
    for (int k = 0; k < 2; k++) {
        bool a = (k == 1) == synGroove;
        c.roundRect(18 + k * 80, 84, 76, 26, 3, a ? col : C_TAB);
        tx(c, 56 + k * 80, 97, k ? "GROOVE" : "KLANG", S, a ? C_INK : C_DIM, 0, 1);
    }
    const SampleBuf* db = synT < NDRUM ? eng.drumSample(synT, tr.sample) : nullptr;
    std::string pname = synT < NDRUM ? "SAMPLE: " + std::string(db ? db->name : "-") : fmt("%02d  %s", tr.preset + 1, presetName(synT, tr.preset).c_str());
    dropdown(c, 186, 84, 516, 26, pname, col, synT < NDRUM ? "L2/R2 SAMPLE" : "L2/R2 PRESET", false);
    display(c, 18, 118, 684, 80);
    if (synGroove) {
        tx(c, 32, 136, "GROOVE-CHARAKTER", XS, C_DIM, 3);
        tx(c, 32, 162, kGrooveNames[tr.groove & 3], D, C_TXT, 1);
        static const char* gd[4] = {"STRAFF AUF DEM RASTER", "LOCKER, HINTER DEM BEAT, TONHÖHE WACKELT",
                                    "STRAFF, ANSCHLAG SCHWANKT", "DRIFT BAUT SICH ÜBER LANGE LOOPS AUF"};
        tx(c, 32, 186, gd[tr.groove & 3], XS, col, 1);
        for (int k = 0; k < 16; k++) {
            float off = tr.groove == G_808 ? 3 + (k * 37 % 7) : tr.groove == G_909 ? (k * 13 % 3) : tr.groove == G_MINIMAL ? 6 * std::sin(k * 0.4f) + 6 : 0;
            if (k & 1) off += (tr.swing - 50) * 0.5f;
            float x = 380 + k * 19 + off;
            c.roundRect(x, 140, 3, 40, 1, k % 4 ? dim(col, 0.4f) : col);
        }
    } else if (synT < NDRUM) {
        tx(c, 32, 136, fmt("SAMPLE %d / %d", tr.sample + 1, int(eng.drums[synT].size())), XS, C_DIM, 3);
        tx(c, 32, 164, db ? db->name : "-", D, C_TXT, 0);
        if (db && !db->d.empty()) {
            int n = int(db->d.size());
            for (int k = 0; k < 290; k++) {
                float mx = 0;
                for (int j = k * n / 290; j < (k + 1) * n / 290; j++) mx = std::max(mx, std::fabs(db->d[j]));
                c.fillRect(398 + k, int(158 - mx * 32), 1, std::max(1, int(mx * 64)), col);
            }
        }
    } else {
        bool synth = isSynthTrack(synT);
        tx(c, 32, 136, synth ? fmt("PLAITS ENGINE %d / 21", tr.syn.engine + 1) : std::string("SAMPLE-SPUR (KEYGROUP, GRUNDTON C3)"), XS, C_DIM, 3);
        std::string nm = synth ? kEngineNames[tr.syn.engine % NENGINE] : (eng.keys.empty() ? std::string("E-PIANO") : eng.keys[std::clamp<int>(tr.sample, 0, int(eng.keys.size()) - 1)].name);
        tx(c, 32, 164, nm, D, C_TXT, 0);
        tx(c, 32, 186, fmt("%s · %s", kModeNames[tr.syn.mode % 3], kFilterNames[tr.syn.ftype & 3]), XS, col, 2);
        float pxp = 0, pyp = 0;
        for (int k = 0; k < 220; k++) {
            float t = k / 219.f;
            float ph = std::fmod(t * 3.f, 1.f);
            float saw = ph * 2 - 1, sq = ph < 0.5f ? 1.f : -1.f, sn = std::sin(ph * 6.2832f);
            float m = tr.syn.morph / 100.f, tb = tr.syn.timbre / 100.f;
            float y = (sn * (1 - m) + (saw * (1 - tb) + sq * tb) * m) * 26;
            float px = 380 + t * 230, py = 158 - y;
            if (k) c.line(pxp, pyp, px, py, col, 2.f);
            pxp = px; pyp = py;
        }
        tx(c, 688, 164, noteName(lastNote[synT]), D, C_TXT, 0, 2);
    }
    if (synGroove) {
        section(c, 18, 206, 684, 222, col, "GROOVE");
        section(c, 18, 436, 684, 156, col, "SO WIRKT ES");
        const char* lines[5] = {"808: LOCKER, ETWAS HINTER DEM BEAT, LEICHTE TONHÖHEN-SCHWANKUNG",
                                "909: STRAFF, NUR KLEINE UNTERSCHIEDE IM ANSCHLAG",
                                "MINIMAL: DRIFT WÄCHST MIT JEDEM DURCHLAUF - HYPNOTISCH BEI LANGEN LOOPS",
                                "SWING PRO SPUR KOMMT ZUM GLOBALEN SWING DAZU",
                                "LÄNGE: EIGENE SPURLÄNGE (Z. B. 7 STEPS) LÄUFT GEGEN DIE ANDEREN SPUREN"};
        for (int k = 0; k < 5; k++) tx(c, 34, 470 + k * 24, lines[k], XS, C_TXT, 1);
    } else if (synT < NDRUM) {
        section(c, 18, 206, 336, 222, col, "SAMPLE");
        section(c, 366, 206, 336, 222, col, "SENDS & MIX");
        section(c, 18, 436, 684, 156, col, "SAMPLES IM ORDNER");
        int n = int(eng.drums[synT].size());
        for (int k = 0; k < std::min(n, 12); k++) {
            float x = 34 + (k % 4) * 168, y = 470 + (k / 4) * 32;
            bool a = k == tr.sample;
            c.roundRect(x, y - 12, 160, 24, 3, a ? col : C_DISP);
            tx(c, x + 8, y, eng.drums[synT][k].name, XS, a ? C_INK : (k < eng.builtinCount[synT] ? C_DIM : C_TXT), 0);
        }
    } else {
        section(c, 18, 206, 336, 222, col, synT == T_SAMPLE ? "SAMPLE" : "OSC");
        section(c, 366, 206, 336, 222, col, "FILTER");
        section(c, 18, 436, 336, 156, col, "ENVELOPE");
        section(c, 366, 436, 336, 156, col, "FX");
    }
    std::vector<Ctl> cs;
    buildSynCtls(cs);
    for (int i = 0; i < int(cs.size()); i++) drawCtl(c, cs[i], i == synSel);
    c.roundRect(18, 600, 684, 58, 5, rgb(0x141519));
    c.roundRect(28, 606, 220, 3, 1, dim(col, 0.6f));
    float x = 28;
    const char* ks[6][2] = {{"A", "ANHÖREN"}, {"B+STICK", "WERT"}, {"L1/R1", "SPUR"}, {"L2/R2", synT < NDRUM ? "SAMPLE" : "PRESET"},
                            {"X", "KLANG/GROOVE"}, {"Y", synT < NDRUM ? "-" : "SPEICHERN"}};
    for (auto& k : ks) {
        chip(c, x, 634, k[0], k[1], C_TXT);
        x += Canvas::textW(k[0], XS, 1) + Canvas::textW(k[1], XS, 1) + 30;
    }
    drawFooter(c, "KREUZ: REGLER WÄHLEN · B + AUF/AB: FEIN · B + A: STANDARDWERT", "SEL+Y: WÜRFELN · SEL+X: UNDO");
}

// ================================================================ Menue + MIDI-Test
void App::drawMenu(Canvas& c) {
    drawHeader(c, "R36 · MENÜ");
    section(c, 18, 52, 684, 560, C_TXT, "EINSTELLUNGEN");
    Pattern& pat = prj.pat[prj.curPat];
    static const char* kr[3] = {"AUS", "SELTEN", "ÖFTER"};
    std::string vals[MI_COUNT] = {
        fmt("%d BPM", prj.bpm), fmt("%d %%", prj.swing), fmt("%d STEPS (P%02d)", pat.len, prj.curPat + 1), kNoteNames[prj.root],
        kScaleNames[prj.scale], kr[prj.krabbo % 3], prj.limiter ? "AN" : "AUS", fmt("%d", prj.master),
        fmt("%d GERÄT(E) · A", int(midi.devs.size())), fmt("%d GELADEN · A", samplesLoaded), "A", confirmNew ? "NOCHMAL A" : "A"};
    for (int i = 0; i < MI_COUNT; i++) {
        float y = 84 + i * 43;
        bool sel = i == menuSel;
        c.roundRect(34, y, 652, 35, 4, sel ? rgb(0x2c2f38) : C_DISP);
        if (sel) c.roundStroke(34, y, 652, 35, 4, C_WHITE, 1.5f);
        tx(c, 50, y + 17, kMenuLabels[i], S, sel ? C_WHITE : C_TXT, 1);
        bool action = i >= MI_MIDI;
        std::string v = action ? vals[i] : "‹ " + vals[i] + " ›";
        tx(c, 670, y + 17, v, S, sel ? (i == MI_NEW && confirmNew ? C_RED : C_GO) : C_DIM, 1, 2);
    }
    display(c, 18, 620, 684, 40);
    tx(c, 32, 640, fmt("R36 GENRA V0.2 · CPU %d %%", int(eng.cpuLoad * 100)), XS, C_DIM, 1);
    tx(c, 688, 640, "BEENDEN: START + SELECT", S, C_GO, 0, 2);
    drawFooter(c, "KREUZ AUF/AB: EINTRAG · ‹›: WERT · A: AUSFÜHREN · B: ZURÜCK", "TEMPO: A HALTEN = 10ER-SCHRITTE");
}

void App::drawMidiTest(Canvas& c) {
    drawHeader(c, "R36 · MIDI-TEST");
    section(c, 18, 52, 336, 300, C_GO, "GERÄTE");
    if (midi.devs.empty()) {
        tx(c, 186, 150, "KEIN MIDI-GERÄT GEFUNDEN", S, C_DIM, 1, 1);
        tx(c, 186, 176, "CONTROLLER AN DEN OTG-ANSCHLUSS", XS, C_DIM, 1, 1);
        tx(c, 186, 194, "(NICHT DC) · SUCHE ALLE 2 SEKUNDEN", XS, C_DIM, 1, 1);
    }
    for (int i = 0; i < int(midi.devs.size()) && i < 6; i++) {
        float y = 96 + i * 40;
        circle(c, 44, y, 6, C_GO);
        tx(c, 58, y - 6, midi.devs[i].name, S, C_TXT, 1);
        tx(c, 58, y + 10, midi.devs[i].path, XS, C_DIM, 0);
    }
    section(c, 366, 52, 336, 300, C_GO, "LETZTE NOTEN");
    for (int i = 0; i < int(midi.log.size()) && i < 10; i++)
        tx(c, 382, 90 + i * 25, midi.log[i], XS, i == 0 ? C_WHITE : C_TXT, 1);
    tx(c, 688, 336, fmt("%d EREIGNISSE", midi.eventCount), XS, C_DIM, 1, 2);
    section(c, 18, 364, 684, 250, C_GO, "ZUORDNUNG");
    for (int t = 0; t < NT; t++) {
        float x = 34 + (t % 4) * 168, y = 410 + (t / 4) * 56;
        c.roundRect(x, y - 18, 156, 40, 4, C_DISP);
        c.roundRect(x, y - 18, 4, 40, 2, kTrackCol[t]);
        tx(c, x + 14, y - 5, noteName(36 + t), S, C_WHITE, 0);
        tx(c, x + 14, y + 11, kTrackNames[t], XS, kTrackCol[t], 1);
    }
    tx(c, 34, 540, "AB C2: GEWÄHLTE INSTRUMENT-SPUR CHROMATISCH (AUF SEQ MIT KREUZ WÄHLEN)", XS, C_TXT, 1);
    tx(c, 34, 562, "AUFNAHME: AUF SEQ SEL + B · NOTEN AUF DEN NÄCHSTEN STEP QUANTISIERT", XS, C_TXT, 1);
    tx(c, 34, 586, rec ? "AUFNAHME IST AN" : "AUFNAHME IST AUS", S, rec ? C_RED : C_DIM, 1);
    drawFooter(c, "B: ZURÜCK · PADS DRÜCKEN ZUM TESTEN", "");
}

// ================================================================ Gesamtbild
void App::draw(Canvas& c) {
    c.clear(C_BG);
    if (midiTest) drawMidiTest(c);
    else if (menuOpen) drawMenu(c);
    else switch (page) {
        case PG_SEQ: drawSeq(c); break;
        case PG_SONG: drawSong(c); break;
        case PG_MIX: drawMix(c); break;
        case PG_FX: drawFx(c); break;
        case PG_SYN: drawSyn(c); break;
    }
    drawKrabbo(c);
    if (now < toastUntil && !toast.empty()) {
        int w = Canvas::textW(toast, M, 1) + 40;
        float x = 360 - w / 2.f, y = 668;
        c.roundRect(x, y, w, 36, 5, rgb(0x2c2f38));
        c.roundStroke(x, y, w, 36, 5, kPageCol[page] == C_TXT ? C_GO : kPageCol[page], 1.5f);
        tx(c, 360, y + 18, toast, M, C_WHITE, 1, 1);
    }
}
