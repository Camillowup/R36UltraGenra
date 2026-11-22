#include "engine.h"
#include "wav.h"
#include "plaits/dsp/voice.h"
#include "stmlib/utils/buffer_allocator.h"
#include <cmath>
#include <algorithm>
#include <chrono>
#include <dirent.h>

static constexpr float PI = 3.14159265358979f;
static constexpr int PBUF = 32768;

float Engine::rnd() {
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return (rng & 0xFFFFFF) / float(0x1000000);
}

static float cutoffHz(int c, float sr) {
    float f = 40.f * std::pow(2.f, c / 100.f * 9.f);
    return std::min(f, sr * 0.45f);
}

Engine::Engine() {}
Engine::~Engine() {
    for (auto& v : iv) {
        delete v.pv;
        delete[] v.pbuf;
    }
}

// ---------------------------------------------------------------- eingebaute Drums
void Engine::buildBuiltins() {
    auto make = [&](int tr, const char* name, float len, auto fn) {
        SampleBuf b; b.name = name; b.sr = sr;
        int n = int(len * sr);
        b.d.resize(n);
        uint32_t seed = 777 + n;
        auto noise = [&]() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return (seed & 0xFFFF) / 32768.f - 1.f; };
        float st[8] = {};
        for (int i = 0; i < n; i++) b.d[i] = fn(i / sr, noise, st);
        float pk = 1e-6f;
        for (float v : b.d) pk = std::max(pk, std::fabs(v));
        for (float& v : b.d) v *= 0.9f / pk;
        drums[tr].push_back(std::move(b));
    };
    const float SR = sr;
    for (int v = 0; v < 4; v++) {
        static const char* nm[4] = {"kick_tief", "kick_punch", "kick_808", "kick_kurz"};
        float dec[4] = {0.45f, 0.3f, 1.1f, 0.18f}, f0[4] = {140, 220, 110, 180}, f1[4] = {45, 52, 42, 60};
        make(T_KICK, nm[v], dec[v] * 1.6f, [=](float t, auto& noise, float* s) {
            float f = f1[v] + (f0[v] - f1[v]) * std::exp(-t * 28.f);
            s[0] += 2 * PI * f / SR;
            float e = std::exp(-t * 4.6f / dec[v]);
            float click = v == 1 ? noise() * std::exp(-t * 400.f) * 0.5f : 0.f;
            return std::tanh(std::sin(s[0]) * e * (v == 2 ? 1.2f : 1.8f)) + click;
        });
    }
    for (int v = 0; v < 4; v++) {
        static const char* nm[4] = {"clap", "clap_weit", "snare", "snare_rim"};
        make(T_CLAP, nm[v], v == 1 ? 0.7f : 0.4f, [=](float t, auto& noise, float* s) {
            float n = noise();
            s[0] += 0.35f * (n - s[0]);
            s[1] += 0.08f * (s[0] - s[1]);
            float bp = s[0] - s[1];
            if (v < 2) {
                float burst = 0;
                for (int k = 0; k < 3; k++) { float tt = t - k * 0.011f; if (tt >= 0) burst += std::exp(-tt * 180.f); }
                return bp * (burst * 0.8f + std::exp(-t * (v == 1 ? 9.f : 18.f))) * 2.f;
            }
            s[2] += 2 * PI * (v == 2 ? 185.f : 330.f) / SR;
            float tone = std::sin(s[2]) * std::exp(-t * (v == 2 ? 30.f : 60.f));
            return bp * std::exp(-t * (v == 2 ? 14.f : 40.f)) * 1.6f + tone * 0.8f;
        });
    }
    for (int v = 0; v < 4; v++) {
        static const char* nm[4] = {"hat_zu", "hat_kurz", "hat_offen", "hat_metall"};
        float dec[4] = {0.05f, 0.025f, 0.35f, 0.12f};
        make(T_HAT, nm[v], dec[v] * 3 + 0.02f, [=](float t, auto& noise, float* s) {
            float n = noise();
            if (v == 3) {
                static const float fr[6] = {205.3f, 304.4f, 369.6f, 522.7f, 540.f, 800.f};
                float m = 0;
                for (int k = 0; k < 6; k++) m += std::sin(2 * PI * fr[k] * 7.f * t) > 0 ? 1.f : -1.f;
                n = n * 0.3f + m / 6.f;
            }
            s[0] += 0.5f * (n - s[0]);
            float hp = n - s[0];
            s[1] += 0.6f * (hp - s[1]);
            return (hp - s[1] * 0.3f) * std::exp(-t * 4.6f / dec[v]);
        });
    }
    for (int v = 0; v < 4; v++) {
        static const char* nm[4] = {"rim", "tom", "glocke", "shaker"};
        make(T_PERC, nm[v], v == 1 ? 0.6f : 0.35f, [=](float t, auto& noise, float* s) {
            if (v == 0) { s[0] += 2 * PI * 820.f / SR; return std::sin(s[0]) * std::exp(-t * 70.f) + noise() * std::exp(-t * 300.f) * 0.4f; }
            if (v == 1) { s[0] += 2 * PI * (120.f + 90.f * std::exp(-t * 20.f)) / SR; return std::sin(s[0]) * std::exp(-t * 7.f); }
            if (v == 2) {
                float a = std::sin(2 * PI * 540.f * t) > 0 ? 1.f : -1.f, b = std::sin(2 * PI * 800.f * t) > 0 ? 1.f : -1.f;
                float x = (a + b) * 0.5f;
                s[0] += 0.3f * (x - s[0]);
                return (x - s[0]) * std::exp(-t * 12.f);
            }
            float n = noise();
            s[0] += 0.6f * (n - s[0]);
            return (n - s[0]) * std::min(1.f, t * 60.f) * std::exp(-t * 22.f);
        });
    }
    for (int i = 0; i < NDRUM; i++) builtinCount[i] = int(drums[i].size());
}

