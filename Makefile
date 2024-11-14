.PHONY: clean

EXEC := eegs

INCLUDE_DIRS := src imgui imgui/backends frontend gl3w/include
OUTPUT_DIR := bin

CXX := c++
CXXFLAGS := $(addprefix -I, $(INCLUDE_DIRS)) $(shell sdl2-config --cflags --libs)
CXXFLAGS += -Ofast -flto=auto -Wall -g
CXXSRC := $(wildcard imgui/*.cpp)
CXXSRC += $(wildcard imgui/backends/imgui_impl_sdl2.cpp)
CXXSRC += $(wildcard imgui/backends/imgui_impl_sdlrenderer2.cpp)
CXXSRC += $(wildcard frontend/*.cpp)
CXXSRC += $(wildcard frontend/renderer/*.cpp)
CXXOBJ := $(CXXSRC:.cpp=.o)

CC := gcc
CFLAGS := $(addprefix -I, $(INCLUDE_DIRS)) $(shell sdl2-config --cflags --libs)
CFLAGS += -Ofast -flto=auto -Wall
CSRC := $(wildcard src/*.c)
CSRC += $(wildcard src/ee/*.c)
CSRC += $(wildcard src/iop/*.c)
CSRC += $(wildcard src/shared/*.c)
CSRC += $(wildcard gl3w/src/*.c)
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