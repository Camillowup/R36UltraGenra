#include "app.h"
#include "generator.h"
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <cstdarg>

using namespace gfx;

// Farben (gedaempfter Biomechanik-/Alien-Look)
static const Col C_TXT = rgb(0xcfc8b4), C_TXTH = rgb(0xd8d2bf), C_DIM = rgb(0x8f8a78), C_GRN = rgb(0x7fae5a),
                 C_GRNL = rgb(0xa9cf86), C_GRNLL = rgb(0xc6e2a6), C_G2 = rgb(0x8fbf62), C_G3 = rgb(0x4a6e34),
                 C_G4 = rgb(0x24361b), C_LINE = rgb(0x3b4a3a), C_LINE2 = rgb(0x303b31), C_BRASS = rgb(0xb08d57),
                 C_PANEL = rgb(0x0f1511), C_DARK = rgb(0x0a0e0b), C_TEAL = rgb(0x7fa3a0), C_INK = rgb(0x0b0d0b),
                 C_BONE = rgb(0x5e6b56);

static const char* kPageNames[PG_COUNT] = {"SEQ", "SONG", "MIX", "FX", "SYN"};
static const float R_ALIEN[4] = {12, 2, 12, 2};

static std::string fmt(const char* f, ...) {
    char b[160];
    va_list a;
    va_start(a, f);
    vsnprintf(b, sizeof b, f, a);
    va_end(a);
    return b;
}

static std::string noteName(int n) { return fmt("%s%d", kNoteNames[((n % 12) + 12) % 12], n / 12 - 1); }

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
    msg(samplesLoaded ? fmt("%d SAMPLES GELADEN", samplesLoaded) : std::string("EINGEBAUTE KLÄNGE AKTIV"));
    for (int t = NDRUM; t < NT; t++) {
        lastNote[t] = uint8_t(prj.root + (t == T_BASS ? 36 : t == T_LEAD ? 60 : 48));
    }
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

bool App::load() {
    FILE* f = fopen((dataDir + "/projekt.r36").c_str(), "rb");
    if (!f) return false;
    Project tmp;
    bool ok = fread(&tmp, sizeof tmp, 1, f) == 1 && !memcmp(tmp.magic, "R36GEN01", 8);
    fclose(f);
    if (ok) {
        prj = tmp;
        prj.curPat %= NPAT;
        for (auto& p : prj.pat) p.len = uint8_t(std::clamp<int>(p.len, 16, MAXSTEPS) / 16 * 16);
    }
    return ok;
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
    // Start + Select: Programm beenden (wie in der Firmware ueblich)
    if (in.held[B_START] && in.held[B_SELECT]) { quit = true; prev = in; return; }

    // Select doppelt antippen: Hauptmenue
    if (pressed[B_SELECT]) selUsed = false;
    if (in.held[B_SELECT]) {
        for (int b = 0; b < B_COUNT; b++) if (b != B_SELECT && pressed[b]) selUsed = true;
    }
    if (released[B_SELECT] && !selUsed && now - heldSince[B_SELECT] < 0.35) {
        if (now - lastSelTap < 0.4) { menuOpen = !menuOpen; menuSel = 0; confirmNew = false; lastSelTap = -10; }
        else lastSelTap = now;
    }

    if (menuOpen) handleMenu(in);
    else {
        // Seite wechseln: Select + Kreuz links/rechts
        if (in.held[B_SELECT] && (pressed[B_LEFT] || pressed[B_RIGHT])) {
            page = (page + (pressed[B_RIGHT] ? 1 : PG_COUNT - 1)) % PG_COUNT;
        } else if (pressed[B_START] && !in.held[B_SELECT]) {
            if (eng.playing) eng.stop(); else eng.start();
        } else {
            switch (page) {
                case PG_SEQ: handleSeq(in, dt); break;
                case PG_SONG: handleSong(in); break;
                case PG_MIX: handleMix(in, dt); break;
                case PG_FX: handleFx(in); break;
                case PG_SYN: handleSyn(in); break;
            }
        }
    }
    krabboTick(dt);
    if (dirty && now - lastSave > 10) save();
    prev = in;
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
    msg(fmt("NEW · %s", kTrackNames[curT]));
}

void App::changeLock(int delta) {
    Step& st = prj.pat[prj.curPat].s[curT][curS];
    const Track& tr = prj.tr[curT];
    if (!st.on) { msg("ERST STEP SETZEN (A)"); return; }
    bool inst = curT >= NDRUM;
    if (lockSel == P_PITCH && inst) {
        st.note = uint8_t(std::clamp(st.note + delta, 12, 108));
        lastNote[curT] = st.note;
    } else {
        int v = stepParam(st, tr, lockSel);
        int lo = lockSel == P_PITCH ? -24 : 0, hi = lockSel == P_PITCH ? 24 : 100;
        v = std::clamp(v + delta, lo, hi);
        st.lockMask |= 1 << lockSel;
        switch (lockSel) {
            case P_PITCH: st.pitch = int8_t(v); break;
            case P_DECAY: st.decay = uint8_t(v); break;
            case P_CUTOFF: st.cutoff = uint8_t(v); break;
            case P_REVERB: st.rev = uint8_t(v); break;
            default: st.dly = uint8_t(v); break;
        }
    }
    dirty = true;
}

