#pragma once
#include <string>
#include <vector>
#include <unordered_map>

struct PatchEntry {
    std::string cls;
    std::string method;
    std::string type;    // "replace_call" | "skip_method" | "nop"
    std::string symbol;  // for replace_call
};

struct StubEntry {
    std::string cls;
    std::string method;
};

struct SymbolOverride {
    std::string cls;
    std::string method;
    std::string symbol;
};

struct TomlConfig {
    std::string game_name;
    std::string game_jar;
    std::string main_class;

    std::vector<PatchEntry>     patches;
    std::vector<StubEntry>      stubs;
    std::vector<SymbolOverride> symbols;
    std::vector<std::string>    extra_cxx_flags;
    std::vector<std::string>    link_libraries;

    bool load(const std::string& path);

    // Lookup helpers
    const PatchEntry*    find_patch(const std::string& cls, const std::string& method) const;
    const StubEntry*     find_stub (const std::string& cls, const std::string& method) const;
    const SymbolOverride* find_symbol(const std::string& cls, const std::string& method) const;
};
