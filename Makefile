# Engine core, CLI renderer and tests. Plain clang++, no dependencies.
CXX ?= clang++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
CORE := src/core/spectrum.cpp src/core/table_bank.cpp src/core/envelope.cpp src/core/patch.cpp src/core/engine.cpp src/core/effects.cpp src/core/arp.cpp
MUTABLE := third_party/mutable/plaits/dsp/physical_modelling/modal_voice.cc third_party/mutable/plaits/dsp/physical_modelling/resonator.cc third_party/mutable/plaits/resources_selected.cc third_party/mutable/stmlib/dsp/units.cc third_party/mutable/stmlib/utils/random.cc
MODULAR := src/modular/graph.cpp src/modular/runtime.cpp src/modular/modules.cpp src/modular/sample.cpp src/modular/spectral.cpp src/modular/timing.cpp src/modular/sequencer.cpp src/modular/pattern.cpp src/modular/performance.cpp $(CORE) $(MUTABLE)
MODFLAGS := -DTEST -Ithird_party/mutable
MODHEADERS := $(wildcard src/modular/*.h) $(wildcard src/core/*.h) $(wildcard tests/*.h) $(wildcard third_party/mutable/plaits/dsp/oscillator/*.h) $(wildcard third_party/mutable/stmlib/dsp/*.h)
HEADERS := $(wildcard src/core/*.h) plugin/Source/TypingKeyboard.h $(wildcard tests/*.h)

all: build/partials-render build/test_core build/test_graph build/test_modular build/test_timing

build:
	mkdir -p build

build/partials-render: $(CORE) src/cli/render.cpp $(HEADERS) | build
	$(CXX) $(CXXFLAGS) -o $@ $(CORE) src/cli/render.cpp

build/test_core: $(CORE) tests/test_core.cpp $(HEADERS) | build
	$(CXX) $(CXXFLAGS) -o $@ $(CORE) tests/test_core.cpp

test: build/test_core build/partials-render build/test_graph build/test_modular build/test_timing
	./build/test_core
	./build/test_graph
	./build/test_modular
	./build/test_timing
	python3 -I tests/test_importer.py
	python3 -I tests/test_renderer.py

test-renderer: build/partials-render
	python3 -I tests/test_renderer.py

build/test_graph: src/modular/graph.cpp src/modular/graph.h tests/test_graph.cpp | build
	$(CXX) $(CXXFLAGS) -o $@ src/modular/graph.cpp tests/test_graph.cpp

test-graph: build/test_graph
	./build/test_graph

build/test_graph_asan: src/modular/graph.cpp src/modular/graph.h tests/test_graph.cpp | build
	$(CXX) -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -o $@ src/modular/graph.cpp tests/test_graph.cpp

asan-graph: build/test_graph_asan
	./build/test_graph_asan

build/test_modular: $(MODULAR) $(MODHEADERS) tests/test_modular.cpp tests/sample_checks.h | build
	$(CXX) $(CXXFLAGS) $(MODFLAGS) -o $@ $(MODULAR) tests/test_modular.cpp

test-modular: build/test_modular
	./build/test_modular

build/test_modular_asan: $(MODULAR) $(MODHEADERS) tests/test_modular.cpp tests/sample_checks.h | build
	$(CXX) -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer $(MODFLAGS) -o $@ $(MODULAR) tests/test_modular.cpp

asan-modular: build/test_modular_asan
	./build/test_modular_asan

build/test_timing: src/modular/timing.cpp src/modular/timing.h tests/test_timing.cpp | build
	$(CXX) $(CXXFLAGS) -o $@ src/modular/timing.cpp tests/test_timing.cpp

test-timing: build/test_timing
	./build/test_timing

build/test_timing_asan: src/modular/timing.cpp src/modular/timing.h tests/test_timing.cpp | build
	$(CXX) -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -o $@ src/modular/timing.cpp tests/test_timing.cpp

asan-timing: build/test_timing_asan
	./build/test_timing_asan

# Same tests under AddressSanitizer and UndefinedBehaviorSanitizer.
build/test_core_asan: $(CORE) tests/test_core.cpp $(HEADERS) | build
	$(CXX) -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -o $@ $(CORE) tests/test_core.cpp

asan: build/test_core_asan
	./build/test_core_asan

clean:
	rm -rf build

.PHONY: all test test-renderer test-graph test-modular test-timing asan-timing asan-modular asan-graph asan clean
