// game_main.cpp — entry point, ties recompiled MIDlet to runtime
#include "j2me_runtime.h"

// Forward declarations — filled by recompiled class files
extern void com_example_Main__startApp____V();
extern void com_example_Main___init_____V(jref);

// Object allocation without glue header dependency
extern jref j2me_new_object(const char* class_name);

static jref g_midlet_instance = nullptr;

static void midlet_start() {
    g_midlet_instance = j2me_new_object("com/example/Main");
    com_example_Main___init_____V(g_midlet_instance);
    com_example_Main__startApp____V();
}
static void midlet_pause()   {}
static void midlet_destroy() {}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    if (!j2me_runtime_init(240, 320, 2, "J2ME Game")) return 1;
    j2me_register_midlet(midlet_start, midlet_pause, midlet_destroy);
    j2me_runtime_run();
    return 0;
}
