#include "bytecode_lifter.h"
#include <cassert>
#include <stdexcept>
#include <iostream>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
// Operand stack simulation — maps JVM stack to SSA value IDs
// ─────────────────────────────────────────────────────────────────────────────
struct Stack {
    struct Slot { int id; IRType type; };
    std::vector<Slot> s;

    void push(int id, IRType t = IRType::Int) { s.push_back({id, t}); }
    Slot pop() { assert(!s.empty()); Slot v=s.back(); s.pop_back(); return v; }
    Slot top() const { assert(!s.empty()); return s.back(); }
    Slot peek(int n) const { return s[s.size()-1-n]; } // 0=top
    void dup()  { s.push_back(s.back()); }
    void dup2() { // Category-2 dup (long/double) or two category-1
        if (s.back().type==IRType::Long||s.back().type==IRType::Double) dup();
        else { Slot a=s.back(); Slot b=s[s.size()-2]; s.push_back(b); s.push_back(a); }
    }
    void dup_x1() { Slot a=pop(),b=pop(); push(a.id,a.type); push(b.id,b.type); push(a.id,a.type); }
    void swap()   { Slot a=pop(),b=pop(); push(a.id,a.type); push(b.id,b.type); }
    bool empty() const { return s.empty(); }
    void clear() { s.clear(); }
};

// ─────────────────────────────────────────────────────────────────────────────
// Bytecode reader helpers
// ─────────────────────────────────────────────────────────────────────────────
static uint8_t  bc_u1(const uint8_t* b, int& p)           { return b[p++]; }
static int8_t   bc_s1(const uint8_t* b, int& p)           { return (int8_t)b[p++]; }
static uint16_t bc_u2(const uint8_t* b, int& p)           { uint16_t v=(uint16_t)((b[p]<<8)|b[p+1]); p+=2; return v; }
static int16_t  bc_s2(const uint8_t* b, int& p)           { int16_t  v=(int16_t) ((b[p]<<8)|b[p+1]); p+=2; return v; }
static int32_t  bc_s4(const uint8_t* b, int& p)           { int32_t  v=(int32_t) ((uint32_t)b[p]<<24|(uint32_t)b[p+1]<<16|(uint32_t)b[p+2]<<8|b[p+3]); p+=4; return v; }

// Count parameters from descriptor "(II[Ljava/lang/String;)V"
static int count_params(const std::string& desc, bool is_static) {
    int n = is_static ? 0 : 1; // 'this'
    size_t i = 1; // skip '('
    while (i < desc.size() && desc[i] != ')') {
        switch (desc[i]) {
            case 'B': case 'C': case 'F': case 'I': case 'S': case 'Z': n++; i++; break;
            case 'D': case 'J': n+=2; i++; break; // wide
            case 'L': n++; while (desc[i]!=';') i++; i++; break;
            case '[': while (desc[i]=='[') i++; if (desc[i]=='L') { while (desc[i]!=';') i++; } i++; n++; break;
            default: i++; break;
        }
    }
    return n;
}

