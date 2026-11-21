// Einstieg fuer den R36 Ultra (und zum Testen am PC): SDL2 fuer Bild, Ton und Tasten.
#include "app.h"
#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <algorithm>

static App app;

static void audioCb(void*, Uint8* stream, int len) {
    app.eng.process(reinterpret_cast<float*>(stream), len / int(sizeof(float) * 2));
}

// Rohe Joystick-Tasten -> eigene Tasten (falls kein Gamecontroller-Mapping vorhanden ist)
struct Config {
    int rotate = 0;
    int raw[B_COUNT];
    bool useController = true;
    bool showRaw = false;
};

static const char* kBtnNames[B_COUNT] = {"hoch", "runter", "links", "rechts", "a", "b", "x", "y",
                                         "l1", "r1", "l2", "r2", "select", "start", "l3", "r3"};

static void writeDefaultConfig(const std::string& path) {
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return;
    fprintf(f,
            "# R36 GENRA Einstellungen\n"
            "# Bild drehen: 0, 90, 180 oder 270\n"
            "drehen=0\n"
            "# 1 = Tasten ueber SDL-Gamecontroller (empfohlen), 0 = rohe Tastennummern unten\n"
            "controller=1\n"
            "# 1 = gedrueckte rohe Tastennummern in log.txt schreiben (zum Einrichten)\n"
            "tasten_log=1\n"
            "# Rohe Tastennummern (nur wenn controller=0 oder kein Mapping gefunden)\n"
            "hoch=8\nrunter=9\nlinks=10\nrechts=11\n"
            "a=1\nb=0\nx=2\ny=3\n"
            "l1=4\nr1=5\nl2=6\nr2=7\n"
            "select=12\nstart=13\nl3=14\nr3=15\n");
    fclose(f);
}

