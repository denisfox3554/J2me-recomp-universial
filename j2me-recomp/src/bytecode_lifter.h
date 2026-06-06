#pragma once
#include "jar_loader.h"
#include <vector>
#include <string>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// IR types
// ─────────────────────────────────────────────────────────────────────────────
enum class IRType { Void, Int, Long, Float, Double, Ref, RetAddr };

enum class IROp {
    Add, Sub, Mul, Div, Rem, Neg,
    Shl, Shr, Ushr, And, Or, Xor,
    I2L, I2F, I2D, L2I, L2F, L2D,
    F2I, F2L, F2D, D2I, D2L, D2F,
    I2B, I2C, I2S,
    Cmp, Cmpl, Cmpg,
    LoadLocal, StoreLocal,
    NewArray, ANewArray, MultiANewArray,
    ArrayLoad, ArrayStore, ArrayLength,
    New, GetField, PutField, GetStatic, PutStatic,
    CheckCast, InstanceOf, Throw,
    InvokeVirtual, InvokeSpecial, InvokeStatic, InvokeInterface,
    Goto, If, TableSwitch, LookupSwitch,
    Return, ReturnValue,
    Const,
    MonitorEnter, MonitorExit,
    Nop,
};

struct IRInstr {
    IROp             op      = IROp::Nop;
    IRType           type    = IRType::Int;
    int              dst     = -1;
    std::vector<int> srcs;
    int64_t          imm_i   = 0;
    double           imm_d   = 0;
    std::string      imm_s;   // resolved name: "com/example/Foo" or "Foo.field:I"
    std::string      imm_s2;  // descriptor for invoke
    std::vector<int> targets; // branch target bc offsets
    int              bc_off  = 0;
};

struct IRMethod {
    std::string            class_name;
    std::string            method_name;
    std::string            descriptor;
    bool                   is_static  = false;
    int                    num_locals = 0;
    int                    num_params = 0;
    std::vector<IRInstr>   instrs;
    int                    next_val_id = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
class BytecodeLifter {
public:
    explicit BytecodeLifter(const ClassFile& cls) : cls_(cls) {}
    std::vector<IRMethod> lift();

private:
    const ClassFile& cls_;
    IRMethod lift_method(const MethodInfo& mi);

    // CP convenience (delegates to ClassFile::cp helpers)
    std::string cp_class(uint16_t idx) const { return cls_.cp_class_name(idx); }
    void cp_member(uint16_t idx, std::string& cls, std::string& name, std::string& desc) const {
        cls_.cp_member_ref(idx, cls, name, desc);
    }
    std::string cp_string(uint16_t idx) const { return cls_.cp_string(idx); }
    IRType cp_return_type(uint16_t idx) const;  // from method descriptor in CP
};
