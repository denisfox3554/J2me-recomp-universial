#include "j2me_internal.h"
#include <iostream>
#include <cassert>

J2MERuntime* g_runtime = nullptr;

// Forward declarations from timer.cpp
void j2me_timer_init();
void j2me_timer_dispatch(SDL_Event* ev);

// ─────────────────────────────────────────────────────────────────────────────
bool j2me_runtime_init(int screen_w, int screen_h,
                       int scale, const char* title)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        std::cerr << "[runtime] SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }

    j2me_timer_init();  // register SDL user event for timers

    g_runtime = new J2MERuntime();

    if (!g_runtime->display.init(screen_w, screen_h, scale, title)) {
        std::cerr << "[runtime] Display init failed\n";
        return false;
    }

#ifdef HAVE_SDL2_MIXER
    if (!g_runtime->audio.init())
        std::cerr << "[runtime] Audio init failed (continuing without audio)\n";
#endif

    g_runtime->rms.init("save");

    std::cout << "[runtime] Initialized: " << screen_w << "x" << screen_h
              << " scale=" << scale << " title=" << title << "\n";
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
void j2me_register_midlet(MIDletStartFn start, MIDletStartFn pause,
                           MIDletStartFn destroy)
{
    assert(g_runtime);
    g_runtime->midlet_start   = start;
    g_runtime->midlet_pause   = pause;
    g_runtime->midlet_destroy = destroy;
}

// ─────────────────────────────────────────────────────────────────────────────
// SDL keycode → MIDP key code mapping
// ─────────────────────────────────────────────────────────────────────────────
static jint sdl_to_midp(SDL_Keycode k) {
    switch (k) {
        case SDLK_UP:     case SDLK_KP_8: return KEY_UP;
        case SDLK_DOWN:   case SDLK_KP_2: return KEY_DOWN;
        case SDLK_LEFT:   case SDLK_KP_4: return KEY_LEFT;
        case SDLK_RIGHT:  case SDLK_KP_6: return KEY_RIGHT;
        case SDLK_RETURN: case SDLK_KP_5: return KEY_FIRE;
        case SDLK_0: return KEY_NUM0;
        case SDLK_1: return KEY_NUM1;
        case SDLK_2: return KEY_NUM2;
        case SDLK_3: return KEY_NUM3;
        case SDLK_4: return KEY_NUM4;
        case SDLK_5: return KEY_NUM5;
        case SDLK_6: return KEY_NUM6;
        case SDLK_7: return KEY_NUM7;
        case SDLK_8: return KEY_NUM8;
        case SDLK_9: return KEY_NUM9;
        case SDLK_ASTERISK: return KEY_STAR;
        case SDLK_HASH:     return KEY_POUND;
        case SDLK_F1:       return KEY_SOFT1;
        case SDLK_F2:       return KEY_SOFT2;
        default: return 0;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void j2me_runtime_run() {
    assert(g_runtime);
    assert(g_runtime->midlet_start && "No MIDlet registered");

    g_runtime->running = true;
    g_runtime->midlet_start();

    SDL_Event ev;
    while (g_runtime->running) {
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT:
                    g_runtime->running = false;
                    break;
                case SDL_KEYDOWN: {
                    jint k = sdl_to_midp(ev.key.keysym.sym);
                    if (k) {
                        g_runtime->last_key_pressed = k;
                        g_runtime->key_state |= (1u << ((-k) & 31));
                    }
                    break;
                }
                case SDL_KEYUP: {
                    jint k = sdl_to_midp(ev.key.keysym.sym);
                    if (k) {
                        g_runtime->last_key_released = k;
                        g_runtime->key_state &= ~(1u << ((-k) & 31));
                    }
                    break;
                }
                default:
                    j2me_timer_dispatch(&ev);
                    break;
            }
        }

        // Present if a repaint was requested
        if (g_runtime->display.dirty) {
            g_runtime->display.present();
            g_runtime->display.dirty = false;
        }

        SDL_Delay(1);
    }

    if (g_runtime->midlet_destroy)
        g_runtime->midlet_destroy();
}

// ─────────────────────────────────────────────────────────────────────────────
void j2me_runtime_quit() {
    if (g_runtime) g_runtime->running = false;
}

void j2me_repaint_stub() {
    if (g_runtime) g_runtime->display.mark_dirty();
}

jlong j2me_current_time_millis() {
    return static_cast<jlong>(SDL_GetTicks64());
}

void j2me_sleep(jlong ms) {
    SDL_Delay(static_cast<uint32_t>(ms < 0 ? 0 : ms));
}

// Key state queries used by generated Canvas code
extern "C" {
    uint32_t j2me_get_key_states()       { return g_runtime ? g_runtime->key_state : 0; }
    jint     j2me_get_last_key_pressed() { return g_runtime ? g_runtime->last_key_pressed : 0; }
    jint     j2me_get_last_key_released(){ return g_runtime ? g_runtime->last_key_released : 0; }
}
