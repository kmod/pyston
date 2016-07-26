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

#include <deque>

#include "asm_writing/assembler.h"
#include "codegen/irgen.h"
#include "core/cfg.h"
#include "runtime/objmodel.h"
#include "runtime/types.h"

using namespace pyston::assembler;

namespace pyston {

class SJit {
private:
    FunctionMetadata* md;
    SourceInfo* source;
    ParamNames* param_names;
    const OSREntryDescriptor* entry_descriptor;
    EffortLevel effort;
    ExceptionStyle exception_style;
    FunctionSpecialization* spec;
    llvm::StringRef nameprefix;

    Assembler a;
    int sp_adjustment;
    int scratch_rsp_offset;
    int vregs_rsp_offset;

    typedef UnboundJump<1048576> Jump;
    std::deque<llvm::SmallVector<std::unique_ptr<Jump>, 2>> jump_list;

    Indirect vregStackSlot(AST_Name* name) {
        assert(name->lookup_type == ScopeInfo::VarScopeType::FAST
               || name->lookup_type == ScopeInfo::VarScopeType::CLOSURE);

        int vreg = name->vreg;
        assert(vreg != -1);
        int rsp_offset = vregs_rsp_offset + 8 * vreg;
        return Indirect(RSP, rsp_offset);
    }

    Indirect scratchSlot(int idx) {
        assert(idx < (vregs_rsp_offset - scratch_rsp_offset) / 8);
        return Indirect(RSP, scratch_rsp_offset + 8 * idx);
    }

    Location evalExpr(AST_expr* expr, Location dest = Location::any(), bool can_use_others = false) {
        assert(dest.type == Location::Register || dest.type == Location::AnyReg);
        Register dest_reg = RAX;
        if (dest.type == Location::Register)
            dest_reg = dest.asRegister();

        if (expr->type == AST_TYPE::MakeFunction || expr->type == AST_TYPE::MakeClass) {
            if (VERBOSITY())
                printf("Aborting sjit; unhandled expr type %d\n", expr->type);
            return Location();
        }

        if (expr->type == AST_TYPE::Num) {
            auto num = ast_cast<AST_Num>(expr);
            RELEASE_ASSERT(num->num_type == AST_Num::INT, "");

            Box* b = source->parent_module->getIntConstant(num->n_int);
            a.mov(Immediate(b), dest_reg);
            return dest_reg;
        }

        if (expr->type == AST_TYPE::Name) {
            auto name = ast_cast<AST_Name>(expr);

            RELEASE_ASSERT(name->lookup_type == ScopeInfo::VarScopeType::FAST, "");

            auto slot = vregStackSlot(name);

            bool definitely_defined = false;

            if (!source->cfg->getVRegInfo().isUserVisibleVReg(name->vreg))
                definitely_defined = true;

            if (!definitely_defined) {
                assert(can_use_others);

                // TODO: use definedness analysis here
                a.mov(slot, RDI);
                a.test(RDI, RDI);
                a.setne(RDI);
                a.mov(Immediate((void*)name->id.c_str()), RSI);
                a.mov(Immediate(NameError), RDX);
                a.mov(Immediate(1), RCX);
                RELEASE_ASSERT(exception_style == CXX, "");
                //a.call(Immediate((void*)assertNameDefined));
                a.mov(Immediate((void*)assertNameDefined), R11);
                a.callq(R11);
            }

            a.mov(slot, dest_reg);
            return dest_reg;
        }

        if (expr->type == AST_TYPE::Compare) {
            auto compare = ast_cast<AST_Compare>(expr);
            assert(compare->ops.size() == 1);
            assert(compare->comparators.size() == 1);

            assert(can_use_others);

            auto r1 = evalExpr(compare->left, RDI);
            assert(r1 == RDI);
            auto r2 = evalExpr(compare->comparators[0], RSI);
            assert(r2 == RSI);
            a.mov(Immediate(compare->ops[0]), RDX);
            //a.call(Immediate((void*)pyston::compare));
            a.mov(Immediate((void*)pyston::compare), R11);
            a.callq(R11);
            if (dest.type != Location::AnyReg && dest_reg != RAX) {
                a.mov(RAX, dest_reg);
                return dest_reg;
            }
            return RAX;
        }

        if (expr->type == AST_TYPE::BinOp) {
            auto binop = ast_cast<AST_BinOp>(expr);

            assert(can_use_others);

            auto r1 = evalExpr(binop->left, RDI);
            assert(r1 == RDI);
            auto r2 = evalExpr(binop->right, RSI);
            assert(r2 == RSI);
            a.mov(Immediate(binop->op_type), RDX);
            //a.call(Immediate((void*)pyston::binop));
            a.mov(Immediate((void*)pyston::binop), R11);
            a.callq(R11);
            if (dest.type != Location::AnyReg && dest_reg != RAX) {
                a.mov(RAX, dest_reg);
                return dest_reg;
            }
            return RAX;
        }

        RELEASE_ASSERT(0, "unhandled expr type: %d", expr->type);
    }

public:
#define MEM_SIZE (1024 * 64)

