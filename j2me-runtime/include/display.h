#pragma once
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// Display — wraps SDL2 window + renderer + pixel buffer.
// Matches javax.microedition.lcdui.Display / Canvas semantics.
// ─────────────────────────────────────────────────────────────────────────────

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

struct J2MEDisplay {
    SDL_Window*   window   = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture*  texture  = nullptr;

    int canvas_w = 0;   // J2ME logical resolution
    int canvas_h = 0;
    int scale    = 1;   // integer pixel-perfect scale

    uint32_t* pixels = nullptr;  // ARGB8888 framebuffer

    bool init(int w, int h, int scale, const char* title);
    void destroy();

    // Called by generated Graphics methods
    void set_pixel(int x, int y, uint32_t argb);
    void fill_rect(int x, int y, int w, int h, uint32_t argb);
    void draw_rect(int x, int y, int w, int h, uint32_t argb);
    void draw_line(int x1, int y1, int x2, int y2, uint32_t argb);
    void draw_image(int x, int y, int img_w, int img_h,
                    const uint32_t* img_pixels, int anchor);
    void draw_string(const char* str, int x, int y, uint32_t argb);
    void clear(uint32_t argb);

    // Flip framebuffer to screen
    void present();

    // Pending repaint flag
    bool dirty = false;
    void mark_dirty() { dirty = true; }
};

// ARGB color helpers
inline uint32_t j2me_rgb(int r, int g, int b) {
    return 0xFF000000u | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}
inline uint32_t j2me_argb(int a, int r, int g, int b) {
    return ((a & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}
