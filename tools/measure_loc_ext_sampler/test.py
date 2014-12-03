import os
import sys
os.chdir(os.path.join('.', os.path.dirname(__file__)))
__file__ = os.path.basename(__file__)

build_dn = "build/lib.linux-x86_64-2.7"
assert os.path.exists(build_dn), build_dn
sys.path.append(build_dn)
# sys.path.append("..")

import cPickle
import runpy
import signal
assert hasattr(signal, "setitimer")

# Copied + modified from https://github.com/bdarnell/plop/blob/master/plop/collector.py
MODES = {
    'prof': (signal.ITIMER_PROF, signal.SIGPROF),
    'virtual': (signal.ITIMER_VIRTUAL, signal.SIGVTALRM),
    'real': (signal.ITIMER_REAL, signal.SIGALRM),
    }

times = {}
def signal_handler(sig, frame):
    loc = frame.f_code.co_filename, frame.f_lineno
    times[loc] = times.get(loc, 0) + 1
    # if loc == ("/usr/lib/pymodules/python2.7/django/utils/encoding.py", 54):
        # 1/0

def get_times():
    return times

# from measure_loc import signal_handler, get_times

if __name__ == "__main__":
    old_sys_argv = sys.argv
    fn = sys.argv[1]

    if fn == '-m':
        module = sys.argv[2]
        args = sys.argv[3:]
    else:
        args = sys.argv[2:]
    sys.argv = [sys.argv[0]] + args

    assert sys.path[0] == os.path.abspath(os.path.dirname(__file__))
    sys.path[0] = os.path.abspath(os.path.dirname(fn))

    timer, sig = MODES["real"]

    signal.signal(sig, signal_handler)
    INTERVAL = 0.00001
    signal.setitimer(timer, INTERVAL, INTERVAL)

    # del sys.modules["__main__"] # do we need this?
    if fn == '-m':
        runpy.run_module(module, run_name="__main__")
    else:
        runpy.run_path(fn, run_name="__main__")
    signal.setitimer(timer, 0, 0)

    # measure_loc.dump()

    times = get_times().items()
    with open("measure_loc.pkl", "w") as f:
        cPickle.dump(times, f)
    times.sort(key=lambda (l, t): t, reverse=True)

    total = 0.0
    for l, t in times:
        total += t
    print "Found %d unique lines with %d samples" % (len(times), total)

    FRACTION = 0.99
    DISPLAY_THRESH = 20

    sofar = 0.0
    total_lines = 0
    for (l, t) in times:
        if not l:
            continue
        fn, lineno = l
        total_lines += 1
        sofar += t
        if total_lines <= DISPLAY_THRESH:
            print ("%s:%s" % (fn, lineno)).ljust(40), "% 3d %4.1f%% % 3d %4.1f%%" % (t, t / total * 100, total_lines, sofar / total * 100.0)
        if sofar >= total * FRACTION:
            break

    if total_lines > DISPLAY_THRESH:
        print "(and %d more -- see mesaure_loc.pkl)" % (total_lines - DISPLAY_THRESH)

    print "Picked %d lines out of %d to reach %.2f%%" % (total_lines, len(times), sofar / total * 100.0)
