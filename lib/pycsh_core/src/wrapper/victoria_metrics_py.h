#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

PyObject * pycsh_vm_start(PyObject * self, PyObject * args, PyObject * kwds);
PyObject * pycsh_vm_stop(PyObject * self, PyObject * args);
