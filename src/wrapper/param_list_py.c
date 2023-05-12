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
#include "../parameter/parameter.h"
#include "../parameter/pythonparameter.h"

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
	//	Had it been easier to detect when an incorrect version is used, we would've raised an exception instead.
	if (param_list_download(node, timeout, version, include_remotes) < 1) {  // We assume a connection error has occurred if we don't receive any parameters.
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		return NULL;
	}

	/* Despite the nastiness, we reallocate the downloaded parameters, such that they become embedded in a ParameterObject.
		Embedding the parameters allows us to create new parameters from Python, without the need for a lookup table for Python callbacks. */
	param_list_iterator i = {};
	param_t * iter_param = param_list_iterate(&i);

	while (iter_param) {

		if (i.phase == 0)
			continue;  // We cannot reallocate static parameters.
			/* TODO Kevin: We could, however, consider reusing their .addr for our new parameter.
				But not here in .list_download() */

		param_t * param = iter_param;  // Free the current parameter after we have used it to iterate.
		iter_param = param_list_iterate(&i);

		if (Parameter_wraps_param(param))
			continue;  // This parameter doesn't need reallocation.

		ParameterObject * pyparam = (ParameterObject *)_pycsh_Parameter_from_param(&ParameterType, param, NULL, INT_MIN, pycsh_dfl_timeout, 1, 2);

		if (pyparam == NULL) {
			PyErr_SetString(PyExc_MemoryError, "Failed to create ParameterObject for downloaded parameter. Parameter list may be corrupted.");
            return NULL;
        }

		// TODO Kevin: Infinite loop here ?

		// Using param_list_remove_specific() means we iterate thrice, but it is simpler.
		param_list_remove_specific(param, 0, 1);

		param_list_add(&pyparam->param);
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
