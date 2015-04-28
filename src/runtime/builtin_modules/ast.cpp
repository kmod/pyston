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

static BoxedClass* AST_cls;

static BoxedClass* alias_cls;
static BoxedClass* arguments_cls;
static BoxedClass* Assert_cls;
static BoxedClass* Assign_cls;
static BoxedClass* Attribute_cls;
static BoxedClass* AugAssign_cls;
static BoxedClass* BinOp_cls;
static BoxedClass* BoolOp_cls;
static BoxedClass* Call_cls;
static BoxedClass* ClassDef_cls;
static BoxedClass* Compare_cls;
static BoxedClass* comprehension_cls;
static BoxedClass* Delete_cls;
static BoxedClass* Dict_cls;
static BoxedClass* Exec_cls;
static BoxedClass* ExceptHandler_cls;
static BoxedClass* ExtSlice_cls;
static BoxedClass* Expr_cls;
static BoxedClass* For_cls;
static BoxedClass* FunctionDef_cls;
static BoxedClass* GeneratorExp_cls;
static BoxedClass* Global_cls;
static BoxedClass* If_cls;
static BoxedClass* IfExp_cls;
static BoxedClass* Import_cls;
static BoxedClass* ImportFrom_cls;
static BoxedClass* Index_cls;
static BoxedClass* keyword_cls;
static BoxedClass* Lambda_cls;
static BoxedClass* List_cls;
static BoxedClass* ListComp_cls;
static BoxedClass* Module_cls;
static BoxedClass* Num_cls;
static BoxedClass* Name_cls;
static BoxedClass* Pass_cls;
static BoxedClass* Pow_cls;
static BoxedClass* Print_cls;
static BoxedClass* Raise_cls;
static BoxedClass* Repr_cls;
static BoxedClass* Return_cls;
static BoxedClass* Slice_cls;
static BoxedClass* Str_cls;
static BoxedClass* Subscript_cls;
static BoxedClass* TryExcept_cls;
static BoxedClass* TryFinally_cls;
static BoxedClass* Tuple_cls;
static BoxedClass* UnaryOp_cls;
static BoxedClass* With_cls;
static BoxedClass* While_cls;
static BoxedClass* Yield_cls;
static BoxedClass* Store_cls;
static BoxedClass* Load_cls;
static BoxedClass* Param_cls;
static BoxedClass* Not_cls;
static BoxedClass* In_cls;
static BoxedClass* Is_cls;
static BoxedClass* IsNot_cls;
static BoxedClass* Or_cls;
static BoxedClass* And_cls;
static BoxedClass* Eq_cls;
static BoxedClass* NotEq_cls;
static BoxedClass* NotIn_cls;
static BoxedClass* GtE_cls;
static BoxedClass* Gt_cls;
static BoxedClass* Mod_cls;
static BoxedClass* Add_cls;
static BoxedClass* Continue_cls;
static BoxedClass* Lt_cls;
static BoxedClass* LtE_cls;
static BoxedClass* Break_cls;
static BoxedClass* Sub_cls;
static BoxedClass* Del_cls;
static BoxedClass* Mult_cls;
static BoxedClass* Div_cls;
static BoxedClass* USub_cls;
static BoxedClass* BitAnd_cls;
static BoxedClass* BitOr_cls;
static BoxedClass* BitXor_cls;
static BoxedClass* RShift_cls;
static BoxedClass* LShift_cls;
static BoxedClass* Invert_cls;
static BoxedClass* UAdd_cls;
static BoxedClass* FloorDiv_cls;
static BoxedClass* DictComp_cls;
static BoxedClass* Set_cls;
static BoxedClass* Ellipsis_cls;
static BoxedClass* Expression_cls;
static BoxedClass* SetComp_cls;
static BoxedClass* Suite_cls;



class BoxedAST : public Box {
public:
    AST* ast;

    BoxedAST() {}
};

static std::unordered_map<int, BoxedClass*> type_to_cls;

