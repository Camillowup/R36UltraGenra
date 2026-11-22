// Datenmodell: Projekt, Patterns, Spuren, Steps.
#pragma once
// Altes Dateiformat (v0.1), nur zum Einlesen alter projekt.r36-Dateien.
#include <cstdint>
#include <cstring>

namespace v1 {
constexpr int NT = 8;          // Spuren: 0-3 Drums, 4-7 Instrumente
constexpr int NDRUM = 4;
constexpr int MAXSTEPS = 64;
constexpr int NPAT = 16;
constexpr int NPARAM = 5;      // PITCH/NOTE, DECAY/LAENGE, CUTOFF, REVERB, DELAY

enum Param { P_PITCH = 0, P_DECAY, P_CUTOFF, P_REVERB, P_DELAY };
enum TrackId { T_KICK = 0, T_CLAP, T_HAT, T_PERC, T_PAD, T_BASS, T_LEAD, T_KEYS };

struct Step {
    uint8_t on = 0;
    int8_t sample = -1;      // -1 = Sample der Spur
    uint8_t note = 48;       // MIDI-Note (Instrumente)
    uint8_t lockMask = 0;    // Bit je Param: Wert ist gelockt
    int8_t pitch = 0;        // Halbtoene (Drums)
    uint8_t decay = 50, cutoff = 100, rev = 0, dly = 0;
    uint8_t vel = 100;
    uint8_t chord = 0;       // Pad/Keys: 1 = Dreiklang
    uint8_t reserved = 0;
};

struct Track {
    uint8_t vol = 80, pan = 50, mute = 0, solo = 0;
    int8_t sample = 0;       // Standard-Sample aus dem Ordner
    int8_t pitch = 0;
    uint8_t decay = 50, cutoff = 100, rev = 0, dly = 0;
    uint8_t randPct = 0;     // Zufalls-Sample in Prozent
    uint8_t reserved[5] = {};
};

struct Pattern {
    uint8_t len = 16;
    uint8_t reserved[3] = {};
    Step s[NT][MAXSTEPS];
};

struct Project {
    char magic[8] = {'R', '3', '6', 'G', 'E', 'N', '0', '1'};
    uint16_t bpm = 120;
    uint8_t swing = 50;
    uint8_t genre = 3;
    uint8_t curPat = 0;
    uint8_t root = 9;        // A
    uint8_t scale = 0;       // 0 Moll, 1 Dur, 2 Dorisch, 3 Moll-Pentatonik
    uint8_t krabbo = 1;      // 0 aus, 1 selten, 2 oefter
    uint8_t dlyTime = 1, dlyFb = 45, dlyLvl = 70;
    uint8_t revSize = 70, revDamp = 50, revLvl = 70;
    uint8_t master = 80, limiter = 1;
    uint8_t reserved[16] = {};
    Track tr[NT];
    Pattern pat[NPAT];
};

}  // namespace v1
