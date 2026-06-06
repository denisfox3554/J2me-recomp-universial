#pragma once
#include "cfg_builder.h"

// ─────────────────────────────────────────────────────────────────────────────
// SSA renaming — walks the dominator tree and rewrites value references
// so every use refers to the single reaching definition (phi or store).
// Implements the "Simple and Efficient Construction of Static Single
// Assignment Form" (Braun et al., 2013) variant.
// ─────────────────────────────────────────────────────────────────────────────
class SSABuilder {
public:
    void rename(CFG& cfg, IRMethod& m);

private:
    // Current definition of each local per block during walk
    // def_stack[var] = stack of value ids (top = current reaching def)
    std::vector<std::vector<int>> def_stack_;
    int num_locals_ = 0;

    void rename_block(CFG& cfg, IRMethod& m, int block_id);
    void push_def(int var, int val_id);
    int  current_def(int var) const;
    void pop_def(int var);
};
