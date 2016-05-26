// Copyright (c) 2014-2016 Dropbox, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PYSTON_CORE_CFG_H
#define PYSTON_CORE_CFG_H

/*
 * This CFG is a relatively high-level CFG, closely corresponding to the input Python source.  We break down
 * control-flow constructs,
 * but it doesn't do things like decompose IfExpressions or short-circuit conditional expressions.
 * Those will have to get broken into low-level control flow, so this CFG doesn't exactly correspond to the llvm-level
 * one we will eventually
 * generate; this one is (at least for now) meant to be a slightly-lowered version of the input AST, for doing
 * relatively high-level things such
 * as type analysis, liveness, etc, and then using as the source representation for the next lowering pass (emitting
 * llvm SSA)
 */

#include <vector>

#include "core/ast.h"
#include "core/common.h"
#include "core/stringpool.h"

namespace pyston {

class AST_stmt;
class Box;

class CFG;
class ParamNames;
class ScopeInfo;

class CFGBlock {
public:
    CFG* cfg;

    // Baseline JIT helper fields:
    // contains address to the start of the code of this basic block
    void* code;
    // contains the address of the entry function
    std::pair<CFGBlock*, Box*>(*entry_code)(void* interpeter, CFGBlock* block, Box** vregs);

    llvm::SmallVector<AST_stmt*, 4> body;
    llvm::SmallVector<CFGBlock*, 2> predecessors, successors;
    int idx; // index in the CFG
    const char* info;

    typedef llvm::SmallVector<AST_stmt*, 4>::iterator iterator;

    CFGBlock(CFG* cfg, int idx) : cfg(cfg), code(NULL), entry_code(NULL), idx(idx), info(NULL) {}

    void connectTo(CFGBlock* successor, bool allow_backedge = false);
    void unconnectFrom(CFGBlock* successor);

    void push_back(AST_stmt* node) { body.push_back(node); }
    void print(llvm::raw_ostream& stream = llvm::outs());
    void _print() { print(); }
};

// the vregs are split into three parts.
// user visible: used for all non compiler generated names, name could be used in a single block or multiple
//               all frames contain atleast this vregs in order to do frame introspection
// cross block : used for compiler generated names which get used in several blocks or which have closure scope
// single block: used by compiler created names which are only used in a single block.
//               get reused for different names
//
// we assign the lowest numbers to the user visible ones, followed by the cross block ones and finally the single block
// ones. we do this because not all tiers use all of the vregs and it still makes it fast to switch between tiers.
//
// usage by our different tiers:
// interpreter : [user visible] [cross block] [single block]
// baseline jit: [user visible] [cross block]
// llvm jit    : [user visible]
class VRegInfo {
private:
    llvm::DenseMap<InternedString, int> sym_vreg_map_user_visible;
    llvm::DenseMap<InternedString, int> sym_vreg_map;

    // Reverse map, from vreg->symbol name.
    std::vector<InternedString> vreg_sym_map;

    int num_vregs_cross_block = -1;
    int num_vregs = -1;

public:
    // map of all assigned names. if the name is block local the vreg number is not unique because this vregs get reused
    // between blocks.
    const llvm::DenseMap<InternedString, int>& getSymVRegMap() { return sym_vreg_map; }
    const llvm::DenseMap<InternedString, int>& getUserVisibleSymVRegMap() { return sym_vreg_map_user_visible; }

    int getVReg(InternedString name) const {
        assert(hasVRegsAssigned());
        assert(sym_vreg_map.count(name));
        auto it = sym_vreg_map.find(name);
        assert(it != sym_vreg_map.end());
        assert(it->second != -1);
        return it->second;
    }

    InternedString getName(int vreg) const {
        assert(hasVRegsAssigned());
        assert(vreg >= 0 && vreg < num_vregs);
        return vreg_sym_map[vreg];
    }

    bool isUserVisibleVReg(int vreg) const { return vreg < sym_vreg_map_user_visible.size(); }
    bool isCrossBlockVReg(int vreg) const { return !isUserVisibleVReg(vreg) && vreg < num_vregs_cross_block; }
    bool isBlockLocalVReg(int vreg) const { return vreg >= num_vregs_cross_block; }

    int getTotalNumOfVRegs() const { return num_vregs; }
    int getNumOfUserVisibleVRegs() const { return sym_vreg_map_user_visible.size(); }
    int getNumOfCrossBlockVRegs() const { return num_vregs_cross_block; }

    bool hasVRegsAssigned() const { return num_vregs != -1; }
    void assignVRegs(CFG* cfg, const ParamNames& param_names, ScopeInfo* scope_info);
};

// Control Flow Graph
class CFG {
private:
    int next_idx;
    VRegInfo vreg_info;

public:
    std::vector<CFGBlock*> blocks;

public:
    CFG() : next_idx(0) {}

    CFGBlock* getStartingBlock() { return blocks[0]; }
    VRegInfo& getVRegInfo() { return vreg_info; }

    CFGBlock* addBlock() {
        int idx = next_idx;
        next_idx++;
        CFGBlock* block = new CFGBlock(this, idx);
        blocks.push_back(block);

        return block;
    }

    // Creates a block which must be placed later, using placeBlock().
    // Must be placed on same CFG it was created on.
    // You can also safely delete it without placing it.
    CFGBlock* addDeferredBlock() {
        CFGBlock* block = new CFGBlock(this, -1);
        return block;
    }

    void placeBlock(CFGBlock* block) {
        assert(block->idx == -1);
        block->idx = next_idx;
        next_idx++;
        blocks.push_back(block);
    }

    void print(llvm::raw_ostream& stream = llvm::outs());
};

class SourceInfo;
CFG* computeCFG(SourceInfo* source, std::vector<AST_stmt*> body, const ParamNames& param_names);
void printCFG(CFG* cfg);
}

#endif
