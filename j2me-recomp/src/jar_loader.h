#pragma once
#include <string>
#include <vector>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// Constant pool — kept on ClassFile so the lifter can resolve refs
// ─────────────────────────────────────────────────────────────────────────────
struct CPEntry {
    uint8_t     tag  = 0;
    std::string str;        // tag 1 (Utf8), tag 8 (String→utf8), tag 7 (Class→utf8)
    uint16_t    ref1 = 0;   // Class/String/Fieldref/Methodref/NameAndType .ref1
    uint16_t    ref2 = 0;   // Fieldref/Methodref/NameAndType .ref2
    int32_t     ival = 0;
    int64_t     lval = 0;
    float       fval = 0.0f;
    double      dval = 0.0;
};

// ─────────────────────────────────────────────────────────────────────────────
// ClassFile — flat representation of a parsed .class file
// ─────────────────────────────────────────────────────────────────────────────
struct MethodInfo {
    std::string name;
    std::string descriptor;
    std::vector<uint8_t> bytecode;
    uint16_t max_stack  = 0;
    uint16_t max_locals = 0;
    bool     is_static  = false;
    bool     is_native  = false;
    bool     skip       = false;
    std::string override_symbol;
};

struct FieldInfo {
    std::string name;
    std::string descriptor;
    bool is_static = false;
};

struct ClassFile {
    std::string name;
    std::string super_name;
    std::vector<std::string> interfaces;
    std::vector<FieldInfo>   fields;
    std::vector<MethodInfo>  methods;

    // Full constant pool — index 0 unused (JVM CP is 1-based)
    std::vector<CPEntry> cp;

    // ── CP lookup helpers ─────────────────────────────────────────────────
    const std::string& cp_utf8(uint16_t idx) const {
        static const std::string empty;
        if (idx == 0 || idx >= cp.size()) return empty;
        return cp[idx].str;
    }
    std::string cp_class_name(uint16_t idx) const {
        if (idx == 0 || idx >= cp.size()) return {};
        if (cp[idx].tag == 7) return cp_utf8(cp[idx].ref1);
        return {};
    }
    // Fieldref/Methodref/InterfaceMethodref → class_name, method_name, descriptor
    void cp_member_ref(uint16_t idx,
                       std::string& cls, std::string& name, std::string& desc) const {
        if (idx == 0 || idx >= cp.size()) return;
        const CPEntry& e = cp[idx];
        if (e.tag != 9 && e.tag != 10 && e.tag != 11) return;
        cls  = cp_class_name(e.ref1);
        // ref2 → NameAndType
        if (e.ref2 > 0 && e.ref2 < cp.size() && cp[e.ref2].tag == 12) {
            name = cp_utf8(cp[e.ref2].ref1);
            desc = cp_utf8(cp[e.ref2].ref2);
        }
    }
    std::string cp_string(uint16_t idx) const {
        if (idx == 0 || idx >= cp.size()) return {};
        if (cp[idx].tag == 8) return cp_utf8(cp[idx].ref1);
        if (cp[idx].tag == 1) return cp[idx].str;
        return {};
    }

    std::string flat_name() const {
        std::string r = name;
        for (char& c : r) if (c == '/') c = '_';
        return r;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// JarLoader
// ─────────────────────────────────────────────────────────────────────────────
class JarLoader {
public:
    bool load(const std::string& jar_path);
    const std::vector<ClassFile>& classes() const { return classes_; }
    std::vector<ClassFile>&       classes()       { return classes_; }
private:
    std::vector<ClassFile> classes_;
};
