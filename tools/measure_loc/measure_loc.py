import os
import sys

import cPickle
import runpy
import signal

class SamplingProfiler(object):
    # Copied + modified from https://github.com/bdarnell/plop/blob/master/plop/collector.py
    MODES = {
        'prof': (signal.ITIMER_PROF, signal.SIGPROF),
        'virtual': (signal.ITIMER_VIRTUAL, signal.SIGVTALRM),
        'real': (signal.ITIMER_REAL, signal.SIGALRM),
        }

    def __init__(self, sighandler, dumper, mode, interval=0.0001):
        self.sighandler = sighandler
        self.dumper = dumper
        self.mode = mode
        self.interval = interval

    def start(self):
        timer, sig = SamplingProfiler.MODES[self.mode]

        signal.signal(sig, signal_handler)
        signal.setitimer(timer, self.interval, self.interval)

    def stop(self):
        timer, sig = SamplingProfiler.MODES[self.mode]
        signal.setitimer(timer, 0, 0)
        signal.signal(sig, signal.SIG_DFL)
        return self.dumper()

class TracingProfiler(object):
    def __init__(self, tracefunc, dumper):
        self.tracefunc = tracefunc
        self.dumper = dumper

    def start(self):
        sys.settrace(self.tracefunc)

    def stop(self):
        sys.settrace(None)
        return self.dumper()

times = {}
def signal_handler(sig, frame):
    loc = frame.f_code.co_filename, frame.f_lineno
    times[loc] = times.get(loc, 0) + 1

def trace_count(frame, event, arg):
    if event == "line":
        loc = frame.f_code.co_filename, frame.f_lineno
        times[loc] = times.get(loc, 0) + 1

    return trace_count

def get_times():
    return times.items()

python_sampler = SamplingProfiler(signal_handler, get_times, "real", interval=0.00001)
python_trace_counter = TracingProfiler(trace_count, get_times)

def run(sampler, kind):
    fn = sys.argv[1]

    if fn == '-m':
        module = sys.argv[2]
        args = sys.argv[3:]
    else:
        args = sys.argv[2:]
    sys.argv = [sys.argv[0]] + args

    sys.path[0] = os.path.abspath(os.path.dirname(fn))

    sampler.start()

    # del sys.modules["__main__"] # do we need this?
    if fn == '-m':
        runpy.run_module(module, run_name="__main__")
    else:
        runpy.run_path(fn, run_name="__main__")

    times = sampler.stop()

    times.sort(key=lambda (l, t): t, reverse=True)
    with open("measure_loc.pkl", "w") as f:
        cPickle.dump(times, f)

    total = 0.0
    for l, t in times:
        total += t
    if kind == "time":
        print "Found %d unique lines for a total of %.2fs" % (len(times), total)
    else:
        print "Found %d unique lines with %d samples" % (len(times), total)

    FRACTIONS = [0.5, 0.75, 0.9, 0.99, 1]
    frac_counts = []
    frac_fracs = []
    frac_idx = 0
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
            if kind == "time":
                print ("%s:%s" % (fn, lineno)).ljust(50), "%.4fs %4.1f%% % 3d %4.1f%%" % (t, t / total * 100, total_lines, sofar / total * 100.0)
            else:
                print ("%s:%s" % (fn, lineno)).ljust(50), "% 3d %4.1f%% % 3d %4.1f%%" % (t, t / total * 100, total_lines, sofar / total * 100.0)
        if sofar >= total * FRACTIONS[frac_idx]:
            if FRACTIONS[frac_idx] == 1:
                break

            frac_counts.append(total_lines)
            frac_fracs.append(sofar)
            frac_idx += 1

    if len(times) > DISPLAY_THRESH:
        print "(and %d more -- see measure_loc.pkl)" % (len(times) - DISPLAY_THRESH)

    assert len(frac_counts) == len(FRACTIONS) -1
    for i in xrange(len(frac_counts)):
        print "Picked %d lines out of %d to reach %.2f%%" % (frac_counts[i], len(times), frac_fracs[i] / total * 100.0)

if __name__ == "__main__":
    run(python_sampler, "count")
    # run(python_trace_counter, "count")
