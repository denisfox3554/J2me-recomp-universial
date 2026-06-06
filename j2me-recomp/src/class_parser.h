#pragma once
#include "jar_loader.h"
#include <vector>
#include <cstdint>

class ClassParser {
public:
    ClassFile parse(const std::vector<uint8_t>& data);

private:
    const uint8_t* buf_ = nullptr;
    size_t         pos_ = 0;
    size_t         len_ = 0;

    uint8_t  u1();
    uint16_t u2();
    uint32_t u4();
    void     skip(size_t n);

    // cp_ is a local alias — will be moved into ClassFile::cp at end of parse()
    std::vector<CPEntry> cp_;

    std::string cp_utf8(uint16_t idx) const;
    std::string cp_class(uint16_t idx) const;
    std::string cp_name_and_type(uint16_t idx, std::string& desc) const;

    void        parse_constant_pool(uint16_t count);
    FieldInfo   parse_field();
    MethodInfo  parse_method();
};
