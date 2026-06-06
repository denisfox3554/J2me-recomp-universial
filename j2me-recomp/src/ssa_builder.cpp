#include "ssa_builder.h"
#include <cassert>

void SSABuilder::push_def(int var, int val_id) {
    if (var < 0 || var >= num_locals_) return;
    def_stack_[var].push_back(val_id);
}
int SSABuilder::current_def(int var) const {
    if (var < 0 || var >= num_locals_) return -1;
    if (def_stack_[var].empty()) return -1;
    return def_stack_[var].back();
}
void SSABuilder::pop_def(int var) {
    if (var < 0 || var >= num_locals_) return;
    if (!def_stack_[var].empty()) def_stack_[var].pop_back();
}

void SSABuilder::rename_block(CFG& cfg, IRMethod& m, int block_id) {
    BasicBlock& bb = cfg.block(block_id);

    // Track which vars we pushed in this block (to pop on exit)
    std::vector<int> pushed_vars;

    // ── Register phi destinations as new definitions ──────────────────────
    for (auto& [var, phi_dst] : bb.phi_dsts) {
        push_def(var, phi_dst);
        pushed_vars.push_back(var);
    }

    // ── Rename instructions ───────────────────────────────────────────────
    for (int idx : bb.instr_indices) {
        IRInstr& ins = m.instrs[idx];

        // Rewrite uses of LoadLocal
        if (ins.op == IROp::LoadLocal) {
            int var = (int)ins.imm_i;
            int cur = current_def(var);
            if (cur != -1) {
                // Replace this load with a direct reference to cur
                // The emitter will treat this LoadLocal's dst as an alias
                ins.srcs = {cur};  // src[0] = reaching def
            }
        }

        // Rewrite new definition from StoreLocal
        if (ins.op == IROp::StoreLocal) {
            int var = (int)ins.imm_i;
            // dst of store is the value being stored (in srcs[0])
            int new_def = ins.srcs.empty() ? -1 : ins.srcs[0];
            if (new_def != -1) {
                push_def(var, new_def);
                pushed_vars.push_back(var);
            }
        }
    }

    // ── Fill phi sources in successors ────────────────────────────────────
    for (int succ_id : bb.succs) {
        BasicBlock& succ = cfg.block(succ_id);
        for (auto& [var, phi_dst] : succ.phi_dsts) {
            int cur = current_def(var);
            succ.phi_srcs[phi_dst].push_back({block_id, cur});
        }
    }

    // ── Recurse into dominator children ──────────────────────────────────
    for (int child : bb.dom_children)
        rename_block(cfg, m, child);

    // ── Pop defs introduced in this block ─────────────────────────────────
    for (int var : pushed_vars) pop_def(var);
}

void SSABuilder::rename(CFG& cfg, IRMethod& m) {
    num_locals_ = m.num_locals;
    def_stack_.assign(num_locals_, {});

    // Seed parameters
    for (int i = 0; i < m.num_params && i < num_locals_; ++i) {
        int param_val = m.next_val_id++;
        push_def(i, param_val);
    }

    // Iterative dominator-tree walk using explicit stack
    // Each entry: {block_id, child_index, pushed_vars_snapshot_size}
    struct Frame {
        int block_id;
        int child_idx;
        std::vector<int> pushed_vars;
    };

    std::vector<Frame> stk;
    stk.push_back({cfg.entry, 0, {}});

    // Process entry block immediately
    {
        Frame& f = stk.back();
        BasicBlock& bb = cfg.block(f.block_id);

        for (auto& [var, phi_dst] : bb.phi_dsts) {
            push_def(var, phi_dst);
            f.pushed_vars.push_back(var);
        }
        for (int idx : bb.instr_indices) {
            IRInstr& ins = m.instrs[idx];
            if (ins.op == IROp::LoadLocal) {
                int cur = current_def((int)ins.imm_i);
                if (cur != -1) ins.srcs = {cur};
            }
            if (ins.op == IROp::StoreLocal) {
                int var = (int)ins.imm_i;
                int new_def = ins.srcs.empty() ? -1 : ins.srcs[0];
                if (new_def != -1) { push_def(var, new_def); f.pushed_vars.push_back(var); }
            }
        }
        for (int succ_id : bb.succs) {
            BasicBlock& succ = cfg.block(succ_id);
            for (auto& [var, phi_dst] : succ.phi_dsts) {
                int cur = current_def(var);
                succ.phi_srcs[phi_dst].push_back({f.block_id, cur});
            }
        }
    }

    while (!stk.empty()) {
        Frame& f = stk.back();
        BasicBlock& bb = cfg.block(f.block_id);

        if (f.child_idx < (int)bb.dom_children.size()) {
            int child_id = bb.dom_children[f.child_idx++];
            Frame child_frame;
            child_frame.block_id  = child_id;
            child_frame.child_idx = 0;

            BasicBlock& child = cfg.block(child_id);
            for (auto& [var, phi_dst] : child.phi_dsts) {
                push_def(var, phi_dst);
                child_frame.pushed_vars.push_back(var);
            }
            for (int idx : child.instr_indices) {
                IRInstr& ins = m.instrs[idx];
                if (ins.op == IROp::LoadLocal) {
                    int cur = current_def((int)ins.imm_i);
                    if (cur != -1) ins.srcs = {cur};
                }
                if (ins.op == IROp::StoreLocal) {
                    int var = (int)ins.imm_i;
                    int new_def = ins.srcs.empty() ? -1 : ins.srcs[0];
                    if (new_def != -1) { push_def(var, new_def); child_frame.pushed_vars.push_back(var); }
                }
            }
            for (int succ_id : child.succs) {
                BasicBlock& succ = cfg.block(succ_id);
                for (auto& [var, phi_dst] : succ.phi_dsts) {
                    int cur = current_def(var);
                    succ.phi_srcs[phi_dst].push_back({child_id, cur});
                }
            }
            stk.push_back(std::move(child_frame));
        } else {
            // Pop: restore defs
            for (int var : f.pushed_vars) pop_def(var);
            stk.pop_back();
        }
    }
}
