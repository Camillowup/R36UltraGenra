# R36 GENRA – Build fuer den R36 Ultra (ARM64) oder zum Testen am PC
CXX ?= g++
OPT ?= -O2
CXXFLAGS ?= $(OPT) -std=c++17 -Wall -Wno-unused-local-typedefs
CXXFLAGS += -DTEST -Ithird_party/eurorack -Isrc
SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null)

CORE := src/gfx.cpp src/engine.cpp src/generator.cpp src/app.cpp src/midi.cpp
PLAITS := $(shell find third_party/eurorack/plaits -name '*.cc') \
          third_party/eurorack/stmlib/dsp/units.cc third_party/eurorack/stmlib/dsp/atan.cc \
          third_party/eurorack/stmlib/utils/random.cc
OBJ := $(patsubst %.cpp,build/%.o,$(CORE)) $(patsubst %.cc,build/%.o,$(PLAITS))

build/%.o: %.cpp src/*.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -c $< -o $@

build/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -w -c $< -o $@

r36genra: $(OBJ) build/src/main_sdl.o
	$(CXX) $(OBJ) build/src/main_sdl.o -o $@ $(SDL_LIBS) -static-libstdc++ -static-libgcc -lm

build/src/main_sdl.o: src/main_sdl.cpp src/*.h
	@mkdir -p build/src
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) -c $< -o $@

# Test ohne Bildschirm: Seiten als Bilder, Audio als WAV
r36genra_test: $(OBJ) build/src/main_test.o
	$(CXX) $(OBJ) build/src/main_test.o -o $@ -lm

build/src/main_test.o: src/main_test.cpp src/*.h
	@mkdir -p build/src
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: r36genra_test
	mkdir -p testausgabe && ./r36genra_test testausgabe

clean:
	rm -rf build r36genra r36genra_test

.PHONY: test clean
