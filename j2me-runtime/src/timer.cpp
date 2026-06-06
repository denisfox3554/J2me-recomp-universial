#include "j2me_internal.h"
#include <vector>
#include <mutex>
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
// Timer — SDL_AddTimer → SDL user event → main thread dispatch
// ─────────────────────────────────────────────────────────────────────────────

static uint32_t USER_TIMER_EVENT = 0;

struct TimerTask {
    int         id        = 0;
    bool        repeating = false;
    uint32_t    interval  = 0;
    void      (*fn)(void*)= nullptr;
    void*       userdata  = nullptr;
    SDL_TimerID sdl_id    = 0;
    bool        cancelled = false;
};

static std::vector<TimerTask*> g_timers;
static std::mutex              g_timer_mutex;
static int                     g_next_timer_id = 1;

static uint32_t sdl_timer_cb(uint32_t interval, void* param) {
    auto* task = static_cast<TimerTask*>(param);
    if (task->cancelled) return 0;
    SDL_Event ev{};
    ev.type      = USER_TIMER_EVENT;
    ev.user.code = task->id;
    ev.user.data1= task;
    SDL_PushEvent(&ev);
    return task->repeating ? interval : 0u;
}

void j2me_timer_init() {
    USER_TIMER_EVENT = SDL_RegisterEvents(1);
}

void j2me_timer_dispatch(SDL_Event* ev) {
    if (!ev || (uint32_t)ev->type != USER_TIMER_EVENT) return;
    auto* task = static_cast<TimerTask*>(ev->user.data1);
    if (task && !task->cancelled && task->fn)
        task->fn(task->userdata);
}

int j2me_schedule_timer(unsigned delay_ms, unsigned period_ms,
                         void(*fn)(void*), void* userdata) {
    auto* task      = new TimerTask();
    task->id        = g_next_timer_id++;
    task->repeating = (period_ms > 0);
    task->interval  = period_ms > 0 ? period_ms : delay_ms;
    task->fn        = fn;
    task->userdata  = userdata;
    {
        std::lock_guard<std::mutex> lk(g_timer_mutex);
        g_timers.push_back(task);
    }
    task->sdl_id = SDL_AddTimer(delay_ms > 0 ? delay_ms : 1, sdl_timer_cb, task);
    if (!task->sdl_id)
        std::cerr << "[timer] SDL_AddTimer failed: " << SDL_GetError() << "\n";
    return task->id;
}

void j2me_cancel_timer(int id) {
    std::lock_guard<std::mutex> lk(g_timer_mutex);
    for (auto* t : g_timers) {
        if (t->id == id) {
            t->cancelled = true;
            if (t->sdl_id) SDL_RemoveTimer(t->sdl_id);
            break;
        }
    }
}
