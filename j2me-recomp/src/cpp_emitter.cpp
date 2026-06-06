#include "cpp_emitter.h"
#include <fstream>
#include <iostream>
#include <cassert>
#include <algorithm>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
std::string CppEmitter::indent_str(int n) { return std::string(n * 4, ' '); }

std::string CppEmitter::cpp_type(IRType t) {
    switch (t) {
        case IRType::Void:   return "void";
        case IRType::Int:    return "jint";
        case IRType::Long:   return "jlong";
        case IRType::Float:  return "jfloat";
        case IRType::Double: return "jdouble";
        case IRType::Ref:    return "jref";
        default:             return "jint";
    }
}

std::string CppEmitter::cpp_type_from_desc(char d) {
    switch (d) {
        case 'V': return "void";
        case 'Z': case 'B': case 'C': case 'S': case 'I': return "jint";
        case 'J': return "jlong";
        case 'F': return "jfloat";
        case 'D': return "jdouble";
        case 'L': case '[': return "jref";
        default:  return "jint";
    }
}

std::string CppEmitter::return_type_from_desc(const std::string& desc) {
    auto p = desc.find(')');
    if (p == std::string::npos || p + 1 >= desc.size()) return "void";
    return cpp_type_from_desc(desc[p + 1]);
}

// Build C++ parameter list from descriptor, e.g. "(ILjava/lang/String;)V"
std::string CppEmitter::params_from_desc(const std::string& desc,
                                          bool is_static,
                                          const std::string& class_name) {
    std::string result;
    if (!is_static) {
        result += "jref p_this";
    }
    size_t i = 1; // skip '('
    int    pn = 0;
    while (i < desc.size() && desc[i] != ')') {
        if (!result.empty()) result += ", ";
        char c = desc[i];
        std::string t;
        if (c == 'L') {
            t = "jref"; while (i < desc.size() && desc[i] != ';') ++i; ++i;
        } else if (c == '[') {
            t = "jref"; while (i < desc.size() && desc[i] == '[') ++i;
            if (i < desc.size() && desc[i] == 'L') { while (i < desc.size() && desc[i] != ';') ++i; ++i; }
            else ++i;
        } else {
            t = cpp_type_from_desc(c); ++i;
            if (c == 'J' || c == 'D') { /* wide — already advanced */ }
        }
        result += t + " p" + std::to_string(pn++);
    }
    return result;
}

// ClassName + method + descriptor → safe C identifier
std::string CppEmitter::mangle(const std::string& cls,
                                const std::string& method,
                                const std::string& desc) {
    auto sanitize = [](const std::string& s) {
        std::string r;
        r.reserve(s.size());
        for (unsigned char c : s) {
            if (std::isalnum(c)) r += (char)c;
            else                 r += '_';
        }
        return r;
    };
    return sanitize(cls) + "__" + sanitize(method) + "__" + sanitize(desc);
}

std::string CppEmitter::invoke_sym(int64_t cp_idx, const std::string& hint) {
    if (!hint.empty() && hint != "ldc" && hint != "ldc_w" && hint != "ldc2_w")
        return hint;
    return "j2me_invoke_" + std::to_string(cp_idx);
}