void App::handleSeq(const Input& in, double dt) {
    Pattern& pat = prj.pat[prj.curPat];
    int len = pat.len;
    bool sel = in.held[B_SELECT], bh = in.held[B_B];
    if (pressed[B_B]) bUsed = false;

    if (sel) {
        if (pressed[B_UP] || pressed[B_DOWN]) {
            prj.genre = uint8_t((prj.genre + (pressed[B_UP] ? NGENRE - 1 : 1)) % NGENRE);
            msg(fmt("GENRE: %s", kGenreNames[prj.genre]));
            dirty = true;
        }
        if (pressed[B_Y]) newAll();
        if (pressed[B_X]) doUndo();
        return;
    }
    if (bh) {
        // P-Lock: B halten + Kreuz/Stick
        if (rep[B_LEFT]) { lockSel = (lockSel + NPARAM - 1) % NPARAM; bUsed = true; }
        if (rep[B_RIGHT]) { lockSel = (lockSel + 1) % NPARAM; bUsed = true; }
        if (rep[B_UP]) { changeLock(+1); bUsed = true; }
        if (rep[B_DOWN]) { changeLock(-1); bUsed = true; }
        if (std::fabs(in.ly) > 0.25f) {
            float speed = (lockSel == P_PITCH ? 14.f : 60.f) * (std::fabs(in.ly) - 0.2f) / 0.8f;
            stickAcc += -in.ly / std::fabs(in.ly) * speed * dt;
            int d = int(stickAcc);
            if (d) { stickAcc -= d; changeLock(d); }
            bUsed = true;
        } else stickAcc = 0;
        if (pressed[B_A]) {
            Step& st = pat.s[curT][curS];
            st.lockMask = 0;
            bUsed = true;
            msg("LOCKS GELÖSCHT");
            dirty = true;
        }
        return;
    }
    if (released[B_B] && !bUsed && pat.s[curT][curS].on) eng.preview(curT, pat.s[curT][curS]);

    if (rep[B_UP]) curT = (curT + NT - 1) % NT;
    if (rep[B_DOWN]) curT = (curT + 1) % NT;
    if (rep[B_LEFT]) curS = (curS + len - 1) % len;
    if (rep[B_RIGHT]) curS = (curS + 1) % len;
    curS = std::min(curS, len - 1);

    Step& st = pat.s[curT][curS];
    if (pressed[B_A]) {
        st.on = !st.on;
        if (st.on) {
            if (curT >= NDRUM) st.note = lastNote[curT];
            if (st.vel == 0) st.vel = 100;
            if (!eng.playing) eng.preview(curT, st);
        }
        dirty = true;
    }
    if ((rep[B_L2] || rep[B_R2]) && st.on) {
        int d = rep[B_R2] ? 1 : -1;
        if (curT < NDRUM) {
            int n = int(eng.drums[curT].size());
            int v = st.sample + d;  // -1 = Spur-Sample
            if (v < -1) v = n - 1;
            if (v >= n) v = -1;
            st.sample = int8_t(v);
        } else if (curT == T_KEYS && in.held[B_L1]) {
            st.chord = !st.chord;
        } else {
            st.note = uint8_t(std::clamp(st.note + d, 12, 108));
            lastNote[curT] = st.note;
        }
        eng.preview(curT, st);
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
    if (rep[B_LEFT]) songSel = (songSel + NPAT - 1) % NPAT;
    if (rep[B_RIGHT]) songSel = (songSel + 1) % NPAT;
    if (rep[B_UP]) songSel = (songSel + NPAT - 4) % NPAT;
    if (rep[B_DOWN]) songSel = (songSel + 4) % NPAT;
    if (pressed[B_A]) {
        if (eng.playing) { eng.queuedPat = songSel; msg(fmt("NÄCHSTES PATTERN: %02d", songSel + 1)); }
        else prj.curPat = uint8_t(songSel);
        hasUndo = false;
    }
    if (pressed[B_X]) { clipPat = songSel; msg(fmt("PATTERN %02d KOPIERT", songSel + 1)); }
    if (pressed[B_Y]) {
        if (clipPat < 0) msg("ERST KOPIEREN (X)");
        else if (clipPat != songSel) {
            prj.pat[songSel] = prj.pat[clipPat];
            dirty = true;
            msg(fmt("EINGEFÜGT IN %02d", songSel + 1));
        }
    }
    (void)in;
}

void App::handleMix(const Input& in, double dt) {
    static const int keys[NT] = {B_L1, B_L2, B_R1, B_R2, B_A, B_B, B_X, B_Y};
    for (int t = 0; t < NT; t++) {
        if (!pressed[keys[t]]) continue;
        if (in.held[B_SELECT]) prj.tr[t].solo = !prj.tr[t].solo;
        else prj.tr[t].mute = !prj.tr[t].mute;
        dirty = true;
    }
    if (in.held[B_SELECT]) return;
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

static const int kFxRows = 6;
void App::handleFx(const Input& in) {
    if (rep[B_UP]) fxSel = (fxSel + kFxRows - 1) % kFxRows;
    if (rep[B_DOWN]) fxSel = (fxSel + 1) % kFxRows;
    int d = rep[B_RIGHT] ? 1 : rep[B_LEFT] ? -1 : 0;
    if (!d) return;
    if (in.held[B_B]) d *= 10;
    uint8_t* v[kFxRows] = {&prj.dlyTime, &prj.dlyFb, &prj.dlyLvl, &prj.revSize, &prj.revDamp, &prj.revLvl};
    if (fxSel == 0) prj.dlyTime = uint8_t((prj.dlyTime + (d > 0 ? 1 : 3)) % 4);
    else *v[fxSel] = uint8_t(std::clamp(*v[fxSel] + d, 0, 100));
    dirty = true;
}

static const int kSynRows = 7;
void App::handleSyn(const Input& in) {
    if (pressed[B_L1]) synT = (synT + NT - 1) % NT;
    if (pressed[B_R1]) synT = (synT + 1) % NT;
    if (rep[B_UP]) synSel = (synSel + kSynRows - 1) % kSynRows;
    if (rep[B_DOWN]) synSel = (synSel + 1) % kSynRows;
    Track& t = prj.tr[synT];
    int d = rep[B_RIGHT] ? 1 : rep[B_LEFT] ? -1 : 0;
    if (pressed[B_A]) {
        Step st;
        st.on = 1;
        st.note = lastNote[synT];
        eng.preview(synT, st);
    }
    if (!d) return;
    if (in.held[B_B] && synSel > 1) d *= 10;
    switch (synSel) {
        case 0: {
            int n = synT < NDRUM ? int(eng.drums[synT].size()) : int(eng.keys.size());
            if (n > 0) t.sample = int8_t((t.sample + d + n) % n);
            break;
        }
        case 1: t.randPct = uint8_t(std::clamp(t.randPct + d * 5, 0, 100)); break;
        case 2:
            if (synT < NDRUM) t.pitch = int8_t(std::clamp(t.pitch + d, -24, 24));
            else lastNote[synT] = uint8_t(std::clamp(lastNote[synT] + d, 12, 108));
            break;
        case 3: t.decay = uint8_t(std::clamp(t.decay + d, 0, 100)); break;
        case 4: t.cutoff = uint8_t(std::clamp(t.cutoff + d, 0, 100)); break;
        case 5: t.rev = uint8_t(std::clamp(t.rev + d, 0, 100)); break;
        case 6: t.dly = uint8_t(std::clamp(t.dly + d, 0, 100)); break;
    }
    if (!eng.playing && (synSel == 0 || synSel == 2)) {
        Step st;
        st.on = 1;
        st.note = lastNote[synT];
        eng.preview(synT, st);
    }
    dirty = true;
}

enum MenuItem { MI_TEMPO, MI_SWING, MI_LEN, MI_ROOT, MI_SCALE, MI_KRABBO, MI_LIMIT, MI_MASTER, MI_RELOAD, MI_SAVE, MI_NEW, MI_COUNT };
static const char* kMenuLabels[MI_COUNT] = {"TEMPO", "SWING", "PATTERN-LÄNGE", "GRUNDTON", "TONLEITER", "KRABBO",
                                           "LIMITER MAIN", "MAIN-PEGEL", "SAMPLES NEU LADEN", "JETZT SPEICHERN", "NEUES PROJEKT"};

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
                // neue Takte mit dem ersten Takt fuellen
                for (int s = pat.len; s < nl; s++)
                    for (int t = 0; t < NT; t++) pat.s[t][s] = pat.s[t][s % 16];
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
        if (menuSel == MI_RELOAD) {
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
void App::krabboTick(double dt) {
    (void)dt;
    if (prj.krabbo == 0 || menuOpen) { if (krMode >= 0) krMode = -1; return; }
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
    // Beine
    for (int side = -1; side <= 1; side += 2)
        for (int k = 0; k < 3; k++) {
            float a = std::sin(t * 14.f + k * 2.1f + (side > 0 ? 1.f : 0.f)) * 3.f * s;
            float bx = x + side * (10 + k * 6) * s, by = y + 6 * s;
            c.line(bx, by, bx + side * 9 * s, by + (10 + a) * s, rgb(0x4a6e34), 2.5f * s);
            c.line(bx + side * 9 * s, by + (10 + a) * s, bx + side * 13 * s, by + (17 + a) * s, rgb(0x3f5f2c), 2.f * s);
        }
    // Scheren
    for (int side = -1; side <= 1; side += 2) {
        float cx = x + side * 30 * s, cy = y - 4 * s + std::sin(t * 6.f) * 2 * s;
        c.line(x + side * 18 * s, y, cx, cy, rgb(0x5a7e42), 3.f * s);
        c.ellipse(cx + side * 3 * s, cy - 3 * s, 7 * s, 5 * s, rgb(0xa9cf86), rgb(0x3f5f2c), 0.35f, 0.3f);
        c.ellipse(cx + side * 8 * s, cy - 7 * s, 2.5f * s, 2.5f * s, rgb(0x070908), rgb(0x070908));
    }
    // Panzer
    c.ellipse(x, y, 24 * s, 14 * s, rgb(0xa9cf86), rgb(0x2f4a22), 0.4f, 0.25f);
    for (int k = -1; k <= 1; k++) c.ellipse(x + k * 9 * s, y - 3 * s, 3 * s, 2 * s, rgb(0x2f4a22, 160), rgb(0x2f4a22, 160));
    c.ellipse(x, y + 7 * s, 13 * s, 3 * s, rgb(0xb08d57, 140), rgb(0xb08d57, 60));
    // Stielaugen
    for (int side = -1; side <= 1; side += 2) {
        float ex = x + side * 7 * s + std::sin(t * 3.f + side) * 1.5f * s, ey = y - 24 * s;
        c.line(x + side * 5 * s, y - 10 * s, ex, ey + 4 * s, rgb(0x5a7e42), 2.5f * s);
        c.ellipse(ex, ey, 5.5f * s, 5.5f * s, rgb(0xe8e4d4), rgb(0xc9cdb4));
        c.ellipse(ex + look * 2 * s, ey + 0.5f * s, 2.4f * s, 2.4f * s, rgb(0x050605), rgb(0x050605));
    }
}

void App::drawKrabbo(Canvas& c) {
    if (krMode < 0 || menuOpen) return;
    float t = float(now - krStart), dur = 4.5f;
    float x = 0, y = 0, look = std::sin(t * 2.f);
    int cx0 = c.clipX0, cx1 = c.clipX1;
    if (krMode == 0) {  // lugt von rechts herein
        float in = std::min(1.f, t / 0.6f), out = std::min(1.f, std::max(0.f, (dur - t) / 0.6f));
        x = 700 + 20 - 50 * std::min(in, out);
        y = krY;
        look = -1;
        c.clipX1 = 700;
    } else if (krMode == 1) {  // krabbelt unten durch
        x = -40 + (760.f) * (t / dur);
        y = 668;
    } else {  // turnt an der Bass-Spur
        x = 60 + 8 * std::sin(t * 2.5f);
        float in = std::min(1.f, t / 0.5f), out = std::min(1.f, std::max(0.f, (dur - t) / 0.5f));
        y = 128 + 5 * 44 + 12 - 26 + 40 * (1 - std::min(in, out)) - 6 * std::fabs(std::sin(t * 5.f));
        c.clipX0 = 20;
    }
    // Nie ueber dem Cursor: auf der SEQ-Seite ausblenden, wenn er in der Naehe ist
    drawKrabboAt(c, x, y, 1.f, t, look);
    c.clipX0 = cx0;
    c.clipX1 = cx1;
}

// ================================================================ Zeichnen: Rahmen
void App::buildBg() {
    // Radialer Hintergrund
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            float dx = (x - 360) / 360.f, dy = (y - 324) / 340.f;
            float d = std::sqrt(dx * dx + dy * dy);
            Col cc = d < 0.55f ? mix(rgb(0x16201a), rgb(0x0d110e), d / 0.55f) : mix(rgb(0x0d110e), rgb(0x070908), (d - 0.55f) / 0.6f);
            bg.px[y * W + x] = cc;
        }
    // Knochensaeulen links/rechts
    for (int y = 0; y < H; y++) {
        int m = y % 22;
        Col cc = m < 7 ? mix(rgb(0x2f3a30), rgb(0x7d8a72), m / 7.f) : m < 15 ? mix(rgb(0x7d8a72), rgb(0x2f3a30), (m - 7) / 8.f) : m < 17 ? rgb(0x2f3a30) : rgb(0x070908);
        for (int x = 0; x < 20; x++) {
            float shade = 0.75f + 0.25f * std::sin(x / 20.f * 3.14159f);
            Col s = mix(rgb(0x070908), cc, shade);
            bg.px[y * W + x] = s;
            bg.px[y * W + (W - 1 - x)] = s;
        }
        for (int x = 0; x < 14; x++) {
            int a = int(40 * (1 - x / 14.f));
            bg.blend(20 + x, y, rgb(0x7fae5a, a));
            bg.blend(W - 21 - x, y, rgb(0x7fae5a, a));
        }
    }
    // Rippen unten
    for (int y = H - 26; y < H; y++)
        for (int x = 20; x < W - 20; x++) {
            int m = (x - 20) % 28;
            Col cc = m < 10 ? mix(rgb(0x26302a), rgb(0x5e6b56), m / 10.f) : m < 22 ? mix(rgb(0x5e6b56), rgb(0x26302a), (m - 10) / 12.f) : m < 24 ? rgb(0x0d110e) : rgb(0x0d110e);
            bg.px[y * W + x] = (y == H - 26) ? rgb(0x3b4a3a) : cc;
        }
    bgReady = true;
}

void App::drawEye(Canvas& c, float x, float y) {
    c.glow(x - 30, y - 20, 60, 40, 20, rgb(0x7fae5a, 50), 6);
    c.ellipse(x, y, 33, 23, rgb(0x1f2a21), rgb(0x1f2a21));
    c.ellipse(x, y, 30, 20, rgb(0x5e6b56), rgb(0x5e6b56));
    c.ellipse(x, y, 26, 16, rgb(0x050605), rgb(0x050605));
    // Lid blinzelt ab und zu
    float blink = std::fmod(float(now), 6.3f) > 6.15f ? 0.15f : 1.f;
    c.ellipse(x, y, 21, 11 * blink, rgb(0xd6d9c2), rgb(0xa9ad94));
    // Pupille folgt dem Abspielkopf
    float px = 0;
    if (eng.playing && eng.playStep >= 0) {
        int len = prj.pat[prj.curPat].len;
        px = ((eng.playStep % 16) / 15.f - 0.5f) * 20.f;
        (void)len;
    }
    if (blink > 0.5f) {
        c.ellipse(x + px, y, 7.5f, 7.5f, rgb(0x8fbf62), rgb(0x2f4a22));
        c.ellipse(x + px, y, 3.f, 3.f, rgb(0x050605), rgb(0x050605));
    }
}

void App::drawHeader(Canvas& c, const char* title, const std::string& sub, bool tabs) {
    c.text(38, 14, title, L, C_TXTH, 5);
    c.text(38, 46, sub, XS, C_DIM, 3);
    if (tabs) {
        int x = 300;
        for (int p = 0; p < PG_COUNT; p++) {
            int w = Canvas::textW(kPageNames[p], S, 2) + 20;
            if (p == page) c.roundRect(x, 24, w, 28, R_ALIEN, C_GRN, C_GRN);
            else c.roundStroke(x, 24, w, 28, R_ALIEN, C_LINE);
            c.text(x + 10, 30, kPageNames[p], S, p == page ? C_INK : C_TXT, 2);
            x += w + 4;
        }
    }
    drawEye(c, 640, 38);
}

void App::drawFooter(Canvas& c, const std::string& l, const std::string& r) {
    c.text(38, 664, l, XS, C_DIM, 1);
    c.text(682 - Canvas::textW(r, XS, 1), 664, r, XS, C_DIM, 1);
}

static void meterBar(Canvas& c, float x, float y, float w, float h, float pct, Col a, Col b) {
    c.roundRect(x, y, w, h, h / 2, rgb(0x161e18));
    c.roundStroke(x, y, w, h, h / 2, rgb(0x2a352b));
    float fw = std::max(0.f, std::min(1.f, pct)) * (w - 2);
    if (fw > 1) {
        float rr[4] = {h / 2 - 1, h / 2 - 1, h / 2 - 1, h / 2 - 1};
        // horizontaler Verlauf in Streifen
        for (int i = 0; i < int(fw); i += 4) {
            float ww = std::min(4.f, fw - i);
            Col cc = mix(a, b, i / std::max(1.f, w));
            float r0[4] = {i == 0 ? rr[0] : 0, 0, 0, i == 0 ? rr[3] : 0};
            if (i + 4 >= int(fw)) { r0[1] = rr[1]; r0[2] = rr[2]; }
            c.roundRect(x + 1 + i, y + 1, ww, h - 2, r0, cc, cc);
        }
    }
}

// ================================================================ SEQ
void App::drawSeq(Canvas& c) {
    Pattern& pat = prj.pat[prj.curPat];
    int len = pat.len;
    int view = (curS / 16) * 16;
    std::string sub = fmt("PAT %02d / %d BPM / SWING %d", prj.curPat + 1, prj.bpm, prj.swing);
    if (len > 16) sub += fmt(" / TAKT %d-%d", view / 16 + 1, len / 16);
    if (eng.queuedPat >= 0) sub += fmt(" › %02d", eng.queuedPat + 1);
    drawHeader(c, "XENO·SEQ", sub, true);

    // Generator-Leiste
    float gr[4] = {22, 4, 22, 4};
    c.roundRect(38, 74, 644, 44, gr, rgb(0x141c16), rgb(0x0e1410));
    c.roundStroke(38, 74, 644, 44, gr, C_LINE);
    c.text(52, 89, "GENRE", XS, C_DIM, 3);
    std::string gname = kGenreNames[prj.genre];
    int gw = Canvas::textW(gname, M, 3) + 56;
    c.roundRect(106, 82, gw, 28, 14, C_DARK);
    c.roundStroke(106, 82, gw, 28, 14, C_BONE);
    c.text(118, 87, "‹", M, C_DIM);
    c.text(134, 87, gname, M, C_GRNL, 3);
    c.text(106 + gw - 20, 87, "›", M, C_DIM);
    c.text(106 + gw + 8, 90, "SEL+↑↓", XS, C_DIM, 1);
    struct Btn_ { const char* n; const char* k; int style; };
    Btn_ bs[3] = {{"NEW ALL", "SEL + Y", 0}, {"NEW", "Y", 1}, {"UNDO", "SEL + X", 2}};
    int bx = 668;
    for (int i = 2; i >= 0; i--) {
        int w = Canvas::textW(bs[i].n, S, 2) + 22;
        bx -= w;
        if (bs[i].style == 0) {
            c.glow(bx, 79, w, 22, 11, rgb(0x7fae5a, 60), 5);
            c.roundRect(bx, 79, w, 22, 11, C_GRNL, rgb(0x3f5f2c));
            c.text(bx + 11, 82, bs[i].n, S, C_INK, 2);
        } else {
            c.roundStroke(bx, 79, w, 22, 11, bs[i].style == 1 ? C_GRN : C_BONE);
            c.text(bx + 11, 82, bs[i].n, S, bs[i].style == 1 ? C_GRNL : C_TXT, 2);
        }
        int kw = Canvas::textW(bs[i].k, XS, 1);
        c.text(bx + w / 2 - kw / 2, 102, bs[i].k, XS, C_DIM, 1);
        bx -= 10;
    }

    // Spuren
    const float gx0 = 110, gx1 = 682, gap = 5, grp = 6;
    float sw = (gx1 - gx0 - 15 * gap - 3 * grp) / 16.f;
    bool anySolo = false;
    for (auto& t : prj.tr) anySolo |= t.solo != 0;
    for (int t = 0; t < NT; t++) {
        float y = 128 + t * 44 + (t >= NDRUM ? 12 : 0);
        if (t == NDRUM) {
            for (float x = 38; x < 682; x += 8) c.fillRect(int(x), int(y - 9), 4, 1, C_LINE);
        }
        bool aud = anySolo ? prj.tr[t].solo : !prj.tr[t].mute;
        Col lc = t == curT ? C_GRNL : (t < NDRUM ? C_TXT : rgb(0xb9b29c));
        if (!aud) lc = rgb(0x5e5a4e);
        c.text(38, int(y) + 9, kTrackNames[t], M, lc, 3);
        if (prj.tr[t].mute) c.text(38, int(y) + 27, "STUMM", XS, C_BRASS, 1);
        else if (prj.tr[t].solo) c.text(38, int(y) + 27, "SOLO", XS, C_GRNL, 1);
        for (int i = 0; i < 16; i++) {
            int s = view + i;
            float x = gx0 + i * (sw + gap) + (i / 4) * grp;
            const Step& st = pat.s[t][s];
            bool inst = t >= NDRUM;
            float rr[4];
            if (inst) { rr[0] = 6; rr[1] = 14; rr[2] = 6; rr[3] = 14; }
            else { rr[0] = rr[1] = rr[2] = rr[3] = 14; }
            bool play = eng.playing && eng.playStep == s;
            if (st.on) {
                if (play) c.glow(x, y + 3, sw, 32, 14, rgb(0x8fbf62, 110), 6);
                else c.glow(x, y + 3, sw, 32, 14, rgb(0x7fae5a, 45), 3);
                c.roundRect(x, y + 3, sw, 32, rr, aud ? C_G2 : rgb(0x4b5a44), aud ? C_G4 : rgb(0x1c241b));
                c.ellipse(x + sw / 2, y + 13, sw * 0.36f, 7, rgb(0xc6e2a6, aud ? 200 : 60), rgb(0x8fbf62, 0));
                if (inst) {
                    std::string nn = noteName(st.note);
                    if (t == T_PAD || (t == T_KEYS && st.chord)) nn = kNoteNames[st.note % 12];
                    int tw = Canvas::textW(nn, XS);
                    c.text(int(x + sw / 2 - tw / 2), int(y) + 19, nn, XS, rgb(0x0b0d0b, 220));
                } else if (st.sample >= 0) {
                    c.ellipse(x + sw / 2, y + 27, 2.5f, 2.5f, rgb(0x0b0d0b, 200), rgb(0x0b0d0b, 200));
                }
                if (st.lockMask) c.ellipse(x + sw - 5, y + 7, 3, 3, C_BRASS, rgb(0x6d5530));
            } else if (play) {
                c.roundRect(x, y + 3, sw, 32, rr, rgb(0x1c2a1e), rgb(0x1c2a1e));
                c.roundStroke(x, y + 3, sw, 32, rr, rgb(0x5e7a4e));
            } else {
                c.roundRect(x, y + 3, sw, 32, rr, rgb(0x1f2721), rgb(0x0f1411));
                c.roundStroke(x, y + 3, sw, 32, rr, C_LINE2);
            }
            if (t == curT && s == curS) {
                float r2[4] = {rr[0] + 3, rr[1] + 3, rr[2] + 3, rr[3] + 3};
                c.roundStroke(x - 4, y - 1, sw + 8, 40, r2, C_TXTH, 2.f);
            }
        }
    }

    // P-Lock-Feld
    const Step& st = pat.s[curT][curS];
    const Track& tr = prj.tr[curT];
    bool inst = curT >= NDRUM;
    float pr[4] = {22, 4, 22, 4};
    c.roundRect(38, 506, 644, 92, pr, C_PANEL, C_PANEL);
    c.roundStroke(38, 506, 644, 92, pr, prev.held[B_B] ? C_GRN : C_LINE);
    c.text(54, 516, fmt("P-LOCK // %s // %d", kTrackNames[curT], curS + 1), S, C_GRNL, 2);
    std::string right;
    if (!inst) {
        int idx = st.sample >= 0 ? st.sample : tr.sample;
        const SampleBuf* b = eng.drumSample(curT, idx);
        right = (st.sample < 0 ? std::string("SPUR: ") : std::string("")) + (b ? b->name : "-");
        if (tr.randPct) right += fmt("  ZUFALL %d%%", tr.randPct);
    } else {
        right = (curT == T_PAD || (curT == T_KEYS && st.chord)) ? "AKKORD" : "EINZELNOTE";
        if (curT == T_KEYS) right += eng.keys.empty() ? " · E-PIANO" : " · KEYGROUP";
    }
    if (!st.on) right = "STEP AUS";
    c.text(666 - Canvas::textW(right, S, 1), 516, right, S, C_DIM, 1);
    for (int p = 0; p < NPARAM; p++) {
        float x = 54 + p * 125;
        const char* label = inst ? kParamNamesInst[p] : kParamNamesDrum[p];
        int v = stepParam(st, tr, p);
        std::string val;
        float pct;
        if (p == P_PITCH && inst) { val = noteName(st.note); pct = (st.note - 24) / 72.f; }
        else if (p == P_PITCH) { val = v > 0 ? fmt("+%d", v) : fmt("%d", v); pct = (v + 24) / 48.f; }
        else if (p == P_DECAY && inst) { val = fmt("%d ST", 1 + (v * 15 + 50) / 100); pct = v / 100.f; }
        else { val = fmt("%d", v); pct = v / 100.f; }
        bool locked = st.lockMask & (1 << p);
        bool selp = p == lockSel;
        Col lc2 = selp ? C_GRNL : C_TXT;
        c.text(int(x), 544, label, XS, lc2, 1);
        int vw = Canvas::textW(val, S);
        c.text(int(x + 108 - vw), 542, val, S, locked ? C_BRASS : C_TXTH);
        meterBar(c, x, 564, 108, 10, pct, rgb(0x3f5f2c), selp ? C_GRNL : C_G2);
        if (selp) c.fillRect(int(x), 580, 108, 2, C_GRNL);
    }
    c.text(54, 584, "MESSING = GELOCKT · B+A: LOCKS LÖSCHEN", XS, rgb(0x6d6a5c), 1);

    drawFooter(c, "KREUZ: STEP · A: SETZEN · L2/R2: " + std::string(inst ? "NOTE" : "SAMPLE") + " · L1/R1: PATTERN · X: STUMM",
               "B HALTEN + ‹› / ↑↓ / STICK: P-LOCK");
    c.text(38, 680 - 60, "START: PLAY/STOP · SEL+‹›: SEITE · SEL 2×: MENÜ", XS, rgb(0x6d6a5c), 1);
}

// ================================================================ SONG (Pattern-Pool)
void App::drawSong(Canvas& c) {
    drawHeader(c, "XENO·SONG", fmt("PAT %02d / %d BPM", prj.curPat + 1, prj.bpm), true);
    float pr[4] = {4, 26, 4, 26};
    c.roundRect(38, 80, 644, 440, pr, C_PANEL, C_PANEL);
    c.roundStroke(38, 80, 644, 440, pr, C_LINE);
    c.text(56, 94, "PATTERN-POOL", XS, C_DIM, 3);
    for (int i = 0; i < NPAT; i++) {
        float x = 56 + (i % 4) * 154, y = 118 + (i / 4) * 98;
        bool used = false;
        int count = 0;
        for (int t = 0; t < NT; t++)
            for (int s = 0; s < prj.pat[i].len; s++) count += prj.pat[i].s[t][s].on;
        used = count > 0;
        bool cur = i == prj.curPat, q = i == eng.queuedPat, sel = i == songSel;
        float rr[4] = {18, 4, 18, 4};
        if (cur) c.glow(x, y, 140, 84, 18, rgb(0x7fae5a, 60), 5);
        c.roundRect(x, y, 140, 84, rr, cur ? rgb(0x2c4024) : (used ? rgb(0x172116) : C_DARK), cur ? rgb(0x172116) : C_DARK);
        c.roundStroke(x, y, 140, 84, rr, sel ? C_TXTH : (q ? C_BRASS : (cur ? C_GRNL : C_LINE)), sel ? 2.f : 1.f);
        c.text(int(x) + 14, int(y) + 10, fmt("%02d", i + 1), L, cur ? C_GRNL : (used ? C_TXT : rgb(0x4a5048)), 2);
        c.text(int(x) + 14, int(y) + 50, used ? fmt("%d STEPS · %d TAKT", count, prj.pat[i].len / 16) : std::string("LEER"), XS, C_DIM, 1);
        if (q) c.text(int(x) + 90, int(y) + 16, "NÄCHST", XS, C_BRASS, 1);
        if (clipPat == i) c.text(int(x) + 90, int(y) + 32, "KOPIE", XS, C_TEAL, 1);
        // Mini-Ansicht Kick-Spur
        for (int s = 0; s < 16; s++)
            if (prj.pat[i].s[0][s].on || prj.pat[i].s[1][s].on)
                c.fillRect(int(x) + 14 + s * 7, int(y) + 70, 5, 4, prj.pat[i].s[0][s].on ? C_G2 : C_TEAL);
    }
    float br[4] = {22, 4, 22, 4};
    c.roundRect(38, 532, 644, 64, br, C_PANEL, C_PANEL);
    c.roundStroke(38, 532, 644, 64, br, C_LINE);
    c.text(56, 544, "SONG-LISTE UND ZEITLEISTE", S, C_TXT, 2);
    c.text(56, 566, "KOMMEN IM NÄCHSTEN SCHRITT (V0.2)", XS, C_DIM, 2);
    drawFooter(c, "KREUZ: PATTERN · A: SPIELEN · X: KOPIEREN · Y: EINFÜGEN", "START: PLAY/STOP");
}

// ================================================================ MIX
void App::drawMix(Canvas& c) {
    int nm = 0, ns = 0;
    for (auto& t : prj.tr) { nm += t.mute; ns += t.solo; }
    drawHeader(c, "XENO·MIX", fmt("PAT %02d / %d BPM", prj.curPat + 1, prj.bpm), true);
    c.text(560, 62, fmt("%d MUTE · %d SOLO", nm, ns), XS, C_DIM, 1);
    static const char* keys[NT] = {"L1", "L2", "R1", "R2", "A", "B", "X", "Y"};
    float x = 38;
    const float sw = 64, h = 510, y = 80;
    for (int i = 0; i <= NT; i++) {
        bool master = i == NT;
        float w = master ? 74 : sw;
        if (i == NDRUM) x += 6;
        if (master) x += 4;
        bool sel = i == mixSel;
        bool m = !master && prj.tr[i].mute;
        float rr[4];
        if (master) { rr[0] = 22; rr[1] = 4; rr[2] = 22; rr[3] = 4; }
        else if (i >= NDRUM) { rr[0] = 6; rr[1] = 18; rr[2] = 6; rr[3] = 18; }
        else { rr[0] = 18; rr[1] = 6; rr[2] = 18; rr[3] = 6; }
        c.roundRect(x, y, w, h, rr, sel ? rgb(0x1a2619) : C_PANEL, sel ? rgb(0x1a2619) : C_PANEL);
        c.roundStroke(x, y, w, h, rr, sel ? C_GRNL : C_LINE);
        const char* name = master ? "MAIN" : kTrackNames[i];
        int nw = Canvas::textW(name, S, 2);
        c.text(int(x + w / 2 - nw / 2), int(y) + 12, name, S, sel ? C_GRNL : (m ? rgb(0x6d6a5c) : C_TXT), 2);
        // Mute-Taste
        if (!master) {
            if (m) c.roundRect(x + 6, y + 36, w - 12, 24, 10, C_BRASS);
            else if (prj.tr[i].solo) c.roundRect(x + 6, y + 36, w - 12, 24, 10, C_GRN);
            else c.roundStroke(x + 6, y + 36, w - 12, 24, 10, C_LINE);
            int kw = Canvas::textW(keys[i], S);
            c.text(int(x + w / 2 - kw / 2), int(y) + 40, keys[i], S, (m || prj.tr[i].solo) ? C_INK : C_DIM);
        }
        // Panorama-Knopf
        float kx = x + w / 2, ky = y + 92;
        c.ellipse(kx, ky, 17, 17, rgb(0x5e6b56), rgb(0x5e6b56));
        c.ellipse(kx, ky, 15, 15, rgb(0x3a3d35), rgb(0x1a1c17), 0.4f, 0.35f);
        float pan = master ? 0.5f : prj.tr[i].pan / 100.f;
        float ang = (pan - 0.5f) * 2.6f;
        c.line(kx, ky, kx + std::sin(ang) * 12, ky - std::cos(ang) * 12, C_TXTH, 2.f);
        std::string ps = master ? std::string("LIMIT") : (prj.tr[i].pan == 50 ? std::string("C") : prj.tr[i].pan < 50 ? fmt("L%d", 50 - prj.tr[i].pan) : fmt("R%d", prj.tr[i].pan - 50));
        int pw = Canvas::textW(ps, XS);
        c.text(int(kx - pw / 2), int(ky) + 20, ps, XS, master && prj.limiter ? C_GRNL : C_DIM);
        // Sends
        if (!master) {
            c.text(int(x) + 6, int(y) + 138, "REV", XS, C_DIM);
            meterBar(c, x + 30, y + 142, w - 36, 6, prj.tr[i].rev / 100.f, C_GRN, C_GRN);
            c.text(int(x) + 6, int(y) + 154, "DLY", XS, C_DIM);
            meterBar(c, x + 30, y + 158, w - 36, 6, prj.tr[i].dly / 100.f, C_TEAL, C_TEAL);
        }
        // Fader + Pegel
        float fy0 = y + 182, fy1 = y + h - 40;
        float fx = x + w / 2 - 12;
        c.roundRect(fx, fy0, 14, fy1 - fy0, 7, C_DARK);
        c.roundStroke(fx, fy0, 14, fy1 - fy0, 7, C_LINE2);
        int vol = master ? prj.master : prj.tr[i].vol;
        float cy = fy1 - (fy1 - fy0) * vol / 100.f;
        float cr[4] = {9, 3, 9, 3};
        c.roundRect(fx - 7, cy - 9, 28, 18, cr, C_TXTH, C_DIM);
        c.fillRect(int(fx - 3), int(cy), 20, 1, rgb(0x3a3d35));
        float mx = fx + 22;
        c.roundRect(mx, fy0, 6, fy1 - fy0, 3, C_DARK);
        float lv = master ? std::max(eng.meterL, eng.meterR) : std::min(1.f, eng.meter[i] * 2.5f);
        if (m) lv = 0;
        float mh = (fy1 - fy0) * std::min(1.f, lv);
        if (mh > 1) c.roundRect(mx, fy1 - mh, 6, mh, 3, lv > 0.9f ? C_BRASS : C_GRNLL, C_G3);
        std::string vs = fmt("%d", vol);
        int vw = Canvas::textW(vs, M);
        c.text(int(x + w / 2 - vw / 2), int(y + h) - 30, vs, M, C_TXTH);
        x += w + 6;
    }
    drawFooter(c, "L1 L2 R1 R2 A B X Y: MUTE · SEL + TASTE: SOLO · KREUZ ‹›: SPUR", "STICK L / ↑↓: LAUT · STICK R: PAN");
    c.text(38, 620, "START: PLAY/STOP · SEL+‹›: SEITE · SEL 2×: MENÜ", XS, rgb(0x6d6a5c), 1);
}

// ================================================================ FX
void App::drawFx(Canvas& c) {
    drawHeader(c, "XENO·FX", fmt("PAT %02d / %d BPM", prj.curPat + 1, prj.bpm), true);
    static const char* dt[4] = {"1/8", "3/16", "1/4", "1/16"};
    const char* labels[kFxRows] = {"ZEIT", "FEEDBACK", "PEGEL", "GRÖSSE", "DÄMPFUNG", "PEGEL"};
    int vals[kFxRows] = {0, prj.dlyFb, prj.dlyLvl, prj.revSize, prj.revDamp, prj.revLvl};
    for (int blk = 0; blk < 2; blk++) {
        float y = 84 + blk * 250;
        float rr[4] = {4, 26, 4, 26};
        c.roundRect(38, y, 644, 236, rr, C_PANEL, C_PANEL);
        c.roundStroke(38, y, 644, 236, rr, C_LINE);
        c.text(58, int(y) + 14, blk == 0 ? "SEND 1 // DELAY (PING-PONG)" : "SEND 2 // HALL", M, blk == 0 ? C_TEAL : C_GRNL, 3);
        c.text(520, int(y) + 18, "MODELL: KLASSIK", XS, C_DIM, 1);
        for (int r = 0; r < 3; r++) {
            int row = blk * 3 + r;
            float ry = y + 54 + r * 58;
            bool sel = row == fxSel;
            float r2[4] = {12, 3, 12, 3};
            c.roundRect(58, ry, 604, 46, r2, sel ? rgb(0x1c2a1e) : C_DARK, sel ? rgb(0x1c2a1e) : C_DARK);
            c.roundStroke(58, ry, 604, 46, r2, sel ? C_GRNL : rgb(0x232c24));
            c.text(74, int(ry) + 14, labels[row], S, sel ? C_TXTH : C_TXT, 2);
            std::string v = row == 0 ? std::string(dt[prj.dlyTime & 3]) : fmt("%d", vals[row]);
            if (row != 0) meterBar(c, 260, ry + 18, 300, 10, vals[row] / 100.f, rgb(0x3f5f2c), blk == 0 ? C_TEAL : C_G2);
            std::string vv = "‹ " + v + " ›";
            c.text(646 - Canvas::textW(vv, S, 1), int(ry) + 14, vv, S, sel ? C_GRNL : C_DIM, 1);
        }
    }
    drawFooter(c, "KREUZ ↑↓: EINTRAG · ‹›: WERT · B HALTEN: GROBE SCHRITTE", "SENDS PRO SPUR: SEITE SYN · PRO STEP: P-LOCK");
    c.text(38, 620, "START: PLAY/STOP · SEL+‹›: SEITE · SEL 2×: MENÜ", XS, rgb(0x6d6a5c), 1);
}

// ================================================================ SYN (Spur-Einstellungen)
void App::drawSyn(Canvas& c) {
    drawHeader(c, "XENO·SYN", fmt("SPUR %s", kTrackNames[synT]), true);
    // Reiter
    float x = 38;
    for (int t = 0; t < NT; t++) {
        int w = Canvas::textW(kTrackNames[t], S, 2) + 20;
        float rr[4] = {12, 2, 12, 2};
        if (t == synT) c.roundRect(x, 76, w, 28, rr, C_GRN, C_GRN);
        else c.roundStroke(x, 76, w, 28, rr, C_LINE);
        c.text(int(x) + 10, 82, kTrackNames[t], S, t == synT ? C_INK : (t < NDRUM ? C_TXT : rgb(0xb9b29c)), 2);
        x += w + 5 + (t == NDRUM - 1 ? 8 : 0);
    }
    Track& tr = prj.tr[synT];
    bool inst = synT >= NDRUM;
    std::string smp;
    if (!inst) { const SampleBuf* b = eng.drumSample(synT, tr.sample); smp = b ? b->name : "-"; }
    else if (synT == T_KEYS) smp = eng.keys.empty() ? std::string("E-PIANO (EINGEBAUT)") : eng.keys[std::clamp<int>(tr.sample, 0, int(eng.keys.size()) - 1)].name;
    else smp = synT == T_PAD ? "PAD (2× SÄGEZAHN)" : synT == T_BASS ? "BASS (SÄGE + SUB)" : "LEAD (PULS)";
    int nS = !inst ? int(eng.drums[synT].size()) : int(eng.keys.size());
    std::string rows[kSynRows][2] = {
        {inst ? "KLANG" : "SAMPLE", smp + (nS > 1 ? fmt("  (%d/%d)", tr.sample + 1, nS) : std::string(""))},
        {"ZUFALLS-SAMPLE", inst ? std::string("–") : fmt("%d %%", tr.randPct)},
        {inst ? "NOTE (A: ANHÖREN)" : "PITCH", inst ? noteName(lastNote[synT]) : fmt("%+d", tr.pitch)},
        {inst ? "LÄNGE" : "DECAY", inst ? fmt("%d STEPS", 1 + (tr.decay * 15 + 50) / 100) : fmt("%d", tr.decay)},
        {"CUTOFF", fmt("%d", tr.cutoff)},
        {"HALL-SEND", fmt("%d", tr.rev)},
        {"DELAY-SEND", fmt("%d", tr.dly)},
    };
    float pr[4] = {4, 26, 4, 26};
    c.roundRect(38, 116, 644, 420, pr, C_PANEL, C_PANEL);
    c.roundStroke(38, 116, 644, 420, pr, C_LINE);
    c.text(56, 128, "GRUNDWERTE DER SPUR (STEPS OHNE P-LOCK)", XS, C_DIM, 3);
    for (int r = 0; r < kSynRows; r++) {
        float ry = 150 + r * 52;
        bool sel = r == synSel;
        float r2[4] = {12, 3, 12, 3};
        c.roundRect(56, ry, 608, 42, r2, sel ? rgb(0x1c2a1e) : C_DARK, sel ? rgb(0x1c2a1e) : C_DARK);
        c.roundStroke(56, ry, 608, 42, r2, sel ? C_GRNL : rgb(0x232c24));
        c.text(72, int(ry) + 12, rows[r][0], S, sel ? C_TXTH : C_TXT, 2);
        std::string vv = "‹ " + rows[r][1] + " ›";
        c.text(648 - Canvas::textW(vv, S, 1), int(ry) + 12, vv, S, sel ? C_GRNL : C_DIM, 1);
    }
    float br[4] = {22, 4, 22, 4};
    c.roundRect(38, 548, 644, 56, br, C_PANEL, C_PANEL);
    c.roundStroke(38, 548, 644, 56, br, C_LINE);
    c.text(56, 558, inst ? "PLAITS-ENGINES, PRESETS UND KEYGROUP-SEITE FOLGEN" : "EIGENE SAMPLES: ORDNER samples/kick, clap, hat, perc", S, C_TXT, 1);
    c.text(56, 580, inst ? "JETZT: EINFACHE EINGEBAUTE SYNTHS ZUM TESTEN" : "WAV-DATEIEN, DANN MENÜ › SAMPLES NEU LADEN", XS, C_DIM, 1);
    drawFooter(c, "L1/R1: SPUR · KREUZ ↑↓: EINTRAG · ‹›: WERT · A: ANHÖREN", "B HALTEN: GROBE SCHRITTE");
    c.text(38, 620, "START: PLAY/STOP · SEL+‹›: SEITE · SEL 2×: MENÜ", XS, rgb(0x6d6a5c), 1);
}

// ================================================================ Menue
void App::drawMenu(Canvas& c) {
    drawHeader(c, "XENO·MENÜ", "PROJEKT: projekt.r36", false);
    c.text(400, 24, "SELECT 2×: SCHLIESSEN", XS, C_DIM, 2);
    c.text(400, 40, "B: ZURÜCK", XS, C_DIM, 2);
    float pr[4] = {4, 26, 4, 26};
    c.roundRect(38, 78, 644, 504, pr, C_PANEL, C_PANEL);
    c.roundStroke(38, 78, 644, 504, pr, C_LINE);
    c.text(56, 90, "EINSTELLUNGEN", XS, C_DIM, 3);
    Pattern& pat = prj.pat[prj.curPat];
    static const char* kr[3] = {"AUS", "SELTEN", "ÖFTER"};
    std::string vals[MI_COUNT] = {
        fmt("%d BPM", prj.bpm), fmt("%d %%", prj.swing), fmt("%d STEPS (PAT %02d)", pat.len, prj.curPat + 1),
        kNoteNames[prj.root], kScaleNames[prj.scale], kr[prj.krabbo % 3], prj.limiter ? "AN" : "AUS",
        fmt("%d", prj.master), fmt("%d GELADEN", samplesLoaded), "A", confirmNew ? "NOCHMAL A" : "A"};
    for (int i = 0; i < MI_COUNT; i++) {
        float y = 110 + i * 42;
        bool sel = i == menuSel;
        float r2[4] = {12, 3, 12, 3};
        c.roundRect(56, y, 608, 36, r2, sel ? rgb(0x1c2a1e) : C_DARK, sel ? rgb(0x1c2a1e) : C_DARK);
        c.roundStroke(56, y, 608, 36, r2, sel ? C_GRNL : rgb(0x232c24));
        c.text(70, int(y) + 9, kMenuLabels[i], S, sel ? C_TXTH : C_TXT, 2);
        bool action = i >= MI_RELOAD;
        std::string v = action ? vals[i] : "‹ " + vals[i] + " ›";
        c.text(650 - Canvas::textW(v, S, 1), int(y) + 9, v, S, sel ? (i == MI_NEW && confirmNew ? C_BRASS : C_GRNL) : C_DIM, 1);
    }
    float br[4] = {22, 4, 22, 4};
    c.roundRect(38, 592, 644, 40, br, C_PANEL, C_PANEL);
    c.roundStroke(38, 592, 644, 40, br, C_LINE);
    c.text(56, 603, "R36 GENRA · V0.1 · GPL-3.0", S, C_DIM, 2);
    std::string q = "BEENDEN: START + SELECT";
    c.text(666 - Canvas::textW(q, S, 1), 603, q, S, C_GRNL, 1);
    drawFooter(c, "KREUZ ↑↓: EINTRAG · ‹›: WERT · A: AUSFÜHREN", "TEMPO: A HALTEN = 10ER-SCHRITTE");
}

// ================================================================ Gesamtbild
void App::draw(Canvas& c) {
    if (!bgReady) buildBg();
    c.px = bg.px;
    if (menuOpen) drawMenu(c);
    else switch (page) {
        case PG_SEQ: drawSeq(c); break;
        case PG_SONG: drawSong(c); break;
        case PG_MIX: drawMix(c); break;
        case PG_FX: drawFx(c); break;
        case PG_SYN: drawSyn(c); break;
    }
    drawKrabbo(c);
    // Laufanzeige unten
    if (eng.playing) {
        float ph = float(std::max(0, (int)eng.playStep)) / std::max(1, (int)prj.pat[prj.curPat].len);
        c.glow(40 + ph * 560, 690, 120, 10, 5, rgb(0x8fbf62, 90), 4);
    }
    if (now < toastUntil && !toast.empty()) {
        int w = Canvas::textW(toast, M, 2) + 40;
        float rr[4] = {18, 4, 18, 4};
        float x = 360 - w / 2.f, y = 610;
        c.roundRect(x, y, w, 36, rr, rgb(0x1c2a1e, 240), rgb(0x101710, 240));
        c.roundStroke(x, y, w, 36, rr, C_GRN);
        c.text(int(x) + 20, int(y) + 9, toast, M, C_GRNLL, 2);
    }
}
