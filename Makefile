# We need a different Makefile for macOS
PLATFORM := $(shell uname -s)

ifeq ($(PLATFORM),Darwin)
include Makefile.macos
else
include Makefile.linux
endif