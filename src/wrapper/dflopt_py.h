/*
 * dflopt_py.h
 *
 * Wrappers for lib/slash/src/dflopt.c
 *
 */

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

PyObject * pycsh_slash_node(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * pycsh_slash_timeout(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * pycsh_slash_verbose(PyObject * self, PyObject * args);
