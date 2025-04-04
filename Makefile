# We need a different Makefile for macOS
ifeq ($(PLATFORM),Darwin)
include Makefile.macos
else
include Makefile.linux
endif