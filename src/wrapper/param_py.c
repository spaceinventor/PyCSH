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
#include <param/param_client.h>
#include <param/param_server.h>

#include "../pycsh.h"
#include "../utils.h"

#include "param_py.h"

static char queue_buf[PARAM_SERVER_MTU];
param_queue_t param_queue = { .buffer = queue_buf, .buffer_size = PARAM_SERVER_MTU, .type = PARAM_QUEUE_TYPE_EMPTY, .version = 2 };

PyObject * pycsh_param_get(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!csp_initialized()) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before .init() has been called.");
		return NULL;
	}

	PyObject * param_identifier;  // Raw argument object/type passed. Identify its type when needed.
	int node = pycsh_dfl_node;
	int server = INT_MIN;
	int paramver = 2;
	int offset = INT_MIN;  // Using INT_MIN as the least likely value as array Parameters should be backwards subscriptable like lists.
	int timeout = pycsh_dfl_timeout;
	int retries = 1;

	static char *kwlist[] = {"param_identifier", "node", "server", "paramver", "offset", "timeout", "retries", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|iiiiii", kwlist, &param_identifier, &node, &server, &paramver, &offset, &timeout, &retries))
		return NULL;  // TypeError is thrown

	param_t *param = _pycsh_util_find_param_t(param_identifier, node);

	if (param == NULL)  // Did not find a match.
		return NULL;  // Raises TypeError or ValueError.

	/* Select destination, host overrides parameter node */
	int dest = node;
	if (server > 0)
		dest = server;

	// _pycsh_util_get_single() and _pycsh_util_get_array() will return NULL for exceptions, which is fine with us.
	if (param->array_size > 1 && param->type != PARAM_TYPE_STRING)
		return _pycsh_util_get_array(param, 1, dest, timeout, retries, paramver);
	return _pycsh_util_get_single(param, offset, 1, dest, timeout, retries, paramver);
}

PyObject * pycsh_param_set(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!csp_initialized()) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before .init() has been called.");
		return NULL;
	}

	PyObject * param_identifier;  // Raw argument object/type passed. Identify its type when needed.
	PyObject * value;
	int node = pycsh_dfl_node;
	int server = INT_MIN;
	int paramver = 2;
	int offset = INT_MIN;  // Using INT_MIN as the least likely value as Parameter arrays should be backwards subscriptable like lists.
	int timeout = pycsh_dfl_timeout;
	int retries = 1;

	static char *kwlist[] = {"param_identifier", "value", "node", "server", "paramver", "offset", "timeout", "retries", NULL};
	
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO|iiiiii", kwlist, &param_identifier, &value, &node, &server, &paramver, &offset, &timeout, &retries)) {
		return NULL;  // TypeError is thrown
	}

	param_t *param = _pycsh_util_find_param_t(param_identifier, node);

	if (param == NULL)  // Did not find a match.
		return NULL;  // Raises TypeError or ValueError.

	int dest = node;
	if (server > 0)
		dest = server;

	if((PyIter_Check(value) || PySequence_Check(value)) && !PyObject_TypeCheck(value, &PyUnicode_Type)) {
		if (_pycsh_util_set_array(param, value, dest, timeout, retries, paramver))
			return NULL;  // Raises one of many possible exceptions.
	} else {
#if 0  /* TODO Kevin: When should we use queues with the new cmd system? */
		param_queue_t *usequeue = autosend ? NULL : &param_queue_set;
#endif
		if (_pycsh_util_set_single(param, value, offset, dest, timeout, retries, paramver, 1))
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

	unsigned int node = pycsh_dfl_node;
	unsigned int timeout = pycsh_dfl_timeout;
	char * include_mask_str = NULL;
	char * exclude_mask_str = NULL;
	int paramver = 2;

	static char *kwlist[] = {"node", "timeout", "imask", "emask", "paramver", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|IIssi", kwlist, &node, &timeout, &include_mask_str, &exclude_mask_str, &paramver)) {
		return NULL;
	}


	int param_pull_res;
	Py_BEGIN_ALLOW_THREADS;
	uint32_t include_mask = 0xFFFFFFFF;
	uint32_t exclude_mask = PM_REMOTE | PM_HWREG;

	if (include_mask_str != NULL)
		include_mask = param_maskstr_to_mask(include_mask_str);
	if (exclude_mask_str != NULL)
		exclude_mask = param_maskstr_to_mask(exclude_mask_str);

	param_pull_res = param_pull_all(1, node, include_mask, exclude_mask, timeout, paramver);
	Py_END_ALLOW_THREADS;
	if (param_pull_res) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		return NULL;
	}

	Py_RETURN_NONE;
}

PyObject * pycsh_param_cmd_new(PyObject * self, PyObject * args, PyObject * kwds) {

	char *type;
	char *name;
	int paramver = 2;

	static char *kwlist[] = {"type", "name", "paramver", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "is|i", kwlist, &type, &name, &paramver, &paramver)) {
		return NULL;
	}

	/* Set/get */
	if (strncmp(type, "get", 4) == 0) {
		param_queue.type = PARAM_QUEUE_TYPE_GET;
	} else if (strncmp(type, "set", 4) == 0) {
		param_queue.type = PARAM_QUEUE_TYPE_SET;
	} else {
		PyErr_SetString(PyExc_ValueError, "Must specify 'get' or 'set'");
		return NULL;
	}

	/* Command name */
	strncpy(param_queue.name, name, sizeof(param_queue.name) - 1);

	param_queue.used = 0;
	param_queue.version = paramver;

	printf("Initialized new command: %s\n", name);

	Py_RETURN_NONE;
}

PyObject * pycsh_param_cmd_done(PyObject * self, PyObject * args) {
	param_queue.type = PARAM_QUEUE_TYPE_EMPTY;
	Py_RETURN_NONE;
}