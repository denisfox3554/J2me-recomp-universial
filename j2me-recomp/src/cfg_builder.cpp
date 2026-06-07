#include "cfg_builder.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <functional>
#include <climits>
#include <stack>
#include <queue>

// ─────────────────────────────────────────────────────────────────────────────
// Phase 1 — find basic block leaders
// A leader is:
//   • the first instruction
//   • any branch target
//   • the instruction immediately after a branch/return
// ─────────────────────────────────────────────────────────────────────────────
std::unordered_set<int> CFGBuilder::find_leaders(const IRMethod& m) {
    std::unordered_set<int> leaders;
    if (m.instrs.empty()) return leaders;
    leaders.insert(m.instrs[0].bc_off);  // entry = first instruction's bc_off

    for (int i = 0; i < (int)m.instrs.size(); ++i) {
        const IRInstr& ins = m.instrs[i];
        bool is_branch = (ins.op == IROp::Goto   ||
                          ins.op == IROp::If      ||
                          ins.op == IROp::TableSwitch ||
                          ins.op == IROp::LookupSwitch||
                          ins.op == IROp::Return  ||
                          ins.op == IROp::ReturnValue ||
                          ins.op == IROp::Throw);
        if (is_branch) {
            // Branch targets are already bc_off values
            for (int t : ins.targets) leaders.insert(t);
            // Instruction after branch
            if (i+1 < (int)m.instrs.size())
                leaders.insert(m.instrs[i+1].bc_off);
        }
    }
    return leaders;
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 2 — build block list and CFG edges
// ─────────────────────────────────────────────────────────────────────────────
void CFGBuilder::build_blocks(CFG& cfg, IRMethod& m,
                               const std::unordered_set<int>& leaders)
{
    // leaders contains bc_off values.
    // Build a map: bc_off → instruction index
    std::unordered_map<int,int> bcoff_to_instr;
    for (int i = 0; i < (int)m.instrs.size(); ++i)
        bcoff_to_instr[m.instrs[i].bc_off] = i;

    // Sort leader bc_offs
    std::vector<int> sorted_leaders(leaders.begin(), leaders.end());
    std::sort(sorted_leaders.begin(), sorted_leaders.end());

    cfg.blocks.clear();
    offset_to_block_.clear();

    // For each leader range, find the instruction indices that belong to it
    for (int i = 0; i < (int)sorted_leaders.size(); ++i) {
        int leader_off = sorted_leaders[i];
        int next_off   = (i+1 < (int)sorted_leaders.size())
                         ? sorted_leaders[i+1]
                         : INT_MAX;

        // Find start instruction index for this leader
        auto it = bcoff_to_instr.find(leader_off);
        if (it == bcoff_to_instr.end()) continue; // leader has no instruction

        BasicBlock bb;
        bb.id       = (int)cfg.blocks.size();
        bb.bc_start = leader_off;
        bb.bc_end   = next_off;

        // Collect all instructions whose bc_off is in [leader_off, next_off)
        int start_idx = it->second;
        for (int j = start_idx; j < (int)m.instrs.size(); ++j) {
            if (m.instrs[j].bc_off >= next_off) break;
            bb.instr_indices.push_back(j);
        }

        offset_to_block_[leader_off] = bb.id;
        cfg.blocks.push_back(std::move(bb));
    }

    // Wire edges
    for (auto& bb : cfg.blocks) {
        if (bb.instr_indices.empty()) continue;
        const IRInstr& last = m.instrs[bb.instr_indices.back()];

        auto add_edge = [&](int src, int dst_off) {
            auto it2 = offset_to_block_.find(dst_off);
            if (it2 == offset_to_block_.end()) return;
            int dst = it2->second;
            // Avoid duplicate edges
            for (int s : cfg.blocks[src].succs)
                if (s == dst) return;
            cfg.blocks[src].succs.push_back(dst);
            cfg.blocks[dst].preds.push_back(src);
        };

        switch (last.op) {
            case IROp::Return:
            case IROp::ReturnValue:
            case IROp::Throw:
                break;
            case IROp::Goto:
                if (!last.targets.empty()) add_edge(bb.id, last.targets[0]);
                break;
            case IROp::If:
                if (last.targets.size() >= 2) {
                    add_edge(bb.id, last.targets[0]);
                    add_edge(bb.id, last.targets[1]);
                }
                break;
            case IROp::TableSwitch:
            case IROp::LookupSwitch:
                for (int t : last.targets) add_edge(bb.id, t);
                break;
            default:
                // Fall-through: find the block that starts right after this one
                if (bb.id + 1 < cfg.num_blocks())
                    add_edge(bb.id, bb.id + 1);
                break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 3 — Cooper et al. iterative dominator algorithm
// ─────────────────────────────────────────────────────────────────────────────
void CFGBuilder::compute_rpo(CFG& cfg) {
    int n = cfg.num_blocks();
    std::vector<bool> visited(n, false);
    cfg.rpo.clear();

    // Iterative DFS using explicit stack to avoid stack overflow
    std::vector<std::pair<int,int>> stk; // {block_id, successor_index}
    stk.reserve(n);
    stk.push_back({0, 0});
    visited[0] = true;

    while (!stk.empty()) {
        auto& [u, si] = stk.back();
        if (si < (int)cfg.blocks[u].succs.size()) {
            int s = cfg.blocks[u].succs[si++];
            if (!visited[s]) {
                visited[s] = true;
                stk.push_back({s, 0});
            }
        } else {
            cfg.rpo.push_back(u);
            stk.pop_back();
        }
    }
    std::reverse(cfg.rpo.begin(), cfg.rpo.end());
}

static int intersect(const std::vector<int>& doms,
                     const std::vector<int>& rpo_num, int b1, int b2) {
    while (b1 != b2) {
        while (rpo_num[b1] > rpo_num[b2]) b1 = doms[b1];
        while (rpo_num[b2] > rpo_num[b1]) b2 = doms[b2];
    }
    return b1;
}

void CFGBuilder::compute_dominators(CFG& cfg) {
    compute_rpo(cfg);
    int n = cfg.num_blocks();

    std::vector<int> rpo_num(n, 0);
    for (int i = 0; i < (int)cfg.rpo.size(); ++i)
        rpo_num[cfg.rpo[i]] = i;

    std::vector<int> doms(n, -1);
    doms[cfg.entry] = cfg.entry;

    bool changed = true;
    while (changed) {
        changed = false;
        for (int b : cfg.rpo) {
            if (b == cfg.entry) continue;
            int new_idom = -1;
            for (int p : cfg.blocks[b].preds) {
                if (doms[p] == -1) continue;
                if (new_idom == -1) { new_idom = p; continue; }
                new_idom = intersect(doms, rpo_num, p, new_idom);
            }
            if (new_idom != -1 && doms[b] != new_idom) {
                doms[b] = new_idom;
                changed = true;
            }
        }
    }

    for (int i = 0; i < n; ++i) {
        cfg.blocks[i].idom = doms[i];
        if (i != cfg.entry && doms[i] != -1)
            cfg.blocks[doms[i]].dom_children.push_back(i);
    }
}

void CFGBuilder::compute_dom_frontiers(CFG& cfg) {
    for (auto& bb : cfg.blocks) {
        if (bb.preds.size() < 2) continue;  // only join points have DF
        for (int b : bb.preds) {
            int runner = b;
            int safety = cfg.num_blocks() + 1;
            while (runner != bb.idom && safety-- > 0) {
                if (runner < 0 || runner >= cfg.num_blocks()) break;
                cfg.blocks[runner].dom_frontier.insert(bb.id);
                int next = cfg.blocks[runner].idom;
                if (next == runner || next < 0) break;
                runner = next;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 4 — phi insertion
// Track which locals are defined in each block, insert phi nodes at DF
// boundaries for each local that has multiple reaching definitions.
// ─────────────────────────────────────────────────────────────────────────────
void CFGBuilder::insert_phis(CFG& cfg, IRMethod& m) {
    int n  = cfg.num_blocks();
    int nl = m.num_locals;
    if (nl <= 0 || n <= 1) return;

    std::vector<std::unordered_set<int>> def_blocks(nl);

    for (auto& bb : cfg.blocks) {
        for (int idx : bb.instr_indices) {
            if (idx < 0 || idx >= (int)m.instrs.size()) continue;
            const IRInstr& ins = m.instrs[idx];
            if (ins.op == IROp::StoreLocal) {
                int var = (int)ins.imm_i;
                if (var >= 0 && var < nl)
                    def_blocks[var].insert(bb.id);
            }
        }
    }

    for (int var = 0; var < nl; ++var) {
        if (def_blocks[var].empty()) continue;
        std::vector<int> worklist(def_blocks[var].begin(), def_blocks[var].end());
        std::unordered_set<int> phi_placed;

        int safety = n * nl + 1;
        while (!worklist.empty() && safety-- > 0) {
            int b = worklist.back(); worklist.pop_back();
            if (b < 0 || b >= n) continue;
            for (int df : cfg.blocks[b].dom_frontier) {
                if (df < 0 || df >= n) continue;
                if (phi_placed.count(df)) continue;
                phi_placed.insert(df);
                int phi_dst = m.next_val_id++;
                cfg.blocks[df].phi_dsts[var] = phi_dst;
                if (!def_blocks[var].count(df))
                    worklist.push_back(df);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public entry
// ─────────────────────────────────────────────────────────────────────────────
CFG CFGBuilder::build(IRMethod& m) {
    CFG cfg;
    cfg.entry = 0;

    if (m.instrs.empty()) {
        BasicBlock bb; bb.id=0; bb.bc_start=0; bb.bc_end=0;
        cfg.blocks.push_back(bb); cfg.rpo.push_back(0);
        return cfg;
    }

    auto leaders = find_leaders(m);

    build_blocks(cfg, m, leaders);

    compute_dominators(cfg);

    compute_dom_frontiers(cfg);

    insert_phis(cfg, m);

    return cfg;
}

int CFGBuilder::block_for_offset(const CFG& cfg, int bc_off) const {
    auto it = offset_to_block_.find(bc_off);
    return (it != offset_to_block_.end()) ? it->second : -1;
}
