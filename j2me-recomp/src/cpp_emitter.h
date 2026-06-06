#pragma once
#include "bytecode_lifter.h"
#include "cfg_builder.h"
#include "ssa_builder.h"
#include "toml_config.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>

// ─────────────────────────────────────────────────────────────────────────────
// CppEmitter — walks CFG in RPO, emits C++17 per-class .cpp file.
// Strategy: goto-based CFG (one label per basic block), SSA values as
// local C variables, phi nodes as assignments at block entry.
// ─────────────────────────────────────────────────────────────────────────────

class CppEmitter {
public:
    CppEmitter(const std::vector<IRMethod>& methods, const TomlConfig& cfg)
        : methods_(methods), cfg_(cfg) {}

    bool emit(const std::string& out_path);

    // Emit shared glue: vtable dispatch table + runtime includes header
    static bool emit_glue(const std::string& out_dir,
                          const std::vector<ClassFile>& classes,
                          const TomlConfig& cfg);

private:
    const std::vector<IRMethod>& methods_;
    const TomlConfig&            cfg_;

    // ── Per-method emission ───────────────────────────────────────────────
    void emit_method(std::ostream& out, const IRMethod& m);
    void emit_method_signature(std::ostream& out, const IRMethod& m,
                                bool definition);
    void emit_block(std::ostream& out, const IRMethod& m,
                    const CFG& cfg, const BasicBlock& bb,
                    const std::unordered_map<int,std::string>& val_names);
    void emit_instr(std::ostream& out, const IRInstr& ins,
                    const std::unordered_map<int,std::string>& val_names,
                    int indent);

    // ── Type helpers ──────────────────────────────────────────────────────
    static std::string cpp_type(IRType t);
    static std::string cpp_type_from_desc(char d);
    static std::string return_type_from_desc(const std::string& desc);
    static std::string params_from_desc(const std::string& desc,
                                        bool is_static,
                                        const std::string& class_name);
    static std::string val_name(int id)  { return "v" + std::to_string(id); }
    static std::string block_label(int id) { return "bb_" + std::to_string(id); }
    static std::string mangle(const std::string& cls,
                               const std::string& method,
                               const std::string& desc);
    static std::string indent_str(int n);

    // ── Invoke helpers ────────────────────────────────────────────────────
    // cp_idx → "ClassName_methodName_descriptor" string (best-effort without CP)
    static std::string invoke_sym(int64_t cp_idx, const std::string& hint);
};
