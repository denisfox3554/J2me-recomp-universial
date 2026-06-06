#pragma once
#include "jar_loader.h"
#include "toml_config.h"

class PatchApplier {
public:
    explicit PatchApplier(const TomlConfig& cfg) : cfg_(cfg) {}
    void apply(ClassFile& cls) const;
private:
    const TomlConfig& cfg_;
};
