/*
 * param_list_py.c
 *
 * Wrappers for lib/param/src/param/list/param_list_slash.c
 *
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "../pycsh.h"
#include "../utils.h"

#include "param_list_py.h"

PyObject * pycsh_param_list(PyObject * self, PyObject * args, PyObject * kwds) {

	int node = pycsh_dfl_node;
    int verbosity = 1;
    char * maskstr = NULL;
	char * globstr = NULL;

	static char *kwlist[] = {"node", "verbose", "mask", "globstr", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|iiss", kwlist, &node, &verbosity, &maskstr, &globstr)) {
		return NULL;
	}

	/* Interpret maskstring */
    uint32_t mask = 0xFFFFFFFF;
    if (maskstr != NULL) {
        mask = param_maskstr_to_mask(maskstr);
    }

	param_list_print(mask, node, globstr, verbosity);

	return pycsh_util_parameter_list();
}

PyObject * pycsh_param_list_download(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!csp_initialized()) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before .init() has been called.");
		return NULL;
	}

	unsigned int node = pycsh_dfl_node;
    unsigned int timeout = pycsh_dfl_timeout;
    unsigned int version = 2;
	int include_remotes = 0;

	static char *kwlist[] = {"node", "timeout", "version", "remotes", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|IIII", kwlist, &node, &timeout, &version, &include_remotes))
		return NULL;  // TypeError is thrown

	// TODO Kevin: Downloading parameters with an incorrect version, can lead to a segmentation fault.
	//	Had it been easier to detect when a incorrect version is used, we would've raised an exception instead.
	if (param_list_download(node, timeout, version, include_remotes) < 1) {  // We assume a connection error has occurred if we don't receive any parameters.
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		return NULL;
	}

	return pycsh_util_parameter_list();

}

PyObject * pycsh_param_list_forget(PyObject * self, PyObject * args, PyObject * kwds) {

	int node = pycsh_dfl_node;
    int verbose = 1;

	static char *kwlist[] = {"node", "verbose", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ii", kwlist, &node, &verbose))
		return NULL;  // TypeError is thrown

	int res = param_list_remove(node, verbose);
	printf("Removed %i parameters\n", res);
	return Py_BuildValue("i", res);;
}
