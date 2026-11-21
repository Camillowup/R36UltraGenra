// Audio: Sequencer, Drum-Sampler, einfache Synths, Delay und Hall.
// Alle Aenderungen am Projekt/Engine aus dem Hauptthread nur bei gesperrtem Audio (SDL_LockAudioDevice).
#pragma once
#include "project.h"
#include <vector>
#include <string>
#include <cstdint>

struct SampleBuf {
    std::string name;
    std::vector<float> d;  // mono
    float sr = 44100;
};

struct DrumVoice {
    const SampleBuf* buf = nullptr;
    double pos = 0, rate = 1;
    float env = 0, envMul = 1, gain = 0, panL = 0, panR = 0, sendR = 0, sendD = 0;
    float g = 1, k = 1.4f, ic1 = 0, ic2 = 0;  // SVF Tiefpass
    bool filt = false, active = false;
};

struct SynthVoice {
    bool active = false;
    int track = 0, note = 0;
    uint32_t age = 0;
    float freq = 100, target = 100, glideMul = 1;
    float ph[3] = {0, 0, 0};
    float env = 0, fenv = 0;
    int stage = 0;  // 0 Anschlag, 1 Abfall/Halten, 2 Ausklang
    int gate = 0;   // Samples bis Note-Aus
    float cutBase = 1000, envAmt = 0;
    float ic1 = 0, ic2 = 0;
    float vel = 1, panL = 0.7f, panR = 0.7f, sendR = 0, sendD = 0;
    float t = 0;    // Zeit seit Anschlag
    const SampleBuf* smp = nullptr;
    double spos = 0, srate = 1;
};

class Engine {
public:
    Project* prj = nullptr;
    std::vector<SampleBuf> drums[NDRUM];   // je Spur: eingebaute + geladene Samples
    std::vector<SampleBuf> keys;           // Keygroup-Samples (leer = eingebautes E-Piano)
    int builtinCount[NDRUM] = {};

    // Anzeige
    volatile bool playing = false;
    volatile int playStep = -1;
    int queuedPat = -1;
    float meter[NT] = {}, meterL = 0, meterR = 0;

    void init(Project* p, float sampleRate);
    int loadSamples(const std::string& baseDir);  // gibt Anzahl geladener WAVs zurueck
    void process(float* out, int frames);         // Stereo, verschraenkt
    void start();
    void stop();
    void preview(int track, const Step& st);      // Step sofort anspielen
    void allNotesOff();
    const SampleBuf* drumSample(int track, int idx) const;

private:
    float sr = 44100;
    double toNext = 0;
    int step = 0;
    uint32_t rng = 12345, ageCounter = 0;
    DrumVoice dv[NDRUM];
    SynthVoice sv[16];
    std::vector<float> dlyL, dlyR;
    int dlyPos = 0;
    // Freeverb
    struct Comb { std::vector<float> b; int i = 0; float store = 0; };
    struct AP { std::vector<float> b; int i = 0; };
    Comb combL[8], combR[8];
    AP apL[4], apR[4];
    float hpL = 0, hpR = 0, limGain = 1;

    float rnd();
    void trigger(int track, const Step& st, int stepDurSamples);
    void triggerDrum(int track, const Step& st);
    void triggerSynth(int track, int note, const Step& st, int gateSamples);
    void buildBuiltins();
    void advance();
    double stepLen(int s) const;
};
