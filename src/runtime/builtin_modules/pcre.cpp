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

#include <pcre.h>

#include "core/types.h"
#include "runtime/objmodel.h"
#include "runtime/types.h"

namespace pyston {

BoxedModule* pcre_module;

Box* match(BoxedString* pattern, BoxedString* string) {
    RELEASE_ASSERT(pattern->cls == str_cls, "");
    RELEASE_ASSERT(string->cls == str_cls, "");

    const char* error;
    int erroffset;

#define OVECCOUNT 30
    int ovector[OVECCOUNT];

    pcre* re = pcre_compile(pattern->c_str(), 0, &error, &erroffset, NULL);
    RELEASE_ASSERT(re != NULL, "");

    int rc = pcre_exec(re, NULL, string->c_str(), string->size(), 0, 0, ovector, OVECCOUNT);
    if (rc == PCRE_ERROR_NOMATCH) {
        pcre_free(re);
        return None;
    }
    RELEASE_ASSERT(rc >= 0, "");

    //printf("Match succeeded at offset %d\n", (int)ovector[0]);
    RELEASE_ASSERT(rc != 0, "");

    for (int i = 0; i < rc; i++) {
        const char* substring_start = string->c_str() + ovector[2 * i];
        size_t substring_length = ovector[2 * i + 1] - ovector[2 * i];
        //printf("%2d: %.*s\n", i, (int)substring_length, (char*)substring_start);
    }

    pcre_free(re);
    return None;
}

void setupPcre() {
    pcre_module = createModule(boxString("pcre"));

    pcre_module->giveAttr("match",
                            new BoxedBuiltinFunctionOrMethod(boxRTFunction((void*)match, UNKNOWN, 2), "match"));
}
}
