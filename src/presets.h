// Plaits-Engines und Werks-Presets pro Spur (PAD, BASS, LEAD, SAMPLE).
#pragma once
#include "project.h"

constexpr int NENGINE = 21;  // Plaits 1.2 ohne die drei Drum-Engines
static const char* const kEngineNames[NENGINE] = {
    "VA + FILTER", "PHASE DIST", "6-OP FM 1", "6-OP FM 2", "6-OP FM 3", "WAVE TERRAIN", "STRING MACHINE",
    "CHIPTUNE", "VIRTUAL ANALOG", "WAVESHAPER", "FM 2-OP", "FORMANT", "ADDITIV", "WAVETABLE", "AKKORD",
    "SPRACHE", "SWARM", "RAUSCHEN", "PARTIKEL", "SAITE", "MODAL"};
static const char* const kEngineShort[NENGINE] = {
    "VA-VCF", "PD", "FM6-1", "FM6-2", "FM6-3", "TERRAIN", "STRINGS", "CHIP", "VA", "WSHAPE", "FM2", "FORMANT",
    "ADDITIV", "WTABLE", "AKKORD", "SPRACHE", "SWARM", "NOISE", "PARTIKEL", "SAITE", "MODAL"};
static const char* const kFilterNames[4] = {"LADDER 24", "TIEFPASS 12", "BANDPASS", "HOCHPASS"};
static const char* const kModeNames[3] = {"MONO", "LEGATO", "POLY"};

struct Preset {
    const char* name;
    uint8_t engine, timbre, harm, morph, sub, glide, accent, mode;
    uint8_t ftype, cutoff, res, fenv, drive, fatk, fdec, key;
    uint8_t a, d, s, r, chorus, dist, lofi, rev, dly, len;
};