void Engine::init(Project* p, float sampleRate) {
    prj = p;
    sr = sampleRate;
    for (auto& d : drums) d.clear();
    buildBuiltins();
    dlyL.assign(int(sr * 3.f), 0.f);
    dlyR.assign(int(sr * 3.f), 0.f);
    preBuf.assign(int(sr * 0.1f) + 2, 0.f);
    static const int ct[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
    static const int at[4] = {556, 441, 341, 225};
    float sc = sr / 44100.f;
    for (int i = 0; i < 8; i++) {
        combL[i].b.assign(int(ct[i] * sc), 0.f);
        combR[i].b.assign(int((ct[i] + 23) * sc), 0.f);
    }
    for (int i = 0; i < 4; i++) {
        apL[i].b.assign(int(at[i] * sc), 0.f);
        apR[i].b.assign(int((at[i] + 23) * sc), 0.f);
    }
    for (auto& c : chBuf) c.assign(2048, 0.f);
    ringL.assign(1 << 18, 0.f);
    ringR.assign(1 << 18, 0.f);
    pend.reserve(256);
    for (int i = 0; i < NPV; i++) {
        if (!iv[i].pv) {
            iv[i].pv = new plaits::Voice();
            iv[i].pbuf = new char[PBUF];
            stmlib::BufferAllocator a(iv[i].pbuf, PBUF);
            iv[i].pv->Init(&a);
        }
    }
}

int Engine::loadSamples(const std::string& base) {
    static const char* dirs[NDRUM] = {"kick", "clap", "hat", "perc"};
    int count = 0;
    auto loadDir = [&](const std::string& dir, std::vector<SampleBuf>& into) {
        DIR* d = opendir(dir.c_str());
        if (!d) return;
        std::vector<std::string> names;
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n.size() > 4 && n[0] != '.') {
                std::string ext = n.substr(n.size() - 4);
                for (auto& c : ext) c = char(tolower(c));
                if (ext == ".wav") names.push_back(n);
            }
        }
        closedir(d);
        std::sort(names.begin(), names.end());
        for (auto& n : names) {
            if (into.size() >= 120) break;
            SampleBuf b;
            if (loadWav(dir + "/" + n, b.d, b.sr)) {
                b.name = n.substr(0, n.size() - 4);
                into.push_back(std::move(b));
                count++;
            }
        }
    };
    for (int i = 0; i < NDRUM; i++) {
        drums[i].resize(builtinCount[i]);
        loadDir(base + "/" + dirs[i], drums[i]);
    }
    keys.clear();
    loadDir(base + "/sample", keys);
    loadDir(base + "/keys", keys);  // alter Ordnername aus v0.1
    return count;
}

const SampleBuf* Engine::drumSample(int track, int idx) const {
    const auto& v = drums[track];
    if (v.empty()) return nullptr;
    if (idx < 0 || idx >= int(v.size())) idx = 0;
    return &v[idx];
}

// ---------------------------------------------------------------- Sequencer
double Engine::stepLen(int s) const {
    double base = 60.0 / std::max<int>(30, prj->bpm) / 4.0 * sr;
    double sw = (std::clamp<int>(prj->swing, 50, 80) - 50) / 50.0;
    return (s & 1) ? base * (1 - sw) : base * (1 + sw);
}

float Engine::stepPhase() const {
    if (!playing) return 0;
    double len = stepLen(gstep > 0 ? gstep - 1 : 0);
    return float(std::clamp(1.0 - toNext / len, 0.0, 1.0));
}

void Engine::resetPositions() {
    gstep = 0;
    songBarStep = 0;
    for (int t = 0; t < NT; t++) { tstep[t] = 0; loops[t] = 0; drift[t] = driftP[t] = 0; }
}

void Engine::start() {
    resetPositions();
    toNext = 0;
    songMode = false;
    playing = true;
}

void Engine::startSong(int row) {
    if (prj->songLen == 0) return;
    songRow = std::clamp(row, 0, int(prj->songLen) - 1);
    prj->curPat = prj->song[songRow].pat;
    queuedPat = -1;
    resetPositions();
    toNext = 0;
    songMode = true;
    playing = true;
}

void Engine::stop() {
    playing = false;
    songMode = false;
    playStep = -1;
    for (int t = 0; t < NT; t++) playStepT[t] = -1;
    if (queuedPat >= 0) { prj->curPat = uint8_t(queuedPat); queuedPat = -1; }
    pend.clear();
    for (auto& v : iv) if (v.active) { v.stage = 2; v.gate = 0; }
}