// ─────────────────────────────────────────────────────────────────────────────
// Method signature
// ─────────────────────────────────────────────────────────────────────────────
void CppEmitter::emit_method_signature(std::ostream& out,
                                        const IRMethod& m,
                                        bool definition) {
    // Apply symbol override if present
    std::string sym = m.method_name.empty() ? "unknown" : m.method_name;
    if (!m.is_static) {
        // instance method: ClassName__methodName__descriptor
        sym = mangle(m.class_name, m.method_name, m.descriptor);
    } else {
        sym = mangle(m.class_name, m.method_name, m.descriptor);
    }

    std::string ret  = return_type_from_desc(m.descriptor);
    std::string pars = params_from_desc(m.descriptor, m.is_static, m.class_name);

    out << ret << " " << sym << "(" << pars << ")";
    if (!definition) out << ";";
    out << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Single instruction → C++ statement
// ─────────────────────────────────────────────────────────────────────────────
static const char* cmp_op_str(int64_t cond, IRType t) {
    // For If: cond 0=eq,1=ne,2=lt,3=ge,4=gt,5=le
    // For Ref: 0=eq,1=ne
    switch (cond) {
        case 0: return "==";
        case 1: return "!=";
        case 2: return "<";
        case 3: return ">=";
        case 4: return ">";
        case 5: return "<=";
        default: return "!=";
    }
}

void CppEmitter::emit_instr(std::ostream& out, const IRInstr& ins,
                              const std::unordered_map<int,std::string>& vn,
                              int ind) {
    auto S = indent_str(ind);
    auto v = [&](int id) -> std::string {
        auto it = vn.find(id);
        return (it != vn.end()) ? it->second : val_name(id);
    };
    auto dst_decl = [&](IRType t) -> std::string {
        return cpp_type(t) + " " + v(ins.dst) + " = ";
    };

    switch (ins.op) {
    case IROp::Nop: break;

    case IROp::Const: {
        if (ins.dst < 0) break;
        if (ins.type == IRType::Float) {
            out << S << dst_decl(ins.type) << (float)ins.imm_d << "f;\n";
        } else if (ins.type == IRType::Double) {
            out << S << dst_decl(ins.type) << ins.imm_d << ";\n";
        } else if (ins.type == IRType::Long) {
            out << S << dst_decl(ins.type) << ins.imm_i << "LL;\n";
        } else if (ins.type == IRType::Ref && ins.imm_i == 0 && ins.imm_s.empty()) {
            out << S << dst_decl(ins.type) << "nullptr;\n";
        } else if (ins.type == IRType::Ref && !ins.imm_s.empty()) {
            // String literal ref
            out << S << dst_decl(ins.type)
                << "j2me_string_literal(\"" << ins.imm_s << "\");\n";
        } else {
            out << S << dst_decl(ins.type) << ins.imm_i << ";\n";
        }
        break;
    }

    case IROp::LoadLocal: {
        if (ins.dst < 0) break;
        if (!ins.srcs.empty() && ins.srcs[0] >= 0) {
            // SSA rewritten: reaching def is srcs[0]
            // Elide if name resolves to the same string (e.g. both map to p_this)
            std::string dst_name = v(ins.dst);
            std::string src_name = v(ins.srcs[0]);
            if (dst_name != src_name)
                out << S << cpp_type(ins.type) << " " << dst_name
                    << " = " << src_name << ";\n";
        } else {
            out << S << cpp_type(ins.type) << " " << v(ins.dst)
                << " = local_" << ins.imm_i << ";\n";
        }
        break;
    }
    case IROp::StoreLocal:
        if (!ins.srcs.empty())
            out << S << "local_" << ins.imm_i << " = " << v(ins.srcs[0]) << ";\n";
        break;

    // ── Arithmetic ─────────────────────────────────────────────────────────
    case IROp::Add: out << S << dst_decl(ins.type) << v(ins.srcs[0]) << " + " << v(ins.srcs[1]) << ";\n"; break;
    case IROp::Sub: out << S << dst_decl(ins.type) << v(ins.srcs[0]) << " - " << v(ins.srcs[1]) << ";\n"; break;
    case IROp::Mul: out << S << dst_decl(ins.type) << v(ins.srcs[0]) << " * " << v(ins.srcs[1]) << ";\n"; break;
    case IROp::Div:
        out << S << dst_decl(ins.type);
        if (ins.type == IRType::Int || ins.type == IRType::Long)
            out << "j2me_idiv(" << v(ins.srcs[0]) << ", " << v(ins.srcs[1]) << ");\n";
        else
            out << v(ins.srcs[0]) << " / " << v(ins.srcs[1]) << ";\n";
        break;
    case IROp::Rem:
        out << S << dst_decl(ins.type);
        if (ins.type == IRType::Int || ins.type == IRType::Long)
            out << "j2me_irem(" << v(ins.srcs[0]) << ", " << v(ins.srcs[1]) << ");\n";
        else
            out << "std::fmod(" << v(ins.srcs[0]) << ", " << v(ins.srcs[1]) << ");\n";
        break;
    case IROp::Neg: out << S << dst_decl(ins.type) << "-" << v(ins.srcs[0]) << ";\n"; break;
    case IROp::Shl: out << S << dst_decl(ins.type) << v(ins.srcs[0]) << " << (" << v(ins.srcs[1]) << " & " << (ins.type==IRType::Long?63:31) << ");\n"; break;
    case IROp::Shr: out << S << dst_decl(ins.type) << "((" << cpp_type(ins.type) << ")" << v(ins.srcs[0]) << ") >> (" << v(ins.srcs[1]) << " & " << (ins.type==IRType::Long?63:31) << ");\n"; break;
    case IROp::Ushr:
        if (ins.type == IRType::Long)
            out << S << dst_decl(ins.type) << "(jlong)((uint64_t)" << v(ins.srcs[0]) << " >> (" << v(ins.srcs[1]) << " & 63));\n";
        else
            out << S << dst_decl(ins.type) << "(jint)((uint32_t)" << v(ins.srcs[0]) << " >> (" << v(ins.srcs[1]) << " & 31));\n";
        break;
    case IROp::And: out << S << dst_decl(ins.type) << v(ins.srcs[0]) << " & " << v(ins.srcs[1]) << ";\n"; break;
    case IROp::Or:  out << S << dst_decl(ins.type) << v(ins.srcs[0]) << " | " << v(ins.srcs[1]) << ";\n"; break;
    case IROp::Xor: out << S << dst_decl(ins.type) << v(ins.srcs[0]) << " ^ " << v(ins.srcs[1]) << ";\n"; break;

    // ── Conversions ────────────────────────────────────────────────────────
    case IROp::I2L: out << S << dst_decl(IRType::Long)   << "(jlong)"   << v(ins.srcs[0]) << ";\n"; break;
    case IROp::I2F: out << S << dst_decl(IRType::Float)  << "(jfloat)"  << v(ins.srcs[0]) << ";\n"; break;
    case IROp::I2D: out << S << dst_decl(IRType::Double) << "(jdouble)" << v(ins.srcs[0]) << ";\n"; break;
    case IROp::L2I: out << S << dst_decl(IRType::Int)    << "(jint)"    << v(ins.srcs[0]) << ";\n"; break;
    case IROp::L2F: out << S << dst_decl(IRType::Float)  << "(jfloat)"  << v(ins.srcs[0]) << ";\n"; break;
    case IROp::L2D: out << S << dst_decl(IRType::Double) << "(jdouble)" << v(ins.srcs[0]) << ";\n"; break;
    case IROp::F2I: out << S << dst_decl(IRType::Int)    << "j2me_f2i(" << v(ins.srcs[0]) << ");\n"; break;
    case IROp::F2L: out << S << dst_decl(IRType::Long)   << "j2me_f2l(" << v(ins.srcs[0]) << ");\n"; break;
    case IROp::F2D: out << S << dst_decl(IRType::Double) << "(jdouble)" << v(ins.srcs[0]) << ";\n"; break;
    case IROp::D2I: out << S << dst_decl(IRType::Int)    << "j2me_d2i(" << v(ins.srcs[0]) << ");\n"; break;
    case IROp::D2L: out << S << dst_decl(IRType::Long)   << "j2me_d2l(" << v(ins.srcs[0]) << ");\n"; break;
    case IROp::D2F: out << S << dst_decl(IRType::Float)  << "(jfloat)"  << v(ins.srcs[0]) << ";\n"; break;
    case IROp::I2B: out << S << dst_decl(IRType::Int) << "(jint)(jbyte)" << v(ins.srcs[0]) << ";\n"; break;
    case IROp::I2C: out << S << dst_decl(IRType::Int) << "(jint)(jchar)" << v(ins.srcs[0]) << ";\n"; break;
    case IROp::I2S: out << S << dst_decl(IRType::Int) << "(jint)(jshort)" << v(ins.srcs[0]) << ";\n"; break;

    // ── Comparisons ────────────────────────────────────────────────────────
    case IROp::Cmp:
        out << S << dst_decl(IRType::Int)
            << "(" << v(ins.srcs[0]) << " > " << v(ins.srcs[1]) << " ? 1 : "
            << v(ins.srcs[0]) << " < " << v(ins.srcs[1]) << " ? -1 : 0);\n";
        break;
    case IROp::Cmpl: // NaN → -1
        out << S << dst_decl(IRType::Int)
            << "j2me_cmpl(" << v(ins.srcs[0]) << ", " << v(ins.srcs[1]) << ");\n";
        break;
    case IROp::Cmpg: // NaN → +1
        out << S << dst_decl(IRType::Int)
            << "j2me_cmpg(" << v(ins.srcs[0]) << ", " << v(ins.srcs[1]) << ");\n";
        break;

    // ── Array ops ──────────────────────────────────────────────────────────
    case IROp::NewArray:
        out << S << dst_decl(IRType::Ref)
            << "j2me_new_array<" << cpp_type(ins.type) << ">(" << v(ins.srcs[0]) << ");\n";
        break;
    case IROp::ANewArray:
        out << S << dst_decl(IRType::Ref)
            << "j2me_new_array<jref>(" << v(ins.srcs[0]) << ");\n";
        break;
    case IROp::MultiANewArray:
        out << S << dst_decl(IRType::Ref) << "j2me_multi_anewarray(";
        for (int s : ins.srcs) out << v(s) << ", ";
        out << ins.imm_i << ");\n";
        break;
    case IROp::ArrayLoad:
        out << S << dst_decl(ins.type)
            << "j2me_array_get((" << cpp_type(ins.type) << "*)" << v(ins.srcs[1])
            << ", " << v(ins.srcs[0]) << ");\n";
        break;
    case IROp::ArrayStore:
        out << S << "j2me_array_get((" << cpp_type(ins.type) << "*)" << v(ins.srcs[0])
            << ", " << v(ins.srcs[1]) << ") = " << v(ins.srcs[2]) << ";\n";
        break;
    case IROp::ArrayLength:
        out << S << dst_decl(IRType::Int)
            << "j2me_array_length((jint*)" << v(ins.srcs[0]) << ");\n";
        break;

    // ── Object ops ─────────────────────────────────────────────────────────
    case IROp::New:
        out << S << dst_decl(IRType::Ref)
            << "j2me_new_object(\"" << ins.imm_s << "\");\n";
        break;
    case IROp::GetField:
        out << S << dst_decl(ins.type)
            << "j2me_get_field_" << cpp_type(ins.type) << "("
            << v(ins.srcs[0]) << ", \"" << ins.imm_s << "\");\n";
        break;
    case IROp::PutField:
        out << S << "j2me_put_field_" << cpp_type(ins.type) << "("
            << v(ins.srcs[0]) << ", \"" << ins.imm_s << "\", "
            << v(ins.srcs[1]) << ");\n";
        break;
    case IROp::GetStatic:
        out << S << dst_decl(ins.type)
            << "j2me_get_static_" << cpp_type(ins.type)
            << "(\"" << ins.imm_s << "\");\n";
        break;
    case IROp::PutStatic:
        out << S << "j2me_put_static_" << cpp_type(ins.type)
            << "(\"" << ins.imm_s << "\", " << v(ins.srcs[0]) << ");\n";
        break;
    case IROp::CheckCast:
        out << S << dst_decl(IRType::Ref)
            << "j2me_checkcast(" << v(ins.srcs[0])
            << ", \"" << ins.imm_s << "\");\n";
        break;
    case IROp::InstanceOf:
        out << S << dst_decl(IRType::Int)
            << "j2me_instanceof(" << v(ins.srcs[0])
            << ", \"" << ins.imm_s << "\");\n";
        break;
    case IROp::Throw:
        out << S << "j2me_throw(" << v(ins.srcs[0]) << ");\n";
        break;

    // ── Invokes ────────────────────────────────────────────────────────────
    // ins.imm_s = "com/example/Foo::methodName"
    // ins.imm_s2 = descriptor "(II)V"
    // ins.srcs = [this?, arg0, arg1, ...]
    case IROp::InvokeVirtual:
    case IROp::InvokeInterface:
    case IROp::InvokeSpecial:
    case IROp::InvokeStatic: {
        bool is_dyn = (ins.imm_s == "invokedynamic");
        std::string sym;
        if (is_dyn) {
            sym = "j2me_invoke_dynamic";
        } else if (!ins.imm_s.empty() && !ins.imm_s2.empty()) {
            // Build mangled name from "cls::method" + descriptor
            auto sep = ins.imm_s.find("::");
            std::string cls_part  = (sep != std::string::npos) ? ins.imm_s.substr(0, sep) : ins.imm_s;
            std::string meth_part = (sep != std::string::npos) ? ins.imm_s.substr(sep+2)  : "unknown";
            sym = mangle(cls_part, meth_part, ins.imm_s2);
        } else {
            // Fallback: unresolved CP ref
            sym = (ins.op == IROp::InvokeVirtual || ins.op == IROp::InvokeInterface)
                  ? "j2me_invoke_virtual"
                  : "j2me_invoke_static";
        }

        // For virtual: emit as function-pointer call through vtable comment;
        // we still emit a direct call — the linker will resolve or give a clear error.
        if (ins.dst >= 0 && ins.type != IRType::Void)
            out << S << cpp_type(ins.type) << " " << v(ins.dst) << " = ";
        else
            out << S;

        out << sym << "(";
        if (is_dyn) out << ins.imm_i;
        for (size_t i = 0; i < ins.srcs.size(); ++i) {
            if (i > 0 || is_dyn) out << ", ";
            out << v(ins.srcs[i]);
        }
        out << ");\n";
        break;
    }

    // ── Control flow ───────────────────────────────────────────────────────
    case IROp::Goto:
        if (!ins.targets.empty())
            out << S << "goto " << block_label(ins.targets[0]) << ";\n";
        break;

    case IROp::If: {
        if (ins.targets.size() < 2) break;
        const char* op = cmp_op_str(ins.imm_i, ins.type);
        out << S << "if (" << v(ins.srcs[0]) << " " << op << " " << v(ins.srcs[1]) << ") "
            << "goto " << block_label(ins.targets[0]) << "; "
            << "else goto " << block_label(ins.targets[1]) << ";\n";
        break;
    }

    case IROp::TableSwitch: {
        if (ins.srcs.empty()) break;
        int64_t lo = ins.imm_i;
        out << S << "switch (" << v(ins.srcs[0]) << ") {\n";
        // targets[0] = default, targets[1..] = cases starting at lo
        for (int i = 1; i < (int)ins.targets.size(); ++i) {
            out << S << "    case " << (lo + i - 1) << ": goto "
                << block_label(ins.targets[i]) << ";\n";
        }
        out << S << "    default: goto " << block_label(ins.targets[0]) << ";\n";
        out << S << "}\n";
        break;
    }

    case IROp::LookupSwitch: {
        if (ins.srcs.empty()) break;
        out << S << "switch (" << v(ins.srcs[0]) << ") {\n";
        // Parse imm_s: "match:target_offset;" pairs
        std::string pairs = ins.imm_s;
        size_t pos = 0;
        while (pos < pairs.size()) {
            auto semi = pairs.find(';', pos);
            if (semi == std::string::npos) break;
            auto colon = pairs.find(':', pos);
            if (colon == std::string::npos || colon > semi) { pos = semi+1; continue; }
            std::string match = pairs.substr(pos, colon - pos);
            std::string tgt   = pairs.substr(colon+1, semi - colon - 1);
            out << S << "    case " << match << ": goto bb_" << tgt << ";\n";
            pos = semi + 1;
        }
        out << S << "    default: goto " << block_label(ins.targets[0]) << ";\n";
        out << S << "}\n";
        break;
    }

    case IROp::Return:
        out << S << "return;\n";
        break;
    case IROp::ReturnValue:
        if (!ins.srcs.empty())
            out << S << "return " << v(ins.srcs[0]) << ";\n";
        else
            out << S << "return 0;\n";
        break;

    case IROp::MonitorEnter:
    case IROp::MonitorExit:
        out << S << "/* monitor " << (ins.op==IROp::MonitorEnter?"enter":"exit")
            << " — stub */\n";
        break;

    default:
        out << S << "/* unhandled op " << (int)ins.op << " */\n";
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Emit one basic block: label, phi assignments, instructions
// ─────────────────────────────────────────────────────────────────────────────
void CppEmitter::emit_block(std::ostream& out, const IRMethod& m,
                             const CFG& cfg, const BasicBlock& bb,
                             const std::unordered_map<int,std::string>& vn) {
    out << "    " << block_label(bb.id) << ":;\n";

    // Emit phi assignments (declared at function top, assigned here)
    for (auto& [var, phi_dst] : bb.phi_dsts) {
        // phi_dst = phi(pred0:v_x, pred1:v_y, ...)
        // Emitted as: phi_N = chosen_src (filled from phi_srcs)
        auto it = bb.phi_srcs.find(phi_dst);
        if (it == bb.phi_srcs.end() || it->second.empty()) continue;
        // Emit as a chain of conditional assignments — simplified to first src
        // A full emitter would emit the phi as a pre-header mux; here we
        // use a __builtin approach via a declared variable + assignments in preds.
        // We already handle this by emitting "phi_N = src" at the end of each pred block.
        // So here we just note the variable exists.
        (void)phi_dst;
    }

    // Emit instructions
    for (int idx : bb.instr_indices) {
        const IRInstr& ins = m.instrs[idx];
        // Skip control-flow terminators here — handled separately below
        if (ins.op == IROp::Goto || ins.op == IROp::If ||
            ins.op == IROp::Return || ins.op == IROp::ReturnValue ||
            ins.op == IROp::TableSwitch || ins.op == IROp::LookupSwitch ||
            ins.op == IROp::Throw)
        {
            // Emit phi src assignments to successors before the jump
            for (int succ_id : bb.succs) {
                const BasicBlock& succ = cfg.block(succ_id);
                for (auto& [var, phi_dst] : succ.phi_dsts) {
                    auto sit = succ.phi_srcs.find(phi_dst);
                    if (sit == succ.phi_srcs.end()) continue;
                    for (auto& [pred_id, src_val] : sit->second) {
                        if (pred_id == bb.id && src_val >= 0) {
                            auto vit = vn.find(src_val);
                            std::string src_name = (vit != vn.end()) ? vit->second : val_name(src_val);
                            out << "    phi_" << phi_dst << " = " << src_name << ";\n";
                        }
                    }
                }
            }
            emit_instr(out, ins, vn, 1);
        } else {
            emit_instr(out, ins, vn, 1);
        }
    }

    // If block has no explicit terminator, fall through to next block
    if (!bb.succs.empty() && !bb.instr_indices.empty()) {
        const IRInstr& last = m.instrs[bb.instr_indices.back()];
        bool has_term = (last.op == IROp::Goto || last.op == IROp::If ||
                         last.op == IROp::Return || last.op == IROp::ReturnValue ||
                         last.op == IROp::TableSwitch || last.op == IROp::LookupSwitch ||
                         last.op == IROp::Throw);
        if (!has_term)
            out << "    goto " << block_label(bb.succs[0]) << ";\n";
    }
    out << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Emit a complete method
// ─────────────────────────────────────────────────────────────────────────────
void CppEmitter::emit_method(std::ostream& out, const IRMethod& m) {
    // Work on a mutable copy — CFGBuilder and SSABuilder modify it
    IRMethod mm = m;

    // Build CFG
    CFGBuilder cfg_builder;
    CFG cfg;
    try {
        cfg = cfg_builder.build(mm);
    } catch (...) {
        out << "// [CFG build failed for " << m.method_name << "]\n";
        emit_method_signature(out, m, true);
        out << "{ return; }\n\n";
        return;
    }

    // SSA renaming
    try {
        SSABuilder ssa;
        ssa.rename(cfg, mm);
    } catch (...) {
        // SSA failure is non-fatal — emit without SSA
    }

    // Build value name map
    std::unordered_map<int,std::string> vn;
    for (auto& ins : mm.instrs)
        if (ins.dst >= 0) vn[ins.dst] = val_name(ins.dst);
    for (auto& bb : cfg.blocks)
        for (auto& [var, phi_dst] : bb.phi_dsts)
            vn[phi_dst] = "phi_" + std::to_string(phi_dst);

    // Map param LoadLocals to pN/p_this names
    for (auto& ins : mm.instrs) {
        if (ins.op == IROp::LoadLocal && ins.dst >= 0) {
            int li = (int)ins.imm_i;
            if (li < mm.num_params) {
                std::string pname = (!mm.is_static && li == 0)
                    ? "p_this"
                    : "p" + std::to_string(mm.is_static ? li : li - 1);
                vn[ins.dst] = pname;
                if (!ins.srcs.empty() && ins.srcs[0] >= 0)
                    vn[ins.srcs[0]] = pname;
            }
        }
    }

    // Signature
    emit_method_signature(out, mm, true);
    out << "{\n";

    // Declare locals
    int param_slot = 0;
    for (int i = 0; i < mm.num_locals; ++i) {
        bool is_param = (i < mm.num_params);
        std::string init;
        if (is_param) {
            if (!mm.is_static && param_slot == 0) init = "p_this";
            else init = "p" + std::to_string(mm.is_static ? param_slot : param_slot - 1);
            ++param_slot;
        } else {
            init = "0";
        }
        out << "    jint local_" << i << " = (jint)(intptr_t)" << init
            << ";\n";
    }

    // Declare phi variables
    for (auto& bb : cfg.blocks)
        for (auto& [var, phi_dst] : bb.phi_dsts)
            out << "    jint phi_" << phi_dst << " = 0;\n";

    out << "\n";

    // Emit blocks in RPO
    for (int bid : cfg.rpo) {
        if (bid < 0 || bid >= cfg.num_blocks()) continue;
        const BasicBlock& bb = cfg.block(bid);

        out << "    " << block_label(bb.id) << ":;\n";

        for (int idx : bb.instr_indices) {
            // Bounds check — critical
            if (idx < 0 || idx >= (int)mm.instrs.size()) continue;
            const IRInstr& ins = mm.instrs[idx];

            bool is_term = (ins.op == IROp::Goto   || ins.op == IROp::If     ||
                            ins.op == IROp::Return  || ins.op == IROp::ReturnValue ||
                            ins.op == IROp::TableSwitch || ins.op == IROp::LookupSwitch ||
                            ins.op == IROp::Throw);

            if (is_term) {
                // Emit phi-src assignments before jump
                for (int succ_id : bb.succs) {
                    if (succ_id < 0 || succ_id >= cfg.num_blocks()) continue;
                    const BasicBlock& succ = cfg.block(succ_id);
                    for (auto& [var, phi_dst] : succ.phi_dsts) {
                        auto sit = succ.phi_srcs.find(phi_dst);
                        if (sit == succ.phi_srcs.end()) continue;
                        for (auto& [pred_id, src_val] : sit->second) {
                            if (pred_id == bb.id && src_val >= 0) {
                                auto vit = vn.find(src_val);
                                std::string sn = (vit != vn.end()) ? vit->second : val_name(src_val);
                                out << "    phi_" << phi_dst << " = " << sn << ";\n";
                            }
                        }
                    }
                }
            }
            // Log before emit so crash shows which op failed
            std::cerr << "      [instr] op=" << (int)ins.op
                      << " dst=" << ins.dst
                      << " srcs=" << ins.srcs.size()
                      << " bc=" << ins.bc_off << "\n";
            std::cerr.flush();
            emit_instr(out, ins, vn, 1);
        }

        // Fall-through if no explicit terminator
        if (!bb.succs.empty() && !bb.instr_indices.empty()) {
            int last_idx = bb.instr_indices.back();
            if (last_idx >= 0 && last_idx < (int)mm.instrs.size()) {
                const IRInstr& last = mm.instrs[last_idx];
                bool has_term = (last.op == IROp::Goto   || last.op == IROp::If     ||
                                 last.op == IROp::Return  || last.op == IROp::ReturnValue ||
                                 last.op == IROp::TableSwitch || last.op == IROp::LookupSwitch ||
                                 last.op == IROp::Throw);
                if (!has_term && bb.succs[0] >= 0 && bb.succs[0] < cfg.num_blocks())
                    out << "    goto " << block_label(bb.succs[0]) << ";\n";
            }
        }
        out << "\n";
    }

    out << "}\n\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Public: emit one class file
// ─────────────────────────────────────────────────────────────────────────────
bool CppEmitter::emit(const std::string& out_path) {
    std::ofstream f(out_path);
    if (!f) {
        std::cerr << "[emitter] Cannot open: " << out_path << "\n";
        return false;
    }

    // File header
    f << "// Auto-generated by j2me-recomp — DO NOT EDIT\n";
    f << "#include \"j2me_runtime.h\"\n";
    f << "#include \"j2me_glue.h\"\n";
    f << "#include <cmath>\n\n";

    // Forward declarations
    f << "// Forward declarations\n";
    for (auto& m : methods_) {
        emit_method_signature(f, m, /*definition=*/false);
    }
    f << "\n";

    // Method bodies
    for (auto& m : methods_) {
        emit_method(f, m);
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Static: emit shared glue header (vtable dispatch + ldc table stubs)
// ─────────────────────────────────────────────────────────────────────────────
bool CppEmitter::emit_glue(const std::string& out_dir,
                             const std::vector<ClassFile>& classes,
                             const TomlConfig& cfg) {
    // ── j2me_glue.h ──────────────────────────────────────────────────────────
    {
        std::ofstream h(out_dir + "/j2me_glue.h");
        if (!h) return false;

        h << "// Auto-generated by j2me-recomp — DO NOT EDIT\n";
        h << "#pragma once\n";
        h << "#include \"j2me_runtime.h\"\n";
        h << "#include \"j2me_stubs.h\"\n\n";

        // Integer arithmetic helpers (Java semantics)
        h << R"(
#include <cmath>
#include <cstdint>
#include <climits>

// ── Arithmetic helpers ────────────────────────────────────────────────────────
inline jint  j2me_idiv(jint  a, jint  b) { return b ? a/b : 0; }
inline jlong j2me_ldiv(jlong a, jlong b) { return b ? a/b : 0LL; }
inline jint  j2me_irem(jint  a, jint  b) { return b ? a%b : 0; }
inline jlong j2me_lrem(jlong a, jlong b) { return b ? a%b : 0LL; }
inline jint  j2me_f2i(jfloat  v) { if (std::isnan(v)) return 0; if (v>=(float)INT32_MAX) return INT32_MAX; if (v<=(float)INT32_MIN) return INT32_MIN; return (jint)v; }
inline jlong j2me_f2l(jfloat  v) { if (std::isnan(v)) return 0; if (v>=9.2e18f) return INT64_MAX; if (v<=-9.2e18f) return INT64_MIN; return (jlong)v; }
inline jint  j2me_d2i(jdouble v) { if (std::isnan(v)) return 0; if (v>=(double)INT32_MAX) return INT32_MAX; if (v<=(double)INT32_MIN) return INT32_MIN; return (jint)v; }
inline jlong j2me_d2l(jdouble v) { if (std::isnan(v)) return 0; if (v>=9.2e18) return INT64_MAX; if (v<=-9.2e18) return INT64_MIN; return (jlong)v; }
inline jint j2me_cmpl(jfloat  a, jfloat  b) { if (std::isnan(a)||std::isnan(b)) return -1; return (a>b)?1:(a<b)?-1:0; }
inline jint j2me_cmpg(jfloat  a, jfloat  b) { if (std::isnan(a)||std::isnan(b)) return  1; return (a>b)?1:(a<b)?-1:0; }
inline jint j2me_cmpl(jdouble a, jdouble b) { if (std::isnan(a)||std::isnan(b)) return -1; return (a>b)?1:(a<b)?-1:0; }
inline jint j2me_cmpg(jdouble a, jdouble b) { if (std::isnan(a)||std::isnan(b)) return  1; return (a>b)?1:(a<b)?-1:0; }

// String literal — returns a stable jref for a C string constant
inline jref j2me_string_literal(const char* s) { return (jref)(uintptr_t)s; }

// ── Object / array helpers ────────────────────────────────────────────────────
jref   j2me_new_object(const char* class_name);
jref   j2me_checkcast (jref obj, const char* class_name);
jint   j2me_instanceof(jref obj, const char* class_name);
void   j2me_throw     (jref exc);
jref   j2me_multi_anewarray(int dims, ...);

// ── Field access — typed, string-keyed ────────────────────────────────────────
jint    j2me_get_field_jint   (jref obj, const char* key);
jlong   j2me_get_field_jlong  (jref obj, const char* key);
jfloat  j2me_get_field_jfloat (jref obj, const char* key);
jdouble j2me_get_field_jdouble(jref obj, const char* key);
jref    j2me_get_field_jref   (jref obj, const char* key);
void    j2me_put_field_jint   (jref obj, const char* key, jint    val);
void    j2me_put_field_jlong  (jref obj, const char* key, jlong   val);
void    j2me_put_field_jfloat (jref obj, const char* key, jfloat  val);
void    j2me_put_field_jdouble(jref obj, const char* key, jdouble val);
void    j2me_put_field_jref   (jref obj, const char* key, jref    val);

jint    j2me_get_static_jint   (const char* key);
jlong   j2me_get_static_jlong  (const char* key);
jfloat  j2me_get_static_jfloat (const char* key);
jdouble j2me_get_static_jdouble(const char* key);
jref    j2me_get_static_jref   (const char* key);
void    j2me_put_static_jint   (const char* key, jint    val);
void    j2me_put_static_jlong  (const char* key, jlong   val);
void    j2me_put_static_jfloat (const char* key, jfloat  val);
void    j2me_put_static_jdouble(const char* key, jdouble val);
void    j2me_put_static_jref   (const char* key, jref    val);

// ── Virtual dispatch fallback ─────────────────────────────────────────────────
jint j2me_invoke_virtual(const char* sym, ...);
jint j2me_invoke_static (const char* sym, ...);
jint j2me_invoke_dynamic(jint cp_idx, ...);
)";
        h << "\n";
    }

    // ── j2me_glue.cpp ─────────────────────────────────────────────────────────
    {
        std::ofstream c(out_dir + "/j2me_glue.cpp");
        if (!c) return false;

        c << "// Auto-generated by j2me-recomp — DO NOT EDIT\n";
        c << "#include \"j2me_glue.h\"\n";
        c << "#include <cstring>\n";
        c << "#include <cstdarg>\n";
        c << "#include <cstdio>\n";
        c << "#include <unordered_map>\n";
        c << "#include <string>\n\n";

        c << R"(
// ── Object layout: flat struct of 64 jlong-aligned slots + type tag ───────────
struct J2MEObject {
    const char* class_name = nullptr;
    static constexpr int NSLOTS = 64;
    union Slot { jint i; jlong l; jfloat f; jdouble d; jref r; };
    Slot slots[NSLOTS] = {};
    // Field name → slot index (populated by generated class init)
    std::unordered_map<std::string, int> field_map;
};

static J2MEObject* obj_cast(jref r) { return static_cast<J2MEObject*>(r); }

jref j2me_new_object(const char* class_name) {
    auto* o = new J2MEObject();
    o->class_name = class_name;
    return o;
}
jref j2me_checkcast(jref obj, const char* class_name) {
    if (!obj) return nullptr;
    auto* o = obj_cast(obj);
    if (o->class_name && strcmp(o->class_name, class_name) != 0)
        j2me_throw_stub("ClassCastException", class_name);
    return obj;
}
jint j2me_instanceof(jref obj, const char* class_name) {
    if (!obj) return 0;
    auto* o = obj_cast(obj);
    return (o->class_name && strcmp(o->class_name, class_name) == 0) ? 1 : 0;
}
void j2me_throw(jref exc) {
    const char* name = exc ? obj_cast(exc)->class_name : "null";
    j2me_throw_stub(name ? name : "Exception", "thrown");
}
jref j2me_multi_anewarray(int dims, ...) {
    (void)dims; return j2me_new_array<jref>(dims);
}

// ── Field helpers (instance) ──────────────────────────────────────────────────
static int field_slot(jref obj, const char* key) {
    if (!obj) j2me_null_deref();
    auto* o = obj_cast(obj);
    auto it = o->field_map.find(key);
    if (it != o->field_map.end()) return it->second;
    // Auto-assign next slot
    int slot = (int)o->field_map.size();
    if (slot >= J2MEObject::NSLOTS) { std::fprintf(stderr,"[glue] Field overflow: %s\n",key); return 0; }
    o->field_map[key] = slot;
    return slot;
}

jint    j2me_get_field_jint   (jref o, const char* k) { return obj_cast(o)->slots[field_slot(o,k)].i; }
jlong   j2me_get_field_jlong  (jref o, const char* k) { return obj_cast(o)->slots[field_slot(o,k)].l; }
jfloat  j2me_get_field_jfloat (jref o, const char* k) { return obj_cast(o)->slots[field_slot(o,k)].f; }
jdouble j2me_get_field_jdouble(jref o, const char* k) { return obj_cast(o)->slots[field_slot(o,k)].d; }
jref    j2me_get_field_jref   (jref o, const char* k) { return obj_cast(o)->slots[field_slot(o,k)].r; }
void    j2me_put_field_jint   (jref o, const char* k, jint    v) { obj_cast(o)->slots[field_slot(o,k)].i = v; }
void    j2me_put_field_jlong  (jref o, const char* k, jlong   v) { obj_cast(o)->slots[field_slot(o,k)].l = v; }
void    j2me_put_field_jfloat (jref o, const char* k, jfloat  v) { obj_cast(o)->slots[field_slot(o,k)].f = v; }
void    j2me_put_field_jdouble(jref o, const char* k, jdouble v) { obj_cast(o)->slots[field_slot(o,k)].d = v; }
void    j2me_put_field_jref   (jref o, const char* k, jref    v) { obj_cast(o)->slots[field_slot(o,k)].r = v; }

// ── Static fields ─────────────────────────────────────────────────────────────
static std::unordered_map<std::string, J2MEObject::Slot> g_statics;

jint    j2me_get_static_jint   (const char* k) { return g_statics[k].i; }
jlong   j2me_get_static_jlong  (const char* k) { return g_statics[k].l; }
jfloat  j2me_get_static_jfloat (const char* k) { return g_statics[k].f; }
jdouble j2me_get_static_jdouble(const char* k) { return g_statics[k].d; }
jref    j2me_get_static_jref   (const char* k) { return g_statics[k].r; }
void    j2me_put_static_jint   (const char* k, jint    v) { g_statics[k].i = v; }
void    j2me_put_static_jlong  (const char* k, jlong   v) { g_statics[k].l = v; }
void    j2me_put_static_jfloat (const char* k, jfloat  v) { g_statics[k].f = v; }
void    j2me_put_static_jdouble(const char* k, jdouble v) { g_statics[k].d = v; }
void    j2me_put_static_jref   (const char* k, jref    v) { g_statics[k].r = v; }

// ── Virtual dispatch fallback ─────────────────────────────────────────────────
jint j2me_invoke_virtual(const char* sym, ...) {
    std::fprintf(stderr, "[glue] Unresolved virtual: %s\n", sym ? sym : "?");
    return 0;
}
jint j2me_invoke_static(const char* sym, ...) {
    std::fprintf(stderr, "[glue] Unresolved static: %s\n", sym ? sym : "?");
    return 0;
}
jint j2me_invoke_dynamic(jint cp_idx, ...) {
    std::fprintf(stderr, "[glue] invokedynamic cp=%d\n", cp_idx);
    return 0;
}
)";
    }

    std::cout << "[emitter] Wrote j2me_glue.h + j2me_glue.cpp to " << out_dir << "\n";
    return true;
}