static Config loadConfig(const std::string& dir) {
    Config c;
    int def[B_COUNT] = {8, 9, 10, 11, 1, 0, 2, 3, 4, 5, 6, 7, 12, 13, 14, 15};
    memcpy(c.raw, def, sizeof def);
    std::string path = dir + "/einstellungen.txt";
    std::ifstream in(path);
    if (!in) { writeDefaultConfig(path); in.open(path); }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        int v = atoi(line.c_str() + eq + 1);
        if (k == "drehen") c.rotate = v;
        else if (k == "controller") c.useController = v != 0;
        else if (k == "tasten_log") c.showRaw = v != 0;
        else for (int b = 0; b < B_COUNT; b++) if (k == kBtnNames[b]) c.raw[b] = v;
    }
    return c;
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    char* base = SDL_GetBasePath();
    std::string dir = base ? base : "./";
    SDL_free(base);
    if (!dir.empty() && dir.back() == '/') dir.pop_back();
    setvbuf(stdout, nullptr, _IOLBF, 0);
    printf("R36 GENRA v0.1, Ordner: %s\n", dir.c_str());

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    Config cfg = loadConfig(dir);
    SDL_GameControllerAddMappingsFromFile((dir + "/gamecontrollerdb.txt").c_str());
    if (const char* f = getenv("SDL_GAMECONTROLLERCONFIG_FILE")) SDL_GameControllerAddMappingsFromFile(f);

    SDL_DisplayMode dm;
    int sw = 720, sh = 720;
    if (SDL_GetDesktopDisplayMode(0, &dm) == 0) { sw = dm.w; sh = dm.h; }
    printf("Bildschirm: %dx%d\n", sw, sh);
    bool desktop = getenv("R36_WINDOW") != nullptr;
    SDL_Window* win = SDL_CreateWindow("R36 GENRA", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       desktop ? 720 : sw, desktop ? 720 : sh, desktop ? 0 : SDL_WINDOW_FULLSCREEN);
    if (!win) { printf("Fenster: %s\n", SDL_GetError()); return 1; }
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { printf("Renderer: %s\n", SDL_GetError()); return 1; }
    SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, gfx::W, gfx::H);
    SDL_ShowCursor(SDL_DISABLE);

    // Audio
    SDL_AudioSpec want{}, have{};
    want.freq = 44100;
    want.format = AUDIO_F32SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = audioCb;
    app.init(dir, 44100);
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!dev) printf("Audio: %s\n", SDL_GetError());
    else { printf("Audio: %d Hz, Puffer %d\n", have.freq, have.samples); SDL_PauseAudioDevice(dev, 0); }

    SDL_GameController* pad = nullptr;
    SDL_Joystick* joy = nullptr;
    auto openPads = [&]() {
        for (int i = 0; i < SDL_NumJoysticks(); i++) {
            printf("Joystick %d: %s (Controller-Mapping: %s)\n", i, SDL_JoystickNameForIndex(i), SDL_IsGameController(i) ? "ja" : "nein");
            if (!pad && cfg.useController && SDL_IsGameController(i)) pad = SDL_GameControllerOpen(i);
            else if (!joy && !pad) joy = SDL_JoystickOpen(i);
        }
    };
    openPads();

    Input in;
    bool keys[B_COUNT] = {};
    gfx::Canvas canvas;
    Uint32 last = SDL_GetTicks();
    bool running = true;
    while (running && !app.quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_JOYDEVICEADDED && !pad && !joy) openPads();
            if (e.type == SDL_JOYBUTTONDOWN && cfg.showRaw) printf("Taste roh: %d\n", e.jbutton.button);
            if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                bool d = e.type == SDL_KEYDOWN;
                switch (e.key.keysym.sym) {
                    case SDLK_UP: keys[B_UP] = d; break;
                    case SDLK_DOWN: keys[B_DOWN] = d; break;
                    case SDLK_LEFT: keys[B_LEFT] = d; break;
                    case SDLK_RIGHT: keys[B_RIGHT] = d; break;
                    case SDLK_x: keys[B_A] = d; break;
                    case SDLK_z: keys[B_B] = d; break;
                    case SDLK_s: keys[B_X] = d; break;
                    case SDLK_a: keys[B_Y] = d; break;
                    case SDLK_q: keys[B_L1] = d; break;
                    case SDLK_w: keys[B_R1] = d; break;
                    case SDLK_1: keys[B_L2] = d; break;
                    case SDLK_2: keys[B_R2] = d; break;
                    case SDLK_RSHIFT: case SDLK_BACKSPACE: keys[B_SELECT] = d; break;
                    case SDLK_RETURN: keys[B_START] = d; break;
                    case SDLK_ESCAPE: if (d) running = false; break;
                    default: break;
                }
            }
        }
        // Tasten zusammenstellen
        for (int b = 0; b < B_COUNT; b++) in.held[b] = keys[b];
        in.lx = in.ly = in.rx = in.ry = 0;
        if (pad) {
            // Positionsgetreu: SDL nutzt Xbox-Positionen, der R36 hat Nintendo-Beschriftung (A rechts, B unten)
            static const struct { int ours; SDL_GameControllerButton sdl; } map[] = {
                {B_UP, SDL_CONTROLLER_BUTTON_DPAD_UP}, {B_DOWN, SDL_CONTROLLER_BUTTON_DPAD_DOWN},
                {B_LEFT, SDL_CONTROLLER_BUTTON_DPAD_LEFT}, {B_RIGHT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT},
                {B_A, SDL_CONTROLLER_BUTTON_B}, {B_B, SDL_CONTROLLER_BUTTON_A},
                {B_X, SDL_CONTROLLER_BUTTON_Y}, {B_Y, SDL_CONTROLLER_BUTTON_X},
                {B_L1, SDL_CONTROLLER_BUTTON_LEFTSHOULDER}, {B_R1, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
                {B_SELECT, SDL_CONTROLLER_BUTTON_BACK}, {B_START, SDL_CONTROLLER_BUTTON_START},
                {B_L3, SDL_CONTROLLER_BUTTON_LEFTSTICK}, {B_R3, SDL_CONTROLLER_BUTTON_RIGHTSTICK},
            };
            for (auto& m : map) in.held[m.ours] |= SDL_GameControllerGetButton(pad, m.sdl) != 0;
            in.held[B_L2] |= SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 12000;
            in.held[B_R2] |= SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 12000;
            in.lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) / 32768.f;
            in.ly = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) / 32768.f;
            in.rx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX) / 32768.f;
            in.ry = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY) / 32768.f;
        } else if (joy) {
            int nb = SDL_JoystickNumButtons(joy);
            for (int b = 0; b < B_COUNT; b++)
                if (cfg.raw[b] >= 0 && cfg.raw[b] < nb) in.held[b] |= SDL_JoystickGetButton(joy, cfg.raw[b]) != 0;
            if (SDL_JoystickNumHats(joy) > 0) {
                Uint8 h = SDL_JoystickGetHat(joy, 0);
                in.held[B_UP] |= (h & SDL_HAT_UP) != 0;
                in.held[B_DOWN] |= (h & SDL_HAT_DOWN) != 0;
                in.held[B_LEFT] |= (h & SDL_HAT_LEFT) != 0;
                in.held[B_RIGHT] |= (h & SDL_HAT_RIGHT) != 0;
            }
            int na = SDL_JoystickNumAxes(joy);
            if (na >= 2) { in.lx = SDL_JoystickGetAxis(joy, 0) / 32768.f; in.ly = SDL_JoystickGetAxis(joy, 1) / 32768.f; }
            if (na >= 4) { in.rx = SDL_JoystickGetAxis(joy, 2) / 32768.f; in.ry = SDL_JoystickGetAxis(joy, 3) / 32768.f; }
        }
        Uint32 t = SDL_GetTicks();
        double dt = (t - last) / 1000.0;
        last = t;
        if (dev) SDL_LockAudioDevice(dev);
        app.update(in, dt);
        if (dev) SDL_UnlockAudioDevice(dev);

        app.draw(canvas);
        SDL_UpdateTexture(tex, nullptr, canvas.px.data(), gfx::W * 4);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        int ow, oh;
        SDL_GetRendererOutputSize(ren, &ow, &oh);
        bool side = cfg.rotate == 90 || cfg.rotate == 270;
        int vw = side ? oh : ow, vh = side ? ow : oh;
        int sz = std::min(vw, vh);
        SDL_Rect dst{(ow - sz) / 2, (oh - sz) / 2, sz, sz};
        SDL_RenderCopyEx(ren, tex, nullptr, &dst, cfg.rotate, nullptr, SDL_FLIP_NONE);
        SDL_RenderPresent(ren);
        Uint32 spent = SDL_GetTicks() - t;
        if (spent < 33) SDL_Delay(33 - spent);  // ca. 30 Bilder pro Sekunde
    }
    if (dev) SDL_LockAudioDevice(dev);
    app.save();
    if (dev) { SDL_UnlockAudioDevice(dev); SDL_CloseAudioDevice(dev); }
    printf("Beendet, gespeichert.\n");
    SDL_Quit();
    return 0;
}
