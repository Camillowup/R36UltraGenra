# Anleitung für Claude

**Lies zuerst Project-2501 (Masterbrain):** Repo `Camillowup/Project-2501` – `REGELN.md`, `PROJEKTE.md`,
`projekte/r36-sequencer.md`. Die Regeln dort gelten hier (Deutsch antworten, erst Skizze, erst auf „bauen“,
kein Release ohne „Release“, wenige Pushes, Doku-Commits mit `[skip ci]`).

Projektstand und Aufbau: [PROJEKT.md](PROJEKT.md). Bedienung: [docs/ANLEITUNG.md](docs/ANLEITUNG.md).

- Oberflächen-Texte auf dem Gerät: Deutsch (eigenes Gerät, nicht veröffentlicht). README: Englisch.
- Look: Design-Artifact „R36 Sequencer Screens“ (Alien-Look), Farben in `src/app.cpp` oben.
- Schrift neu erzeugen: `python3 tools/makefont.py <FiraSans-SemiBold.ttf> <FiraSans-Bold.ttf>`.
- Vor jedem Push lokal `make test` und die Bilder in `testausgabe/` ansehen.
