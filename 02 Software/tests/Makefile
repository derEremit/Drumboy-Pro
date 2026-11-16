# Host-side unit tests for hardware-independent firmware code.
#   make         build and run all tests (with address and undefined-behaviour sanitizers)

CXX      ?= g++
APP       = ../Drumboy-Pro-App-H723/Core/Library
CXXFLAGS  = -std=gnu++14 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -I$(APP)/Global
BUILD    ?= build

TESTS = test_fill test_routing test_beatlist test_euclid test_pitch test_preview test_songchain test_midi test_autosave test_beatnote test_undo test_crash test_laneview test_loadmeter test_filtercurve test_statusline test_sampleview test_dubmix test_dubview test_noteview test_pan test_pingpong

.PHONY: all clean
all: $(addprefix $(BUILD)/,$(TESTS))
	@for t in $^; do echo "== $$t"; ./$$t || exit 1; done

$(BUILD)/%: %.cpp $(wildcard $(APP)/Global/*.h)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -rf $(BUILD)
