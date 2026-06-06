# J2ME Recomp Universal

Static recompiler for J2ME JARs → native C++ executables.  
Architecture inspired by **N64Recomp** (tool + runtime split, TOML config)  
and **PS2Recomp** (multi-stage pipeline, analyzer → emitter → runtime).

---

## Structure

```
j2me-recomp-universal/
├── CMakeLists.txt          # Top-level: orchestrates all stages
├── Makefile                # Convenience wrapper (mingw32-make / make)
├── game.toml               # Per-game config: JAR path, patches, stubs, symbols
│
├── j2me-recomp/            # Stage 1 — Recomp tool (JAR → C++ sources)
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp            CLI: --jar --main --toml --output
│       ├── jar_loader.cpp      ZIP/JAR extractor (zlib inflate)
│       ├── class_parser.cpp    JVM .class parser → ClassFile IR
│       ├── bytecode_lifter.cpp JVM bytecode → typed IR (~200 opcodes)
│       ├── cfg_builder.cpp     CFG + dominator tree + SSA phi insertion
│       ├── ssa_builder.cpp     SSA construction (Braun et al.)
│       ├── cpp_emitter.cpp     IR → C++17 with goto-based CFG
│       ├── toml_config.cpp     game.toml parser (patches/stubs/symbols)
│       └── patch_applier.cpp   Applies TOML patches before lifting
│
├── j2me-runtime/           # Stage 2 — Runtime library (linked with output)
│   ├── CMakeLists.txt
│   ├── include/
│   │   ├── j2me_runtime.h  Master public header (included by generated code)
│   │   ├── display.h       SDL2 framebuffer + Graphics primitives
│   │   ├── audio.h         SDL2_mixer Player/Manager
│   │   ├── gc.h            Allocator + array helpers
│   │   └── rms.h           Record Management System
│   └── src/
│       ├── j2me_runtime.cpp  Init, main loop, key dispatch
│       ├── display.cpp       Pixel-perfect scaling, Bresenham, blit
│       ├── audio.cpp         WAV/MP3/tone Player lifecycle
│       ├── timer.cpp         SDL2 timer → main-thread callbacks
│       ├── midlet.cpp        MIDlet lifecycle + JAD property loader
│       ├── rms.cpp           Persistent save data (binary .rms files)
│       └── gc.cpp            malloc-backed allocator, OOB/NPE stubs
│
├── cmake/
│   ├── ParseGameToml.cmake   Extracts name/jar/main_class from game.toml
│   └── ApplyPatches.cmake    Applies build flags/libs from game.toml
│
└── game_src/               # Generated C++ (output of recomp step)
```

---

## Build

### Windows — MinGW64 (MSYS2)

```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-SDL2 \
          mingw-w64-x86_64-SDL2_mixer mingw-w64-x86_64-zlib

# Full pipeline: configure → recomp → game
mingw32-make

# Individual steps
mingw32-make tool       # build j2me_recomp_tool only
mingw32-make recomp     # run JAR → C++ step
mingw32-make game       # compile generated sources
mingw32-make rebuild    # incremental (skip recomp)
```

### Linux

```bash
sudo apt install cmake libsdl2-dev libsdl2-mixer-dev zlib1g-dev
make
```

---

## game.toml Reference

```toml
[game]
name        = "MyGame"             # output executable name
jar         = "game.jar"           # input JAR
main_class  = "com/example/Main"   # MIDlet entry point

[[patches]]                        # replace/skip individual methods
class  = "com/example/Canvas"
method = "repaint"
type   = "replace_call"            # replace_call | skip_method | nop
symbol = "j2me_repaint_stub"

[[stubs]]                          # route to runtime stubs
class  = "javax/microedition/lcdui/Display"
method = "getDisplay"

[[symbols]]                        # override C symbol name
class   = "com/example/Main"
method  = "<init>"
symbol  = "game_main_init"

[build]
extra_cxx_flags = ["-O2", "-DGAME_TITLE=\"MyGame\""]
link_libraries  = ["SDL2", "SDL2_mixer"]
```

---

## Pipeline Stages

```
game.jar
   │
   ▼ jar_loader        (ZIP inflate → raw .class bytes)
   ▼ class_parser      (.class → ClassFile IR)
   ▼ patch_applier     (TOML patches/stubs/symbols applied)
   ▼ bytecode_lifter   (JVM bytecode → typed IR)
   ▼ cfg_builder       (basic blocks, dominators, SSA phis)
   ▼ cpp_emitter       (IR → C++17 per-class .cpp files)
   │
   ▼ CMake / mingw32-make
   │   generated .cpp  +  j2me-runtime (SDL2 + SDL2_mixer)
   │
   ▼ MyGame.exe / MyGame
```

---

## Requirements

| Component       | Version  |
|-----------------|----------|
| CMake           | ≥ 3.20   |
| C++ compiler    | C++17    |
| SDL2            | ≥ 2.0.18 |
| SDL2_mixer      | ≥ 2.0    |
| zlib            | any      |
| mingw32-make    | MSYS2    |
