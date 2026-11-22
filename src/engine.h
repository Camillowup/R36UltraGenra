// Audio v0.2: Sequencer (Groove, Ratchets, Varianten, Song), Drum-Sampler, Plaits-Synths, SAMPLE-Spur,
// Spur-Effekte, Delay/Hall-Modelle, Ducking, Live-Effekte (Filter, Stutter/Freeze).
// Alle Aenderungen aus dem Hauptthread nur bei gesperrtem Audio (SDL_LockAudioDevice).
#pragma once
#include "project.h"
#include <vector>
#include <string>
#include <cstdint>

namespace plaits { class Voice; }

struct SampleBuf {
    std::string name;
    std::vector<float> d;  // mono
    float sr = 48000;
};

struct DrumVoice {
    const SampleBuf* buf = nullptr;
    double pos = 0, rate = 1;
    float env = 0, envMul = 1, gain = 0;
    float g = 1, k = 1.4f, ic1 = 0, ic2 = 0;
    bool filt = false, active = false;
};

struct InstVoice {
    bool active = false;
    int track = 0, note = 0;
    uint32_t age = 0;
    float curNote = 48, target = 48, glideCoef = 1;
    int gate = 0;            // Samples bis Note-Aus (-1 = haelt bis liveOff)
    bool retrig = false;
    int stage = 0;           // 0 Anschlag, 1 Abfall/Halten, 2 Ausklang
    float env = 0, attInc = 1, decMul = 1, sus = 1, relMul = 1;
    float fenv = 0, fAttInc = 1, fDecMul = 1;
    int fstage = 0;
    float vel = 1, accent = 0, cents = 0;
    int cutLock = -1;
    // Filter
    float s[4] = {0, 0, 0, 0}, ic1 = 0, ic2 = 0, fg = 0.5f, fk = 0, cnt = 0;
    float subPh = 0;
    // Plaits
    plaits::Voice* pv = nullptr;
    char* pbuf = nullptr;
    bool plaitsTrig = false;
    // SAMPLE-Spur
    const SampleBuf* smp = nullptr;
    double spos = 0, srate = 1;
    float ph[3] = {0, 0, 0}, t = 0;  // eingebautes E-Piano
};

struct Pending {
    int track;
    Step st;
    int delay;     // Samples
    float cents;
    bool drumOnly;
};

class Engine {
public:
    Project* prj = nullptr;
    std::vector<SampleBuf> drums[NDRUM];
    std::vector<SampleBuf> keys;           // SAMPLE-Spur (leer = eingebautes E-Piano)
    int builtinCount[NDRUM] = {};

    // Anzeige / Steuerung
    volatile bool playing = false;
    volatile int playStep = -1;            // globaler Step
    volatile int playStepT[NT] = {-1, -1, -1, -1, -1, -1, -1, -1};
    int queuedPat = -1;
    bool songMode = false;
    int songRow = 0, songBarStep = 0;
    float meter[NT] = {}, meterL = 0, meterR = 0;
    float cpuLoad = 0;                     // Anteil der Audiozeit (0..1)
    // Live-Effekte (von der Oberflaeche gesetzt)
    float perfFilter = 0;                  // -1 Tiefpass … 0 aus … +1 Hochpass
    int perfStutter = 0;                   // 0 aus, 1 1/4, 2 1/8, 3 1/16, 4 Freeze
    bool trigFlash[NT] = {};

    Engine();
    ~Engine();
    void init(Project* p, float sampleRate);
    int loadSamples(const std::string& baseDir);
    void process(float* out, int frames);  // Stereo, verschraenkt
    void start();
    void startSong(int row);
    void stop();
    void preview(int track, const Step& st);
    void liveOn(int track, int note, int vel);
    void liveOff(int track, int note);
    void allNotesOff();
    void resetPositions();
    const SampleBuf* drumSample(int track, int idx) const;
    float sampleRate() const { return sr; }
    float stepPhase() const;              // 0..1 innerhalb des aktuellen Steps

private:
    float sr = 48000;
    double toNext = 0;
    int gstep = 0;                         // Step innerhalb des Patterns (global)
    int tstep[NT] = {};
    int loops[NT] = {};
    uint32_t rng = 12345, ageCounter = 0;
    DrumVoice dv[NDRUM];
    static constexpr int NPV = 8, NSV = 4;
    InstVoice iv[NPV + NSV];               // 0..7 Plaits, 8..11 SAMPLE
    std::vector<Pending> pend;
    // Groove-Zustand
    float drift[NT] = {}, driftP[NT] = {}, ph808[NT] = {};
    // Spur-Bus
    float bus[NT][24];
    float trRev[NT] = {}, trDly[NT] = {}, trRevT[NT] = {}, trDlyT[NT] = {};
    std::vector<float> chBuf[NT];
    int chPos = 0;
    float chPh = 0;
    float lofiHold[NT] = {}, lofiCnt[NT] = {};
    float duckEnv = 0;
    // Delay
    std::vector<float> dlyL, dlyR;
    int dlyPos = 0;
    float dlyLp = 0, dlyLpR = 0, wowPh = 0;
    // Hall
    struct Comb { std::vector<float> b; int i = 0; float store = 0; };
    struct AP { std::vector<float> b; int i = 0; };
    Comb combL[8], combR[8];
    AP apL[4], apR[4];
    std::vector<float> preBuf;
    int prePos = 0;
    float limGain = 1;
    // Live-Effekte
    std::vector<float> ringL, ringR;
    int ringPos = 0, stutMode = 0, stutStart = 0, stutLen = 1, stutI = 0;
    float stutMix = 0;
    float pfL[2] = {0, 0}, pfR[2] = {0, 0}, pfSm = 0;

    float rnd();
    double stepLen(int s) const;
    void advance(int offset);
    void schedule(int track, const Step& st, int delay, int s);
    void fire(const Pending& p);
    void triggerDrum(int track, const Step& st, float cents);
    void triggerInst(int track, const Step& st, float cents, int gateSamples, int noteOverride = -1);
    void noteOn(int track, int note, const Step& st, float cents, int gate);
    void buildBuiltins();
    void renderVoice(InstVoice& v, float* dst, int n);
};
