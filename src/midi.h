// USB-MIDI-Eingang ueber die rohen ALSA-Geraete (/dev/snd/midiC*D*), ohne Zusatzbibliothek.
#pragma once
#include <string>
#include <vector>
#include <deque>
#include <cstdint>

struct MidiEvent { uint8_t status, d1, d2; };

class MidiIn {
public:
    struct Dev { std::string path, name; int fd = -1; uint8_t running = 0; int need = 0, have = 0; uint8_t buf[3] = {}; };
    std::vector<Dev> devs;
    std::deque<std::string> log;   // letzte Ereignisse fuer die Testanzeige
    int eventCount = 0;

    ~MidiIn();
    void poll(double now, std::vector<MidiEvent>& out);  // alle ~2 s nach neuen Geraeten suchen
    void addLog(const std::string& s);

private:
    double lastScan = -10;
    void scan();
};