// Laenge (len) = Track.decay: 4 = 1 Step, 7 = 2, 13 = 3, 47 = 8, 100 = 16 Steps
static const Preset kPadPresets[] = {
    {"WARM ANALOG", 0, 55, 40, 40, 0, 0, 20, VM_POLY, F_LP24, 55, 20, 10, 10, 30, 60, 40, 45, 70, 80, 60, 40, 0, 0, 50, 20, 100},
    {"GLAS", 13, 60, 50, 30, 0, 0, 20, VM_POLY, F_LP12, 70, 15, 15, 0, 40, 70, 40, 35, 70, 75, 65, 50, 0, 0, 60, 30, 100},
    {"STREICHER", 6, 50, 60, 50, 0, 0, 20, VM_POLY, F_LP24, 62, 15, 10, 0, 30, 60, 40, 50, 70, 85, 70, 60, 0, 0, 55, 15, 100},
    {"AKKORD WOLKE", 14, 40, 50, 60, 0, 0, 20, VM_POLY, F_LP12, 60, 10, 10, 0, 30, 60, 40, 55, 80, 80, 70, 50, 0, 0, 60, 25, 100},
    {"ORGEL", 12, 50, 40, 30, 0, 0, 10, VM_POLY, F_LP12, 80, 5, 0, 5, 0, 40, 40, 5, 50, 90, 25, 30, 10, 0, 30, 10, 47},
    {"FORMANT CHOR", 11, 45, 55, 50, 0, 0, 20, VM_POLY, F_LP12, 70, 10, 10, 0, 30, 60, 40, 50, 70, 80, 65, 40, 0, 0, 60, 20, 100},
    {"SWARM NEBEL", 16, 50, 60, 40, 0, 0, 20, VM_POLY, F_LP24, 50, 20, 15, 10, 40, 70, 40, 70, 80, 85, 80, 30, 0, 0, 70, 30, 100},
    {"WAVE TERRAIN", 5, 50, 50, 50, 0, 0, 20, VM_POLY, F_LP12, 65, 15, 20, 0, 35, 65, 40, 40, 70, 75, 60, 40, 0, 0, 55, 25, 100},
};
static const Preset kBassPresets[] = {
    {"ACID WURM", 8, 45, 20, 80, 0, 25, 70, VM_LEGATO, F_LP24, 35, 72, 70, 45, 0, 28, 30, 0, 35, 40, 8, 0, 20, 0, 5, 15, 7},
    {"SUB TIEF", 8, 20, 0, 10, 80, 0, 30, VM_MONO, F_LP24, 28, 10, 15, 10, 0, 35, 40, 0, 60, 80, 12, 0, 0, 0, 0, 0, 13},
    {"REESE", 8, 50, 75, 90, 30, 8, 40, VM_MONO, F_LP24, 42, 25, 20, 35, 0, 40, 30, 2, 70, 80, 15, 35, 25, 0, 10, 0, 13},
    {"FM KNURR", 10, 45, 40, 25, 20, 0, 50, VM_MONO, F_LP12, 65, 15, 30, 30, 0, 22, 40, 0, 40, 50, 10, 0, 15, 0, 0, 10, 7},
    {"DUB BASS", 9, 20, 30, 35, 40, 5, 30, VM_MONO, F_LP24, 30, 20, 10, 15, 2, 40, 30, 3, 70, 70, 30, 0, 0, 0, 15, 20, 13},
    {"TECHNO RUMPEL", 8, 35, 15, 60, 40, 0, 60, VM_MONO, F_LP24, 24, 35, 45, 70, 0, 25, 40, 0, 30, 30, 10, 0, 40, 0, 35, 0, 4},
    {"SAITEN-PLUCK", 19, 50, 40, 50, 30, 0, 40, VM_POLY, F_LP12, 75, 10, 10, 10, 0, 30, 50, 0, 55, 40, 30, 0, 0, 0, 20, 20, 7},
    {"CHIP BASS", 7, 50, 50, 40, 20, 0, 40, VM_MONO, F_LP12, 80, 5, 0, 10, 0, 30, 50, 0, 40, 80, 10, 0, 0, 30, 5, 10, 7},
};
static const Preset kLeadPresets[] = {
    {"SAW LEAD", 8, 60, 30, 80, 0, 10, 50, VM_LEGATO, F_LP24, 60, 35, 40, 20, 0, 35, 50, 1, 40, 70, 20, 20, 10, 0, 20, 35, 13},
    {"PHASE LEAD", 1, 50, 40, 50, 0, 5, 50, VM_MONO, F_LP12, 75, 20, 25, 10, 0, 30, 50, 1, 40, 70, 20, 0, 0, 0, 20, 35, 13},
    {"FM GLOCKE", 2, 50, 50, 50, 0, 0, 40, VM_POLY, F_LP12, 90, 5, 0, 0, 0, 30, 50, 0, 60, 30, 40, 0, 0, 0, 35, 30, 20},
    {"SPRACHE", 15, 50, 40, 30, 0, 0, 40, VM_MONO, F_LP12, 90, 5, 0, 0, 0, 30, 50, 1, 50, 70, 20, 0, 0, 0, 25, 25, 13},
    {"MALLET", 20, 50, 50, 60, 0, 0, 50, VM_POLY, F_LP12, 85, 10, 0, 0, 0, 30, 50, 0, 50, 30, 40, 0, 0, 0, 35, 25, 13},
    {"WAVESHAPE", 9, 55, 45, 50, 0, 0, 50, VM_MONO, F_LP24, 60, 30, 35, 30, 0, 30, 50, 1, 40, 60, 20, 0, 20, 0, 20, 30, 13},
    {"CHIP LEAD", 7, 60, 40, 50, 0, 5, 40, VM_MONO, F_LP12, 90, 5, 0, 0, 0, 30, 50, 0, 40, 80, 15, 0, 0, 20, 15, 30, 13},
    {"PARTIKEL", 18, 50, 50, 50, 0, 0, 40, VM_POLY, F_LP12, 80, 10, 0, 0, 0, 30, 50, 0, 60, 50, 40, 20, 0, 0, 50, 40, 13},
};
static const Preset kSamplePresets[] = {
    {"KEYGROUP", 0, 50, 50, 50, 0, 0, 30, VM_POLY, F_LP12, 100, 5, 0, 0, 0, 30, 50, 0, 70, 80, 30, 0, 0, 0, 25, 0, 13},
    {"WARM", 0, 50, 50, 50, 0, 0, 30, VM_POLY, F_LP24, 55, 15, 10, 10, 0, 40, 40, 2, 70, 80, 40, 20, 0, 0, 35, 10, 13},
    {"STAB", 0, 50, 50, 50, 0, 0, 50, VM_POLY, F_LP12, 70, 20, 35, 0, 0, 20, 50, 0, 25, 0, 15, 0, 10, 0, 30, 30, 4},
    {"LO-FI", 0, 50, 50, 50, 0, 0, 30, VM_POLY, F_LP12, 60, 10, 0, 20, 0, 30, 40, 1, 70, 80, 35, 30, 10, 45, 30, 15, 13},
};

inline const Preset* presetList(int t, int& n) {
    switch (t) {
        case T_PAD: n = int(sizeof kPadPresets / sizeof *kPadPresets); return kPadPresets;
        case T_BASS: n = int(sizeof kBassPresets / sizeof *kBassPresets); return kBassPresets;
        case T_LEAD: n = int(sizeof kLeadPresets / sizeof *kLeadPresets); return kLeadPresets;
        case T_SAMPLE: n = int(sizeof kSamplePresets / sizeof *kSamplePresets); return kSamplePresets;
        default: n = 0; return nullptr;
    }
}

inline void applyPreset(Track& tr, const Preset& p) {
    Synth& s = tr.syn;
    s.engine = p.engine; s.timbre = p.timbre; s.harm = p.harm; s.morph = p.morph; s.sub = p.sub;
    s.glide = p.glide; s.accent = p.accent; s.mode = p.mode;
    s.ftype = p.ftype; s.res = p.res; s.fenv = p.fenv; s.drive = p.drive; s.fatk = p.fatk; s.fdec = p.fdec; s.key = p.key;
    s.a = p.a; s.d = p.d; s.s = p.s; s.r = p.r; s.chorus = p.chorus; s.dist = p.dist; s.lofi = p.lofi;
    tr.cutoff = p.cutoff; tr.rev = p.rev; tr.dly = p.dly; tr.decay = p.len;
}
