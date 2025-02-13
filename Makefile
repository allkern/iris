.PHONY: clean

EXEC := eegs

INCLUDE_DIRS := imgui imgui/backends frontend gl3w/include tomlplusplus/include
OUTPUT_DIR := bin

CXX := g++
CXXFLAGS := $(addprefix -I, $(INCLUDE_DIRS)) -iquote src $(shell sdl2-config --cflags --libs)
CXXFLAGS += -O3 -march=native -mtune=native -flto=auto -Wall -Werror -g -D_EE_USE_INTRINSICS
CXXSRC := $(wildcard imgui/*.cpp)
CXXSRC += $(wildcard imgui/backends/imgui_impl_sdl2.cpp)
CXXSRC += $(wildcard imgui/backends/imgui_impl_opengl3.cpp)
CXXSRC += $(wildcard frontend/*.cpp)
CXXSRC += $(wildcard frontend/ui/*.cpp)
CXXSRC += $(wildcard src/ee/renderer/*.cpp)
CXXOBJ := $(CXXSRC:.cpp=.o)

CC := gcc
CFLAGS := $(addprefix -I, $(INCLUDE_DIRS)) -iquote src $(shell sdl2-config --cflags --libs)
CFLAGS += -O3 -ffast-math -march=native -mtune=native -Werror -pedantic
CFLAGS += -flto=auto -Wall -mssse3 -msse4 -D_EE_USE_INTRINSICS
CSRC := $(wildcard src/*.c)
CSRC += $(wildcard src/ee/*.c)
CSRC += $(wildcard src/iop/*.c)
CSRC += $(wildcard src/shared/*.c)
CSRC += $(wildcard src/dev/*.c)
CSRC += $(wildcard gl3w/src/*.c)
CSRC += $(wildcard frontend/tfd/*.c)
COBJ := $(CSRC:.c=.o)

all: $(OUTPUT_DIR) $(COBJ) $(CXXOBJ) $(OUTPUT_DIR)/$(EXEC)

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

$(COBJ): %.o: %.c
	@echo " CC " $<; $(CC) -c $< -o $@ $(CFLAGS)

$(CXXOBJ): %.o: %.cpp
	@echo " CXX" $<; $(CXX) -c $< -o $@ $(CXXFLAGS)

$(OUTPUT_DIR)/$(EXEC): $(COBJ) $(CXXOBJ)
	$(CXX) $(COBJ) $(CXXOBJ) main.cpp -o $(OUTPUT_DIR)/$(EXEC) $(CXXFLAGS)

clean:
	rm -rf $(OUTPUT_DIR)
	rm $(COBJ) $(CXXOBJ)