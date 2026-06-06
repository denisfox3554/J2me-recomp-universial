#pragma once
// Internal header — used only by runtime .cpp files, never by generated code.
// Pulls in SDL2 and the full display/audio/rms structures.
#include "j2me_runtime.h"
#include "display.h"
#include "audio.h"
#include "rms.h"
#include <SDL2/SDL.h>

struct J2MERuntime {
    J2MEDisplay display;
    J2MEAudio   audio;
    J2MORMS     rms;

    MIDletStartFn midlet_start   = nullptr;
    MIDletStartFn midlet_pause   = nullptr;
    MIDletStartFn midlet_destroy = nullptr;

    bool     running           = false;
    uint32_t key_state         = 0;
    jint     last_key_pressed  = 0;
    jint     last_key_released = 0;
};

extern J2MERuntime* g_runtime;
