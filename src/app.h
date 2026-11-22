// Bedienung und Bildschirm-Seiten (GENRA-Look).
#pragma once
#include "project.h"
#include "engine.h"
#include "gfx.h"
#include "midi.h"
#include <string>
#include <vector>
#include <functional>

enum Btn { B_UP, B_DOWN, B_LEFT, B_RIGHT, B_A, B_B, B_X, B_Y, B_L1, B_R1, B_L2, B_R2, B_SELECT, B_START, B_L3, B_R3, B_COUNT };

struct Input {
    bool held[B_COUNT] = {};
    float lx = 0, ly = 0, rx = 0, ry = 0;  // -1..1
};

enum Page { PG_SEQ = 0, PG_SONG, PG_MIX, PG_FX, PG_SYN, PG_COUNT };

// Ein Regler oder Auswahlfeld auf den Seiten SYN und FX
struct Ctl {
    float x, y;                 // Mitte
    float r;                    // Radius (Regler) bzw. halbe Breite (Feld)
    bool drop;                  // Auswahlfeld statt Regler
    const char* label;
    uint8_t* u8 = nullptr;
    int8_t* i8 = nullptr;
    int lo = 0, hi = 100;
    uint32_t col = 0;
    std::function<std::string(int)> fmt;
    int get() const { return u8 ? *u8 : i8 ? *i8 : 0; }
    void set(int v) const { if (u8) *u8 = uint8_t(v); else if (i8) *i8 = int8_t(v); }
};

struct UserPreset {
    char name[16];
    Track tr;
};

class App {
public:
    Project prj;
    Engine eng;
    MidiIn midi;
    std::string dataDir = ".";
    bool quit = false;
    int samplesLoaded = 0;

    void init(const std::string& dir, float sampleRate);
    // Muss bei gesperrtem Audio aufgerufen werden
    void update(const Input& in, double dt);
    void draw(gfx::Canvas& c);
    void save();
    bool load();

private:
    Input prev;
    double now = 0;
    bool pressed[B_COUNT] = {}, released[B_COUNT] = {};
    double heldSince[B_COUNT] = {}, nextRepeat[B_COUNT] = {};
    bool rep[B_COUNT] = {};
    int page = PG_SEQ;
    bool menuOpen = false, midiTest = false;
    int menuSel = 0;
    int curT = 0, curS = 0, lockSel = 1;
    int synT = T_BASS, synSel = 0, fxSel = 0, mixSel = 0, songPad = 0, songSel = 0;
    bool synGroove = false, songFocusList = true;
    int clipPat = -1;
    bool selUsed = false, bUsed = false, r3Used = false;
    double lastSelTap = -10;
    double stickAcc = 0, stickAccR = 0;
    bool rec = false;
    Pattern undoPat;
    bool hasUndo = false;
    uint8_t undoBpm = 0, undoSwing = 0;
    Track undoTrack;
    int undoTrackIdx = -1;
    std::string toast;
    double toastUntil = 0;
    bool dirty = false;
    double lastSave = 0;
    bool confirmNew = false;
    uint32_t seed = 1;
    uint8_t lastNote[NT] = {0, 0, 0, 0, 57, 45, 69, 57};
    std::vector<UserPreset> userPresets[NT];
    std::vector<MidiEvent> midiEv;
    // Krabbo
    int krMode = -1;
    double krStart = 0, krNext = 25;
    float krY = 0;

    void msg(const std::string& s) { toast = s; toastUntil = now + 1.8; }
    void handleSeq(const Input& in, double dt);
    void handleSong(const Input& in);
    void handleMix(const Input& in, double dt);
    void handleCtls(const Input& in, double dt, std::vector<Ctl>& cs, int& sel, bool needB);
    void handleFx(const Input& in, double dt);
    void handleSyn(const Input& in, double dt);
    void handleMenu(const Input& in);
    void handleMidi();
    void livePerf(const Input& in);
    void newAll();
    void newTrack();
    void doUndo();
    void storeUndo();
    void changeLock(int delta);
    void rotateTrack(int dir);
    void switchVariant(int dir);
    void loadPreset(int t, int idx);
    int presetCount(int t) const;
    std::string presetName(int t, int idx) const;
    void saveUserPreset(int t);
    void loadUserPresets();
    void writeUserPresets();
    void randomizeTrack(int t);
    void krabboTick();

    void buildFxCtls(std::vector<Ctl>& cs);
    void buildSynCtls(std::vector<Ctl>& cs);

    void drawHeader(gfx::Canvas& c, const std::string& sub);
    void drawFooter(gfx::Canvas& c, const std::string& l1, const std::string& l2);
    void drawCtl(gfx::Canvas& c, const Ctl& k, bool focus);
    void drawSeq(gfx::Canvas& c);
    void drawSong(gfx::Canvas& c);
    void drawMix(gfx::Canvas& c);
    void drawFx(gfx::Canvas& c);
    void drawSyn(gfx::Canvas& c);
    void drawMenu(gfx::Canvas& c);
    void drawMidiTest(gfx::Canvas& c);
    void drawKrabbo(gfx::Canvas& c);
};

void drawKrabboAt(gfx::Canvas& c, float x, float y, float s, float t, float look);
std::string noteName(int n);
