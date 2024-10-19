#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

PyObject * pycsh_slash_execute(PyObject * self, PyObject * args, PyObject * kwds);