void Engine::allNotesOff() {
    for (auto& v : iv) v.active = false;
    for (auto& d : dv) d.active = false;
    pend.clear();
}

void Engine::advance(int offset) {
    Pattern& p = prj->pat[prj->curPat];
    for (int t = 0; t < NT; t++) {
        int len = trackLen(*prj, p, t);
        int s = tstep[t] % len;
        const Step& st = p.s[t][p.var[t] & 3][s];
        playStepT[t] = s;
        if (st.on) schedule(t, st, offset, s);
        if (++tstep[t] >= len) { tstep[t] = 0; loops[t]++; }
    }
    playStep = gstep;
    toNext += stepLen(gstep);
    gstep++;
    songBarStep++;
    if (songMode) {
        const SongRow& r = prj->song[songRow];
        if (songBarStep >= std::max(1, int(r.bars)) * 16) {
            songRow++;
            if (songRow >= prj->songLen) {
                if (prj->songLoop) songRow = 0;
                else { songRow = 0; playing = false; songMode = false; for (auto& v : iv) if (v.active) { v.stage = 2; v.gate = 0; } return; }
            }
            prj->curPat = prj->song[songRow].pat;
            gstep = 0; songBarStep = 0;
            for (int t = 0; t < NT; t++) tstep[t] = 0;
        }
        return;
    }
    if (gstep >= p.len) {
        gstep = 0;
        if (queuedPat >= 0) {
            prj->curPat = uint8_t(queuedPat);
            queuedPat = -1;
            for (int t = 0; t < NT; t++) tstep[t] = 0;
        }
    }
}

void Engine::schedule(int t, const Step& st, int offset, int s) {
    const Track& tr = prj->tr[t];
    double base = 60.0 / std::max<int>(30, prj->bpm) / 4.0 * sr;
    double d = offset;
    if (s & 1) d += (std::clamp<int>(tr.swing, 50, 80) - 50) / 50.0 * 0.5 * base;
    float cents = 0, velMul = 1;
    switch (tr.groove) {
        case G_808:
            d += sr * (0.002 + rnd() * 0.004);
            ph808[t] += 0.11f;
            cents = 6.f * std::sin(ph808[t]);
            velMul = 0.95f + rnd() * 0.1f;
            break;
        case G_909:
            d += sr * rnd() * 0.0012;
            velMul = 0.88f + rnd() * 0.24f;
            break;
        case G_MINIMAL: {
            float amt = std::min(1.f, 0.3f + loops[t] / 24.f);
            drift[t] = std::clamp(drift[t] + (rnd() - 0.5f) * 0.0007f * amt, 0.f, 0.009f);
            driftP[t] = std::clamp(driftP[t] + (rnd() - 0.5f) * 3.f * amt, -14.f, 14.f);
            d += sr * drift[t];
            cents = driftP[t];
            velMul = 0.92f + rnd() * 0.08f;
            break;
        }
        default: break;
    }
    Step s2 = st;
    float v = st.vel * velMul;
    if (tr.veloRnd) v *= 1.f - rnd() * tr.veloRnd / 100.f;
    int rep = stepParam(st, tr, P_REPEAT);
    for (int k = 0; k < rep && pend.size() < 250; k++) {
        s2.vel = uint8_t(std::clamp(v * std::pow(0.85f, float(k)), 1.f, 127.f));
        pend.push_back({t, s2, int(d + k * base / rep), cents, false});
    }
}

void Engine::fire(const Pending& p) {
    if (p.track < NDRUM) { triggerDrum(p.track, p.st, p.cents); return; }
    int base = int(60.0 / std::max<int>(30, prj->bpm) / 4.0 * sr);
    const Track& tr = prj->tr[p.track];
    int lenSteps = 1 + (stepParam(p.st, tr, P_DECAY) * 15 + 50) / 100;
    int rep = stepParam(p.st, tr, P_REPEAT);
    int gate = rep > 1 ? std::max(1, base / rep - base / (rep * 6)) : std::max(1, lenSteps * base - base / 8);
    triggerInst(p.track, p.st, p.cents, gate);
}

void Engine::preview(int track, const Step& st) {
    Pending p{track, st, 0, 0, false};
    fire(p);
}

static void chordNotes(int note, int root, int scale, int out[3]) {
    int n;
    const int* sc = scaleSteps(scale, n);
    int rel = ((note - root) % 12 + 12) % 12, oct = (note - root - rel) / 12;
    int deg = 0;
    for (int i = 0; i < n; i++) if (sc[i] <= rel) deg = i;
    for (int k = 0; k < 3; k++) {
        int d = deg + k * 2;
        out[k] = root + oct * 12 + sc[d % n] + 12 * (d / n) + (k == 0 ? rel - sc[deg] : 0);
    }
}