static IRType desc_return_type(const std::string& desc) {
    size_t p = desc.find(')');
    if (p == std::string::npos || p+1 >= desc.size()) return IRType::Void;
    char c = desc[p+1];
    switch (c) {
        case 'V': return IRType::Void;
        case 'J': return IRType::Long;
        case 'F': return IRType::Float;
        case 'D': return IRType::Double;
        case 'L': case '[': return IRType::Ref;
        default:  return IRType::Int;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
IRMethod BytecodeLifter::lift_method(const MethodInfo& mi) {
    IRMethod m;
    m.class_name   = cls_.name;
    m.method_name  = mi.name;
    m.descriptor   = mi.descriptor;
    m.is_static    = mi.is_static;
    m.num_locals   = mi.max_locals;
    m.num_params   = count_params(mi.descriptor, mi.is_static);

    if (mi.skip || mi.is_native || mi.bytecode.empty()) {
        // Emit a bare return
        IRInstr ret{}; ret.op = IROp::Return; ret.type = IRType::Void;
        m.instrs.push_back(ret);
        return m;
    }

    const uint8_t* bc  = mi.bytecode.data();
    int            len = (int)mi.bytecode.size();
    int            pc  = 0;
    Stack          stk;

    // local[i] → current SSA id mapping
    std::vector<int> locals(mi.max_locals, -1);
    // Pre-fill parameters as locals 0..num_params-1
    // (we emit LoadLocal with id= param index for the emitter)
    for (int i = 0; i < mi.max_locals; ++i) locals[i] = -(i+1); // sentinel

    auto emit = [&](IRInstr ins) -> int {
        ins.bc_off = pc;
        m.instrs.push_back(ins);
        return ins.dst;
    };

    auto new_val = [&](IRType t = IRType::Int) -> int {
        return m.next_val_id++;
    };

    auto const_int = [&](int64_t v) -> int {
        int d = new_val(IRType::Int);
        IRInstr ins{}; ins.op=IROp::Const; ins.dst=d; ins.type=IRType::Int; ins.imm_i=v;
        emit(ins); stk.push(d, IRType::Int);
        return d;
    };
    auto const_long = [&](int64_t v) -> int {
        int d = new_val(IRType::Long);
        IRInstr ins{}; ins.op=IROp::Const; ins.dst=d; ins.type=IRType::Long; ins.imm_i=v;
        emit(ins); stk.push(d, IRType::Long);
        return d;
    };
    auto const_float = [&](float v) -> int {
        int d = new_val(IRType::Float);
        IRInstr ins{}; ins.op=IROp::Const; ins.dst=d; ins.type=IRType::Float; ins.imm_d=(double)v;
        emit(ins); stk.push(d, IRType::Float);
        return d;
    };
    auto const_double = [&](double v) -> int {
        int d = new_val(IRType::Double);
        IRInstr ins{}; ins.op=IROp::Const; ins.dst=d; ins.type=IRType::Double; ins.imm_d=v;
        emit(ins); stk.push(d, IRType::Double);
        return d;
    };
    auto load_local = [&](int idx, IRType t) -> int {
        int d = new_val(t);
        IRInstr ins{}; ins.op=IROp::LoadLocal; ins.dst=d; ins.type=t; ins.imm_i=idx;
        emit(ins); stk.push(d, t);
        return d;
    };
    auto store_local = [&](int idx, IRType t) {
        auto [sid, st] = stk.pop();
        IRInstr ins{}; ins.op=IROp::StoreLocal; ins.type=t; ins.imm_i=idx; ins.srcs={sid};
        emit(ins);
        locals[idx] = sid;
    };
    auto binop = [&](IROp op, IRType t) {
        auto [b,tb]=stk.pop(); auto [a,ta]=stk.pop();
        int d=new_val(t);
        IRInstr ins{}; ins.op=op; ins.dst=d; ins.type=t; ins.srcs={a,b};
        emit(ins); stk.push(d,t);
    };
    auto unop = [&](IROp op, IRType src_t, IRType dst_t) {
        auto [a,ta]=stk.pop(); int d=new_val(dst_t);
        IRInstr ins{}; ins.op=op; ins.dst=d; ins.type=dst_t; ins.srcs={a};
        emit(ins); stk.push(d,dst_t);
    };

    while (pc < len) {
        int opc_pc = pc;
        uint8_t opc = bc_u1(bc, pc);

        switch (opc) {
        // ── Constants ──────────────────────────────────────────────────────
        case 0x00: { IRInstr n{}; n.op=IROp::Nop; emit(n); break; } // nop
        case 0x01: { int d=new_val(IRType::Ref); IRInstr n{}; n.op=IROp::Const; n.dst=d; n.type=IRType::Ref; n.imm_i=0; emit(n); stk.push(d,IRType::Ref); break; } // aconst_null
        case 0x02: const_int(-1); break; // iconst_m1
        case 0x03: const_int(0);  break;
        case 0x04: const_int(1);  break;
        case 0x05: const_int(2);  break;
        case 0x06: const_int(3);  break;
        case 0x07: const_int(4);  break;
        case 0x08: const_int(5);  break;
        case 0x09: const_long(0); break;
        case 0x0a: const_long(1); break;
        case 0x0b: const_float(0.0f); break;
        case 0x0c: const_float(1.0f); break;
        case 0x0d: const_float(2.0f); break;
        case 0x0e: const_double(0.0); break;
        case 0x0f: const_double(1.0); break;
        case 0x10: const_int((int8_t)bc_s1(bc,pc)); break; // bipush
        case 0x11: const_int(bc_s2(bc,pc)); break;         // sipush
        case 0x12: case 0x13: { // ldc / ldc_w
            uint16_t idx = (opc==0x12) ? bc_u1(bc,pc) : bc_u2(bc,pc);
            IRInstr n{}; n.op=IROp::Const; n.imm_i=idx;
            if (idx < cls_.cp.size()) {
                const CPEntry& e = cls_.cp[idx];
                if (e.tag==3) { n.type=IRType::Int;  n.imm_i=e.ival; }
                else if (e.tag==4) { n.type=IRType::Float; n.imm_d=e.fval; }
                else if (e.tag==8) { n.type=IRType::Ref;   n.imm_s=cls_.cp_string(idx); n.imm_i=idx; }
                else if (e.tag==7) { n.type=IRType::Ref;   n.imm_s=cls_.cp_class_name(idx); n.imm_i=idx; }
                else               { n.type=IRType::Int;   n.imm_i=idx; }
            } else { n.type=IRType::Int; }
            n.dst=new_val(n.type);
            emit(n); stk.push(n.dst, n.type);
            break;
        }
        case 0x14: { // ldc2_w (long or double)
            uint16_t idx=bc_u2(bc,pc);
            IRInstr n{}; n.op=IROp::Const; n.imm_i=idx;
            if (idx < cls_.cp.size()) {
                const CPEntry& e = cls_.cp[idx];
                if (e.tag==5) { n.type=IRType::Long;   n.imm_i=e.lval; }
                else if (e.tag==6) { n.type=IRType::Double; n.imm_d=e.dval; }
                else { n.type=IRType::Long; }
            } else { n.type=IRType::Long; }
            n.dst=new_val(n.type);
            emit(n); stk.push(n.dst, n.type);
            break;
        }

        // ── Loads ──────────────────────────────────────────────────────────
        case 0x15: load_local(bc_u1(bc,pc), IRType::Int);    break; // iload
        case 0x16: load_local(bc_u1(bc,pc), IRType::Long);   break; // lload
        case 0x17: load_local(bc_u1(bc,pc), IRType::Float);  break; // fload
        case 0x18: load_local(bc_u1(bc,pc), IRType::Double); break; // dload
        case 0x19: load_local(bc_u1(bc,pc), IRType::Ref);    break; // aload
        case 0x1a: load_local(0, IRType::Int);    break; // iload_0
        case 0x1b: load_local(1, IRType::Int);    break;
        case 0x1c: load_local(2, IRType::Int);    break;
        case 0x1d: load_local(3, IRType::Int);    break;
        case 0x1e: load_local(0, IRType::Long);   break; // lload_0
        case 0x1f: load_local(1, IRType::Long);   break;
        case 0x20: load_local(2, IRType::Long);   break;
        case 0x21: load_local(3, IRType::Long);   break;
        case 0x22: load_local(0, IRType::Float);  break; // fload_0
        case 0x23: load_local(1, IRType::Float);  break;
        case 0x24: load_local(2, IRType::Float);  break;
        case 0x25: load_local(3, IRType::Float);  break;
        case 0x26: load_local(0, IRType::Double); break; // dload_0
        case 0x27: load_local(1, IRType::Double); break;
        case 0x28: load_local(2, IRType::Double); break;
        case 0x29: load_local(3, IRType::Double); break;
        case 0x2a: load_local(0, IRType::Ref);    break; // aload_0
        case 0x2b: load_local(1, IRType::Ref);    break;
        case 0x2c: load_local(2, IRType::Ref);    break;
        case 0x2d: load_local(3, IRType::Ref);    break;
        // Array loads
        case 0x2e: { auto a=stk.pop(),i=stk.pop(); int d=new_val(IRType::Int);    IRInstr n{}; n.op=IROp::ArrayLoad; n.dst=d; n.type=IRType::Int;    n.srcs={i.id,a.id}; emit(n); stk.push(d,IRType::Int); break; } // iaload
        case 0x2f: { auto a=stk.pop(),i=stk.pop(); int d=new_val(IRType::Long);   IRInstr n{}; n.op=IROp::ArrayLoad; n.dst=d; n.type=IRType::Long;   n.srcs={i.id,a.id}; emit(n); stk.push(d,IRType::Long); break; } // laload
        case 0x30: { auto a=stk.pop(),i=stk.pop(); int d=new_val(IRType::Float);  IRInstr n{}; n.op=IROp::ArrayLoad; n.dst=d; n.type=IRType::Float;  n.srcs={i.id,a.id}; emit(n); stk.push(d,IRType::Float); break; } // faload
        case 0x31: { auto a=stk.pop(),i=stk.pop(); int d=new_val(IRType::Double); IRInstr n{}; n.op=IROp::ArrayLoad; n.dst=d; n.type=IRType::Double; n.srcs={i.id,a.id}; emit(n); stk.push(d,IRType::Double); break; } // daload
        case 0x32: { auto a=stk.pop(),i=stk.pop(); int d=new_val(IRType::Ref);    IRInstr n{}; n.op=IROp::ArrayLoad; n.dst=d; n.type=IRType::Ref;    n.srcs={i.id,a.id}; emit(n); stk.push(d,IRType::Ref); break; } // aaload
        case 0x33: { auto a=stk.pop(),i=stk.pop(); int d=new_val(IRType::Int);    IRInstr n{}; n.op=IROp::ArrayLoad; n.dst=d; n.type=IRType::Int;    n.srcs={i.id,a.id}; emit(n); stk.push(d,IRType::Int); break; } // baload
        case 0x34: { auto a=stk.pop(),i=stk.pop(); int d=new_val(IRType::Int);    IRInstr n{}; n.op=IROp::ArrayLoad; n.dst=d; n.type=IRType::Int;    n.srcs={i.id,a.id}; emit(n); stk.push(d,IRType::Int); break; } // caload
        case 0x35: { auto a=stk.pop(),i=stk.pop(); int d=new_val(IRType::Int);    IRInstr n{}; n.op=IROp::ArrayLoad; n.dst=d; n.type=IRType::Int;    n.srcs={i.id,a.id}; emit(n); stk.push(d,IRType::Int); break; } // saload

        // ── Stores ─────────────────────────────────────────────────────────
        case 0x36: store_local(bc_u1(bc,pc), IRType::Int);    break; // istore
        case 0x37: store_local(bc_u1(bc,pc), IRType::Long);   break;
        case 0x38: store_local(bc_u1(bc,pc), IRType::Float);  break;
        case 0x39: store_local(bc_u1(bc,pc), IRType::Double); break;
        case 0x3a: store_local(bc_u1(bc,pc), IRType::Ref);    break;
        case 0x3b: store_local(0, IRType::Int);    break; // istore_0
        case 0x3c: store_local(1, IRType::Int);    break;
        case 0x3d: store_local(2, IRType::Int);    break;
        case 0x3e: store_local(3, IRType::Int);    break;
        case 0x3f: store_local(0, IRType::Long);   break; // lstore_0
        case 0x40: store_local(1, IRType::Long);   break;
        case 0x41: store_local(2, IRType::Long);   break;
        case 0x42: store_local(3, IRType::Long);   break;
        case 0x43: store_local(0, IRType::Float);  break; // fstore_0
        case 0x44: store_local(1, IRType::Float);  break;
        case 0x45: store_local(2, IRType::Float);  break;
        case 0x46: store_local(3, IRType::Float);  break;
        case 0x47: store_local(0, IRType::Double); break; // dstore_0
        case 0x48: store_local(1, IRType::Double); break;
        case 0x49: store_local(2, IRType::Double); break;
        case 0x4a: store_local(3, IRType::Double); break;
        case 0x4b: store_local(0, IRType::Ref);    break; // astore_0
        case 0x4c: store_local(1, IRType::Ref);    break;
        case 0x4d: store_local(2, IRType::Ref);    break;
        case 0x4e: store_local(3, IRType::Ref);    break;
        // Array stores
        case 0x4f: { auto v=stk.pop(),i=stk.pop(),a=stk.pop(); IRInstr n{}; n.op=IROp::ArrayStore; n.type=IRType::Int;    n.srcs={a.id,i.id,v.id}; emit(n); break; }
        case 0x50: { auto v=stk.pop(),i=stk.pop(),a=stk.pop(); IRInstr n{}; n.op=IROp::ArrayStore; n.type=IRType::Long;   n.srcs={a.id,i.id,v.id}; emit(n); break; }
        case 0x51: { auto v=stk.pop(),i=stk.pop(),a=stk.pop(); IRInstr n{}; n.op=IROp::ArrayStore; n.type=IRType::Float;  n.srcs={a.id,i.id,v.id}; emit(n); break; }
        case 0x52: { auto v=stk.pop(),i=stk.pop(),a=stk.pop(); IRInstr n{}; n.op=IROp::ArrayStore; n.type=IRType::Double; n.srcs={a.id,i.id,v.id}; emit(n); break; }
        case 0x53: { auto v=stk.pop(),i=stk.pop(),a=stk.pop(); IRInstr n{}; n.op=IROp::ArrayStore; n.type=IRType::Ref;    n.srcs={a.id,i.id,v.id}; emit(n); break; }
        case 0x54: case 0x55: case 0x56: { auto v=stk.pop(),i=stk.pop(),a=stk.pop(); IRInstr n{}; n.op=IROp::ArrayStore; n.type=IRType::Int; n.srcs={a.id,i.id,v.id}; emit(n); break; } // bastore castore sastore

        // ── Stack ops ──────────────────────────────────────────────────────
        case 0x57: stk.pop();  break; // pop
        case 0x58: stk.pop(); if (!stk.empty()) stk.pop(); break; // pop2
        case 0x59: stk.dup();  break; // dup
        case 0x5a: stk.dup_x1(); break; // dup_x1
        case 0x5b: { auto a=stk.pop(),b=stk.pop(),c=stk.pop(); stk.push(a.id,a.type); stk.push(c.id,c.type); stk.push(b.id,b.type); stk.push(a.id,a.type); break; } // dup_x2
        case 0x5c: stk.dup2(); break; // dup2
        case 0x5d: { auto a=stk.pop(),b=stk.pop(); stk.push(a.id,a.type); stk.push(b.id,b.type); stk.push(a.id,a.type); break; } // dup2_x1
        case 0x5e: { auto a=stk.pop(),b=stk.pop(),c=stk.pop(); stk.push(a.id,a.type); stk.push(c.id,c.type); stk.push(b.id,b.type); stk.push(a.id,a.type); break; } // dup2_x2
        case 0x5f: stk.swap(); break; // swap

        // ── Arithmetic ─────────────────────────────────────────────────────
        case 0x60: binop(IROp::Add, IRType::Int);    break; // iadd
        case 0x61: binop(IROp::Add, IRType::Long);   break;
        case 0x62: binop(IROp::Add, IRType::Float);  break;
        case 0x63: binop(IROp::Add, IRType::Double); break;
        case 0x64: binop(IROp::Sub, IRType::Int);    break;
        case 0x65: binop(IROp::Sub, IRType::Long);   break;
        case 0x66: binop(IROp::Sub, IRType::Float);  break;
        case 0x67: binop(IROp::Sub, IRType::Double); break;
        case 0x68: binop(IROp::Mul, IRType::Int);    break;
        case 0x69: binop(IROp::Mul, IRType::Long);   break;
        case 0x6a: binop(IROp::Mul, IRType::Float);  break;
        case 0x6b: binop(IROp::Mul, IRType::Double); break;
        case 0x6c: binop(IROp::Div, IRType::Int);    break;
        case 0x6d: binop(IROp::Div, IRType::Long);   break;
        case 0x6e: binop(IROp::Div, IRType::Float);  break;
        case 0x6f: binop(IROp::Div, IRType::Double); break;
        case 0x70: binop(IROp::Rem, IRType::Int);    break;
        case 0x71: binop(IROp::Rem, IRType::Long);   break;
        case 0x72: binop(IROp::Rem, IRType::Float);  break;
        case 0x73: binop(IROp::Rem, IRType::Double); break;
        case 0x74: unop(IROp::Neg, IRType::Int,    IRType::Int);    break;
        case 0x75: unop(IROp::Neg, IRType::Long,   IRType::Long);   break;
        case 0x76: unop(IROp::Neg, IRType::Float,  IRType::Float);  break;
        case 0x77: unop(IROp::Neg, IRType::Double, IRType::Double); break;
        case 0x78: binop(IROp::Shl,  IRType::Int);  break; // ishl
        case 0x79: binop(IROp::Shl,  IRType::Long); break;
        case 0x7a: binop(IROp::Shr,  IRType::Int);  break;
        case 0x7b: binop(IROp::Shr,  IRType::Long); break;
        case 0x7c: binop(IROp::Ushr, IRType::Int);  break;
        case 0x7d: binop(IROp::Ushr, IRType::Long); break;
        case 0x7e: binop(IROp::And, IRType::Int);   break;
        case 0x7f: binop(IROp::And, IRType::Long);  break;
        case 0x80: binop(IROp::Or,  IRType::Int);   break;
        case 0x81: binop(IROp::Or,  IRType::Long);  break;
        case 0x82: binop(IROp::Xor, IRType::Int);   break;
        case 0x83: binop(IROp::Xor, IRType::Long);  break;
        case 0x84: { // iinc
            uint8_t idx=(uint8_t)bc_u1(bc,pc);
            int8_t  cv =(int8_t) bc_s1(bc,pc);
            // load, add const, store
            int ld=new_val(IRType::Int);
            IRInstr li{}; li.op=IROp::LoadLocal; li.dst=ld; li.type=IRType::Int; li.imm_i=idx; emit(li);
            int cv_id=new_val(IRType::Int);
            IRInstr ci{}; ci.op=IROp::Const; ci.dst=cv_id; ci.type=IRType::Int; ci.imm_i=cv; emit(ci);
            int res=new_val(IRType::Int);
            IRInstr ai{}; ai.op=IROp::Add; ai.dst=res; ai.type=IRType::Int; ai.srcs={ld,cv_id}; emit(ai);
            IRInstr si{}; si.op=IROp::StoreLocal; si.type=IRType::Int; si.imm_i=idx; si.srcs={res}; emit(si);
            break;
        }

        // ── Conversions ────────────────────────────────────────────────────
        case 0x85: unop(IROp::I2L, IRType::Int,    IRType::Long);   break;
        case 0x86: unop(IROp::I2F, IRType::Int,    IRType::Float);  break;
        case 0x87: unop(IROp::I2D, IRType::Int,    IRType::Double); break;
        case 0x88: unop(IROp::L2I, IRType::Long,   IRType::Int);    break;
        case 0x89: unop(IROp::L2F, IRType::Long,   IRType::Float);  break;
        case 0x8a: unop(IROp::L2D, IRType::Long,   IRType::Double); break;
        case 0x8b: unop(IROp::F2I, IRType::Float,  IRType::Int);    break;
        case 0x8c: unop(IROp::F2L, IRType::Float,  IRType::Long);   break;
        case 0x8d: unop(IROp::F2D, IRType::Float,  IRType::Double); break;
        case 0x8e: unop(IROp::D2I, IRType::Double, IRType::Int);    break;
        case 0x8f: unop(IROp::D2L, IRType::Double, IRType::Long);   break;
        case 0x90: unop(IROp::D2F, IRType::Double, IRType::Float);  break;
        case 0x91: unop(IROp::I2B, IRType::Int,    IRType::Int);    break;
        case 0x92: unop(IROp::I2C, IRType::Int,    IRType::Int);    break;
        case 0x93: unop(IROp::I2S, IRType::Int,    IRType::Int);    break;

        // ── Comparisons ────────────────────────────────────────────────────
        case 0x94: binop(IROp::Cmp,  IRType::Long);   break; // lcmp
        case 0x95: binop(IROp::Cmpl, IRType::Float);  break; // fcmpl
        case 0x96: binop(IROp::Cmpg, IRType::Float);  break; // fcmpg
        case 0x97: binop(IROp::Cmpl, IRType::Double); break; // dcmpl
        case 0x98: binop(IROp::Cmpg, IRType::Double); break; // dcmpg

        // ── If branches ────────────────────────────────────────────────────
        case 0x99: case 0x9a: case 0x9b: case 0x9c: case 0x9d: case 0x9e: { // ifeq..ifle
            int16_t off=bc_s2(bc,pc); int tgt=opc_pc+off;
            auto [a,ta]=stk.pop(); int zero=new_val(IRType::Int);
            IRInstr ci{}; ci.op=IROp::Const; ci.dst=zero; ci.type=IRType::Int; ci.imm_i=0; emit(ci);
            IRInstr n{}; n.op=IROp::If; n.type=IRType::Int; n.srcs={a,zero}; n.imm_i=opc-0x99; n.targets={tgt,pc};
            emit(n); break;
        }
        case 0x9f: case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: { // if_icmpeq..if_icmple
            int16_t off=bc_s2(bc,pc); int tgt=opc_pc+off;
            auto [b,tb]=stk.pop(); auto [a,ta]=stk.pop();
            IRInstr n{}; n.op=IROp::If; n.type=IRType::Int; n.srcs={a,b}; n.imm_i=opc-0x9f; n.targets={tgt,pc};
            emit(n); break;
        }
        case 0xa5: case 0xa6: { // if_acmpeq if_acmpne
            int16_t off=bc_s2(bc,pc); int tgt=opc_pc+off;
            auto [b,tb]=stk.pop(); auto [a,ta]=stk.pop();
            IRInstr n{}; n.op=IROp::If; n.type=IRType::Ref; n.srcs={a,b}; n.imm_i=opc-0xa5; n.targets={tgt,pc};
            emit(n); break;
        }

        // ── Goto ───────────────────────────────────────────────────────────
        case 0xa7: { int16_t off=bc_s2(bc,pc); IRInstr n{}; n.op=IROp::Goto; n.targets={opc_pc+off}; emit(n); break; }
        case 0xc8: { int32_t off=bc_s4(bc,pc); IRInstr n{}; n.op=IROp::Goto; n.targets={opc_pc+(int)off}; emit(n); break; } // goto_w

        // ── jsr / ret (deprecated, stub) ──────────────────────────────────
        case 0xa8: bc_s2(bc,pc); { IRInstr n{}; n.op=IROp::Nop; emit(n); } break; // jsr
        case 0xa9: bc_u1(bc,pc); { IRInstr n{}; n.op=IROp::Nop; emit(n); } break; // ret

        // ── Switch ─────────────────────────────────────────────────────────
        case 0xaa: { // tableswitch
            while (pc%4) pc++;
            int32_t def=bc_s4(bc,pc), lo=bc_s4(bc,pc), hi=bc_s4(bc,pc);
            auto [k,tk]=stk.pop();
            IRInstr n{}; n.op=IROp::TableSwitch; n.srcs={k}; n.imm_i=lo;
            n.targets.push_back(opc_pc+def);
            for (int32_t v=lo; v<=hi; ++v) n.targets.push_back(opc_pc+bc_s4(bc,pc));
            emit(n); break;
        }
        case 0xab: { // lookupswitch
            while (pc%4) pc++;
            int32_t def=bc_s4(bc,pc), npairs=bc_s4(bc,pc);
            auto [k,tk]=stk.pop();
            IRInstr n{}; n.op=IROp::LookupSwitch; n.srcs={k};
            n.targets.push_back(opc_pc+def);
            for (int32_t i=0; i<npairs; ++i) {
                int32_t match=bc_s4(bc,pc), off=bc_s4(bc,pc);
                n.imm_s += std::to_string(match)+":"+std::to_string(opc_pc+off)+";";
            }
            emit(n); break;
        }

        // ── Returns ────────────────────────────────────────────────────────
        case 0xac: { auto [v,t]=stk.pop(); IRInstr n{}; n.op=IROp::ReturnValue; n.type=IRType::Int;    n.srcs={v}; emit(n); break; } // ireturn
        case 0xad: { auto [v,t]=stk.pop(); IRInstr n{}; n.op=IROp::ReturnValue; n.type=IRType::Long;   n.srcs={v}; emit(n); break; } // lreturn
        case 0xae: { auto [v,t]=stk.pop(); IRInstr n{}; n.op=IROp::ReturnValue; n.type=IRType::Float;  n.srcs={v}; emit(n); break; } // freturn
        case 0xaf: { auto [v,t]=stk.pop(); IRInstr n{}; n.op=IROp::ReturnValue; n.type=IRType::Double; n.srcs={v}; emit(n); break; } // dreturn
        case 0xb0: { auto [v,t]=stk.pop(); IRInstr n{}; n.op=IROp::ReturnValue; n.type=IRType::Ref;    n.srcs={v}; emit(n); break; } // areturn
        case 0xb1: { IRInstr n{}; n.op=IROp::Return; n.type=IRType::Void; emit(n); break; }                                          // return

        // ── Field access ───────────────────────────────────────────────────
        case 0xb2: { // getstatic
            uint16_t idx=bc_u2(bc,pc);
            std::string fcls,fname,fdesc; cp_member(idx,fcls,fname,fdesc);
            IRType ft = desc_return_type(fdesc.empty() ? "()I" : "()" + fdesc);
            int d=new_val(ft);
            IRInstr n{}; n.op=IROp::GetStatic; n.dst=d; n.type=ft;
            n.imm_s=fcls+"."+fname; n.imm_s2=fdesc; n.imm_i=idx;
            emit(n); stk.push(d,ft); break;
        }
        case 0xb3: { // putstatic
            uint16_t idx=bc_u2(bc,pc);
            std::string fcls,fname,fdesc; cp_member(idx,fcls,fname,fdesc);
            auto [v,tv]=stk.pop();
            IRInstr n{}; n.op=IROp::PutStatic;
            n.imm_s=fcls+"."+fname; n.imm_s2=fdesc; n.imm_i=idx; n.srcs={v};
            emit(n); break;
        }
        case 0xb4: { // getfield
            uint16_t idx=bc_u2(bc,pc);
            std::string fcls,fname,fdesc; cp_member(idx,fcls,fname,fdesc);
            IRType ft = desc_return_type(fdesc.empty() ? "()I" : "()" + fdesc);
            auto [obj,to]=stk.pop(); int d=new_val(ft);
            IRInstr n{}; n.op=IROp::GetField; n.dst=d; n.type=ft;
            n.imm_s=fcls+"."+fname; n.imm_s2=fdesc; n.imm_i=idx; n.srcs={obj};
            emit(n); stk.push(d,ft); break;
        }
        case 0xb5: { // putfield
            uint16_t idx=bc_u2(bc,pc);
            std::string fcls,fname,fdesc; cp_member(idx,fcls,fname,fdesc);
            auto [v,tv]=stk.pop(); auto [obj,to]=stk.pop();
            IRInstr n{}; n.op=IROp::PutField;
            n.imm_s=fcls+"."+fname; n.imm_s2=fdesc; n.imm_i=idx; n.srcs={obj,v};
            emit(n); break;
        }

        // ── Invokes ────────────────────────────────────────────────────────
        case 0xb6: case 0xb7: case 0xb8: case 0xb9: { // invokevirtual..invokeinterface
            uint16_t idx=bc_u2(bc,pc);
            if (opc==0xb9) { bc_u1(bc,pc); bc_u1(bc,pc); } // count+0
            IROp iop = (opc==0xb8) ? IROp::InvokeStatic :
                       (opc==0xb7) ? IROp::InvokeSpecial :
                       (opc==0xb9) ? IROp::InvokeInterface : IROp::InvokeVirtual;
            std::string icls,iname,idesc; cp_member(idx,icls,iname,idesc);
            IRType ret_t = desc_return_type(idesc);

            // Pop arguments from stack: count from descriptor + implicit 'this'
            int nargs = count_params(idesc, opc==0xb8);
            std::vector<int> args(nargs);
            for (int a=nargs-1; a>=0; --a) args[a]=stk.pop().id;

            IRInstr n{}; n.op=iop; n.type=ret_t;
            n.imm_s=icls+"::"+iname; n.imm_s2=idesc; n.imm_i=idx;
            n.srcs=args;
            if (ret_t != IRType::Void) {
                n.dst = new_val(ret_t);
                emit(n);
                stk.push(n.dst, ret_t);
            } else {
                n.dst = -1;
                emit(n);
            }
            break;
        }
        case 0xba: { // invokedynamic
            uint16_t idx=bc_u2(bc,pc); bc_u2(bc,pc);
            int d=new_val(IRType::Ref);
            IRInstr n{}; n.op=IROp::InvokeStatic; n.dst=d; n.imm_i=idx;
            n.imm_s="invokedynamic"; n.type=IRType::Ref;
            emit(n); stk.push(d,IRType::Ref); break;
        }

        // ── Object creation ────────────────────────────────────────────────
        case 0xbb: { // new
            uint16_t idx=bc_u2(bc,pc);
            std::string cname = cp_class(idx);
            int d=new_val(IRType::Ref);
            IRInstr n{}; n.op=IROp::New; n.dst=d; n.imm_s=cname; n.imm_i=idx;
            emit(n); stk.push(d,IRType::Ref); break;
        }
        case 0xbc: { // newarray
            uint8_t atype=bc_u1(bc,pc); auto [cnt,tc]=stk.pop(); int d=new_val(IRType::Ref);
            IRInstr n{}; n.op=IROp::NewArray; n.dst=d; n.srcs={cnt}; n.imm_i=atype;
            emit(n); stk.push(d,IRType::Ref); break;
        }
        case 0xbd: { // anewarray
            uint16_t idx=bc_u2(bc,pc);
            std::string cname = cp_class(idx);
            auto [cnt,tc]=stk.pop(); int d=new_val(IRType::Ref);
            IRInstr n{}; n.op=IROp::ANewArray; n.dst=d; n.srcs={cnt}; n.imm_s=cname; n.imm_i=idx;
            emit(n); stk.push(d,IRType::Ref); break;
        }
        case 0xbe: { // arraylength
            auto [arr,ta]=stk.pop(); int d=new_val(IRType::Int);
            IRInstr n{}; n.op=IROp::ArrayLength; n.dst=d; n.srcs={arr};
            emit(n); stk.push(d,IRType::Int); break;
        }
        case 0xbf: { // athrow
            auto [exc,te]=stk.pop();
            IRInstr n{}; n.op=IROp::Throw; n.srcs={exc}; emit(n); break;
        }
        case 0xc0: { // checkcast
            uint16_t idx=bc_u2(bc,pc);
            std::string cname=cp_class(idx);
            auto [obj,to]=stk.pop(); int d=new_val(IRType::Ref);
            IRInstr n{}; n.op=IROp::CheckCast; n.dst=d; n.srcs={obj}; n.imm_s=cname; n.imm_i=idx;
            emit(n); stk.push(d,IRType::Ref); break;
        }
        case 0xc1: { // instanceof
            uint16_t idx=bc_u2(bc,pc);
            std::string cname=cp_class(idx);
            auto [obj,to]=stk.pop(); int d=new_val(IRType::Int);
            IRInstr n{}; n.op=IROp::InstanceOf; n.dst=d; n.srcs={obj}; n.imm_s=cname; n.imm_i=idx;
            emit(n); stk.push(d,IRType::Int); break;
        }
        case 0xc2: { IRInstr n{}; n.op=IROp::MonitorEnter; auto[o,t]=stk.pop(); n.srcs={o}; emit(n); break; } // monitorenter
        case 0xc3: { IRInstr n{}; n.op=IROp::MonitorExit;  auto[o,t]=stk.pop(); n.srcs={o}; emit(n); break; } // monitorexit

        // ── Wide prefix ────────────────────────────────────────────────────
        case 0xc4: { // wide
            uint8_t w_opc=bc_u1(bc,pc); uint16_t w_idx=bc_u2(bc,pc);
            switch (w_opc) {
                case 0x15: load_local(w_idx, IRType::Int);    break;
                case 0x16: load_local(w_idx, IRType::Long);   break;
                case 0x17: load_local(w_idx, IRType::Float);  break;
                case 0x18: load_local(w_idx, IRType::Double); break;
                case 0x19: load_local(w_idx, IRType::Ref);    break;
                case 0x36: store_local(w_idx, IRType::Int);   break;
                case 0x37: store_local(w_idx, IRType::Long);  break;
                case 0x38: store_local(w_idx, IRType::Float); break;
                case 0x39: store_local(w_idx, IRType::Double);break;
                case 0x3a: store_local(w_idx, IRType::Ref);   break;
                case 0x84: { int16_t cv=bc_s2(bc,pc);
                    int ld=new_val(IRType::Int); IRInstr li{}; li.op=IROp::LoadLocal; li.dst=ld; li.type=IRType::Int; li.imm_i=w_idx; emit(li);
                    int cv_id=new_val(IRType::Int); IRInstr ci{}; ci.op=IROp::Const; ci.dst=cv_id; ci.type=IRType::Int; ci.imm_i=cv; emit(ci);
                    int res=new_val(IRType::Int); IRInstr ai{}; ai.op=IROp::Add; ai.dst=res; ai.type=IRType::Int; ai.srcs={ld,cv_id}; emit(ai);
                    IRInstr si{}; si.op=IROp::StoreLocal; si.type=IRType::Int; si.imm_i=w_idx; si.srcs={res}; emit(si); break; }
                default: { IRInstr n{}; n.op=IROp::Nop; emit(n); }
            }
            break;
        }
        case 0xc5: { // multianewarray
            uint16_t idx=bc_u2(bc,pc); uint8_t dims=bc_u1(bc,pc);
            int d=new_val(IRType::Ref);
            IRInstr n{}; n.op=IROp::MultiANewArray; n.dst=d; n.imm_i=idx; n.imm_s=std::to_string(dims);
            for (uint8_t i=0; i<dims; ++i) { auto[s,t]=stk.pop(); n.srcs.push_back(s); }
            emit(n); stk.push(d,IRType::Ref); break;
        }
        case 0xc6: { // ifnull
            int16_t off=bc_s2(bc,pc); auto [a,ta]=stk.pop(); int zero=new_val(IRType::Ref);
            IRInstr ci{}; ci.op=IROp::Const; ci.dst=zero; ci.type=IRType::Ref; ci.imm_i=0; emit(ci);
            IRInstr n{}; n.op=IROp::If; n.type=IRType::Ref; n.srcs={a,zero}; n.imm_i=0; n.targets={opc_pc+off,pc};
            emit(n); break;
        }
        case 0xc7: { // ifnonnull
            int16_t off=bc_s2(bc,pc); auto [a,ta]=stk.pop(); int zero=new_val(IRType::Ref);
            IRInstr ci{}; ci.op=IROp::Const; ci.dst=zero; ci.type=IRType::Ref; ci.imm_i=0; emit(ci);
            IRInstr n{}; n.op=IROp::If; n.type=IRType::Ref; n.srcs={a,zero}; n.imm_i=1; n.targets={opc_pc+off,pc};
            emit(n); break;
        }
        case 0xc9: { // jsr_w (deprecated)
            bc_s4(bc,pc); IRInstr n{}; n.op=IROp::Nop; emit(n); break;
        }
        default:
            std::cerr << "[lifter] Unknown opcode 0x" << std::hex << (int)opc
                      << " at " << std::dec << opc_pc << " in "
                      << cls_.name << "::" << mi.name << "\n";
            { IRInstr n{}; n.op=IROp::Nop; emit(n); }
            break;
        }
    }
    return m;
}

std::vector<IRMethod> BytecodeLifter::lift() {
    std::vector<IRMethod> result;
    for (auto& mi : cls_.methods) {
        try {
            result.push_back(lift_method(mi));
        } catch (const std::exception& e) {
            std::cerr << "[lifter] " << cls_.name << "::" << mi.name
                      << " — " << e.what() << "\n";
        }
    }
    return result;
}

IRType BytecodeLifter::cp_return_type(uint16_t idx) const {
    std::string cls, name, desc;
    cls_.cp_member_ref(idx, cls, name, desc);
    return desc_return_type(desc);
}