    SJit(FunctionMetadata* md, SourceInfo* source, ParamNames* param_names, const OSREntryDescriptor* entry_descriptor,
         EffortLevel effort, ExceptionStyle exception_style, FunctionSpecialization* spec, llvm::StringRef nameprefix)
        : md(md),
          source(source),
          param_names(param_names),
          entry_descriptor(entry_descriptor),
          exception_style(exception_style),
          spec(spec),
          nameprefix(nameprefix),
          a((uint8_t*)malloc(MEM_SIZE), MEM_SIZE) {
        RELEASE_ASSERT(!entry_descriptor, "");

        a.push(RBP);
        a.push(R15);
        a.push(R14);
        a.push(R13);
        a.push(R12);
        a.push(RBX);

        int vreg_size = 8 * source->cfg->getVRegInfo().getTotalNumOfVRegs();
        int scratch_size = 128;
        if (vreg_size % 16 == 0)
            scratch_size += 8;
        sp_adjustment = scratch_size + vreg_size;
        ASSERT(sp_adjustment % 16 == 8, "");
        a.sub(Immediate(sp_adjustment), RSP);

        scratch_rsp_offset = 0;
        vregs_rsp_offset = scratch_size;

        assert(!param_names->arg_names.size());
        assert(!param_names->vararg_name);
        assert(!param_names->kwarg_name);

        a.trap();
    }

    CompiledFunction* run() {
        for (auto block : source->cfg->blocks) {
            for (auto stmt : block->body) {
                if (stmt->type == AST_TYPE::Assign) {
                    auto asgn = ast_cast<AST_Assign>(stmt);
                    auto val = evalExpr(asgn->value, Location::any(), true);
                    if (val.type == Location::Uninitialized)
                        return NULL;

                    assert(asgn->targets.size() == 1);
                    assert(asgn->targets[0]->type == AST_TYPE::Name);
                    auto name = ast_cast<AST_Name>(asgn->targets[0]);
                    RELEASE_ASSERT(name->lookup_type == ScopeInfo::VarScopeType::FAST, "");

                    assert(val.type == Location::Register);
                    a.mov(val.asRegister(), vregStackSlot(name));
                } else if (stmt->type == AST_TYPE::Jump) {
                    auto jmp = ast_cast<AST_Jump>(stmt);
                    auto target_idx = jmp->target->idx;
                    while (jump_list.size() <= target_idx)
                        jump_list.emplace_back();
                    jump_list[target_idx].emplace_back(new Jump(a, UNCONDITIONAL));
                } else if (stmt->type == AST_TYPE::Return) {
                    auto ret = ast_cast<AST_Return>(stmt);

                    if (ret->value) {
                        auto val = evalExpr(ret->value, RAX);
                        a.mov(val.asRegister(), scratchSlot(0));
                    }

                    // TODO Call deinitframe

                    if (ret->value)
                        a.mov(scratchSlot(0), RAX);

                    a.add(Immediate(sp_adjustment), RSP);
                    a.pop(RBX);
                    a.pop(R12);
                    a.pop(R13);
                    a.pop(R14);
                    a.pop(R15);
                    a.pop(RBP);
                    a.retq();
                } else {
                    printf("Failed to sjit:\n");
                    print_ast(stmt);
                    printf("\n");
                    ASSERT(0, "unknown stmt type %d", stmt->type);
                }
            }
        }

        RELEASE_ASSERT(!a.hasFailed(), "");
        for (int idx = 0; idx < jump_list.size(); idx++) {
            RELEASE_ASSERT(jump_list[idx].empty(), "");
        }

        printf("disas %p,%p\n", a.startAddr(), a.curInstPointer());

        return new CompiledFunction(md, spec, a.startAddr(), effort, exception_style, entry_descriptor);
    }
};

CompiledFunction* doCompile(FunctionMetadata* md, SourceInfo* source, ParamNames* param_names,
                            const OSREntryDescriptor* entry_descriptor, EffortLevel effort,
                            ExceptionStyle exception_style, FunctionSpecialization* spec, llvm::StringRef nameprefix) {
    return SJit(md, source, param_names, entry_descriptor, effort, exception_style, spec, nameprefix).run();
}
}