void Engine::triggerDrum(int t, const Step& st, float cents) {
    const Track& tr = prj->tr[t];
    int idx = st.sample >= 0 ? st.sample : tr.sample;
    if (tr.randPct > 0 && rnd() * 100.f < tr.randPct && !drums[t].empty()) idx = int(rnd() * drums[t].size());
    const SampleBuf* b = drumSample(t, idx);
    if (!b || b->d.empty()) return;
    DrumVoice& v = dv[t];
    v.buf = b;
    v.pos = 0;
    v.rate = std::pow(2.0, stepParam(st, tr, P_PITCH) / 12.0 + cents / 1200.0) * b->sr / sr;
    int dec = stepParam(st, tr, P_DECAY);
    if (dec >= 50) v.envMul = 1.f;
    else {
        float T = 0.015f * std::pow(2.f, dec / 50.f * 7.f);
        v.envMul = std::exp(std::log(0.001f) / (T * sr));
    }
    v.env = 1.f;
    int c = stepParam(st, tr, P_CUTOFF);
    v.filt = c < 100;
    v.g = std::tan(PI * cutoffHz(c, sr) / sr);
    v.k = 1.2f;
    v.ic1 = v.ic2 = 0;
    v.gain = st.vel / 100.f;
    v.active = true;
    trRevT[t] = stepParam(st, tr, P_REVERB) / 100.f;
    trDlyT[t] = stepParam(st, tr, P_DELAY) / 100.f;
    if (t == T_KICK && prj->duck) duckEnv = 1.f;
    trigFlash[t] = true;
}

void Engine::triggerInst(int t, const Step& st, float cents, int gate, int noteOverride) {
    const Track& tr = prj->tr[t];
    int note = noteOverride >= 0 ? noteOverride : st.note;
    bool chord = (t == T_PAD && tr.syn.engine != 14 && tr.syn.mode == VM_POLY) || (t == T_SAMPLE && st.chord);
    trRevT[t] = stepParam(st, tr, P_REVERB) / 100.f;
    trDlyT[t] = stepParam(st, tr, P_DELAY) / 100.f;
    trigFlash[t] = true;
    if (chord) {
        // vorherigen Akkord ausklingen lassen (verhindert haengende Toene)
        for (auto& v : iv) if (v.active && v.track == t && v.gate >= 0 && v.stage < 2) { v.stage = 2; v.gate = 0; }
        int nn[3];
        chordNotes(note, prj->root, prj->scale, nn);
        for (int k = 0; k < 3; k++) noteOn(t, nn[k], st, cents, gate);
    } else {
        noteOn(t, note, st, cents, gate);
    }
}

void Engine::noteOn(int t, int note, const Step& st, float cents, int gate) {
    const Track& tr = prj->tr[t];
    const Synth& sy = tr.syn;
    note = std::clamp(note, 12, 108);
    int lo = t == T_SAMPLE ? NPV : 0, hi = t == T_SAMPLE ? NPV + NSV : NPV;
    int mode = t == T_SAMPLE ? VM_POLY : sy.mode;
    int maxV = mode == VM_POLY ? (t == T_PAD ? 3 : t == T_BASS ? 2 : t == T_LEAD ? 3 : 4) : 1;
    InstVoice* v = nullptr;
    bool legato = false;
    if (mode != VM_POLY) {
        for (int i = lo; i < hi; i++) if (iv[i].active && iv[i].track == t) { v = &iv[i]; break; }
        if (v && mode == VM_LEGATO && v->stage < 2) legato = true;
    }
    if (!v) {
        int cnt = 0, oldestT = -1, freeV = -1, oldest = -1;
        for (int i = lo; i < hi; i++) {
            if (iv[i].active) {
                if (iv[i].track == t) { cnt++; if (oldestT < 0 || iv[i].age < iv[oldestT].age) oldestT = i; }
                if (oldest < 0 || iv[i].age < iv[oldest].age) oldest = i;
            } else if (freeV < 0) freeV = i;
        }
        int pick = cnt >= maxV ? oldestT : (freeV >= 0 ? freeV : oldest);
        if (pick < 0) return;
        v = &iv[pick];
        bool reuseSame = v->active && v->track == t;
        if (!reuseSame || sy.glide == 0) v->curNote = float(note);
        if (!v->active) { v->env = 0; v->s[0] = v->s[1] = v->s[2] = v->s[3] = 0; v->ic1 = v->ic2 = 0; }
    }
    if (v->active && v->pv && v->stage < 2) v->retrig = true;
    v->track = t;
    v->note = note;
    v->target = float(note);
    if (sy.glide == 0 && !legato) v->curNote = float(note);
    float gt = 0.002f * std::pow(2.f, sy.glide / 100.f * 8.f);
    v->glideCoef = 1.f - std::exp(-24.f / (gt * sr));
    v->age = ++ageCounter;
    v->gate = gate;
    if (!legato) {
        v->stage = 0;
        v->fstage = 0;
        v->fenv = 0;
        v->t = 0;
        v->ph[0] = v->ph[1] = v->ph[2] = 0;
    } else if (v->stage == 2) v->stage = 0;
    float att = 0.001f * std::pow(2.f, sy.a / 100.f * 12.f);
    float dec = 0.005f * std::pow(2.f, sy.d / 100.f * 10.f);
    float rel = 0.005f * std::pow(2.f, sy.r / 100.f * 10.f);
    v->attInc = 1.f / (att * sr);
    v->decMul = std::exp(std::log(0.001f) / (dec * sr));
    v->sus = sy.s / 100.f;
    v->relMul = std::exp(std::log(0.0005f) / (rel * sr));
    float fa = 0.001f * std::pow(2.f, sy.fatk / 100.f * 11.f);
    float fd = 0.01f * std::pow(2.f, sy.fdec / 100.f * 9.f);
    v->fAttInc = 1.f / (fa * sr);
    v->fDecMul = std::exp(std::log(0.001f) / (fd * sr));
    v->vel = std::min(1.27f, st.vel / 100.f);
    v->accent = sy.accent / 100.f * std::clamp((v->vel - 0.85f) / 0.15f, 0.f, 1.f);
    v->cents = cents;
    v->cutLock = (st.lockMask & (1 << P_CUTOFF)) ? st.cutoff : -1;
    v->cnt = 0;
    if (t == T_SAMPLE) {
        v->smp = nullptr;
        if (!keys.empty()) {
            int idx = st.sample >= 0 ? st.sample : tr.sample;
            if (idx < 0 || idx >= int(keys.size())) idx = 0;
            v->smp = &keys[idx];
            v->spos = 0;
            v->srate = std::pow(2.0, (note - 60) / 12.0 + cents / 1200.0) * v->smp->sr / sr;
        }
    }
    v->active = true;
}

