# R36 GENRA

A groovebox for the **R36 Ultra** handheld (RK3326, 720×720), started as a "Port" from ArkOS / dArkOS / arkos4clone.
Inspired by Koala Sampler, Polyend Play and Sonicware deconstruct MINIMAL, with the GENRA generator and the GENRA
VCV look.

**Status:** v0.2 – test build.

- 8 tracks: KICK, CLAP, HAT, PERC (sample folders, per-step sample, random sample %), PAD, BASS, LEAD
  (Mutable Instruments Plaits engines, filter, envelopes, presets) and SAMPLE (keygroup)
- 16 patterns with variations A–D per track, 16–64 steps, per-track length, parameter locks incl. ratchets
- Groove models per track (808 / 909 / MINIMAL drift), per-track swing and velocity randomness, phrase rotation
- Generator with 10 genres (NEW ALL, NEW per track, UNDO), song list with parts
- Mixer, ping-pong/tape/mono delay, chamber/hall/plate reverb, kick ducking, limiter
- Live effects on the analog sticks (filter sweep, stutter/freeze), USB-MIDI input (pads from C1)

Build: GitHub Actions builds an ARM64 binary in an Ubuntu 18.04 container (glibc 2.27) and uploads the
`R36-GENRA-ports` artifact. Local test without screen: `make test`.

German user guide: [docs/ANLEITUNG.md](docs/ANLEITUNG.md)

License: GPL-3.0. Plaits and stmlib by Emilie Gillet (MIT, `third_party/eurorack`). Font: Montserrat (SIL OFL 1.1).
