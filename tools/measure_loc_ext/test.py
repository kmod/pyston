import os
import sys
sys.path.append("build/lib.linux-x86_64-2.7")
# sys.path.append("..")

import cPickle
import runpy

import measure_loc

"""
sys.settrace(measure_loc.trace)

def f():
    try:
        1/0
    except Exception:
        1+1
f()

def fib(n):
    if n <= 1:
        return n
    return fib(n - 1) + fib(n - 2)
# fib(28)

sys.settrace(None)
measure_loc.dump()
1/0
"""

if __name__ == "__main__":
    old_sys_argv = sys.argv
    fn = sys.argv[1]

    if fn == '-m':
        module = sys.argv[2]
        args = sys.argv[3:]
    else:
        args = sys.argv[2:]
    sys.argv = [sys.argv[0]] + args

    sys.settrace(measure_loc.trace)

    assert sys.path[0] == os.path.abspath(os.path.dirname(__file__))
    sys.path[0] = os.path.abspath(os.path.dirname(fn))

    # del sys.modules["__main__"] # do we need this?
    if fn == '-m':
        runpy.run_module(module, run_name="__main__")
    else:
        runpy.run_path(fn, run_name="__main__")
    sys.settrace(None)

    # measure_loc.dump()

    times = measure_loc.get_times().items()
    with open("measure_loc.pkl", "w") as f:
        cPickle.dump(times, f)
    times.sort(key=lambda (l, t): t, reverse=True)

    total = 0.0
    for l, t in times:
        total += t
    print "Found %d unique lines totalling %.2fs" % (len(times), total)

    FRACTION = 0.99

    sofar = 0.0
    total_lines = 0
    for (l, t) in times:
        if not l:
            continue
        fn, lineno = l
        total_lines += 1
        sofar += t
        if total_lines <= 20:
            print ("%s:%s" % (fn, lineno)).ljust(40), "%.4fs %4.1f%% % 3d %4.1f%%" % (t, t / total * 100, total_lines, sofar / total * 100.0)
        if sofar >= total * FRACTION:
            break
    print "(and %d more -- see mesaure_loc.pkl)" % (total_lines - 20)

    print "Picked %d lines out of %d to reach %.2f%%" % (total_lines, len(times), sofar / total * 100.0)
