#include "midi.h"
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <algorithm>

MidiIn::~MidiIn() {
    for (auto& d : devs) if (d.fd >= 0) close(d.fd);
}

void MidiIn::addLog(const std::string& s) {
    log.push_front(s);
    while (log.size() > 10) log.pop_back();
}

void MidiIn::scan() {
    DIR* dir = opendir("/dev/snd");
    if (!dir) return;
    while (dirent* e = readdir(dir)) {
        std::string n = e->d_name;
        if (n.compare(0, 5, "midiC") != 0) continue;
        std::string path = "/dev/snd/" + n;
        bool known = false;
        for (auto& d : devs) if (d.path == path) known = true;
        if (known) continue;
        int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        Dev d;
        d.path = path;
        d.fd = fd;
        int card = atoi(n.c_str() + 5);
        char p[64];
        snprintf(p, sizeof p, "/proc/asound/card%d/id", card);
        d.name = n;
        if (FILE* f = fopen(p, "r")) {
            char b[64] = {};
            if (fgets(b, sizeof b, f)) { b[strcspn(b, "\n")] = 0; d.name = b; }
            fclose(f);
        }
        printf("MIDI-Geraet: %s (%s)\n", d.name.c_str(), path.c_str());
        addLog("GERÄT: " + d.name);
        devs.push_back(d);
    }
    closedir(dir);
}

void MidiIn::poll(double now, std::vector<MidiEvent>& out) {
    if (now - lastScan > 2.0) { lastScan = now; scan(); }
    for (auto it = devs.begin(); it != devs.end();) {
        Dev& d = *it;
        uint8_t b[256];
        ssize_t r = read(d.fd, b, sizeof b);
        if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            addLog("GETRENNT: " + d.name);
            close(d.fd);
            it = devs.erase(it);
            continue;
        }
        for (ssize_t i = 0; i < r; i++) {
            uint8_t c = b[i];
            if (c >= 0xF8) continue;               // Echtzeit (Clock usw.)
            if (c & 0x80) {
                if (c >= 0xF0) { d.running = 0; d.need = 0; continue; }  // SysEx usw. ignorieren
                d.running = c;
                d.have = 0;
                uint8_t hi = c & 0xF0;
                d.need = (hi == 0xC0 || hi == 0xD0) ? 1 : 2;
                continue;
            }
            if (!d.running) continue;
            d.buf[d.have++] = c;
            if (d.have >= d.need) {
                out.push_back({d.running, d.buf[0], d.need > 1 ? d.buf[1] : uint8_t(0)});
                d.have = 0;
                eventCount++;
            }
        }
        ++it;
    }
}
