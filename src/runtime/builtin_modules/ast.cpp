// Copyright (c) 2014-2015 Dropbox, Inc.
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

#include <algorithm>
#include <cmath>
#include <langinfo.h>
#include <sstream>

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

#include "codegen/unwinding.h"
#include "core/types.h"
#include "gc/collector.h"
#include "runtime/file.h"
#include "runtime/inline/boxing.h"
#include "runtime/int.h"
#include "runtime/types.h"
#include "runtime/util.h"

namespace pyston {

class BoxedAST : public Box {
public:
    AST* ast;
};

void setupAST() {
    BoxedModule* ast_module = createModule("_ast", "__builtin__");

    ast_module->giveAttr("PyCF_ONLY_AST", boxInt(PyCF_ONLY_AST));

#define MAKE_CLS(name)                                                                                                 \
    ast_module->giveAttr(STRINGIFY(name), BoxedHeapClass::create(type_cls, object_cls, NULL, 0, 0, sizeof(BoxedAST),   \
                                                                 false, STRINGIFY(name)))
    // Are these all really their own classes?
    // TODO(kmod) some should definitely inherit from others
    MAKE_CLS(alias);
    MAKE_CLS(arguments);
    MAKE_CLS(Assert);
    MAKE_CLS(Assign);
    MAKE_CLS(Attribute);
    MAKE_CLS(AugAssign);
    MAKE_CLS(BinOp);
    MAKE_CLS(BoolOp);
    MAKE_CLS(Call);
    MAKE_CLS(ClassDef);
    MAKE_CLS(Compare);
    MAKE_CLS(comprehension);
    MAKE_CLS(Delete);
    MAKE_CLS(Dict);
    MAKE_CLS(Exec);
    MAKE_CLS(ExceptHandler);
    MAKE_CLS(ExtSlice);
    MAKE_CLS(Expr);
    MAKE_CLS(For);
    MAKE_CLS(FunctionDef);
    MAKE_CLS(GeneratorExp);
    MAKE_CLS(Global);
    MAKE_CLS(If);
    MAKE_CLS(IfExp);
    MAKE_CLS(Import);
    MAKE_CLS(ImportFrom);
    MAKE_CLS(Index);
    MAKE_CLS(keyword);
    MAKE_CLS(Lambda);
    MAKE_CLS(List);
    MAKE_CLS(ListComp);
    MAKE_CLS(Module);
    MAKE_CLS(Num);
    MAKE_CLS(Name);
    MAKE_CLS(Pass);
    MAKE_CLS(Pow);
    MAKE_CLS(Print);
    MAKE_CLS(Raise);
    MAKE_CLS(Repr);
    MAKE_CLS(Return);
    MAKE_CLS(Slice);
    MAKE_CLS(Str);
    MAKE_CLS(Subscript);
    MAKE_CLS(TryExcept);
    MAKE_CLS(TryFinally);
    MAKE_CLS(Tuple);
    MAKE_CLS(UnaryOp);
    MAKE_CLS(With);
    MAKE_CLS(While);
    MAKE_CLS(Yield);
    MAKE_CLS(Store);
    MAKE_CLS(Load);
    MAKE_CLS(Param);
    MAKE_CLS(Not);
    MAKE_CLS(In);
    MAKE_CLS(Is);
    MAKE_CLS(IsNot);
    MAKE_CLS(Or);
    MAKE_CLS(And);
    MAKE_CLS(Eq);
    MAKE_CLS(NotEq);
    MAKE_CLS(NotIn);
    MAKE_CLS(GtE);
    MAKE_CLS(Gt);
    MAKE_CLS(Mod);
    MAKE_CLS(Add);
    MAKE_CLS(Continue);
    MAKE_CLS(Lt);
    MAKE_CLS(LtE);
    MAKE_CLS(Break);
    MAKE_CLS(Sub);
    MAKE_CLS(Del);
    MAKE_CLS(Mult);
    MAKE_CLS(Div);
    MAKE_CLS(USub);
    MAKE_CLS(BitAnd);
    MAKE_CLS(BitOr);
    MAKE_CLS(BitXor);
    MAKE_CLS(RShift);
    MAKE_CLS(LShift);
    MAKE_CLS(Invert);
    MAKE_CLS(UAdd);
    MAKE_CLS(FloorDiv);
    MAKE_CLS(DictComp);
    MAKE_CLS(Set);
    MAKE_CLS(Ellipsis);
    MAKE_CLS(Expression);
    MAKE_CLS(SetComp);
    MAKE_CLS(Suite);


#undef MAKE_CLS

    // Not sure what the best option is here; we're targeting this version,
    // though we're not there yet.  Not sure how people use this field.
    ast_module->giveAttr("__version__", boxInt(82160));
}
}
