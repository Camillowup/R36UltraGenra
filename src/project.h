// Datenmodell v0.2: Projekt, Patterns mit Varianten A–D, Spuren mit Synth- und Groove-Werten, Song-Liste.
#pragma once
#include <cstdint>
#include <cstring>

constexpr int NT = 8;          // Spuren: 0-3 Drums, 4-6 Plaits-Synths, 7 SAMPLE (Keygroup)
constexpr int NDRUM = 4;
constexpr int MAXSTEPS = 64;
constexpr int NPAT = 16;
constexpr int NVAR = 4;        // Varianten A–D pro Spur
constexpr int NPARAM = 6;      // PITCH/NOTE, DECAY/LÄNGE, CUTOFF, REVERB, DELAY, REPEAT
constexpr int NSONG = 64;
constexpr int NPART = 7;       // Song-Teile (6 = ohne)

enum Param { P_PITCH = 0, P_DECAY, P_CUTOFF, P_REVERB, P_DELAY, P_REPEAT };
enum TrackId { T_KICK = 0, T_CLAP, T_HAT, T_PERC, T_PAD, T_BASS, T_LEAD, T_SAMPLE };
enum Groove { G_AUS = 0, G_808, G_909, G_MINIMAL };
enum VoiceMode { VM_MONO = 0, VM_LEGATO, VM_POLY };
enum FilterType { F_LP24 = 0, F_LP12, F_BP, F_HP };

struct Step {
    uint8_t on = 0;
    int8_t sample = -1;      // -1 = Sample der Spur
    uint8_t note = 48;       // MIDI-Note (36 = C1)
    uint8_t lockMask = 0;    // Bit je Param: Wert ist gelockt
    int8_t pitch = 0;        // Halbtoene (Drums)
    uint8_t decay = 50, cutoff = 100, rev = 0, dly = 0;
    uint8_t vel = 100;
    uint8_t chord = 0;       // Pad/Sample: 1 = Dreiklang
    uint8_t repeat = 1;      // Ratchets 1..4
};

struct Synth {
    uint8_t engine = 8;      // Plaits-Engine (8 = Virtual Analog)
    uint8_t timbre = 50, harm = 50, morph = 50, sub = 0, glide = 0, accent = 40, mode = VM_POLY;
    uint8_t ftype = F_LP24, res = 20, fenv = 30, drive = 0, fatk = 0, fdec = 30, key = 50;
    uint8_t a = 1, d = 40, s = 70, r = 25;
    uint8_t chorus = 0, dist = 0, lofi = 0;
    uint8_t reserved[9] = {};
};

struct Track {
    uint8_t vol = 80, pan = 50, mute = 0, solo = 0;
    int8_t sample = 0;       // Standard-Sample aus dem Ordner
    int8_t pitch = 0;
    uint8_t decay = 50, cutoff = 100, rev = 0, dly = 0;
    uint8_t randPct = 0;     // Zufalls-Sample in Prozent
    uint8_t groove = G_AUS, swing = 50, veloRnd = 0;
    uint8_t len = 0;         // eigene Laenge in Steps (0 = Pattern-Laenge)
    uint8_t preset = 0;
    Synth syn;
    uint8_t reserved[8] = {};
};

struct Pattern {
    uint8_t len = 16;
    uint8_t var[NT] = {};    // aktive Variante je Spur
    uint8_t reserved[7] = {};
    Step s[NT][NVAR][MAXSTEPS];
};

struct SongRow {
    uint8_t pat = 0, part = 6, bars = 4, reserved = 0;
};

struct Project {
    char magic[8] = {'R', '3', '6', 'G', 'E', 'N', '0', '2'};
    uint16_t bpm = 120;
    uint8_t swing = 50;
    uint8_t genre = 3;
    uint8_t curPat = 0;
    uint8_t root = 9;        // A
    uint8_t scale = 0;       // 0 Moll, 1 Dur, 2 Dorisch, 3 Moll-Pentatonik
    uint8_t krabbo = 1;      // 0 aus, 1 selten, 2 oefter
    uint8_t dlyTime = 2, dlyFb = 45, dlyLvl = 70;
    uint8_t revSize = 70, revDamp = 50, revLvl = 70;
    uint8_t master = 80, limiter = 1;
    uint8_t dlyModel = 0, revModel = 0, revPre = 1, dlyOn = 1, revOn = 1;
    uint8_t duck = 0, duckDepth = 50;
    uint8_t songLen = 0, songLoop = 1;
    uint8_t reserved[32] = {};
    Track tr[NT];
    SongRow song[NSONG];
    Pattern pat[NPAT];
};

static const char* const kTrackNames[NT] = {"KICK", "CLAP", "HAT", "PERC", "PAD", "BASS", "LEAD", "SAMPLE"};
static const char* const kParamNamesDrum[NPARAM] = {"PITCH", "DECAY", "CUTOFF", "REVERB", "DELAY", "REPEAT"};
static const char* const kParamNamesInst[NPARAM] = {"NOTE", "LÄNGE", "CUTOFF", "REVERB", "DELAY", "REPEAT"};
static const char* const kScaleNames[4] = {"MOLL", "DUR", "DORISCH", "PENTA"};
static const char* const kNoteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "H"};
static const char* const kGrooveNames[4] = {"AUS", "808", "909", "MINIMAL"};
static const char* const kPartNames[NPART] = {"INTRO", "VERSE", "BUILD", "DROP", "BREAK", "OUTRO", "PATTERN"};
static const uint32_t kPartColors[NPART] = {0x6f86c9, 0x9b7bdc, 0xe0a72a, 0xf0564d, 0x3aaf9c, 0x8a8d99, 0x5a5d68};

inline bool isSynthTrack(int t) { return t >= T_PAD && t <= T_LEAD; }

inline const int* scaleSteps(int scale, int& n) {
    static const int sc[4][8] = {{0, 2, 3, 5, 7, 8, 10}, {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 9, 10}, {0, 3, 5, 7, 10}};
    static const int cnt[4] = {7, 7, 7, 5};
    n = cnt[scale & 3];
    return sc[scale & 3];
}

// Wert eines Step-Parameters: gelockt oder Spur-Standard
inline int stepParam(const Step& st, const Track& t, int p) {
    bool lk = st.lockMask & (1 << p);
    switch (p) {
        case P_PITCH: return lk ? st.pitch : t.pitch;
        case P_DECAY: return lk ? st.decay : t.decay;
        case P_CUTOFF: return lk ? st.cutoff : t.cutoff;
        case P_REVERB: return lk ? st.rev : t.rev;
        case P_DELAY: return lk ? st.dly : t.dly;
        default: return lk ? (st.repeat < 1 ? 1 : st.repeat) : 1;
    }
}

inline int trackLen(const Project& p, const Pattern& pat, int t) {
    int l = p.tr[t].len ? p.tr[t].len : pat.len;
    return l < 1 ? 1 : (l > MAXSTEPS ? MAXSTEPS : l);
}
