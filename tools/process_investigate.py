import re

if __name__ == "__main__":
    f = open("investigate.txt")

    all_patchpoints = []

    def handle_trace(l):
        if any(s in l for s in ("SmallArena", "gc_alloc", "gc_realloc")):
            return "gc_alloc"

        if any(s in l for s in ("allowGLReadPreemption",)):
            return "AGRP"

        if l.count(' ') == 2 and '@' in l:
            ip = int(l.split()[2], 16)
            for desc, (start, end) in all_patchpoints:
                if start < ip <= end:
                    return "%r in %s" % (desc, l.split()[0])
            return "unknown in %s" % (l.split()[0],)

        if l in ("getattr", "setitem", "runtimeCall", "pyston::getiter(pyston::Box*)"):
            return "start of slowpath"

        if l in ("pattern_scanner", "convertitem", "vgetargs1", "sre_umatch", "replace", "PystonType_GenericAlloc"):
            return None

        if l.startswith("_ULx86_64") or l in ("access_mem", "pyston::unwind(pyston::ExcInfo*)"):
            return "unwinding"

        if "PyArg_ParseTuple" in l:
            return "PyArg_ParseTuple"

        if "pyston::wrap_" in l:
            return "misc runtime"

        if any(s in l for s in ("StringMapImpl", "getHCAttrsPtr", "operator new")):
            return None

        if "Box::getattr" in l or "getattrInternalGeneric" in l:
            return "Box::getattr"

        if l in ("pattern_finditer", "createDict", "recordType", "stringlib_parse_args_finds", "pattern_match", "unicode_replace", \
                "unicode_startswith", "sre_usearch"):
            return "misc runtime"

        if any(s in l for s in ("listiter", "listPop", "BoxIterator", "tupleContains", "listIter", "tupleIter", "listIAdd", \
                "BoxedList", "pyston::list", "listInsert")):
            return "misc runtime"

        if any(s in l for s in ("unicodeHashUnboxed", "PyHasher", "dictGetitem", "BoxedTuple::create", \
                "BoxedWrapperDescriptor::__get__", "objectNewNoArgs", "boxString", "BoxedString::BoxedString", \
                "excInfoForRaise", "checkAndThrowCAPIException", "callCLFunc", "int_richcompare", "yield")):
            return "misc runtime"

        if any(s in l for s in ("PyObject_", "PyList_", "PyInt_", "PyTuple_", "PyType_")):
            return "misc runtime"

        if "compileFunc" in l or "compilePartialFunc" in l:
            return "llvm"

        if any(s in l for s in ("cbrt", "Prime_rehash_policy", "nss_hosts_lookup", "memset", "envz_strip")):
            return None

        return "<UNKNOWN>"
        # return NotImplemented

    state = "NONE"
    patchpoints = {}
    cur_patchpoint = None
    lineno = -2
    for l in f:
        l = l.rstrip()
        # if not l.startswith("set lineno"):
            # print l
        if state == "NONE":
            if l == "--starting trace":
                state = "TRACE"
            elif l == "--starting compile":
                state = "COMPILE"
            else:
                assert 0, l
        elif state == "TRACE":
            if l == "--ending trace":
                assert 0, "didn't classify this trace"

            s = handle_trace(l)
            if s is NotImplemented:
                print "<bad>"
                state = "BAD TRACE"
            elif s:
                print s
                state = "FINISHED TRACE"
        elif state == "FINISHED TRACE":
            if l == "--ending trace":
                state = "NONE"
            else:
                continue
        elif state == "BAD TRACE":
            if l == "--ending trace":
                assert 0
        elif state == "COMPILE":
            if l == "--ending compile":
                assert not cur_patchpoint
                patchpoints = {}
                lineno = -2
                state = "NONE"
            elif l.startswith("Doing "):
                assert cur_patchpoint is None
                cur_patchpoint = l[6:]
            elif l.startswith("Emitting patchpoint"):
                ppid = int(l.split()[2])
                if "(type -1)" in l:
                    cur_patchpoint = "non-ic pp"
                assert cur_patchpoint
                assert ppid not in patchpoints
                patchpoints[ppid] = (cur_patchpoint, lineno)
                cur_patchpoint = None
            elif l.startswith("pp ") and " lives at" in l:
                m = re.match(r"pp (\d+) lives at \[(0x[0-9a-f]+), (0x[0-9a-f]+)\)", l)
                ppid, pp_start, pp_end = m.groups()
                ppid = int(ppid)
                all_patchpoints.append((patchpoints[ppid], (int(pp_start, 16), int(pp_end, 16))))
            elif l.startswith("set lineno: "):
                lineno = l[len("set lineno: "):]
            else:
                assert 0, l
        else:
            assert 0, state
