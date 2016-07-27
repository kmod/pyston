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

static void _decref(Box* b) {
    Py_DECREF(b);
}
static void _xdecref(Box* b) {
    Py_XDECREF(b);
}

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
    int scratch_size;
    int scratch_rsp_offset, vregs_rsp_offset, frameinfo_rsp_offset;

    typedef UnboundJump<1048576> Jump;
    std::deque<llvm::SmallVector<std::unique_ptr<Jump>, 2>> jump_list;

    Indirect vregStackSlot(AST_Name* name) {
        assert(name->lookup_type == ScopeInfo::VarScopeType::FAST
               || name->lookup_type == ScopeInfo::VarScopeType::CLOSURE);

        return vregStackSlot(name->vreg);
    }
    Indirect vregStackSlot(int vreg) {
        assert(vreg != -1);
        int rsp_offset = vregs_rsp_offset + 8 * vreg;
        return Indirect(RSP, rsp_offset);
    }

    Indirect scratchSlot(int idx) {
        assert(idx < scratch_size / 8);
        return Indirect(RSP, scratch_rsp_offset + 8 * idx);
    }

    struct SimpleLocation {
        union {
            Indirect mem;
            uint64_t constant;
        };

        bool is_constant;
        bool zero_at_end = false;

        bool operator==(const SimpleLocation& rhs);
        bool operator!=(const SimpleLocation& rhs) { return !(*this == rhs); }

        bool operator==(const Indirect& rhs) {
            return is_constant && mem == rhs;
        }
        bool operator!=(const Indirect& rhs) { return !(*this == rhs); }

        SimpleLocation(uint64_t data) : constant(data), is_constant(true) {}
        SimpleLocation(Indirect mem) : mem(mem), is_constant(false) {}
    };

    void mov(const SimpleLocation& l, Register reg) {
        if (l.is_constant) {
            a.mov(Immediate(l.constant), reg);
        } else {
            a.mov(l.mem, reg);
        }
    }

    // Returns a borrowed ref
    SimpleLocation evalSimpleExpr(AST_expr* expr) {
        if (expr->type == AST_TYPE::Num) {
            auto num = ast_cast<AST_Num>(expr);
            RELEASE_ASSERT(num->num_type == AST_Num::INT, "");

            Box* b = source->parent_module->getIntConstant(num->n_int);

            return (uint64_t)b;
        }

        if (expr->type == AST_TYPE::Name) {
            auto name = ast_cast<AST_Name>(expr);

            RELEASE_ASSERT(name->lookup_type == ScopeInfo::VarScopeType::FAST, "");

            auto slot = vregStackSlot(name);

            RELEASE_ASSERT(!source->cfg->getVRegInfo().isUserVisibleVReg(name->vreg), "");

            auto r = SimpleLocation(slot);
            if (name->is_kill)
                r.zero_at_end = true;
            return r;
        }

        RELEASE_ASSERT(0, "unhandled simple expr type %d\n", expr->type);
    }

    // Returns an owned ref
    bool evalExprInto(AST_expr* expr, Indirect dest) {
        if (expr->type == AST_TYPE::MakeFunction || expr->type == AST_TYPE::MakeClass) {
            if (VERBOSITY())
                printf("Aborting sjit; unhandled expr type %d\n", expr->type);

            return false;
        }

        if (expr->type == AST_TYPE::Compare) {
            auto compare = ast_cast<AST_Compare>(expr);
            assert(compare->ops.size() == 1);
            assert(compare->comparators.size() == 1);

            auto l1 = evalSimpleExpr(compare->left);
            auto l2 = evalSimpleExpr(compare->comparators[0]);
            assert(l1 != dest);
            assert(l2 != dest);

            mov(l1, RDI);
            mov(l2, RSI);
            a.mov(Immediate(compare->ops[0]), RDX);
            // a.call(Immediate((void*)pyston::compare));
            a.mov(Immediate((void*)pyston::compare), R11);
            a.callq(R11);

            a.mov(RAX, dest);

            if (l1.zero_at_end) {
                mov(l1, RDI);
#ifndef Py_REF_DEBUG
                static_assert(0, "want the faster version here");
#endif
                a.mov(Immediate((void*)_decref), R11);
                a.callq(R11);
                a.clear_reg(RAX);
                assert(!l1.is_constant);
                a.mov(RAX, l1.mem);
            }

            if (l2.zero_at_end) {
                mov(l2, RDI);
#ifndef Py_REF_DEBUG
                static_assert(0, "want the faster version here");
#endif
                a.mov(Immediate((void*)_decref), R11);
                a.callq(R11);
                a.clear_reg(RAX);
                assert(!l2.is_constant);
                a.mov(RAX, l2.mem);
            }

            return true;
        }

        if (expr->type == AST_TYPE::BinOp) {
            auto binop = ast_cast<AST_BinOp>(expr);

            auto l1 = evalSimpleExpr(binop->left);
            auto l2 = evalSimpleExpr(binop->right);
            assert(l1 != dest);
            assert(l2 != dest);

            mov(l1, RDI);
            mov(l2, RSI);
            a.mov(Immediate(binop->op_type), RDX);
            // a.call(Immediate((void*)pyston::binop));
            a.mov(Immediate((void*)pyston::binop), R11);
            a.callq(R11);

            a.mov(RAX, dest);

            if (l1.zero_at_end) {
                mov(l1, RDI);
#ifndef Py_REF_DEBUG
                static_assert(0, "want the faster version here");
#endif
                a.mov(Immediate((void*)_decref), R11);
                a.callq(R11);
                a.clear_reg(RAX);
                assert(!l1.is_constant);
                a.mov(RAX, l1.mem);
            }

            if (l2.zero_at_end) {
                mov(l2, RDI);
#ifndef Py_REF_DEBUG
                static_assert(0, "want the faster version here");
#endif
                a.mov(Immediate((void*)_decref), R11);
                a.callq(R11);
                a.clear_reg(RAX);
                assert(!l2.is_constant);
                a.mov(RAX, l2.mem);
            }

            return true;
        }

        if (expr->type == AST_TYPE::Name) {
            auto name = ast_cast<AST_Name>(expr);

            RELEASE_ASSERT(name->lookup_type == ScopeInfo::VarScopeType::FAST, "");

            auto slot = vregStackSlot(name);

            // TODO: use definedness analysis here
            bool definitely_defined = false;
            if (!source->cfg->getVRegInfo().isUserVisibleVReg(name->vreg)) {
                assert(0 && "why is this non-simple?");
                definitely_defined = true;
            }

            assert(!name->is_kill);

            if (!definitely_defined) {
                a.mov(slot, RDI);
                a.test(RDI, RDI);
                a.setne(RDI);
                a.mov(Immediate((void*)name->id.c_str()), RSI);
                a.mov(Immediate(NameError), RDX);
                a.mov(Immediate(1), RCX);
                RELEASE_ASSERT(exception_style == CXX, "");
                // a.call(Immediate((void*)assertNameDefined));
                a.mov(Immediate((void*)assertNameDefined), R11);
                a.callq(R11);
            }

            a.mov(slot, R11);
#ifdef Py_REF_DEBUG
            a.mov(Immediate(&_Py_RefTotal), R12);
            a.incl(Indirect(R12, 0));
#endif
            a.incl(Indirect(R11, offsetof(Box, ob_refcnt)));
            a.mov(R11, dest);
            return true;
        }

        auto loc = evalSimpleExpr(expr);
        assert(!loc.zero_at_end);
        assert(loc != dest);
        mov(loc, R11);
#ifdef Py_REF_DEBUG
        a.mov(Immediate(&_Py_RefTotal), R12);
        a.incl(Indirect(R12, 0));
#endif
        a.incl(Indirect(R11, offsetof(Box, ob_refcnt)));
        a.mov(R11, dest);
        return true;
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
        scratch_size = 32;
        if (vreg_size % 16 == 0)
            scratch_size += 8;
        sp_adjustment = scratch_size + vreg_size + sizeof(FrameInfo);
        ASSERT(sp_adjustment % 16 == 8, "");
        a.sub(Immediate(sp_adjustment), RSP);

        scratch_rsp_offset = 0;
        vregs_rsp_offset = scratch_size;
        frameinfo_rsp_offset = vregs_rsp_offset + vreg_size;

        a.clear_reg(RAX);

        // clear user visible vregs
        // TODO there are faster ways to do this
        int num_user_visible = source->cfg->getVRegInfo().getNumOfUserVisibleVRegs();
        for (int i = 0; i < num_user_visible; i++) {
            a.mov(RAX, vregStackSlot(i));
        }

        a.mov(RAX, Indirect(RSP, frameinfo_rsp_offset + offsetof(FrameInfo, exc.type)));

        assert(!source->getScopeInfo()->usesNameLookup());
        // otherwise need to set boxed_locals=new BoxedDict
        a.mov(RAX, Indirect(RSP, frameinfo_rsp_offset + offsetof(FrameInfo, boxedLocals)));

        a.mov(RAX, Indirect(RSP, frameinfo_rsp_offset + offsetof(FrameInfo, frame_obj)));

        assert(!source->getScopeInfo()->takesClosure());
        a.mov(RAX, Indirect(RSP, frameinfo_rsp_offset + offsetof(FrameInfo, passed_closure)));
        assert(!source->getScopeInfo()->createsClosure());

        assert(source->scoping->areGlobalsFromModule());
        a.mov(Immediate(source->parent_module), R11);
#ifdef Py_REF_DEBUG
        a.mov(Immediate(&_Py_RefTotal), R12);
        a.incl(Indirect(R12, 0));
#endif
        a.incl(Indirect(R11, offsetof(Box, ob_refcnt)));
        a.mov(R11, Indirect(RSP, frameinfo_rsp_offset + offsetof(FrameInfo, globals)));

        a.lea(vregStackSlot(0), R11);
        a.mov(R11, Indirect(RSP, frameinfo_rsp_offset + offsetof(FrameInfo, vregs)));
        // a.movl(Immediate(source->cfg->getVRegInfo().getTotalNumOfVRegs()), Indirect(RSP, frameinfo_rsp_offset +
        // offsetof(FrameInfo, num_vregs)));
        a.movq(Immediate(source->cfg->getVRegInfo().getTotalNumOfVRegs()),
               Indirect(RSP, frameinfo_rsp_offset + offsetof(FrameInfo, num_vregs)));

        a.mov(Immediate(md), R11);
        a.mov(R11, Indirect(RSP, frameinfo_rsp_offset + offsetof(FrameInfo, md)));

        a.lea(Indirect(RSP, frameinfo_rsp_offset), RDI);
        a.mov(Immediate((void*)initFrame), R11);
        a.callq(R11);
        a.trap();
    }

    CompiledFunction* run() {
        assert(!param_names->vararg_name);
        assert(!param_names->kwarg_name);
        assert(!source->is_generator);
        assert(!source->getScopeInfo()->takesClosure());

        for (int i = 0; i < param_names->arg_names.size(); i++) {
        }
        assert(!param_names->arg_names.size());

        std::vector<uint8_t*> block_starts;

        for (auto block : source->cfg->blocks) {
            while (block_starts.size() < block->idx)
                block_starts.emplace_back();
            block_starts.push_back(a.curInstPointer());

            for (auto stmt : block->body) {
                if (stmt->type == AST_TYPE::Assign) {
                    auto asgn = ast_cast<AST_Assign>(stmt);

                    assert(asgn->targets.size() == 1);
                    assert(asgn->targets[0]->type == AST_TYPE::Name);
                    auto name = ast_cast<AST_Name>(asgn->targets[0]);
                    RELEASE_ASSERT(name->lookup_type == ScopeInfo::VarScopeType::FAST, "");

                    int vreg = name->vreg;
                    assert(vreg >= 0);
                    if (source->cfg->getVRegInfo().isUserVisibleVReg(vreg)) {
                        auto dest = vregStackSlot(name);

                        SimpleLocation l = evalSimpleExpr(asgn->value);
                        assert(l != dest);

                        mov(l, R11);
#ifdef Py_REF_DEBUG
                        a.mov(Immediate(&_Py_RefTotal), R12);
                        a.incl(Indirect(R12, 0));
#endif
                        a.incl(Indirect(R11, offsetof(Box, ob_refcnt)));

                        a.mov(dest, RDI);
                        a.mov(R11, dest);

                        // TODO: use definedness analysis here
#ifndef Py_REF_DEBUG
                        static_assert(0, "want the faster version here");
#endif
                        a.mov(Immediate((void*)_xdecref), R11);
                        a.callq(R11);

                        if (l.zero_at_end) {
                            mov(l, RDI);
#ifndef Py_REF_DEBUG
                            static_assert(0, "want the faster version here");
#endif
                            a.mov(Immediate((void*)_decref), R11);
                            a.callq(R11);
                            a.clear_reg(RAX);
                            assert(!l.is_constant);
                            a.mov(RAX, l.mem);
                        }
                    } else {
                        auto dest = vregStackSlot(name);

                        bool ok = evalExprInto(asgn->value, dest);
                        if (!ok)
                            return NULL;
                    }
                } else if (stmt->type == AST_TYPE::Jump) {
                    auto jmp = ast_cast<AST_Jump>(stmt);
                    auto target_idx = jmp->target->idx;
                    while (jump_list.size() <= target_idx)
                        jump_list.emplace_back();
                    jump_list[target_idx].emplace_back(new Jump(a, UNCONDITIONAL));
                } else if (stmt->type == AST_TYPE::Branch) {
                    auto br = ast_cast<AST_Branch>(stmt);
                    auto true_idx = br->iftrue->idx;
                    auto false_idx = br->iffalse->idx;
                    while (jump_list.size() <= true_idx)
                        jump_list.emplace_back();
                    while (jump_list.size() <= false_idx)
                        jump_list.emplace_back();

                    assert(br->test->type == AST_TYPE::LangPrimitive);
                    auto nonzero = ast_cast<AST_LangPrimitive>(br->test);
                    assert(nonzero->opcode == AST_LangPrimitive::NONZERO);
                    auto nonzero_arg = evalSimpleExpr(nonzero->args[0]);

                    mov(nonzero_arg, RDI);
                    a.mov(Immediate((void*)pyston::nonzero), R11);
                    a.callq(R11);

                    if (nonzero_arg.zero_at_end) {
                        auto scratch = scratchSlot(0);
                        a.mov(RAX, scratch);

                        mov(nonzero_arg, RDI);
#ifndef Py_REF_DEBUG
                        static_assert(0, "want the faster version here");
#endif
                        a.mov(Immediate((void*)_decref), R11);
                        a.callq(R11);
                        a.clear_reg(RAX);
                        assert(!nonzero_arg.is_constant);
                        a.mov(RAX, nonzero_arg.mem);

                        a.mov(scratch, RAX);
                    }

                    a.testal();
                    jump_list[true_idx].emplace_back(new Jump(a, COND_NOT_EQUAL));
                    jump_list[false_idx].emplace_back(new Jump(a, UNCONDITIONAL));
                } else if (stmt->type == AST_TYPE::Return) {
                    auto ret = ast_cast<AST_Return>(stmt);

                    auto scratch = scratchSlot(0);
                    if (ret->value) {
                        auto l = evalSimpleExpr(ret->value);
                        assert(!l.zero_at_end);
                        mov(l, R11);
                    } else {
                        printf("%p\n", Py_None);
                        a.mov(Immediate((void*)Py_None), R11);
                    }

#ifdef Py_REF_DEBUG
                    a.mov(Immediate(&_Py_RefTotal), R12);
                    a.incl(Indirect(R12, 0));
#endif
                    a.incl(Indirect(R11, offsetof(Box, ob_refcnt)));

                    a.mov(R11, scratch);

                    a.lea(Indirect(RSP, frameinfo_rsp_offset), RDI);
                    a.mov(Immediate((void*)deinitFrame), R11);
                    a.callq(R11);

                    a.mov(scratch, RAX);

                    a.add(Immediate(sp_adjustment), RSP);
                    a.pop(RBX);
                    a.pop(R12);
                    a.pop(R13);
                    a.pop(R14);
                    a.pop(R15);
                    a.pop(RBP);
                    a.retq();
                } else if (stmt->type == AST_TYPE::Print) {
                    auto print = ast_cast<AST_Print>(stmt);

                    if (print->dest) {
                        auto dest = evalSimpleExpr(print->dest);
                        mov(dest, RDI);
                    } else {
                        a.clear_reg(RDI);
                    }

                    if (print->values.size()) {
                        auto var = evalSimpleExpr(print->values[0]);
                        mov(var, RSI);
                    } else {
                        a.clear_reg(RSI);
                    }

                    a.mov(Immediate(print->nl ? 1 : 0), RDX);
                    a.mov(Immediate((void*)pyston::printHelper), R11);
                    a.callq(R11);

                    if (print->dest) {
                        auto dest = evalSimpleExpr(print->dest);
                        if (dest.zero_at_end) {
                            mov(dest, RDI);
#ifndef Py_REF_DEBUG
                            static_assert(0, "want the faster version here");
#endif
                            a.mov(Immediate((void*)_decref), R11);
                            a.callq(R11);
                            a.clear_reg(RAX);
                            assert(!dest.is_constant);
                            a.mov(RAX, dest.mem);
                        }
                    }

                    if (print->values.size()) {
                        auto var = evalSimpleExpr(print->values[0]);
                        if (var.zero_at_end) {
                            mov(var, RDI);
#ifndef Py_REF_DEBUG
                            static_assert(0, "want the faster version here");
#endif
                            a.mov(Immediate((void*)_decref), R11);
                            a.callq(R11);
                            a.clear_reg(RAX);
                            assert(!var.is_constant);
                            a.mov(RAX, var.mem);
                        }
                    }
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
            for (auto&& j : jump_list[idx]) {
                j->bind(block_starts[idx]);
            }
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
