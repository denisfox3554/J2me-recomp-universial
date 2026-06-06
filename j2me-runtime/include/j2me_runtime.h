#pragma once
#include <cstdint>
#include <cstddef>

// ─────────────────────────────────────────────────────────────────────────────
// J2ME Runtime — public API
// Included by all generated recomp C++ files. Must NOT pull in SDL2 headers.
// ─────────────────────────────────────────────────────────────────────────────

// ── Java primitive typedefs ───────────────────────────────────────────────────
using jbyte   = int8_t;
using jshort  = int16_t;
using jint    = int32_t;
using jlong   = int64_t;
using jfloat  = float;
using jdouble = double;
using jchar   = uint16_t;
using jbool   = uint8_t;
using jref    = void*;

// ── Lifecycle ─────────────────────────────────────────────────────────────────
bool j2me_runtime_init(int screen_w, int screen_h, int scale, const char* title);
void j2me_runtime_run();
void j2me_runtime_quit();

using MIDletStartFn = void(*)();
void j2me_register_midlet(MIDletStartFn start_fn, MIDletStartFn pause_fn,
                           MIDletStartFn destroy_fn);
void j2me_repaint_stub();

// ── Timing ────────────────────────────────────────────────────────────────────
jlong j2me_current_time_millis();
void  j2me_sleep(jlong ms);

// ── Key codes (MIDP 2.0) ──────────────────────────────────────────────────────
enum J2MEKey : jint {
    KEY_NUM0=48, KEY_NUM1=49, KEY_NUM2=50, KEY_NUM3=51,
    KEY_NUM4=52, KEY_NUM5=53, KEY_NUM6=54, KEY_NUM7=55,
    KEY_NUM8=56, KEY_NUM9=57, KEY_STAR=42, KEY_POUND=35,
    KEY_UP=-1,   KEY_DOWN=-2, KEY_LEFT=-3, KEY_RIGHT=-4,
    KEY_FIRE=-5, KEY_SOFT1=-6, KEY_SOFT2=-7,
};

// ── GC — allocator + array helpers (no SDL2 dependency) ──────────────────────
#include "gc.h"

// ── RMS — save data API ───────────────────────────────────────────────────────
// (forward-declared; full type in rms.h, used only by generated midlet code)
struct RecordStore;
RecordStore* j2me_open_record_store(const char* name, bool create);
void         j2me_close_record_store(RecordStore* rs);

// ── MIDlet property ───────────────────────────────────────────────────────────
extern "C" const char* j2me_get_app_property(const char* key);
extern "C" void        j2me_notify_destroyed();
extern "C" void        j2me_notify_paused();

// ── Timer ─────────────────────────────────────────────────────────────────────
// Scheduling is done through opaque handles; functional wrappers in timer.cpp
int  j2me_schedule_timer(unsigned delay_ms, unsigned period_ms, void(*fn)(void*), void* ud);
void j2me_cancel_timer(int id);
