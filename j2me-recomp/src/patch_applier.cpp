#include "patch_applier.h"
#include <iostream>

void PatchApplier::apply(ClassFile& cls) const {
    for (auto& m : cls.methods) {
        // Symbol override
        if (auto* ov = cfg_.find_symbol(cls.name, m.name)) {
            m.override_symbol = ov->symbol;
        }
        // Stub → emit empty body
        if (cfg_.find_stub(cls.name, m.name)) {
            m.skip = true;
            std::cout << "  [patch] stub: " << cls.name << "::" << m.name << "\n";
            continue;
        }
        // Patch entry
        if (auto* p = cfg_.find_patch(cls.name, m.name)) {
            if (p->type == "skip_method") {
                m.skip = true;
                std::cout << "  [patch] skip: " << cls.name << "::" << m.name << "\n";
            } else if (p->type == "replace_call") {
                // Mark for emitter: redirect virtual call to p->symbol
                m.override_symbol = p->symbol;
                std::cout << "  [patch] replace_call: " << cls.name << "::" << m.name
                          << " → " << p->symbol << "\n";
            }
            // "nop" is handled per-bytecode in BytecodeLifter
        }
    }
}
