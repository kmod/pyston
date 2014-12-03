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

static std::unordered_map<Position, int> counts;

static PyObject *
signal_handler(PyObject *self, PyObject *args)
{
    PyObject* _frame;
    int sig;
    if (!PyArg_ParseTuple(args, "nO", &sig, &_frame))
        return NULL;

    assert(PyFrame_Check(_frame));
    PyFrameObject* frame = (PyFrameObject*)_frame;

    PyObject* fn = frame->f_code->co_filename;
    char* fn_str = PyString_AsString(fn);

    Position p(fn_str, frame->f_lineno);
    counts[p]++;

    Py_RETURN_NONE;
}

static PyObject *
get_times(PyObject* self, PyObject* args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    PyObject* d = PyDict_New();
    for (auto& p : counts) {
        // TODO reference handling
        PyObject* fn = PyString_FromString(p.first.first);
        PyObject* lineno = PyInt_FromLong(p.first.second);
        PyObject* tuple = PyTuple_Pack(2, fn, lineno);
        PyObject* val = PyInt_FromLong(p.second);

        PyDict_SetItem(d, tuple, val);
    }

    return d;
}

static PyMethodDef MeasureLocMethods[] = {
    {"signal_handler", signal_handler, METH_VARARGS, "Tracer."},
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
}
