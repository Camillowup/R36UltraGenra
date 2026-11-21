#include "engine.h"
#include "wav.h"
#include <cmath>
#include <algorithm>
#include <dirent.h>

static constexpr float PI = 3.14159265358979f;

float Engine::rnd() {
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return (rng & 0xFFFFFF) / float(0x1000000);
}

static float cutoffHz(int c, float sr) {
    float f = 40.f * std::pow(2.f, c / 100.f * 9.f);
    return std::min(f, sr * 0.45f);
}

static inline float polyBlep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
    if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
    return 0.f;
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
    // KICK
    for (int v = 0; v < 4; v++) {
        static const char* nm[4] = {"bio_kick_tief", "bio_kick_punch", "bio_kick_808", "bio_kick_kurz"};
        float dec[4] = {0.45f, 0.3f, 1.1f, 0.18f}, f0[4] = {140, 220, 110, 180}, f1[4] = {45, 52, 42, 60};
        make(T_KICK, nm[v], dec[v] * 1.6f, [=](float t, auto& noise, float* s) {
            float f = f1[v] + (f0[v] - f1[v]) * std::exp(-t * 28.f);
            s[0] += 2 * PI * f / sr;
            float e = std::exp(-t * 4.6f / dec[v]);
            float click = v == 1 ? noise() * std::exp(-t * 400.f) * 0.5f : 0.f;
            return std::tanh(std::sin(s[0]) * e * (v == 2 ? 1.2f : 1.8f)) + click;
        });
    }
    // CLAP / SNARE
    for (int v = 0; v < 4; v++) {
        static const char* nm[4] = {"bio_clap", "bio_clap_weit", "bio_snare", "bio_snare_rim"};
        make(T_CLAP, nm[v], v == 1 ? 0.7f : 0.4f, [=](float t, auto& noise, float* s) {
            float n = noise();
            // Bandpass ueber zwei Einpol-Filter
            s[0] += 0.35f * (n - s[0]);
            s[1] += 0.08f * (s[0] - s[1]);
            float bp = s[0] - s[1];
            if (v < 2) {
                float burst = 0;
                for (int k = 0; k < 3; k++) { float tt = t - k * 0.011f; if (tt >= 0) burst += std::exp(-tt * 180.f); }
                float tail = std::exp(-t * (v == 1 ? 9.f : 18.f));
                return bp * (burst * 0.8f + tail) * 2.f;
            }
            s[2] += 2 * PI * (v == 2 ? 185.f : 330.f) / sr;
            float tone = std::sin(s[2]) * std::exp(-t * (v == 2 ? 30.f : 60.f));
            return bp * std::exp(-t * (v == 2 ? 14.f : 40.f)) * 1.6f + tone * 0.8f;
        });
    }
    // HAT
    for (int v = 0; v < 4; v++) {
        static const char* nm[4] = {"bio_hat_zu", "bio_hat_kurz", "bio_hat_offen", "bio_hat_metall"};
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
    // PERC
    for (int v = 0; v < 4; v++) {
        static const char* nm[4] = {"bio_rim", "bio_tom", "bio_glocke", "bio_shaker"};
        make(T_PERC, nm[v], v == 1 ? 0.6f : 0.35f, [=](float t, auto& noise, float* s) {
            if (v == 0) { s[0] += 2 * PI * 820.f / sr; return std::sin(s[0]) * std::exp(-t * 70.f) + noise() * std::exp(-t * 300.f) * 0.4f; }
            if (v == 1) { s[0] += 2 * PI * (120.f + 90.f * std::exp(-t * 20.f)) / sr; return std::sin(s[0]) * std::exp(-t * 7.f); }
            if (v == 2) {
                float a = std::sin(2 * PI * 540.f * t) > 0 ? 1.f : -1.f, b = std::sin(2 * PI * 800.f * t) > 0 ? 1.f : -1.f;
                float x = (a + b) * 0.5f;
                s[0] += 0.3f * (x - s[0]);
                return (x - s[0]) * std::exp(-t * 12.f);
            }
            float n = noise();
            s[0] += 0.6f * (n - s[0]);
            float env = std::min(1.f, t * 60.f) * std::exp(-t * 22.f);
            return (n - s[0]) * env;
        });
    }
    for (int i = 0; i < NDRUM; i++) builtinCount[i] = int(drums[i].size());
}

