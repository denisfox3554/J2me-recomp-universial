#pragma once
#include "bytecode_lifter.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>

// ─────────────────────────────────────────────────────────────────────────────
// Basic block
// ─────────────────────────────────────────────────────────────────────────────
struct BasicBlock {
    int              id       = 0;
    int              bc_start = 0;  // bytecode offset of first instruction
    int              bc_end   = 0;  // exclusive
    std::vector<int> instr_indices; // indices into IRMethod::instrs
    std::vector<int> succs;
    std::vector<int> preds;
    int              idom     = -1; // immediate dominator block id
    std::vector<int> dom_children;
    std::set<int>    dom_frontier;

    // Phi nodes: variable (local idx or stack slot id) → phi dst value id
    std::unordered_map<int, int> phi_dsts;  // var → new phi value id
    // Phi sources: phi dst → list of (pred_block_id, src_value_id)
    std::unordered_map<int, std::vector<std::pair<int,int>>> phi_srcs;
};

// ─────────────────────────────────────────────────────────────────────────────
struct CFG {
    std::vector<BasicBlock>  blocks;
    int                      entry = 0;
    // Post-order and RPO lists
    std::vector<int>         rpo;  // reverse post-order block ids

    BasicBlock& block(int id) { return blocks[id]; }
    const BasicBlock& block(int id) const { return blocks[id]; }
    int num_blocks() const { return (int)blocks.size(); }
};

// ─────────────────────────────────────────────────────────────────────────────
class CFGBuilder {
public:
    // Build CFG from flat IR instruction list, then compute dominators + phis
    CFG build(IRMethod& m);

private:
    // Phase 1 — identify basic block leaders
    std::unordered_set<int> find_leaders(const IRMethod& m);
    // Phase 2 — construct blocks and edges
    void build_blocks(CFG& cfg, IRMethod& m,
                      const std::unordered_set<int>& leaders);
    // Phase 3 — dominator tree (Cooper et al. simple iterative)
    void compute_dominators(CFG& cfg);
    void compute_dom_frontiers(CFG& cfg);
    // Phase 4 — phi insertion (standard DF-based algorithm)
    void insert_phis(CFG& cfg, IRMethod& m);
    // Phase 5 — RPO numbering
    void compute_rpo(CFG& cfg);

    // Helper
    int  block_for_offset(const CFG& cfg, int bc_off) const;
    std::unordered_map<int,int> offset_to_block_; // bc_off → block id
};