Box* boxAst(AST* ast) {
    assert(ast);
    BoxedClass* cls = type_to_cls[ast->type];
    assert(cls);

    BoxedAST* rtn = new (cls) BoxedAST();
    assert(rtn->cls == cls);
    rtn->ast = ast;
    return rtn;
}

AST* unboxAst(Box* b) {
    assert(isSubclass(b->cls, AST_cls));
    AST* rtn = static_cast<BoxedAST*>(b)->ast;
    assert(rtn);
    return rtn;
}

extern "C" int PyAST_Check(PyObject* o) noexcept {
    return isSubclass(o->cls, AST_cls);
}

template <typename AST_T, typename FIELD_T, FIELD_T AST_T::*V> class FieldGetter {
public:
    static Box* getField(Box* self, void*);
};

template <typename AST_T, AST_expr* AST_T::*V> class FieldGetter<AST_T, AST_expr*, V> {
public:
    static Box* getField(Box* self, void*) {
        RELEASE_ASSERT(self->cls == type_to_cls[AST_T::TYPE], "");
        AST_T* ast = static_cast<AST_T*>(static_cast<BoxedAST*>(self)->ast);

        return boxAst(ast->*V);
    }
};

template <typename AST_T, std::vector<AST_TYPE::AST_TYPE> AST_T::*V>
class FieldGetter<AST_T, std::vector<AST_TYPE::AST_TYPE>, V> {
public:
    static Box* getField(Box* self, void*) {
        RELEASE_ASSERT(self->cls == type_to_cls[AST_T::TYPE], "");
        AST_T* ast = static_cast<AST_T*>(static_cast<BoxedAST*>(self)->ast);

        BoxedList* rtn = new BoxedList();
        for (auto elt_type : ast->*V) {
            listAppendInternal(rtn, new (type_to_cls[elt_type]) BoxedAST());
        }
        return rtn;
    }
};

template <typename AST_T, std::vector<AST_expr*> AST_T::*V> class FieldGetter<AST_T, std::vector<AST_expr*>, V> {
public:
    static Box* getField(Box* self, void*) {
        RELEASE_ASSERT(self->cls == type_to_cls[AST_T::TYPE], "");
        AST_T* ast = static_cast<AST_T*>(static_cast<BoxedAST*>(self)->ast);

        BoxedList* rtn = new BoxedList();
        for (auto elt : ast->*V) {
            listAppendInternal(rtn, boxAst(elt));
        }
        return rtn;
    }
};

template <typename AST_T, InternedString AST_T::*V> class FieldGetter<AST_T, InternedString, V> {
public:
    static Box* getField(Box* self, void*) {
        RELEASE_ASSERT(self->cls == type_to_cls[AST_T::TYPE], "");
        AST_T* ast = static_cast<AST_T*>(static_cast<BoxedAST*>(self)->ast);

        return boxString((ast->*V).str());
    }
};

template <typename AST_T, AST_TYPE::AST_TYPE AST_T::*V> class FieldGetter<AST_T, AST_TYPE::AST_TYPE, V> {
public:
    static Box* getField(Box* self, void*) {
        RELEASE_ASSERT(self->cls == type_to_cls[AST_T::TYPE], "");
        AST_T* ast = static_cast<AST_T*>(static_cast<BoxedAST*>(self)->ast);

        return new (type_to_cls[ast->*V]) BoxedAST();
    }
};

static Box* strS(Box* self, void*) {
    RELEASE_ASSERT(self->cls == type_to_cls[AST_TYPE::Str], "");
    AST_Str* ast = static_cast<AST_Str*>(static_cast<BoxedAST*>(self)->ast);

    RELEASE_ASSERT(ast->str_type == AST_Str::STR, "");
    return boxString(ast->str_data);
}

