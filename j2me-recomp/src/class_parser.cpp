#include "class_parser.h"
#include <stdexcept>
#include <cstring>
#include <iostream>

// ── Reader helpers ─────────────────────────────────────────────────────────────
uint8_t  ClassParser::u1() { if (pos_+1>len_) throw std::runtime_error("EOF u1"); return buf_[pos_++]; }
uint16_t ClassParser::u2() { return (uint16_t)((u1()<<8)|u1()); }
uint32_t ClassParser::u4() { return ((uint32_t)u1()<<24)|((uint32_t)u1()<<16)|((uint32_t)u1()<<8)|u1(); }
void     ClassParser::skip(size_t n) { if (pos_+n>len_) throw std::runtime_error("EOF skip"); pos_+=n; }

// ── CP helpers ─────────────────────────────────────────────────────────────────
std::string ClassParser::cp_utf8(uint16_t idx) const {
    if (!idx || idx >= cp_.size()) return {};
    if (cp_[idx].tag == 1) return cp_[idx].str;
    return {};
}
std::string ClassParser::cp_class(uint16_t idx) const {
    if (!idx || idx >= cp_.size()) return {};
    if (cp_[idx].tag == 7) return cp_utf8(cp_[idx].ref1);
    return {};
}
std::string ClassParser::cp_name_and_type(uint16_t idx, std::string& desc) const {
    if (!idx || idx >= cp_.size()) return {};
    if (cp_[idx].tag == 12) { desc = cp_utf8(cp_[idx].ref2); return cp_utf8(cp_[idx].ref1); }
    return {};
}

// ── Constant pool ──────────────────────────────────────────────────────────────
void ClassParser::parse_constant_pool(uint16_t count) {
    cp_.resize(count);
    for (uint16_t i = 1; i < count; ++i) {
        uint8_t tag = u1();
        cp_[i].tag  = tag;
        switch (tag) {
            case 1: { uint16_t len=u2(); cp_[i].str.assign((char*)buf_+pos_,len); pos_+=len; break; }
            case 3:  { uint32_t v=u4(); memcpy(&cp_[i].ival,&v,4); break; }
            case 4:  { uint32_t v=u4(); memcpy(&cp_[i].fval,&v,4); break; }
            case 5:  { uint64_t v=((uint64_t)u4()<<32)|u4(); memcpy(&cp_[i].lval,&v,8); ++i; break; }
            case 6:  { uint64_t v=((uint64_t)u4()<<32)|u4(); memcpy(&cp_[i].dval,&v,8); ++i; break; }
            case 7:  cp_[i].ref1=u2(); break;
            case 8:  cp_[i].ref1=u2(); break;
            case 9: case 10: case 11:
                cp_[i].ref1=u2(); cp_[i].ref2=u2(); break;
            case 12: cp_[i].ref1=u2(); cp_[i].ref2=u2(); break;
            case 15: u1(); u2(); break;
            case 16: u2(); break;
            case 17: case 18: u2(); u2(); break;
            default:
                throw std::runtime_error(std::string("Unknown CP tag ")+std::to_string(tag)+" at index "+std::to_string(i));
        }
    }
    // Resolve tag-7 (Class) and tag-8 (String) str fields for fast lookup
    for (uint16_t i = 1; i < count; ++i) {
        if (cp_[i].tag == 7 || cp_[i].tag == 8)
            cp_[i].str = cp_utf8(cp_[i].ref1);
    }
}

// ── Field ──────────────────────────────────────────────────────────────────────
FieldInfo ClassParser::parse_field() {
    FieldInfo fi;
    uint16_t access = u2();
    fi.is_static = (access & 0x0008) != 0;
    fi.name       = cp_utf8(u2());
    fi.descriptor = cp_utf8(u2());
    uint16_t ac = u2();
    for (uint16_t a = 0; a < ac; ++a) { u2(); skip(u4()); }
    return fi;
}

// ── Method ─────────────────────────────────────────────────────────────────────
MethodInfo ClassParser::parse_method() {
    MethodInfo mi;
    uint16_t access = u2();
    mi.is_static = (access & 0x0008) != 0;
    mi.is_native = (access & 0x0100) != 0;
    mi.name       = cp_utf8(u2());
    mi.descriptor = cp_utf8(u2());
    uint16_t ac = u2();
    for (uint16_t a = 0; a < ac; ++a) {
        std::string aname = cp_utf8(u2());
        uint32_t    alen  = u4();
        size_t      after = pos_ + alen;
        if (aname == "Code") {
            mi.max_stack  = u2();
            mi.max_locals = u2();
            uint32_t clen = u4();
            mi.bytecode.resize(clen);
            for (uint32_t i = 0; i < clen; ++i) mi.bytecode[i] = u1();
            // skip exception table
            uint16_t ex = u2(); skip(ex * 8u);
            // skip Code sub-attributes (LineNumberTable, LocalVariableTable…)
            uint16_t sac = u2();
            for (uint16_t s = 0; s < sac; ++s) { u2(); skip(u4()); }
        }
        pos_ = after;
    }
    return mi;
}

// ── Main ───────────────────────────────────────────────────────────────────────
ClassFile ClassParser::parse(const std::vector<uint8_t>& data) {
    buf_ = data.data(); len_ = data.size(); pos_ = 0;
    cp_.clear();
    ClassFile cf;
    try {
        if (u4() != 0xCAFEBABE) { std::cerr << "[class_parser] Bad magic\n"; return cf; }
        u2(); u2(); // minor / major version
        parse_constant_pool(u2());

        uint16_t access = u2(); (void)access;
        cf.name       = cp_class(u2());
        uint16_t super_idx = u2();
        cf.super_name = super_idx ? cp_class(super_idx) : "";

        uint16_t ic = u2();
        for (uint16_t i = 0; i < ic; ++i) cf.interfaces.push_back(cp_class(u2()));

        uint16_t fc = u2();
        for (uint16_t i = 0; i < fc; ++i) cf.fields.push_back(parse_field());

        uint16_t mc = u2();
        for (uint16_t i = 0; i < mc; ++i) cf.methods.push_back(parse_method());

        // class attributes (skip)
        uint16_t cac = u2();
        for (uint16_t a = 0; a < cac; ++a) { u2(); skip(u4()); }

        // ── Hand off CP to ClassFile ───────────────────────────────────────
        cf.cp = std::move(cp_);

    } catch (const std::exception& e) {
        std::cerr << "[class_parser] " << e.what() << "\n";
    }
    return cf;
}
