/*
 * dflopt_py.h
 *
 * Wrappers for lib/slash/src/dflopt.c
 *
 */

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

PyObject * pycsh_apm_load(PyObject * self, PyObject * args, PyObject * kwds);
