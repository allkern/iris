# We need a different Makefile for macOS
PLATFORM := $(shell uname -s)

echo $(PLATFORM)

ifeq ($(PLATFORM),Darwin)
include Makefile.macos
else
include Makefile.linux
endif