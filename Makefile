# R36 GENRA – Build fuer den R36 Ultra (ARM64) oder zum Testen am PC
CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall
SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null)

CORE := src/gfx.cpp src/engine.cpp src/generator.cpp src/app.cpp

r36genra: $(CORE) src/main_sdl.cpp src/*.h
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $(CORE) src/main_sdl.cpp -o $@ $(SDL_LIBS) -static-libstdc++ -static-libgcc -lm

# Test ohne Bildschirm: Seiten als Bilder, Audio als WAV
test: $(CORE) src/main_test.cpp src/*.h
	$(CXX) $(CXXFLAGS) $(CORE) src/main_test.cpp -o r36genra_test
	mkdir -p testausgabe && ./r36genra_test testausgabe

clean:
	rm -f r36genra r36genra_test

.PHONY: test clean