void Engine::liveOn(int t, int note, int vel) {
    Step st;
    st.on = 1;
    st.note = uint8_t(std::clamp(note, 12, 108));
    st.vel = uint8_t(std::clamp(vel * 100 / 127, 1, 127));
    if (t < NDRUM) { triggerDrum(t, st, 0); return; }
    st.chord = 0;
    if (t == T_PAD && prj->tr[t].syn.engine != 14 && prj->tr[t].syn.mode == VM_POLY) {
        int nn[3];
        chordNotes(st.note, prj->root, prj->scale, nn);
        for (int k = 0; k < 3; k++) noteOn(t, nn[k], st, 0, -1);
        for (auto& v : iv) if (v.active && v.track == t && v.gate == -1) v.note = st.note;  // gemeinsam loslassen
    } else noteOn(t, st.note, st, 0, -1);
    trRevT[t] = prj->tr[t].rev / 100.f;
    trDlyT[t] = prj->tr[t].dly / 100.f;
}

void Engine::liveOff(int t, int note) {
    for (auto& v : iv)
        if (v.active && v.track == t && v.gate == -1 && v.note == note) { v.gate = 0; v.stage = 2; }
}

// ---------------------------------------------------------------- Stimme rendern
void Engine::renderVoice(InstVoice& v, float* dst, int n) {
    const Track& tr = prj->tr[v.track];
    const Synth& sy = tr.syn;
    float src[24];
    v.curNote += (v.target - v.curNote) * v.glideCoef;
    float note = v.curNote + v.cents / 100.f;
    if (v.pv) {
        plaits::Patch p{};
        p.note = note;
        p.harmonics = sy.harm / 100.f;
        p.timbre = sy.timbre / 100.f;
        p.morph = sy.morph / 100.f;
        p.engine = std::clamp<int>(sy.engine, 0, 20);
        p.decay = 0.15f + sy.d / 100.f * 0.8f;
        p.lpg_colour = 0.5f;
        plaits::Modulations m{};
        m.trigger_patched = true;
        m.level_patched = true;
        m.level = 1.f;
        m.trigger = (v.stage < 2 && !v.retrig) ? 1.f : 0.f;
        v.retrig = false;
        plaits::Voice::Frame fr[24];
        v.pv->Render(p, m, fr, n);
        for (int i = 0; i < n; i++) src[i] = fr[i].out / 32768.f;
    } else if (v.smp) {
        for (int i = 0; i < n; i++) {
            int ip = int(v.spos);
            if (ip + 1 >= int(v.smp->d.size())) { src[i] = 0; v.stage = 2; v.env = std::min(v.env, 0.0004f); continue; }
            float fr = float(v.spos - ip);
            src[i] = (v.smp->d[ip] + (v.smp->d[ip + 1] - v.smp->d[ip]) * fr) * 1.6f;
            v.spos += v.srate;
        }
    } else {
        // eingebautes E-Piano (wenn keine Samples im Ordner)
        float f = 440.f * std::pow(2.f, (note - 69) / 12.f);
        for (int i = 0; i < n; i++) {
            v.t += 1.f / sr;
            float idx = 2.2f * std::exp(-v.t * 5.f) + 0.25f;
            v.ph[1] += f / sr; v.ph[1] -= std::floor(v.ph[1]);
            v.ph[2] += f * 14.f / sr; v.ph[2] -= std::floor(v.ph[2]);
            v.ph[0] += f / sr; v.ph[0] -= std::floor(v.ph[0]);
            float mod = std::sin(2 * PI * v.ph[1]) * idx;
            src[i] = (std::sin(2 * PI * v.ph[0] + mod) + std::sin(2 * PI * v.ph[2]) * 0.15f * std::exp(-v.t * 12.f)) * 0.8f;
        }
    }
    float freq = 440.f * std::pow(2.f, (note - 69) / 12.f);
    float subAmt = v.pv ? sy.sub / 100.f : 0.f;
    float drive = 1.f + sy.drive / 100.f * 4.f;
    float res = sy.res / 100.f;
    int cut = v.cutLock >= 0 ? v.cutLock : tr.cutoff;
    float cutBase = cutoffHz(cut, sr);
    float fenvAmt = sy.fenv / 100.f * 6.f + v.accent * 1.5f;
    float keyAmt = sy.key / 100.f * (v.curNote - 60.f) / 12.f;
    float gain = (v.track == T_PAD ? 0.32f : v.track == T_BASS ? 0.6f : v.track == T_LEAD ? 0.42f : 0.55f) * v.vel * (1.f + v.accent * 0.25f);
    for (int i = 0; i < n; i++) {
        // Huellkurven
        if (v.gate > 0) { if (--v.gate == 0) v.stage = 2; }
        else if (v.gate == 0 && v.stage < 2) v.stage = 2;
        if (v.stage == 0) { v.env += v.attInc; if (v.env >= 1.f) { v.env = 1.f; v.stage = 1; } }
        else if (v.stage == 1) v.env = v.sus + (v.env - v.sus) * v.decMul;
        else { v.env *= v.relMul; if (v.env < 0.0003f) { v.active = false; break; } }
        if (v.fstage == 0) { v.fenv += v.fAttInc; if (v.fenv >= 1.f) { v.fenv = 1.f; v.fstage = 1; } }
        else v.fenv *= v.fDecMul;
        float x = src[i];
        if (subAmt > 0) {
            v.subPh += freq * 0.5f / sr;
            if (v.subPh >= 1.f) v.subPh -= 1.f;
            x += std::sin(2 * PI * v.subPh) * subAmt * 0.8f;
        }
        x *= drive;
        if (v.cnt <= 0) {
            float fc = std::clamp(cutBase * std::pow(2.f, fenvAmt * v.fenv + keyAmt), 20.f, sr * 0.45f);
            if (sy.ftype == F_LP24) { v.fg = 1.f - std::exp(-2.f * PI * fc / sr); v.fk = res * 3.9f; }
            else { v.fg = std::tan(PI * fc / sr); v.fk = 2.f - 1.9f * res; }
            v.cnt = 8;
        }
        v.cnt--;
        float y;
        if (sy.ftype == F_LP24) {
            float in = std::tanh(x - v.fk * v.s[3]);
            v.s[0] += v.fg * (in - v.s[0]);
            v.s[1] += v.fg * (v.s[0] - v.s[1]);
            v.s[2] += v.fg * (v.s[1] - v.s[2]);
            v.s[3] += v.fg * (v.s[2] - v.s[3]);
            y = v.s[3] * (1.f + v.fk * 0.35f);
        } else {
            float g = v.fg, k = v.fk;
            float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            float v3 = x - v.ic2, v1 = a1 * v.ic1 + a2 * v3, v2 = v.ic2 + a2 * v.ic1 + a3 * v3;
            v.ic1 = 2 * v1 - v.ic1; v.ic2 = 2 * v2 - v.ic2;
            y = sy.ftype == F_LP12 ? v2 : sy.ftype == F_BP ? v1 * k : x - k * v1 - v2;
            if (drive > 1.01f) y = std::tanh(y);
        }
        dst[i] += y * v.env * gain;
    }
}

