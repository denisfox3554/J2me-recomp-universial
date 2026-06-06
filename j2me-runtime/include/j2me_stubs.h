#pragma once
#include "j2me_runtime.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

// ─────────────────────────────────────────────────────────────────────────────
// j2me_stubs.h — inline stubs for standard Java/MIDP classes.
// Included by generated code via j2me_glue.h.
// All Graphics drawing goes through opaque j2me_gfx_* calls (defined in
// j2me_gfx_stubs.cpp, compiled as part of the game target).
// ─────────────────────────────────────────────────────────────────────────────

// ── Graphics drawing API (implemented in j2me_gfx_stubs.cpp) ─────────────────
void j2me_gfx_set_color  (jref g, jint rgb);
void j2me_gfx_fill_rect  (jref g, jint x, jint y, jint w, jint h);
void j2me_gfx_draw_rect  (jref g, jint x, jint y, jint w, jint h);
void j2me_gfx_draw_line  (jref g, jint x1, jint y1, jint x2, jint y2);
void j2me_gfx_draw_string(jref g, jref str, jint x, jint y, jint anchor);
void j2me_gfx_draw_image (jref g, jref img, jint x, jint y, jint anchor);
void j2me_gfx_copy_area  (jref g, jint sx, jint sy, jint w, jint h, jint dx, jint dy, jint anchor);
void j2me_gfx_fill_arc   (jref g, jint x, jint y, jint w, jint h, jint sa, jint arc);
void j2me_gfx_draw_arc   (jref g, jint x, jint y, jint w, jint h, jint sa, jint arc);
jref j2me_gfx_get        ();   // get Graphics for current canvas

// ── java/lang/Object ─────────────────────────────────────────────────────────
inline void  java_lang_Object___init_____V(jref) {}
inline jint  java_lang_Object__hashCode____I(jref s) { return (jint)(intptr_t)s; }
inline jref  java_lang_Object__toString____Ljava_lang_String_(jref) { return nullptr; }
inline jbool java_lang_Object__equals___Ljava_lang_Object__Z(jref a, jref b) { return a==b?1:0; }
inline jref  java_lang_Object__getClass____Ljava_lang_Class_(jref) { return nullptr; }

// ── java/lang/String ─────────────────────────────────────────────────────────
inline jint java_lang_String__length____I(jref s) {
    return s ? (jint)strlen((const char*)s) : 0;
}
inline jint java_lang_String__charAt___I_C(jref s, jint i) {
    return s ? ((const char*)s)[i] : 0;
}
inline jref java_lang_String__valueOf___I_Ljava_lang_String_(jint v) {
    static char buf[32]; snprintf(buf, sizeof(buf), "%d", v); return (jref)buf;
}
inline jref java_lang_String__valueOf___Z_Ljava_lang_String_(jbool v) {
    return (jref)(v ? "true" : "false");
}
inline jref java_lang_String__concat___Ljava_lang_String__Ljava_lang_String_(jref, jref b) {
    return b; // stub
}
inline jbool java_lang_String__equals___Ljava_lang_Object__Z(jref a, jref b) {
    if (!a || !b) return 0;
    return strcmp((const char*)a, (const char*)b) == 0 ? 1 : 0;
}
inline jint java_lang_String__compareTo___Ljava_lang_String__I(jref a, jref b) {
    if (!a || !b) return 0;
    return (jint)strcmp((const char*)a, (const char*)b);
}
inline jref java_lang_String__substring___I_Ljava_lang_String_(jref s, jint from) {
    return s ? (jref)((const char*)s + from) : nullptr;
}
inline jref java_lang_String__substring___II_Ljava_lang_String_(jref s, jint from, jint to) {
    if (!s) return nullptr;
    int len = to - from;
    char* buf = (char*)j2me_alloc(len + 1);
    memcpy(buf, (const char*)s + from, len);
    buf[len] = 0;
    return (jref)buf;
}
inline jint java_lang_String__indexOf___I_I(jref s, jint c) {
    if (!s) return -1;
    const char* p = strchr((const char*)s, c);
    return p ? (jint)(p - (const char*)s) : -1;
}
inline jref java_lang_Integer__toString___I_Ljava_lang_String_(jint v) {
    return java_lang_String__valueOf___I_Ljava_lang_String_(v);
}
inline jint java_lang_Integer__parseInt___Ljava_lang_String__I(jref s) {
    return s ? (jint)atoi((const char*)s) : 0;
}
inline jint java_lang_Integer__intValue____I(jref o) { return (jint)(intptr_t)o; }