void Engine::init(Project* p, float sampleRate) {
    prj = p;
    sr = sampleRate;
    for (auto& d : drums) d.clear();
    buildBuiltins();
    dlyL.assign(int(sr * 2.5f), 0.f);
    dlyR.assign(int(sr * 2.5f), 0.f);
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
            if (n.size() > 4) {
                std::string ext = n.substr(n.size() - 4);
                for (auto& c : ext) c = char(tolower(c));
                if (ext == ".wav" && n[0] != '.') names.push_back(n);
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
    loadDir(base + "/keys", keys);
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

void Engine::start() {
    step = 0;
    toNext = 0;
    playing = true;
}

void Engine::stop() {
    playing = false;
    playStep = -1;
    if (queuedPat >= 0) { prj->curPat = uint8_t(queuedPat); queuedPat = -1; }
    for (auto& v : sv) if (v.active && v.stage < 2) v.gate = 0;
}

void Engine::allNotesOff() {
    for (auto& v : sv) v.active = false;
    for (auto& d : dv) d.active = false;
}

void Engine::advance() {
    Pattern& p = prj->pat[prj->curPat];
    int len = std::clamp<int>(p.len, 1, MAXSTEPS);
    if (step >= len) step = 0;
    int base = int(60.0 / std::max<int>(30, prj->bpm) / 4.0 * sr);
    for (int t = 0; t < NT; t++) {
        const Step& st = p.s[t][step];
        if (st.on) trigger(t, st, base);
    }
    playStep = step;
    toNext += stepLen(step);
    step++;
    if (step >= len) {
        step = 0;
        if (queuedPat >= 0) { prj->curPat = uint8_t(queuedPat); queuedPat = -1; }
    }
}

void Engine::preview(int track, const Step& st) {
    int base = int(60.0 / std::max<int>(30, prj->bpm) / 4.0 * sr);
    trigger(track, st, base);
}

static void chordNotes(int note, int root, int scale, int out[3]) {
    int n;
    const int* sc = scaleSteps(scale, n);
    // Stufe der Note in der Tonleiter suchen (naechste darunter)
    int rel = ((note - root) % 12 + 12) % 12, oct = (note - root - rel) / 12;
    int deg = 0;
    for (int i = 0; i < n; i++) if (sc[i] <= rel) deg = i;
    for (int k = 0; k < 3; k++) {
        int d = deg + k * 2;
        out[k] = root + oct * 12 + sc[d % n] + 12 * (d / n) + (k == 0 ? rel - sc[deg] : 0);
    }
}

void Engine::trigger(int t, const Step& st, int stepDur) {
    if (t < NDRUM) { triggerDrum(t, st); return; }
    const Track& tr = prj->tr[t];
    int lenSteps = 1 + (stepParam(st, tr, P_DECAY) * 15 + 50) / 100;
    int gate = std::max(1, lenSteps * stepDur - stepDur / 8);
    bool chord = t == T_PAD || (t == T_KEYS && st.chord);
    if (chord) {
        int nn[3];
        chordNotes(st.note, prj->root, prj->scale, nn);
        if (t == T_PAD) for (auto& v : sv) if (v.active && v.track == t && v.stage < 2) v.gate = 0;
        for (int k = 0; k < 3; k++) triggerSynth(t, nn[k], st, gate);
    } else {
        triggerSynth(t, st.note, st, gate);
    }
}

void Engine::triggerDrum(int t, const Step& st) {
    const Track& tr = prj->tr[t];
    int idx = st.sample >= 0 ? st.sample : tr.sample;
    if (tr.randPct > 0 && rnd() * 100.f < tr.randPct && !drums[t].empty()) idx = int(rnd() * drums[t].size());
    const SampleBuf* b = drumSample(t, idx);
    if (!b || b->d.empty()) return;
    DrumVoice& v = dv[t];
    v.buf = b;
    v.pos = 0;
    v.rate = std::pow(2.0, stepParam(st, tr, P_PITCH) / 12.0) * b->sr / sr;
    int dec = stepParam(st, tr, P_DECAY);
    // 50 = Sample unveraendert; darunter kuerzer, darueber laenger (bis voll)
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
    v.sendR = stepParam(st, tr, P_REVERB) / 100.f;
    v.sendD = stepParam(st, tr, P_DELAY) / 100.f;
    v.active = true;
}

struct SynthDef { float att, dec, sus, rel, gain, envAmt, fdec; int maxVoices; };
static const SynthDef kDefs[4] = {
    {0.30f, 0.80f, 0.70f, 0.90f, 0.15f, 0.6f, 0.6f, 3},   // PAD
    {0.003f, 0.22f, 0.55f, 0.05f, 0.40f, 3.2f, 0.12f, 1}, // BASS
    {0.010f, 0.35f, 0.60f, 0.25f, 0.17f, 1.6f, 0.25f, 2}, // LEAD
    {0.002f, 1.80f, 0.00f, 0.35f, 0.28f, 0.0f, 0.3f, 4},  // KEYS
};

void Engine::triggerSynth(int t, int note, const Step& st, int gate) {
    const SynthDef& d = kDefs[t - NDRUM];
    const Track& tr = prj->tr[t];
    note = std::clamp(note, 12, 108);
    // Stimme suchen: frei oder aelteste derselben Spur
    int cnt = 0, oldest = -1, freeV = -1;
    for (int i = 0; i < 16; i++) {
        if (sv[i].active && sv[i].track == t) {
            cnt++;
            if (oldest < 0 || sv[i].age < sv[oldest].age) oldest = i;
        } else if (!sv[i].active && freeV < 0) freeV = i;
    }
    int vi = (cnt >= d.maxVoices || freeV < 0) ? oldest : freeV;
    if (vi < 0) return;
    SynthVoice& v = sv[vi];
    bool legato = t == T_BASS && v.active && v.track == t && v.stage < 2;
    v.track = t;
    v.note = note;
    v.age = ++ageCounter;
    v.target = 440.f * std::pow(2.f, (note - 69) / 12.f);
    if (!legato) { v.freq = v.target; v.env = 0; v.stage = 0; v.t = 0; v.ph[0] = v.ph[1] = v.ph[2] = 0; v.ic1 = v.ic2 = 0; }
    else { v.stage = 0; }
    v.glideMul = 1.f;
    v.fenv = 1.f;
    v.gate = gate;
    v.cutBase = cutoffHz(stepParam(st, tr, P_CUTOFF), sr);
    v.envAmt = d.envAmt;
    v.vel = st.vel / 100.f;
    v.sendR = stepParam(st, tr, P_REVERB) / 100.f;
    v.sendD = stepParam(st, tr, P_DELAY) / 100.f;
    v.smp = nullptr;
    if (t == T_KEYS && !keys.empty()) {
        int idx = st.sample >= 0 ? st.sample : tr.sample;
        if (idx < 0 || idx >= int(keys.size())) idx = 0;
        v.smp = &keys[idx];
        v.spos = 0;
        // Grundton der Samples: C4 (60)
        v.srate = std::pow(2.0, (note - 60) / 12.0) * v.smp->sr / sr;
    }
    v.active = true;
}

// ---------------------------------------------------------------- Audio-Schleife
void Engine::process(float* out, int frames) {
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
    int dlySteps[4] = {2, 3, 4, 1};
    int dlen = int(60.0 / std::max<int>(30, P.bpm) / 4.0 * sr * dlySteps[P.dlyTime & 3]);
    dlen = std::clamp(dlen, 1, int(dlyL.size()) - 1);
    float fb = P.dlyFb / 100.f * 0.9f, dlv = P.dlyLvl / 100.f;
    float roomFb = 0.70f + P.revSize / 100.f * 0.28f, damp = P.revDamp / 100.f * 0.4f, rlv = P.revLvl / 100.f * 3.f;
    float mg = P.master / 100.f;
    mg = mg * mg * 1.6f;
    float peak[NT] = {};
    float pkL = 0, pkR = 0;
    float relMuls[4], attIncs[4], decMuls[4], fdecMuls[4];
    for (int k = 0; k < 4; k++) {
        attIncs[k] = 1.f / (kDefs[k].att * sr);
        decMuls[k] = std::exp(std::log(0.01f) / (kDefs[k].dec * sr));
        relMuls[k] = std::exp(std::log(0.001f) / (kDefs[k].rel * sr));
        fdecMuls[k] = std::exp(std::log(0.01f) / (kDefs[k].fdec * sr));
    }

    for (int i = 0; i < frames; i++) {
        if (playing) {
            while (toNext <= 0) advance();
            toNext -= 1.0;
        }
        float L = 0, R = 0, sR = 0, sD = 0;
        // Drums
        for (int t = 0; t < NDRUM; t++) {
            DrumVoice& v = dv[t];
            if (!v.active) continue;
            int ip = int(v.pos);
            if (ip + 1 >= int(v.buf->d.size())) { v.active = false; continue; }
            float fr = float(v.pos - ip);
            float x = v.buf->d[ip] + (v.buf->d[ip + 1] - v.buf->d[ip]) * fr;
            v.pos += v.rate;
            if (v.filt) {
                float a1 = 1.f / (1.f + v.g * (v.g + v.k)), a2 = v.g * a1, a3 = v.g * a2;
                float v3 = x - v.ic2, v1 = a1 * v.ic1 + a2 * v3, v2 = v.ic2 + a2 * v.ic1 + a3 * v3;
                v.ic1 = 2 * v1 - v.ic1; v.ic2 = 2 * v2 - v.ic2;
                x = v2;
            }
            x *= v.env * v.gain;
            v.env *= v.envMul;
            if (v.env < 0.0005f) v.active = false;
            float y = x * tgain[t];
            L += y * tl[t]; R += y * trr[t];
            sR += y * v.sendR; sD += y * v.sendD;
            peak[t] = std::max(peak[t], std::fabs(y));
        }
        // Synths
        for (auto& v : sv) {
            if (!v.active) continue;
            int k = v.track - NDRUM;
            const SynthDef& d = kDefs[k];
            // Huellkurve
            if (v.gate > 0) { if (--v.gate == 0) v.stage = 2; }
            if (v.stage == 0) { v.env += attIncs[k]; if (v.env >= 1.f) { v.env = 1.f; v.stage = 1; } }
            else if (v.stage == 1) v.env = d.sus + (v.env - d.sus) * decMuls[k];
            else { v.env *= relMuls[k]; if (v.env < 0.0003f) { v.active = false; continue; } }
            v.fenv *= fdecMuls[k];
            v.t += 1.f / sr;
            float f = v.freq;
            float x;
            if (v.track == T_KEYS) {
                if (v.smp) {
                    int ip = int(v.spos);
                    if (ip + 1 >= int(v.smp->d.size())) { v.active = false; continue; }
                    float fr = float(v.spos - ip);
                    x = v.smp->d[ip] + (v.smp->d[ip + 1] - v.smp->d[ip]) * fr;
                    v.spos += v.srate;
                    float e = v.stage == 2 ? v.env : 1.f;
                    x *= e * 2.2f;
                } else {
                    float idx = 2.2f * std::exp(-v.t * 5.f) + 0.25f;
                    v.ph[1] += f / sr; v.ph[1] -= std::floor(v.ph[1]);
                    v.ph[2] += f * 14.f / sr; v.ph[2] -= std::floor(v.ph[2]);
                    float mod = std::sin(2 * PI * v.ph[1]) * idx;
                    v.ph[0] += f / sr; v.ph[0] -= std::floor(v.ph[0]);
                    x = std::sin(2 * PI * v.ph[0] + mod) + std::sin(2 * PI * v.ph[2]) * 0.15f * std::exp(-v.t * 12.f);
                    x *= v.env;
                }
            } else {
                if (v.track == T_LEAD) f *= 1.f + 0.004f * std::min(1.f, std::max(0.f, v.t - 0.25f) * 3.f) * std::sin(2 * PI * 5.5f * v.t);
                float dt = f / sr;
                v.ph[0] += dt; if (v.ph[0] >= 1) v.ph[0] -= 1;
                if (v.track == T_PAD) {
                    float dt2 = dt * 1.0065f;
                    v.ph[1] += dt2; if (v.ph[1] >= 1) v.ph[1] -= 1;
                    x = (2 * v.ph[0] - 1 - polyBlep(v.ph[0], dt)) + (2 * v.ph[1] - 1 - polyBlep(v.ph[1], dt2));
                    x *= 0.5f;
                } else if (v.track == T_BASS) {
                    float dt2 = dt * 0.5f;
                    v.ph[1] += dt2; if (v.ph[1] >= 1) v.ph[1] -= 1;
                    float saw = 2 * v.ph[0] - 1 - polyBlep(v.ph[0], dt);
                    float p2 = v.ph[1] + 0.5f; if (p2 >= 1) p2 -= 1;
                    float sq = (v.ph[1] < 0.5f ? 1.f : -1.f) + polyBlep(v.ph[1], dt2) - polyBlep(p2, dt2);
                    x = saw * 0.7f + sq * 0.45f;
                } else {
                    float w = 0.35f, p2 = v.ph[0] + 1 - w; if (p2 >= 1) p2 -= 1;
                    x = (v.ph[0] < w ? 1.f : -1.f) + polyBlep(v.ph[0], dt) - polyBlep(p2, dt);
                }
                // Filter
                float fc = std::min(v.cutBase * std::pow(2.f, v.envAmt * v.fenv), sr * 0.45f);
                float g = std::tan(PI * fc / sr), kq = v.track == T_BASS ? 0.7f : 1.2f;
                float a1 = 1.f / (1.f + g * (g + kq)), a2 = g * a1, a3 = g * a2;
                float v3 = x - v.ic2, v1 = a1 * v.ic1 + a2 * v3, v2 = v.ic2 + a2 * v.ic1 + a3 * v3;
                v.ic1 = 2 * v1 - v.ic1; v.ic2 = 2 * v2 - v.ic2;
                x = v2 * v.env;
            }
            float y = x * d.gain * v.vel * tgain[v.track];
            L += y * tl[v.track]; R += y * trr[v.track];
            sR += y * v.sendR; sD += y * v.sendD;
            peak[v.track] = std::max(peak[v.track], std::fabs(y));
        }
        // Delay (Ping-Pong)
        int rp = dlyPos - dlen; if (rp < 0) rp += int(dlyL.size());
        float dl = dlyL[rp], dr = dlyR[rp];
        dlyL[dlyPos] = sD + dr * fb;
        dlyR[dlyPos] = dl * fb;
        if (++dlyPos >= int(dlyL.size())) dlyPos = 0;
        L += dl * dlv; R += dr * dlv;
        // Hall (Freeverb)
        float in = sR * 0.015f, oL = 0, oR = 0;
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
        // Master
        L *= mg; R *= mg;
        if (P.limiter) {
            float pk = std::max(std::fabs(L), std::fabs(R)) * limGain;
            if (pk > 0.9f) limGain *= 0.9f / pk;
            else limGain = std::min(1.f, limGain + 0.00002f);
            L *= limGain; R *= limGain;
        }
        L = std::tanh(L); R = std::tanh(R);
        out[2 * i] = L; out[2 * i + 1] = R;
        pkL = std::max(pkL, std::fabs(L)); pkR = std::max(pkR, std::fabs(R));
    }
    for (int t = 0; t < NT; t++) meter[t] = std::max(peak[t], meter[t] * 0.85f);
    meterL = std::max(pkL, meterL * 0.85f);
    meterR = std::max(pkR, meterR * 0.85f);
}