void setupAST() {
    BoxedModule* ast_module = createModule("_ast", "__builtin__");

    ast_module->giveAttr("PyCF_ONLY_AST", boxInt(PyCF_ONLY_AST));

// ::create takes care of registering the class as a GC root.
#define _MAKE_CLS(name, base_cls, has_fields)                                                                          \
    name##_cls = BoxedHeapClass::create(type_cls, base_cls, /* gchandler = */ NULL, 0, 0, sizeof(BoxedAST), false,     \
                                        STRINGIFY(name));                                                              \
    ast_module->giveAttr(STRINGIFY(name), name##_cls);                                                                 \
    type_to_cls[AST_TYPE::name] = name##_cls;                                                                          \
    name##_cls->giveAttr("__module__", boxString("_ast"));                                                             \
    BoxedList* name##_fields = new BoxedList();                                                                        \
    if (has_fields)                                                                                                    \
        name##_cls->giveAttr("_fields", name##_fields);                                                                \
    name##_cls->freeze()

#define MAKE_CLS(name, base_cls) _MAKE_CLS(name, base_cls, false)
#define MAKE_CLS_FIELDS(name, base_cls) _MAKE_CLS(name, base_cls, true)

#define ADD_FIELD3(name, pyname, fieldname)                                                                            \
    do {                                                                                                               \
        listAppendInternal(name##_cls->getattr("_fields"), boxString(STRINGIFY(pyname)));                              \
        name##_cls->giveAttr(                                                                                          \
            STRINGIFY(pyname),                                                                                         \
            new (pyston_getset_cls) BoxedGetsetDescriptor(                                                             \
                &FieldGetter<AST_##name, decltype(AST_##name::fieldname), &AST_##name::fieldname>::getField, NULL,     \
                NULL));                                                                                                \
    } while (0)
#define ADD_FIELD(name, field) ADD_FIELD3(name, field, field)

    AST_cls
        = BoxedHeapClass::create(type_cls, object_cls, /* gchandler = */ NULL, 0, 0, sizeof(BoxedAST), false, "AST");
    // ::create takes care of registering the class as a GC root.
    AST_cls->giveAttr("__module__", boxString("_ast"));
    AST_cls->freeze();
    ast_module->giveAttr("AST", AST_cls);

    // TODO(kmod) you can call the class constructors, such as "ast.AST()", so we need new/init
    // TODO(kmod) there is more inheritance than "they all inherit from AST"

    MAKE_CLS(alias, AST_cls);
    MAKE_CLS(arguments, AST_cls);
    MAKE_CLS(Assert, AST_cls);
    MAKE_CLS(Assign, AST_cls);
    MAKE_CLS(Attribute, AST_cls);
    MAKE_CLS(AugAssign, AST_cls);
    MAKE_CLS(BinOp, AST_cls);
    MAKE_CLS(BoolOp, AST_cls);
    MAKE_CLS(Call, AST_cls);
    MAKE_CLS(ClassDef, AST_cls);
    MAKE_CLS_FIELDS(Compare, AST_cls);
    ADD_FIELD(Compare, ops);
    ADD_FIELD(Compare, comparators);
    ADD_FIELD(Compare, left);
    MAKE_CLS(comprehension, AST_cls);
    MAKE_CLS(Delete, AST_cls);
    MAKE_CLS(Dict, AST_cls);
    MAKE_CLS(Exec, AST_cls);
    MAKE_CLS(ExceptHandler, AST_cls);
    MAKE_CLS(ExtSlice, AST_cls);
    MAKE_CLS(Expr, AST_cls);
    MAKE_CLS(For, AST_cls);
    MAKE_CLS(FunctionDef, AST_cls);
    MAKE_CLS(GeneratorExp, AST_cls);
    MAKE_CLS(Global, AST_cls);
    MAKE_CLS(If, AST_cls);
    MAKE_CLS(IfExp, AST_cls);
    MAKE_CLS(Import, AST_cls);
    MAKE_CLS(ImportFrom, AST_cls);
    MAKE_CLS(Index, AST_cls);
    MAKE_CLS(keyword, AST_cls);
    MAKE_CLS(Lambda, AST_cls);
    MAKE_CLS(List, AST_cls);
    MAKE_CLS(ListComp, AST_cls);
    MAKE_CLS(Module, AST_cls);
    MAKE_CLS(Num, AST_cls);
    MAKE_CLS_FIELDS(Name, AST_cls);
    ADD_FIELD(Name, id);
    ADD_FIELD3(Name, ctx, ctx_type);
    MAKE_CLS(Pass, AST_cls);
    MAKE_CLS(Pow, AST_cls);
    MAKE_CLS(Print, AST_cls);
    MAKE_CLS(Raise, AST_cls);
    MAKE_CLS(Repr, AST_cls);
    MAKE_CLS(Return, AST_cls);
    MAKE_CLS(Slice, AST_cls);
    MAKE_CLS_FIELDS(Str, AST_cls);
    listAppendInternal(Str_cls->getattr("_fields"), boxString("s"));
    Str_cls->giveAttr("s", new (pyston_getset_cls) BoxedGetsetDescriptor(strS, NULL, NULL));
    MAKE_CLS(Subscript, AST_cls);
    MAKE_CLS(TryExcept, AST_cls);
    MAKE_CLS(TryFinally, AST_cls);
    MAKE_CLS(Tuple, AST_cls);
    MAKE_CLS(UnaryOp, AST_cls);
    MAKE_CLS(With, AST_cls);
    MAKE_CLS(While, AST_cls);
    MAKE_CLS(Yield, AST_cls);
    MAKE_CLS_FIELDS(Store, AST_cls);
    MAKE_CLS_FIELDS(Load, AST_cls);
    MAKE_CLS_FIELDS(Param, AST_cls);
    MAKE_CLS_FIELDS(Not, AST_cls);
    MAKE_CLS_FIELDS(In, AST_cls);
    MAKE_CLS_FIELDS(Is, AST_cls);
    MAKE_CLS_FIELDS(IsNot, AST_cls);
    MAKE_CLS_FIELDS(Or, AST_cls);
    MAKE_CLS_FIELDS(And, AST_cls);
    MAKE_CLS_FIELDS(Eq, AST_cls);
    MAKE_CLS_FIELDS(NotEq, AST_cls);
    MAKE_CLS_FIELDS(NotIn, AST_cls);
    MAKE_CLS_FIELDS(GtE, AST_cls);
    MAKE_CLS_FIELDS(Gt, AST_cls);
    MAKE_CLS_FIELDS(Mod, AST_cls);
    MAKE_CLS_FIELDS(Add, AST_cls);
    MAKE_CLS(Continue, AST_cls);
    MAKE_CLS(Lt, AST_cls);
    MAKE_CLS(LtE, AST_cls);
    MAKE_CLS(Break, AST_cls);
    MAKE_CLS(Sub, AST_cls);
    MAKE_CLS(Del, AST_cls);
    MAKE_CLS(Mult, AST_cls);
    MAKE_CLS(Div, AST_cls);
    MAKE_CLS(USub, AST_cls);
    MAKE_CLS(BitAnd, AST_cls);
    MAKE_CLS(BitOr, AST_cls);
    MAKE_CLS(BitXor, AST_cls);
    MAKE_CLS(RShift, AST_cls);
    MAKE_CLS(LShift, AST_cls);
    MAKE_CLS(Invert, AST_cls);
    MAKE_CLS(UAdd, AST_cls);
    MAKE_CLS(FloorDiv, AST_cls);
    MAKE_CLS(DictComp, AST_cls);
    MAKE_CLS(Set, AST_cls);
    MAKE_CLS(Ellipsis, AST_cls);

    MAKE_CLS_FIELDS(Expression, AST_cls);
    ADD_FIELD(Expression, body);

    MAKE_CLS(SetComp, AST_cls);
    MAKE_CLS(Suite, AST_cls);


#undef MAKE_CLS

    // Not sure what the best option is here; we're targeting this version,
    // though we're not there yet.  Not sure how people use this field.
    ast_module->giveAttr("__version__", boxInt(82160));
}
}
