#include "j2me_internal.h"
#include <string>
#include <unordered_map>
#include <fstream>
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
// MIDlet — javax.microedition.midlet.MIDlet
// Provides application property lookups (from JAD/manifest) and
// platformRequest stub.
// ─────────────────────────────────────────────────────────────────────────────

static std::unordered_map<std::string, std::string> g_app_props;

// Load properties from a .jad file if present next to the executable
void j2me_load_app_properties(const char* jad_path) {
    if (!jad_path) return;
    std::ifstream f(jad_path);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        auto pos = line.find(':');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        // trim leading space in value
        auto vpos = val.find_first_not_of(" \t");
        if (vpos != std::string::npos) val = val.substr(vpos);
        g_app_props[key] = val;
    }
    std::cout << "[midlet] Loaded " << g_app_props.size()
              << " properties from " << jad_path << "\n";
}

// MIDlet.getAppProperty(String key)
extern "C" const char* j2me_get_app_property(const char* key) {
    auto it = g_app_props.find(key ? key : "");
    if (it != g_app_props.end()) return it->second.c_str();
    return nullptr;
}

// MIDlet.notifyDestroyed() — signal runtime to shut down
extern "C" void j2me_notify_destroyed() {
    j2me_runtime_quit();
}

// MIDlet.notifyPaused() — no-op in desktop context
extern "C" void j2me_notify_paused() {}

// MIDlet.platformRequest(String url) — stub
extern "C" jbool j2me_platform_request(const char* url) {
    std::cout << "[midlet] platformRequest: " << (url ? url : "(null)") << "\n";
    return 0;
}
