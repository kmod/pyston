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


#include "asm_writing/assembler.h"
#include "codegen/irgen.h"
#include "core/cfg.h"

using namespace pyston::assembler;

namespace pyston {

Location evalExpr(AST_expr* expr) {
    if (expr->type == AST_TYPE::MakeFunction || expr->type == AST_TYPE::MakeClass) {
        if (VERBOSITY())
            printf("Aborting sjit; unhandled expr type %d\n", expr->type);
        return Location();
    }

    RELEASE_ASSERT(0, "");
}

CompiledFunction* doCompile(FunctionMetadata* md, SourceInfo* source, ParamNames* param_names,
                            const OSREntryDescriptor* entry_descriptor, EffortLevel effort,
                            ExceptionStyle exception_style, FunctionSpecialization* spec, llvm::StringRef nameprefix) {
    RELEASE_ASSERT(!entry_descriptor, "");

    const int mem_size = 1024 * 1024;
    auto mem = (uint8_t*)malloc(mem_size);

    Assembler a(mem, mem_size);

    a.push(RBP);
    a.push(R15);
    a.push(R14);
    a.push(R13);
    a.push(R12);
    a.push(RBX);
    const int sp_adjustment = 128 + 8;
    static_assert(sp_adjustment % 16 == 8, "stack isn't aligned");
    a.sub(Immediate(sp_adjustment), RSP);

    assert(!param_names->arg_names.size());
    assert(!param_names->vararg_name);
    assert(!param_names->kwarg_name);

    for (auto block : source->cfg->blocks) {
        for (auto stmt : block->body) {
            if (stmt->type == AST_TYPE::Assign) {
                auto asgn = ast_cast<AST_Assign>(stmt);
                auto val = evalExpr(asgn->value);
                if (val.type == Location::Uninitialized)
                    return NULL;

                assert(asgn->targets.size() == 1);
                assert(asgn->targets[0]->type == AST_TYPE::Name);

                int vreg = 0;
                RELEASE_ASSERT(0, "");
            }
            print_ast(stmt);
            ASSERT(0, "%d", stmt->type);
        }
    }

    RELEASE_ASSERT(!a.hasFailed(), "");

    RELEASE_ASSERT(0, "");
}

}
