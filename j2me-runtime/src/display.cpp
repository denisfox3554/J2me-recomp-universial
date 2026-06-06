#include "j2me_internal.h"
#include <SDL2/SDL.h>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <algorithm>

bool J2MEDisplay::init(int w, int h, int sc, const char* title) {
    canvas_w = w;
    canvas_h = h;
    scale    = sc;

    window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w * sc, h * sc,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        std::cerr << "[display] SDL_CreateWindow: " << SDL_GetError() << "\n";
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        std::cerr << "[display] SDL_CreateRenderer: " << SDL_GetError() << "\n";
        return false;
    }

    SDL_RenderSetLogicalSize(renderer, w, h);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");  // nearest-neighbour

    texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        w, h);
    if (!texture) {
        std::cerr << "[display] SDL_CreateTexture: " << SDL_GetError() << "\n";
        return false;
    }

    pixels = static_cast<uint32_t*>(
        std::calloc(static_cast<size_t>(w * h), sizeof(uint32_t)));
    if (!pixels) return false;

    return true;
}

void J2MEDisplay::destroy() {
    std::free(pixels); pixels = nullptr;
    if (texture)  { SDL_DestroyTexture(texture);   texture  = nullptr; }
    if (renderer) { SDL_DestroyRenderer(renderer); renderer = nullptr; }
    if (window)   { SDL_DestroyWindow(window);     window   = nullptr; }
}

// ── Drawing primitives ────────────────────────────────────────────────────────

void J2MEDisplay::set_pixel(int x, int y, uint32_t argb) {
    if (x < 0 || y < 0 || x >= canvas_w || y >= canvas_h) return;
    pixels[y * canvas_w + x] = argb;
}

void J2MEDisplay::clear(uint32_t argb) {
    int n = canvas_w * canvas_h;
    for (int i = 0; i < n; ++i) pixels[i] = argb;
}

void J2MEDisplay::fill_rect(int x, int y, int w, int h, uint32_t argb) {
    int x2 = std::min(x + w, canvas_w);
    int y2 = std::min(y + h, canvas_h);
    x = std::max(x, 0);
    y = std::max(y, 0);
    for (int row = y; row < y2; ++row)
        for (int col = x; col < x2; ++col)
            pixels[row * canvas_w + col] = argb;
}

void J2MEDisplay::draw_rect(int x, int y, int w, int h, uint32_t argb) {
    fill_rect(x,         y,         w, 1, argb);
    fill_rect(x,         y + h - 1, w, 1, argb);
    fill_rect(x,         y,         1, h, argb);
    fill_rect(x + w - 1, y,         1, h, argb);
}

void J2MEDisplay::draw_line(int x0, int y0, int x1, int y1, uint32_t argb) {
    // Bresenham
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        set_pixel(x0, y0, argb);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// anchor: MIDP Graphics anchor constants (TOP|LEFT = 0x14)
void J2MEDisplay::draw_image(int x, int y, int img_w, int img_h,
                              const uint32_t* img_pixels, int anchor)
{
    // Adjust for anchor (simplified: TOP=0,VCENTER=1,BOTTOM=2 / LEFT=0,HCENTER=1,RIGHT=2)
    int hc = (anchor >> 2) & 3;
    int vc = (anchor     ) & 3;
    if (hc == 1) x -= img_w / 2;
    else if (hc == 2) x -= img_w;
    if (vc == 1) y -= img_h / 2;
    else if (vc == 2) y -= img_h;

    for (int row = 0; row < img_h; ++row) {
        int dy = y + row;
        if (dy < 0 || dy >= canvas_h) continue;
        for (int col = 0; col < img_w; ++col) {
            int dx = x + col;
            if (dx < 0 || dx >= canvas_w) continue;
            uint32_t c = img_pixels[row * img_w + col];
            if ((c >> 24) == 0) continue;  // fully transparent
            pixels[dy * canvas_w + dx] = c;
        }
    }
}

// ── Minimal 5×7 bitmap font for debug text ────────────────────────────────────
// 95 printable ASCII chars starting at 0x20
static const uint8_t FONT5x7[95][7] = {
    {0,0,0,0,0,0,0}, // space
    {4,4,4,4,0,4,0}, // !
    {10,10,0,0,0,0,0}, // "
    {10,31,10,31,10,0,0}, // #
    // (abbreviated — fill with your preferred embedded font)
};

void J2MEDisplay::draw_string(const char* str, int x, int y, uint32_t argb) {
    int cx = x;
    for (; *str; ++str, cx += 6) {
        uint8_t c = static_cast<uint8_t>(*str);
        if (c < 32 || c > 126) continue;
        const uint8_t* glyph = FONT5x7[c - 32];
        for (int row = 0; row < 7; ++row) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < 5; ++col) {
                if (bits & (0x10 >> col))
                    set_pixel(cx + col, y + row, argb);
            }
        }
    }
}

// ── Present ───────────────────────────────────────────────────────────────────
void J2MEDisplay::present() {
    SDL_UpdateTexture(texture, nullptr, pixels,
                      canvas_w * static_cast<int>(sizeof(uint32_t)));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}
