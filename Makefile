# ─────────────────────────────────────────────────────────────────────────────
# J2ME Recomp Universal — top-level Makefile
# Works with:  mingw32-make (Windows/MinGW64)
#              make          (Linux/macOS)
# ─────────────────────────────────────────────────────────────────────────────

# Detect host: mingw32-make sets MAKE to "mingw32-make"
ifeq ($(findstring mingw32,$(MAKE)),mingw32)
    HOST_OS   := windows
    GENERATOR := "MinGW Makefiles"
    CMAKE_EXTRA := -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
else
    HOST_OS   := linux
    GENERATOR := "Unix Makefiles"
    CMAKE_EXTRA :=
endif

BUILD_DIR   ?= build
BUILD_TYPE  ?= Release
GAME_TOML   ?= game.toml
JOBS        ?= $(shell nproc 2>/dev/null || echo 4)

CMAKE       := cmake
CMAKE_FLAGS := -S . -B $(BUILD_DIR) \
               -G $(GENERATOR) \
               -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
               -DJ2ME_GAME_TOML=$(GAME_TOML) \
               $(CMAKE_EXTRA)

# ── Targets ──────────────────────────────────────────────────────────────────

.PHONY: all configure recomp game runtime tool clean distclean help

## Default: configure → recomp → build game
all: configure recomp game

## Step 1 — CMake configure
configure:
	$(CMAKE) $(CMAKE_FLAGS)

## Step 2 — Run the recomp tool: JAR → C++ sources
recomp: configure
	$(CMAKE) --build $(BUILD_DIR) --target run_recomp -j$(JOBS)
	$(CMAKE) $(CMAKE_FLAGS)   # re-configure so new sources are picked up

## Step 3 — Build game executable only
game: recomp
	$(CMAKE) --build $(BUILD_DIR) --target all -j$(JOBS)

## Build runtime library only
runtime: configure
	$(CMAKE) --build $(BUILD_DIR) --target j2me_runtime -j$(JOBS)

## Build recomp tool only
tool: configure
	$(CMAKE) --build $(BUILD_DIR) --target j2me_recomp_tool -j$(JOBS)

## Quick rebuild without rerunning recomp
rebuild:
	$(CMAKE) --build $(BUILD_DIR) -j$(JOBS)

clean:
	$(CMAKE) --build $(BUILD_DIR) --target clean

distclean:
	$(CMAKE) -E remove_directory $(BUILD_DIR)

help:
	@echo "Targets:"
	@echo "  all        configure + recomp + game  (default)"
	@echo "  configure  CMake configure only"
	@echo "  recomp     JAR → C++ sources"
	@echo "  game       build game executable"
	@echo "  runtime    build j2me_runtime library"
	@echo "  tool       build j2me_recomp_tool"
	@echo "  rebuild    incremental build (skip recomp)"
	@echo "  clean      clean build artifacts"
	@echo "  distclean  remove build dir"
	@echo ""
	@echo "Variables:"
	@echo "  BUILD_DIR=$(BUILD_DIR)   BUILD_TYPE=$(BUILD_TYPE)"
	@echo "  GAME_TOML=$(GAME_TOML)  JOBS=$(JOBS)"
	@echo "  HOST=$(HOST_OS)"
