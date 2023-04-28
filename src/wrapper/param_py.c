/*
 * param_py.c
 *
 * Wrappers for lib/param/src/param/param_slash.c
 *
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <param/param.h>
#include <param/param_queue.h>

#include "../pycsh.h"

#include "param_py.h"

PyObject * pycsh_param_get(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!csp_initialized()) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before .init() has been called.");
		return NULL;
	}

	PyObject * param_identifier;  // Raw argument object/type passed. Identify its type when needed.
	int host = INT_MIN;
	int node = pycsh_dfl_node;
	int offset = INT_MIN;  // Using INT_MIN as the least likely value as Parameter arrays should be backwards subscriptable like lists.
	int timeout = pycsh_dfl_timeout;
	int retries = 1;

	static char *kwlist[] = {"param_identifier", "host", "node", "offset", "timeout", "retries", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|iiiii", kwlist, &param_identifier, &host, &node, &offset, &timeout, &retries))
		return NULL;  // TypeError is thrown

	param_t *param = _pycsh_util_find_param_t(param_identifier, node);

	if (param == NULL)  // Did not find a match.
		return NULL;  // Raises TypeError or ValueError.

	// _pycsh_util_get_single() and _pycsh_util_get_array() will return NULL for exceptions, which is fine with us.
	if (param->array_size > 1 && param->type != PARAM_TYPE_STRING)
		return _pycsh_util_get_array(param, autosend, host, timeout, retries);
	return _pycsh_util_get_single(param, offset, autosend, host, timeout, retries);
}

PyObject * pycsh_param_set(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!csp_initialized()) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before .init() has been called.");
		return NULL;
	}

	PyObject * param_identifier;  // Raw argument object/type passed. Identify its type when needed.
	PyObject * value;
	int host = INT_MIN;
	int node = pycsh_dfl_node;
	int offset = INT_MIN;  // Using INT_MIN as the least likely value as Parameter arrays should be backwards subscriptable like lists.
	int timeout = pycsh_dfl_timeout;
	int retries = 1;

	static char *kwlist[] = {"param_identifier", "value", "host", "node", "offset", "timeout", "retries", NULL};
	
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO|iiiii", kwlist, &param_identifier, &value, &host, &node, &offset, &timeout, &retries)) {
		return NULL;  // TypeError is thrown
	}

	param_t *param = _pycsh_util_find_param_t(param_identifier, node);

	if (param == NULL)  // Did not find a match.
		return NULL;  // Raises TypeError or ValueError.

	if((PyIter_Check(value) || PySequence_Check(value)) && !PyObject_TypeCheck(value, &PyUnicode_Type)) {
		if (_pycsh_util_set_array(param, value, host, timeout, retries))
			return NULL;  // Raises one of many possible exceptions.
	} else {
		param_queue_t *usequeue = autosend ? NULL : &param_queue_set;
		if (_pycsh_util_set_single(param, value, offset, host, timeout, retries, usequeue))
			return NULL;  // Raises one of many possible exceptions.
		param_print(param, -1, NULL, 0, 2);
	}

	Py_RETURN_NONE;
}

PyObject * pycsh_param_cmd(PyObject * self, PyObject * args) {

	if (param_queue.type == PARAM_QUEUE_TYPE_EMPTY) {
		printf("No active command\n");
	} else {
		printf("Current command size: %d bytes\n", param_queue.used);
		param_queue_print(&param_queue);
	}

	Py_RETURN_NONE;

}

PyObject * pycsh_param_pull(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!csp_initialized()) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before .init() has been called.");
		return NULL;
	}

	unsigned int host = 0;
	unsigned int timeout = pycsh_dfl_timeout;
	uint32_t include_mask = 0xFFFFFFFF;
	uint32_t exclude_mask = PM_REMOTE | PM_HWREG;

	char * _str_include_mask = NULL;
	char * _str_exclude_mask = NULL;

	static char *kwlist[] = {"host", "include_mask", "exclude_mask", "timeout", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|ssI", kwlist, &host, &_str_include_mask, &_str_exclude_mask, &timeout)) {
		return NULL;
	}

	if (_str_include_mask != NULL)
		include_mask = param_maskstr_to_mask(_str_include_mask);
	if (_str_exclude_mask != NULL)
		exclude_mask = param_maskstr_to_mask(_str_exclude_mask);

	int result = -1;
	if (param_queue_get.used == 0) {
		result = param_pull_all(1, host, include_mask, exclude_mask, timeout, paramver);
	} else {
		result = param_pull_queue(&param_queue_get, 1, host, timeout);
	}

	if (result) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		return 0;
	}

	Py_RETURN_NONE;
}