// ---------------------------------------------------------------- Audio-Schleife
void Engine::process(float* out, int frames) {
    auto t0 = std::chrono::steady_clock::now();
    Project& P = *prj;
    bool anySolo = false;
    for (int t = 0; t < NT; t++) anySolo |= P.tr[t].solo != 0;
    float tgain[NT], tl[NT], trr[NT];
    for (int t = 0; t < NT; t++) {
        bool aud = anySolo ? P.tr[t].solo : !P.tr[t].mute;
        float g = P.tr[t].vol / 100.f;
        tgain[t] = aud ? g * g * 1.4f : 0.f;
        float pan = P.tr[t].pan / 100.f;
        tl[t] = std::cos(pan * PI * 0.5f) * 1.414f;
        trr[t] = std::sin(pan * PI * 0.5f) * 1.414f;
    }
    double base = 60.0 / std::max<int>(30, P.bpm) / 4.0 * sr;
    static const int dlySteps[4] = {1, 2, 3, 4};
    float dlen = float(std::clamp(base * dlySteps[P.dlyTime & 3], 16.0, double(dlyL.size()) - 8));
    float fb = P.dlyFb / 100.f * 0.92f, dlv = P.dlyLvl / 100.f;
    float size = P.revSize / 100.f, dampP = P.revDamp / 100.f;
    float roomFb, damp, rgain = 3.f;
    switch (P.revModel % 3) {
        case 1: roomFb = 0.80f + size * 0.18f; damp = dampP * 0.35f; break;   // HALLE
        case 2: roomFb = 0.72f + size * 0.24f; damp = dampP * 0.15f; rgain = 3.6f; break;  // PLATTE
        default: roomFb = 0.68f + size * 0.26f; damp = 0.25f + dampP * 0.5f; break;  // KAMMER
    }
    float rlv = P.revLvl / 100.f * rgain;
    static const float preMs[4] = {0, 10, 30, 60};
    int preLen = std::clamp(int(preMs[P.revPre & 3] * 0.001f * sr), 1, int(preBuf.size()) - 1);
    float mg = P.master / 100.f;
    mg = mg * mg * 1.6f;
    float duckMul = std::exp(-1.f / (0.18f * sr)), duckDepth = P.duck ? P.duckDepth / 100.f : 0.f;
    float chorusInc = 0.6f / sr;
    float peak[NT] = {};
    float pkL = 0, pkR = 0;
    // Stutter umschalten
    if (perfStutter != stutMode) {
        if (perfStutter) {
            int L = perfStutter == 4 ? int(0.07f * sr) : int(base * (perfStutter == 1 ? 4 : perfStutter == 2 ? 2 : 1));
            L = std::clamp(L, 64, int(ringL.size()) / 2);
            stutLen = L;
            stutStart = (ringPos - L + int(ringL.size())) % int(ringL.size());
            stutI = 0;
        }
        stutMode = perfStutter;
    }
    int RN = int(ringL.size());

    for (int pos = 0; pos < frames;) {
        int n = std::min(24, frames - pos);
        if (playing) {
            while (toNext < n && playing) advance(std::max(0, int(toNext)));
            toNext -= n;
        }
        for (size_t k = 0; k < pend.size();) {
            if (pend[k].delay < n) { fire(pend[k]); pend[k] = pend.back(); pend.pop_back(); }
            else { pend[k].delay -= n; k++; }
        }
        for (int t = 0; t < NT; t++) {
            std::fill(bus[t], bus[t] + n, 0.f);
            trRev[t] += (trRevT[t] - trRev[t]) * 0.2f;
            trDly[t] += (trDlyT[t] - trDly[t]) * 0.2f;
        }
        // Drums
        for (int t = 0; t < NDRUM; t++) {
            DrumVoice& v = dv[t];
            if (!v.active) continue;
            for (int i = 0; i < n; i++) {
                int ip = int(v.pos);
                if (ip + 1 >= int(v.buf->d.size())) { v.active = false; break; }
                float fr = float(v.pos - ip);
                float x = v.buf->d[ip] + (v.buf->d[ip + 1] - v.buf->d[ip]) * fr;
                v.pos += v.rate;
                if (v.filt) {
                    float a1 = 1.f / (1.f + v.g * (v.g + v.k)), a2 = v.g * a1, a3 = v.g * a2;
                    float v3 = x - v.ic2, v1 = a1 * v.ic1 + a2 * v3, v2 = v.ic2 + a2 * v.ic1 + a3 * v3;
                    v.ic1 = 2 * v1 - v.ic1; v.ic2 = 2 * v2 - v.ic2;
                    x = v2;
                }
                bus[t][i] += x * v.env * v.gain;
                v.env *= v.envMul;
                if (v.env < 0.0005f) { v.active = false; break; }
            }
        }
        // Synths und SAMPLE
        for (auto& v : iv) if (v.active) renderVoice(v, bus[v.track], n);

        for (int i = 0; i < n; i++) {
            float L = 0, R = 0, sR = 0, sD = 0;
            float dk = 1.f - duckDepth * duckEnv;
            duckEnv *= duckMul;
            chPh += chorusInc * 2 * PI;
            if (chPh > 2 * PI) chPh -= 2 * PI;
            for (int t = 0; t < NT; t++) {
                float x = bus[t][i];
                if (t != T_KICK) x *= dk;
                const Synth& sy = P.tr[t].syn;
                float xl = x, xr = x;
                if (t >= NDRUM) {
                    if (sy.dist) {
                        float dd = 1.f + sy.dist / 100.f * 10.f;
                        x = std::tanh(x * dd) / (1.f + sy.dist / 100.f * 1.2f);
                    }
                    if (sy.lofi) {
                        float hold = 1.f + sy.lofi / 100.f * 15.f;
                        if (++lofiCnt[t] >= hold) { lofiCnt[t] = 0; float q = std::pow(2.f, 15.f - sy.lofi / 100.f * 11.f); lofiHold[t] = std::round(x * q) / q; }
                        x = lofiHold[t];
                    }
                    xl = xr = x;
                    if (sy.chorus) {
                        auto& cb = chBuf[t];
                        cb[chPos] = x;
                        float w = sy.chorus / 100.f;
                        float d1 = (0.012f + 0.004f * std::sin(chPh)) * sr, d2 = (0.012f + 0.004f * std::sin(chPh + PI)) * sr;
                        auto rd = [&](float d) {
                            float p = chPos - d; if (p < 0) p += 2048;
                            int i0 = int(p); float f = p - i0;
                            return cb[i0 & 2047] * (1 - f) + cb[(i0 + 1) & 2047] * f;
                        };
                        xl = x * (1 - w * 0.4f) + rd(d1) * w * 0.7f;
                        xr = x * (1 - w * 0.4f) + rd(d2) * w * 0.7f;
                    }
                }
                float yl = xl * tgain[t] * tl[t], yr = xr * tgain[t] * trr[t];
                L += yl; R += yr;
                float m = (yl + yr) * 0.5f;
                sR += m * trRev[t];
                sD += m * trDly[t];
                peak[t] = std::max(peak[t], std::fabs(m));
            }
            chPos = (chPos + 1) & 2047;
            // Delay
            if (!P.dlyOn) sD = 0;
            int N = int(dlyL.size());
            float dl, dr;
            if (P.dlyModel == 1) {  // TAPE
                wowPh += 0.9f / sr * 2 * PI;
                if (wowPh > 2 * PI) wowPh -= 2 * PI;
                float rp = dlyPos - dlen * (1.f + 0.004f * std::sin(wowPh)) ;
                while (rp < 0) rp += N;
                int i0 = int(rp); float f = rp - i0;
                float x = dlyL[i0 % N] * (1 - f) + dlyL[(i0 + 1) % N] * f;
                dlyLp += 0.33f * (x - dlyLp);
                dl = dlyLp;
                dr = dlyLp;
                dlyL[dlyPos] = std::tanh(sD * 1.2f + dl * fb * 1.05f);
                dlyR[dlyPos] = 0;
            } else {
                int rp = dlyPos - int(dlen); if (rp < 0) rp += N;
                dl = dlyL[rp]; dr = dlyR[rp];
                if (P.dlyModel == 2) { dlyL[dlyPos] = sD + dl * fb; dlyR[dlyPos] = sD + dr * fb; }
                else { dlyL[dlyPos] = sD + dr * fb; dlyR[dlyPos] = dl * fb; }
            }
            if (++dlyPos >= N) dlyPos = 0;
            L += dl * dlv; R += dr * dlv;
            // Hall
            if (!P.revOn) sR = 0;
            preBuf[prePos] = sR;
            int pr = prePos - preLen; if (pr < 0) pr += int(preBuf.size());
            if (++prePos >= int(preBuf.size())) prePos = 0;
            float in = preBuf[pr] * 0.015f, oL = 0, oR = 0;
            for (int c = 0; c < 8; c++) {
                Comb& cl = combL[c];
                float o = cl.b[cl.i];
                cl.store = o * (1 - damp) + cl.store * damp;
                cl.b[cl.i] = in + cl.store * roomFb;
                if (++cl.i >= int(cl.b.size())) cl.i = 0;
                oL += o;
                Comb& cr = combR[c];
                o = cr.b[cr.i];
                cr.store = o * (1 - damp) + cr.store * damp;
                cr.b[cr.i] = in + cr.store * roomFb;
                if (++cr.i >= int(cr.b.size())) cr.i = 0;
                oR += o;
            }
            for (int a = 0; a < 4; a++) {
                AP& al = apL[a];
                float b = al.b[al.i]; al.b[al.i] = oL + b * 0.5f; oL = b - oL; if (++al.i >= int(al.b.size())) al.i = 0;
                AP& ar = apR[a];
                b = ar.b[ar.i]; ar.b[ar.i] = oR + b * 0.5f; oR = b - oR; if (++ar.i >= int(ar.b.size())) ar.i = 0;
            }
            L += oL * rlv; R += oR * rlv;
            // Live-Filter (Stick links)
            pfSm += (perfFilter - pfSm) * 0.002f;
            if (std::fabs(pfSm) > 0.02f) {
                float fc = pfSm < 0 ? 20000.f * std::pow(2.f, pfSm * 7.5f) : 20.f * std::pow(2.f, pfSm * 8.3f);
                fc = std::clamp(fc, 20.f, sr * 0.45f);
                float g = std::tan(PI * fc / sr), k = 1.1f;
                float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
                auto svf = [&](float x, float* st) {
                    float v3 = x - st[1], v1 = a1 * st[0] + a2 * v3, v2 = st[1] + a2 * st[0] + a3 * v3;
                    st[0] = 2 * v1 - st[0]; st[1] = 2 * v2 - st[1];
                    return pfSm < 0 ? v2 : x - k * v1 - v2;
                };
                L = svf(L, pfL); R = svf(R, pfR);
            }
            // Stutter / Freeze (Stick rechts)
            if (!stutMode) { ringL[ringPos] = L; ringR[ringPos] = R; ringPos = (ringPos + 1) % RN; }
            stutMix += ((stutMode ? 1.f : 0.f) - stutMix) * 0.004f;
            if (stutMix > 0.001f) {
                int idx = (stutStart + stutI) % RN;
                float edge = stutMode == 4 ? 600.f : 96.f;
                float w = std::min(1.f, std::min(float(stutI), float(stutLen - stutI)) / edge);
                float yl = ringL[idx] * w, yr = ringR[idx] * w;
                if (++stutI >= stutLen) stutI = 0;
                L = L * (1 - stutMix) + yl * stutMix;
                R = R * (1 - stutMix) + yr * stutMix;
            }
            // Master
            L *= mg; R *= mg;
            if (P.limiter) {
                float pk = std::max(std::fabs(L), std::fabs(R)) * limGain;
                if (pk > 0.9f) limGain *= 0.9f / pk;
                else limGain = std::min(1.f, limGain + 0.00002f);
                L *= limGain; R *= limGain;
            }
            L = std::tanh(L); R = std::tanh(R);
            out[2 * (pos + i)] = L; out[2 * (pos + i) + 1] = R;
            pkL = std::max(pkL, std::fabs(L)); pkR = std::max(pkR, std::fabs(R));
        }
        pos += n;
    }
    for (int t = 0; t < NT; t++) meter[t] = std::max(peak[t], meter[t] * 0.85f);
    meterL = std::max(pkL, meterL * 0.85f);
    meterR = std::max(pkR, meterR * 0.85f);
    double used = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    float load = float(used / (frames / double(sr)));
    cpuLoad = cpuLoad * 0.9f + load * 0.1f;
}
