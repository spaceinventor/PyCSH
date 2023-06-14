/*
 * dflopt_py.c
 *
 * Wrappers for lib/slash/src/dflopt.c
 *
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "../pycsh.h"

#include "dflopt_py.h"

// TODO Kevin: These differ from the newest version of slash/csh

PyObject * pycsh_slash_node(PyObject * self, PyObject * args) {

	int node = -1;

	if (!PyArg_ParseTuple(args, "|i", &node)) {
		return NULL;  // TypeError is thrown
	}

	if (node == -1)
		printf("Default node = %d\n", pycsh_dfl_node);
	else {
		pycsh_dfl_node = node;
		printf("Set default node to %d\n", pycsh_dfl_node);
	}

	return Py_BuildValue("i", pycsh_dfl_node);
}

PyObject * pycsh_slash_timeout(PyObject * self, PyObject * args) {

	int timeout = -1;

	if (!PyArg_ParseTuple(args, "|i", &timeout)) {
		return NULL;  // TypeError is thrown
	}

	if (timeout == -1)
		printf("Default timeout = %d\n", pycsh_dfl_timeout);
	else {
		pycsh_dfl_timeout = timeout;
		printf("Set default timeout to %d\n", pycsh_dfl_timeout);
	}

	return Py_BuildValue("i", pycsh_dfl_timeout);
}