// ── java/lang/Math ────────────────────────────────────────────────────────────
inline jdouble java_lang_Math__abs___D_D(jdouble v)   { return v<0?-v:v; }
inline jfloat  java_lang_Math__abs___F_F(jfloat v)    { return v<0?-v:v; }
inline jint    java_lang_Math__abs___I_I(jint v)       { return v<0?-v:v; }
inline jlong   java_lang_Math__abs___J_J(jlong v)      { return v<0?-v:v; }
inline jdouble java_lang_Math__sqrt___D_D(jdouble v)   { return __builtin_sqrt(v); }
inline jdouble java_lang_Math__sin___D_D(jdouble v)    { return __builtin_sin(v); }
inline jdouble java_lang_Math__cos___D_D(jdouble v)    { return __builtin_cos(v); }
inline jdouble java_lang_Math__tan___D_D(jdouble v)    { return __builtin_tan(v); }
inline jdouble java_lang_Math__floor___D_D(jdouble v)  { return __builtin_floor(v); }
inline jdouble java_lang_Math__ceil___D_D(jdouble v)   { return __builtin_ceil(v); }
inline jdouble java_lang_Math__pow___DD_D(jdouble a, jdouble b) { return __builtin_pow(a,b); }
inline jint    java_lang_Math__max___II_I(jint a, jint b)  { return a>b?a:b; }
inline jint    java_lang_Math__min___II_I(jint a, jint b)  { return a<b?a:b; }
inline jlong   java_lang_Math__max___JJ_J(jlong a, jlong b){ return a>b?a:b; }
inline jlong   java_lang_Math__min___JJ_J(jlong a, jlong b){ return a<b?a:b; }
inline jdouble java_lang_Math__random____D() { return (jdouble)rand()/(jdouble)RAND_MAX; }

// ── java/lang/System ─────────────────────────────────────────────────────────
inline jlong java_lang_System__currentTimeMillis____J() {
    return j2me_current_time_millis();
}
inline void java_lang_System__arraycopy___Ljava_lang_Object_ILjava_lang_Object_II_V(
    jref src, jint sp, jref dst, jint dp, jint len) {
    if (src && dst && len > 0)
        memmove((char*)dst + dp*4, (char*)src + sp*4, (size_t)len*4);
}
inline void java_lang_System__gc____V() {}

// ── java/lang/Thread ─────────────────────────────────────────────────────────
inline void java_lang_Thread__sleep___J_V(jlong ms) { j2me_sleep(ms); }
inline void java_lang_Thread__yield____V() {}

// ── javax/microedition/lcdui/Display ─────────────────────────────────────────
inline jref javax_microedition_lcdui_Display__getDisplay___Ljavax_microedition_midlet_MIDlet__Ljavax_microedition_lcdui_Display_(jref) {
    return (jref)1;
}
inline void javax_microedition_lcdui_Display__setCurrent___Ljavax_microedition_lcdui_Displayable__V(jref, jref) {}
inline void javax_microedition_lcdui_Display__callSerially___Ljava_lang_Runnable__V(jref, jref) {}

// ── javax/microedition/lcdui/game/GameCanvas ─────────────────────────────────
inline jref javax_microedition_lcdui_game_GameCanvas__getGraphics____Ljavax_microedition_lcdui_Graphics_(jref self) {
    return j2me_gfx_get();
}
inline void javax_microedition_lcdui_game_GameCanvas__flushGraphics____V(jref) {
    j2me_repaint_stub();
}
inline void javax_microedition_lcdui_game_GameCanvas__flushGraphics___IIII_V(jref, jint, jint, jint, jint) {
    j2me_repaint_stub();
}
inline jint javax_microedition_lcdui_game_GameCanvas__getKeyStates____I(jref) {
    extern unsigned int j2me_get_key_states();
    return (jint)j2me_get_key_states();
}

