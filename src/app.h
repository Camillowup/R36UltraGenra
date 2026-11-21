// Bedienung und Bildschirm-Seiten.
#pragma once
#include "project.h"
#include "engine.h"
#include "gfx.h"
#include <string>

enum Btn { B_UP, B_DOWN, B_LEFT, B_RIGHT, B_A, B_B, B_X, B_Y, B_L1, B_R1, B_L2, B_R2, B_SELECT, B_START, B_L3, B_R3, B_COUNT };

struct Input {
    bool held[B_COUNT] = {};
    float lx = 0, ly = 0, rx = 0, ry = 0;  // -1..1
};

enum Page { PG_SEQ = 0, PG_SONG, PG_MIX, PG_FX, PG_SYN, PG_COUNT };

class App {
public:
    Project prj;
    Engine eng;
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
    bool rep[B_COUNT] = {};      // gedrueckt oder Wiederholung
    int page = PG_SEQ;
    bool menuOpen = false;
    int menuSel = 0;
    int curT = 0, curS = 0, lockSel = 1;
    int synT = 0, synSel = 0, fxSel = 0, mixSel = 0, songSel = 0;
    int clipPat = -1;
    bool selUsed = false;        // Select als Umschalttaste benutzt
    double lastSelTap = -10;
    bool bUsed = false;
    double stickAcc = 0, stickAccR = 0;
    Pattern undoPat;
    bool hasUndo = false;
    uint8_t undoBpm = 0, undoSwing = 0;
    std::string toast;
    double toastUntil = 0;
    bool dirty = false;
    double lastSave = 0;
    bool confirmNew = false;
    uint32_t seed = 1;
    uint8_t lastNote[NT] = {0, 0, 0, 0, 57, 45, 69, 57};
    // Krabbo
    int krMode = -1;
    double krStart = 0, krNext = 25;
    float krY = 0;
    gfx::Canvas bg;
    bool bgReady = false;

    void msg(const std::string& s) { toast = s; toastUntil = now + 1.6; }
    void handleSeq(const Input& in, double dt);
    void handleSong(const Input& in);
    void handleMix(const Input& in, double dt);
    void handleFx(const Input& in);
    void handleSyn(const Input& in);
    void handleMenu(const Input& in);
    void newAll();
    void newTrack();
    void doUndo();
    void storeUndo();
    void changeLock(int delta);
    void krabboTick(double dt);

    void buildBg();
    void drawHeader(gfx::Canvas& c, const char* title, const std::string& sub, bool tabs);
    void drawFooter(gfx::Canvas& c, const std::string& l, const std::string& r);
    void drawSeq(gfx::Canvas& c);
    void drawSong(gfx::Canvas& c);
    void drawMix(gfx::Canvas& c);
    void drawFx(gfx::Canvas& c);
    void drawSyn(gfx::Canvas& c);
    void drawMenu(gfx::Canvas& c);
    void drawKrabbo(gfx::Canvas& c);
    void drawEye(gfx::Canvas& c, float x, float y);
};

void drawKrabboAt(gfx::Canvas& c, float x, float y, float s, float t, float look);
