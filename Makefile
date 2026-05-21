CC      ?= gcc
SRC     = main.c model.c view.c controller.c
OUT     ?= vn

UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(OS),Windows_NT)
	PLATFORM := windows
	EXE_EXT  := .exe
else ifneq (,$(findstring MINGW,$(UNAME_S)))
	PLATFORM := windows
	EXE_EXT  := .exe
else ifneq (,$(findstring MSYS,$(UNAME_S)))
	PLATFORM := windows
	EXE_EXT  := .exe
else ifeq ($(UNAME_S),Linux)
	PLATFORM := linux
	EXE_EXT  :=
else ifeq ($(UNAME_S),Darwin)
	PLATFORM := macos
	EXE_EXT  :=
else
	PLATFORM := unknown
	EXE_EXT  :=
endif

TARGET := $(OUT)$(EXE_EXT)
VENDOR_DIR := third_party/$(PLATFORM)
VENDOR_HAS_SDL := $(wildcard $(VENDOR_DIR)/include/SDL2/SDL.h)
PKGCONFIG_HAS_SDL := $(shell pkg-config --exists sdl2 SDL2_ttf SDL2_image 2>/dev/null && echo yes)

BASE_CFLAGS := -Wall

ifdef VENDOR_HAS_SDL
	CFLAGS  ?= $(BASE_CFLAGS) -I$(VENDOR_DIR)/include
	ifeq ($(PLATFORM),windows)
		LDFLAGS ?= -L$(VENDOR_DIR)/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -lSDL2_image
	else ifeq ($(PLATFORM),linux)
		LDFLAGS ?= -L$(VENDOR_DIR)/lib -Wl,-rpath,'$$ORIGIN/lib' -lSDL2 -lSDL2_ttf -lSDL2_image
	else
		LDFLAGS ?= -L$(VENDOR_DIR)/lib -lSDL2 -lSDL2_ttf -lSDL2_image
	endif
else ifeq ($(PKGCONFIG_HAS_SDL),yes)
	CFLAGS  ?= $(BASE_CFLAGS) $(shell pkg-config --cflags sdl2 SDL2_ttf SDL2_image)
	LDFLAGS ?= $(shell pkg-config --libs sdl2 SDL2_ttf SDL2_image)
else ifeq ($(PLATFORM),macos)
	CFLAGS  ?= $(BASE_CFLAGS) -I/opt/homebrew/include
	LDFLAGS ?= -L/opt/homebrew/lib -lSDL2 -lSDL2_ttf -lSDL2_image
else
	CFLAGS  ?= $(BASE_CFLAGS)
	LDFLAGS ?= -lSDL2 -lSDL2_ttf -lSDL2_image
endif

.PHONY: all clean copy-runtime print-config \
	export-macos export-linux export-windows export-android export-package export-all

all: $(TARGET) copy-runtime

$(TARGET): $(SRC)
	$(CC) $(SRC) -o $(TARGET) $(CFLAGS) $(LDFLAGS)

copy-runtime:
ifeq ($(PLATFORM),windows)
	@if [ -d "$(VENDOR_DIR)/bin" ]; then cp "$(VENDOR_DIR)"/bin/*.dll . 2>/dev/null || true; fi
else ifeq ($(PLATFORM),linux)
	@if [ -d "$(VENDOR_DIR)/lib" ]; then mkdir -p lib; cp "$(VENDOR_DIR)"/lib/*.so* lib/ 2>/dev/null || true; fi
endif

print-config:
	@echo "PLATFORM=$(PLATFORM)"
	@echo "TARGET=$(TARGET)"
	@echo "CC=$(CC)"
	@echo "CFLAGS=$(CFLAGS)"
	@echo "LDFLAGS=$(LDFLAGS)"
	@echo "VENDOR_DIR=$(VENDOR_DIR)"

clean:
	rm -f $(OUT) $(OUT).exe *.dll
	rm -rf lib

export-macos:
	tools/export.sh macos

export-linux:
	tools/export.sh linux

export-windows:
	tools/export.sh windows

export-android:
	tools/export.sh android

export-package:
	tools/export.sh package

export-all:
	tools/export.sh all