// ── javax/microedition/lcdui/Graphics ────────────────────────────────────────
inline void javax_microedition_lcdui_Graphics__setColor___I_V(jref g, jint c)                          { j2me_gfx_set_color(g,c); }
inline void javax_microedition_lcdui_Graphics__setColor___III_V(jref g, jint r, jint gv, jint b)       { j2me_gfx_set_color(g, 0xFF000000|(r<<16)|(gv<<8)|b); }
inline void javax_microedition_lcdui_Graphics__fillRect___IIII_V(jref g, jint x, jint y, jint w, jint h){ j2me_gfx_fill_rect(g,x,y,w,h); }
inline void javax_microedition_lcdui_Graphics__drawRect___IIII_V(jref g, jint x, jint y, jint w, jint h){ j2me_gfx_draw_rect(g,x,y,w,h); }
inline void javax_microedition_lcdui_Graphics__drawLine___IIII_V(jref g, jint x1,jint y1,jint x2,jint y2){ j2me_gfx_draw_line(g,x1,y1,x2,y2); }
inline void javax_microedition_lcdui_Graphics__drawString___Ljava_lang_String_III_V(jref g, jref s, jint x, jint y, jint a){ j2me_gfx_draw_string(g,s,x,y,a); }
inline void javax_microedition_lcdui_Graphics__drawImage___Ljavax_microedition_lcdui_Image_III_V(jref g, jref img, jint x, jint y, jint a){ j2me_gfx_draw_image(g,img,x,y,a); }
inline void javax_microedition_lcdui_Graphics__fillArc___IIIIII_V(jref g, jint x,jint y,jint w,jint h,jint sa,jint arc){ j2me_gfx_fill_arc(g,x,y,w,h,sa,arc); }
inline void javax_microedition_lcdui_Graphics__drawArc___IIIIII_V(jref g, jint x,jint y,jint w,jint h,jint sa,jint arc){ j2me_gfx_draw_arc(g,x,y,w,h,sa,arc); }
inline void javax_microedition_lcdui_Graphics__copyArea___IIIIIII_V(jref g, jint sx,jint sy,jint w,jint h,jint dx,jint dy,jint a){ j2me_gfx_copy_area(g,sx,sy,w,h,dx,dy,a); }
inline void javax_microedition_lcdui_Graphics__setClip___IIII_V(jref, jint, jint, jint, jint) {}
inline void javax_microedition_lcdui_Graphics__clipRect___IIII_V(jref, jint, jint, jint, jint) {}
inline jint javax_microedition_lcdui_Graphics__getClipX____I(jref)  { return 0; }
inline jint javax_microedition_lcdui_Graphics__getClipY____I(jref)  { return 0; }
inline jint javax_microedition_lcdui_Graphics__getClipWidth____I(jref)  { return 240; }
inline jint javax_microedition_lcdui_Graphics__getClipHeight____I(jref) { return 320; }
inline void javax_microedition_lcdui_Graphics__translate___II_V(jref, jint, jint) {}
inline jint javax_microedition_lcdui_Graphics__getTranslateX____I(jref) { return 0; }
inline jint javax_microedition_lcdui_Graphics__getTranslateY____I(jref) { return 0; }
inline jint javax_microedition_lcdui_Graphics__getColor____I(jref)  { return 0; }
inline void javax_microedition_lcdui_Graphics__setFont___Ljavax_microedition_lcdui_Font__V(jref, jref) {}
inline void javax_microedition_lcdui_Graphics__setStrokeStyle___I_V(jref, jint) {}

// ── javax/microedition/lcdui/Canvas ──────────────────────────────────────────
inline jint javax_microedition_lcdui_Canvas__getWidth____I(jref)  { return 240; }
inline jint javax_microedition_lcdui_Canvas__getHeight____I(jref) { return 320; }
inline void javax_microedition_lcdui_Canvas__repaint____V(jref)   { j2me_repaint_stub(); }
inline void javax_microedition_lcdui_Canvas__repaint___IIII_V(jref,jint,jint,jint,jint) { j2me_repaint_stub(); }
inline void javax_microedition_lcdui_Canvas__serviceRepaints____V(jref) {}
inline jbool javax_microedition_lcdui_Canvas__isDoubleBuffered____Z(jref) { return 1; }
inline jbool javax_microedition_lcdui_Canvas__hasPointerEvents____Z(jref) { return 0; }

// ── javax/microedition/lcdui/Font ────────────────────────────────────────────
inline jref javax_microedition_lcdui_Font__getDefaultFont____Ljavax_microedition_lcdui_Font_(){ return (jref)1; }
inline jref javax_microedition_lcdui_Font__getFont___III_Ljavax_microedition_lcdui_Font_(jint,jint,jint){ return (jref)1; }
inline jint javax_microedition_lcdui_Font__getHeight____I(jref) { return 10; }
inline jint javax_microedition_lcdui_Font__stringWidth___Ljava_lang_String__I(jref, jref s) {
    return s ? (jint)strlen((const char*)s) * 6 : 0;
}
inline jint javax_microedition_lcdui_Font__charWidth___C_I(jref, jint) { return 6; }

// ── javax/microedition/rms ────────────────────────────────────────────────────
inline jref javax_microedition_rms_RecordStore__openRecordStore___Ljava_lang_String_Z_Ljavax_microedition_rms_RecordStore_(jref name, jbool create) {
    return (jref)j2me_open_record_store((const char*)name, create != 0);
}
inline void javax_microedition_rms_RecordStore__closeRecordStore____V(jref rs) {
    j2me_close_record_store((RecordStore*)rs);
}
