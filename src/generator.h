// Pattern-Generator (wie bei GENRA): NEW ALL / NEW pro Spur, 10 Genres.
#pragma once
#include "project.h"
#include <cstdint>

constexpr int NGENRE = 10;
extern const char* const kGenreNames[NGENRE];

// Erzeugt Noten fuer eine Spur (track >= 0) oder alle Spuren (track = -1) im aktuellen Pattern.
// Bei allen Spuren werden auch Tempo, Swing und Spur-Grundwerte passend zum Genre gesetzt.
void generate(Project& p, int track, uint32_t seed);
void initProject(Project& p);
