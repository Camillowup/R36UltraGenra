# R36 GENRA

A step sequencer and groovebox for the **R36 Ultra** handheld (RK3326, 720×720), started as a "Port"
from ArkOS / dArkOS / arkos4clone. Inspired by Koala Sampler and Polyend Play, with the GENRA generator.

**Status:** v0.1 – first test build.

- 8 tracks: KICK, CLAP, HAT, PERC (sample folders, per-step sample choice, random sample %) and
  PAD, BASS, LEAD, KEYS (simple built-in synths for now; Plaits engines planned)
- 16 patterns, 16–64 steps, parameter locks per step (pitch/note, decay/length, cutoff, reverb, delay)
- Generator with 10 genres: NEW ALL, NEW per track, UNDO
- Mixer (mute/solo/volume/pan), ping-pong delay, reverb, limiter
- Biomechanical "alien" look, mascot Krabbo (shows up rarely)

Build: GitHub Actions builds an ARM64 binary in an Ubuntu 18.04 container (glibc 2.27) and uploads
the `R36-GENRA-ports` artifact. Local PC test without screen: `make test`.

German user guide: [docs/ANLEITUNG.md](docs/ANLEITUNG.md)

License: GPL-3.0. Font: Fira Sans (SIL OFL 1.1, see `docs/FONT-OFL.txt`).
