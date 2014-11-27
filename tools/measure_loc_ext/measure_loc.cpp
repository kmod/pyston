#include <unordered_map>
#include <vector>
#include <ctime>

#include <Python.h>
#include <structmember.h>
#include <frameobject.h>

static PyObject* trace_func;

/*
CC=/home/kmod/pyston_deps/llvm-trunk-build/Release/bin/clang++ CFLAGS='-Wno-write-strings -std=c++11' python setup.py build && time LD_PRELOAD='/home/kmod/pyston_deps/gcc-4.8.2-install/lib64/libstdc++.so' python test.py ../../minibenchmarks/raytrace.py
*/

namespace std {
template <typename T1, typename T2> struct hash<pair<T1, T2> > {
    size_t operator()(const pair<T1, T2> p) const { return hash<T1>()(p.first) ^ (hash<T2>()(p.second) << 1); }
};
}
//struct Position : public std::pair<const char*, int> {
    //constexpr Position (const char* c, const char*
//};
typedef std::pair<const char*, int> Position;

static std::unordered_map<Position, double> times;
static double* next_time = NULL;
static double prev_time;

static std::vector<Position> call_stack;

double floattime() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return (double)t.tv_sec + t.tv_usec*0.000001;
}

static PyObject *
trace(PyObject *self, PyObject *args)
{
    if (next_time)
        *next_time += (floattime() - prev_time);

    PyObject* _frame;
    char* event;
    PyObject* arg;
    if (!PyArg_ParseTuple(args, "OsO", &_frame, &event, &arg))
        return NULL;

    assert(PyFrame_Check(_frame));
    PyFrameObject* frame = (PyFrameObject*)_frame;

    PyObject* fn = frame->f_code->co_filename;
    char* fn_str = PyString_AsString(fn);
    //printf("'%s': %s:%d (%p)\n", event, fn_str, frame->f_lineno, frame->f_back);

    if (strcmp(event, "call") == 0) {
        Position p(fn_str, frame->f_lineno);
        call_stack.push_back(p);
        next_time = &times[p];
    } else if (strcmp(event, "line") == 0 || strcmp(event, "exception") == 0) {
        Position p(fn_str, frame->f_lineno);
        next_time = &times[p];
    } else if (strcmp(event, "return") == 0) {
        Position p = call_stack.back();
        call_stack.pop_back();
        next_time = &times[p];
    } else {
        printf("unknown event: %s\n", event);
        PyErr_SetString(PyExc_RuntimeError, "unknown event!");
        Py_FatalError("Unknown event!");
        return NULL;
    }

    prev_time = floattime();
    Py_INCREF(trace_func);
    return trace_func;
}

static PyObject *
dump(PyObject *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    for (auto& p : times) {
        printf("%s:%d: %.3f\n", p.first.first, p.first.second, p.second);
    }

    Py_RETURN_NONE;
}

static PyObject *
get_times(PyObject* self, PyObject* args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    PyObject* d = PyDict_New();
    for (auto& p : times) {
        // TODO reference handling
        PyObject* fn = PyString_FromString(p.first.first);
        PyObject* lineno = PyInt_FromLong(p.first.second);
        PyObject* tuple = PyTuple_Pack(2, fn, lineno);
        PyObject* val = PyFloat_FromDouble(p.second);

        PyDict_SetItem(d, tuple, val);
    }

    return d;
}

static PyMethodDef MeasureLocMethods[] = {
    {"trace", trace, METH_VARARGS, "Tracer."},
    {"dump", dump, METH_VARARGS, "Dump."},
    {"get_times", get_times, METH_VARARGS, "Get logged times."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC
initmeasure_loc(void)
{
    PyObject *m;

    m = Py_InitModule("measure_loc", MeasureLocMethods);
    if (m == NULL)
        return;
    trace_func = PyObject_GetAttrString(m, "trace");
    if (trace_func == NULL)
        return;
}
