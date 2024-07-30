/*
 * dflopt_py.c
 *
 * Wrappers for lib/slash/src/dflopt.c
 *
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "dflopt_py.h"

#include "../pycsh.h"
#include "../csh/known_hosts.h"


// TODO Kevin: These differ from the newest version of slash/csh

PyObject * pycsh_slash_node(PyObject * self, PyObject * args, PyObject * kwds) {

	PyObject * node = NULL;
	int verbose = pycsh_dfl_verbose;

	static char *kwlist[] = {"node", "verbose", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|Oi", kwlist, &node, &verbose)) {
		return NULL;  // TypeError is thrown
	}

	if (node == NULL) {  // No argument passed
		if (verbose) {
			printf("Default node = %d\n", pycsh_dfl_node);
		}
	} else if (PyLong_Check(node)) {

		pycsh_dfl_node = PyLong_AsUnsignedLong(node);
		if (verbose) {
			printf("Set default node to %d\n", pycsh_dfl_node);
		}
	} else if (PyUnicode_Check(node)) {

		slash_dfl_node = known_hosts_get_node(PyUnicode_AsUTF8(node));
		if (slash_dfl_node == 0)
			slash_dfl_node = atoi(PyUnicode_AsUTF8(node));  // Numeric nodes will hopefully be passed as intergers, but we allow strings for compatiblity with CSH.
	} else {
		PyErr_SetString(PyExc_TypeError, "'node' argument must be either str or int");
		return NULL;
	}

	return Py_BuildValue("i", pycsh_dfl_node);
}

PyObject * pycsh_slash_timeout(PyObject * self, PyObject * args, PyObject * kwds) {

	int timeout = -1;
	int verbose = pycsh_dfl_verbose;

	static char *kwlist[] = {"timeout", "verbose", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ii", kwlist, &timeout, &verbose)) {
		return NULL;  // TypeError is thrown
	}

	if (timeout == -1) {
		if (verbose) {
			printf("Default timeout = %d\n", pycsh_dfl_timeout);
		}
	} else {
		pycsh_dfl_timeout = timeout;
		if (verbose) {
			printf("Set default timeout to %d\n", pycsh_dfl_timeout);
		}
	}

	return Py_BuildValue("i", pycsh_dfl_timeout);
}

PyObject * pycsh_slash_verbose(PyObject * self, PyObject * args) {

	int verbose = INT_MIN;

	if (!PyArg_ParseTuple(args, "|i", &verbose)) {
		return NULL;  // TypeError is thrown
	}

	if (verbose == INT_MIN)
		printf("Default verbose = %d\n", pycsh_dfl_verbose);
	else {
		pycsh_dfl_verbose = verbose;
		printf("Set default vebose to %d\n", pycsh_dfl_verbose);
	}

	return Py_BuildValue("i", pycsh_dfl_verbose);
}
