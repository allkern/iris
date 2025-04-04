# We need a different Makefile for macOS
PLATFORM := $(shell uname -s)

$(shell echo $(PLATFORM))

ifeq ($(PLATFORM),Darwin)
include Makefile.macos
else
include Makefile.linux
endif