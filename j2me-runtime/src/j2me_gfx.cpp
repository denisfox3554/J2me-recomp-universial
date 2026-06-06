// j2me_gfx.cpp — Graphics drawing bridge from generated code to J2MEDisplay
#include "j2me_internal.h"
#include <cstring>

// Current draw color (ARGB)
static uint32_t g_color = 0xFF000000u;

static J2MEDisplay* display() {
    return g_runtime ? &g_runtime->display : nullptr;
}

void j2me_gfx_set_color(jref, jint rgb) {
    g_color = 0xFF000000u | (uint32_t)(rgb & 0xFFFFFF);
}

void j2me_gfx_fill_rect(jref, jint x, jint y, jint w, jint h) {
    if (auto* d = display()) d->fill_rect(x, y, w, h, g_color);
}

void j2me_gfx_draw_rect(jref, jint x, jint y, jint w, jint h) {
    if (auto* d = display()) d->draw_rect(x, y, w, h, g_color);
}

void j2me_gfx_draw_line(jref, jint x1, jint y1, jint x2, jint y2) {
    if (auto* d = display()) d->draw_line(x1, y1, x2, y2, g_color);
}

void j2me_gfx_draw_string(jref, jref str, jint x, jint y, jint /*anchor*/) {
    if (auto* d = display())
        d->draw_string(str ? (const char*)str : "", x, y, g_color);
}

void j2me_gfx_draw_image(jref, jref /*img*/, jint /*x*/, jint /*y*/, jint /*anchor*/) {
    // Image blitting: requires Image struct — stub for now
}

void j2me_gfx_copy_area(jref, jint sx, jint sy, jint w, jint h,
                          jint dx, jint dy, jint /*anchor*/) {
    auto* d = display();
    if (!d || !d->pixels) return;
    // Simple non-overlapping blit within framebuffer
    for (int row = 0; row < h; ++row) {
        int src_y = sy + row, dst_y = dy + row;
        if (src_y < 0 || src_y >= d->canvas_h) continue;
        if (dst_y < 0 || dst_y >= d->canvas_h) continue;
        memmove(&d->pixels[dst_y * d->canvas_w + dx],
                &d->pixels[src_y * d->canvas_w + sx],
                (size_t)w * sizeof(uint32_t));
    }
}

void j2me_gfx_fill_arc(jref, jint x, jint y, jint w, jint h,
                         jint start_angle, jint arc_angle) {
    // Simplified: filled ellipse approximation via scanlines
    auto* d = display();
    if (!d) return;
    float cx = x + w * 0.5f, cy = y + h * 0.5f;
    float rx = w * 0.5f, ry = h * 0.5f;
    (void)start_angle; (void)arc_angle;
    for (int row = y; row < y + h; ++row) {
        float dy2 = (row - cy) / (ry > 0 ? ry : 1);
        if (dy2 * dy2 > 1.0f) continue;
        float dx = rx * __builtin_sqrtf(1.0f - dy2 * dy2);
        d->fill_rect((int)(cx - dx), row, (int)(dx * 2), 1, g_color);
    }
}

void j2me_gfx_draw_arc(jref g, jint x, jint y, jint w, jint h,
                         jint start_angle, jint arc_angle) {
    // Stub: just draw rect outline for now
    j2me_gfx_draw_rect(g, x, y, w, h);
    (void)start_angle; (void)arc_angle;
}

jref j2me_gfx_get() {
    return (jref)1; // sentinel; all calls go through global display
}

extern "C" uint32_t j2me_get_key_states() {
    return g_runtime ? g_runtime->key_state : 0u;
}
