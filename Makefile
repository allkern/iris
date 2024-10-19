.PHONY: clean

EXEC := eegs

INCLUDE_DIRS := src
OUTPUT_DIR := bin

CXX := c++
CXXFLAGS := $(addprefix -I, $(INCLUDE_DIRS)) $(shell sdl2-config --cflags --libs)
CXXFLAGS += -Ofast -flto=auto -Wall
# CXXSRC := $(wildcard main.cpp)
CXXOBJ := $(CXXSRC:.cpp=.o)

CC := gcc
CFLAGS := $(addprefix -I, $(INCLUDE_DIRS)) $(shell sdl2-config --cflags --libs)
CSRC := $(wildcard src/*.c)
CSRC += $(wildcard src/ee/*.c)
CSRC += $(wildcard src/iop/*.c)
CSRC += $(wildcard src/shared/*.c)
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