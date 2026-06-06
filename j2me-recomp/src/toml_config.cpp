#include "toml_config.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

// ── Minimal TOML helpers ──────────────────────────────────────────────────────

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

static std::string unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

// Extract value from  key = "value"
static std::string kv_string(const std::string& line, const std::string& key) {
    auto pos = line.find(key);
    if (pos == std::string::npos) return {};
    auto eq = line.find('=', pos + key.size());
    if (eq == std::string::npos) return {};
    return unquote(trim(line.substr(eq + 1)));
}

bool TomlConfig::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) { std::cerr << "[TomlConfig] Cannot open: " << path << "\n"; return false; }

    std::string section;
    // Accumulate current [[array-of-tables]] item
    PatchEntry     cur_patch;
    StubEntry      cur_stub;
    SymbolOverride cur_sym;
    bool in_patch = false, in_stub = false, in_sym = false;

    auto flush_item = [&]() {
        if (in_patch && !cur_patch.cls.empty()) { patches.push_back(cur_patch); cur_patch = {}; }
        if (in_stub  && !cur_stub.cls.empty())  { stubs.push_back(cur_stub);    cur_stub  = {}; }
        if (in_sym   && !cur_sym.cls.empty())   { symbols.push_back(cur_sym);   cur_sym   = {}; }
        in_patch = in_stub = in_sym = false;
    };

    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        // Array-of-tables headers: [[patches]], [[stubs]], [[symbols]]
        if (line == "[[patches]]")  { flush_item(); in_patch = true; continue; }
        if (line == "[[stubs]]")    { flush_item(); in_stub  = true; continue; }
        if (line == "[[symbols]]")  { flush_item(); in_sym   = true; continue; }

        // Section headers: [game], [build]
        if (line[0] == '[' && line[1] != '[') {
            flush_item();
            section = line.substr(1, line.find(']') - 1);
            continue;
        }

        // Key-value inside sections
        if (section == "game") {
            if (line.find("name")       != std::string::npos) game_name   = kv_string(line, "name");
            if (line.find("jar")        != std::string::npos) game_jar    = kv_string(line, "jar");
            if (line.find("main_class") != std::string::npos) main_class  = kv_string(line, "main_class");
        }
        if (section == "build") {
            // Parse arrays: extra_cxx_flags = ["-O2", ...]
            // Simple approach: collect quoted values from array lines
            if (line.find("extra_cxx_flags") != std::string::npos ||
                line.find("link_libraries")  != std::string::npos) {
                bool is_cxx = line.find("extra_cxx_flags") != std::string::npos;
                size_t lb = line.find('['), rb = line.find(']');
                if (lb != std::string::npos && rb != std::string::npos) {
                    std::string arr = line.substr(lb + 1, rb - lb - 1);
                    std::istringstream ss(arr);
                    std::string tok;
                    while (std::getline(ss, tok, ',')) {
                        std::string v = unquote(trim(tok));
                        if (!v.empty()) {
                            if (is_cxx) extra_cxx_flags.push_back(v);
                            else        link_libraries.push_back(v);
                        }
                    }
                }
            }
        }

        // Inside [[patches]]
        if (in_patch) {
            if (line.find("class")  != std::string::npos) cur_patch.cls    = kv_string(line, "class");
            if (line.find("method") != std::string::npos) cur_patch.method = kv_string(line, "method");
            if (line.find("type")   != std::string::npos) cur_patch.type   = kv_string(line, "type");
            if (line.find("symbol") != std::string::npos) cur_patch.symbol = kv_string(line, "symbol");
        }
        // Inside [[stubs]]
        if (in_stub) {
            if (line.find("class")  != std::string::npos) cur_stub.cls    = kv_string(line, "class");
            if (line.find("method") != std::string::npos) cur_stub.method = kv_string(line, "method");
        }
        // Inside [[symbols]]
        if (in_sym) {
            if (line.find("class")  != std::string::npos) cur_sym.cls    = kv_string(line, "class");
            if (line.find("method") != std::string::npos) cur_sym.method = kv_string(line, "method");
            if (line.find("symbol") != std::string::npos) cur_sym.symbol = kv_string(line, "symbol");
        }
    }
    flush_item();
    return true;
}

const PatchEntry* TomlConfig::find_patch(const std::string& cls, const std::string& method) const {
    for (auto& p : patches)
        if (p.cls == cls && p.method == method) return &p;
    return nullptr;
}
const StubEntry* TomlConfig::find_stub(const std::string& cls, const std::string& method) const {
    for (auto& s : stubs)
        if (s.cls == cls && s.method == method) return &s;
    return nullptr;
}
const SymbolOverride* TomlConfig::find_symbol(const std::string& cls, const std::string& method) const {
    for (auto& s : symbols)
        if (s.cls == cls && s.method == method) return &s;
    return nullptr;
}
