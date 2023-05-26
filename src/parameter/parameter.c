/*
 * parameter.c
 *
 * Contains the Parameter base class.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

#include "parameter.h"

#include "structmember.h"

#include <param/param.h>

#include "../pycsh.h"
#include "../utils.h"

/* 1 for success. Comapares the wrapped param_t for parameters, otherwise 0. Assumes self to be a ParameterObject. */
static int Parameter_equal(PyObject *self, PyObject *other) {
	if (PyObject_TypeCheck(other, &ParameterType) && (memcmp(&(((ParameterObject *)other)->param), &(((ParameterObject *)self)->param), sizeof(param_t)) == 0))
		return 1;
	return 0;
}

static PyObject * Parameter_richcompare(PyObject *self, PyObject *other, int op) {

	PyObject *result = Py_NotImplemented;

	switch (op) {
		// case Py_LT:
		// 	break;
		// case Py_LE:
		// 	break;
		case Py_EQ:
			result = (Parameter_equal(self, other)) ? Py_True : Py_False;
			break;
		case Py_NE:
			result = (Parameter_equal(self, other)) ? Py_False : Py_True;
			break;
		// case Py_GT:
		// 	break;
		// case Py_GE:
		// 	break;
	}

    Py_XINCREF(result);
    return result;
}

static PyObject * Parameter_str(ParameterObject *self) {
	char buf[100];
	sprintf(buf, "[id:%i|node:%i] %s | %s", self->param.id, self->param.node, self->param.name, self->type->tp_name);
	return Py_BuildValue("s", buf);
}

static void Parameter_dealloc(ParameterObject *self) {
	free(self->param.addr);
	// Get the type of 'self' in case the user has subclassed 'Parameter'.
	Py_TYPE(self)->tp_free((PyObject *) self);
}

/* May perform black magic and return a ParameterArray instead of the specified type. */
static PyObject * Parameter_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {

	static char *kwlist[] = {"param_identifier", "node", "host", "paramver", "timeout", "retries", NULL};

	PyObject * param_identifier;  // Raw argument object/type passed. Identify its type when needed.
	int node = pycsh_dfl_node;
	int host = INT_MIN;
	int paramver = 2;
	int timeout = pycsh_dfl_timeout;
	int retries = 1;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|iiiii", kwlist, &param_identifier, &node, &host, &paramver, &timeout, &retries)) {
		return NULL;  // TypeError is thrown
	}

	param_t * param = _pycsh_util_find_param_t(param_identifier, node);

	if (param == NULL)  // Did not find a match.
		return NULL;  // Raises TypeError or ValueError.

    return _pycsh_Parameter_from_param(type, param, NULL, host, timeout, retries, paramver);
}

static PyObject * Parameter_get_host(ParameterObject *self, void *closure) {
	if (self->host != INT_MIN)
		return Py_BuildValue("i", self->host);
	Py_RETURN_NONE;
}

static int Parameter_set_host(ParameterObject *self, PyObject *value, void *closure) {

	if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the host attribute");
        return -1;
    }

	if (value == Py_None) {  // Set the host to INT_MIN, which we use as no host.
		self->host = INT_MIN;
		return 0;
	}

	if(!PyLong_Check(value)) {
		PyErr_SetString(PyExc_TypeError,
                        "The host attribute must be set to an int or None");
        return -1;
	}

	int host = _PyLong_AsInt(value);

	if (PyErr_Occurred())
		return -1;  // 'Reraise' the current exception.

	self->host = host;

	return 0;
}

static PyObject * Parameter_get_oldvalue(ParameterObject *self, void *closure) {
	PyErr_SetString(PyExc_AttributeError, "Parameter.value has been changed to .remote_value and .cached_value instead");
	return NULL;
}

static int Parameter_set_oldvalue(ParameterObject *self, PyObject *value, void *closure) {
	PyErr_SetString(PyExc_AttributeError, "Parameter.value has been changed to .remote_value and .cached_value instead");
	return -1;
}

static PyObject * Parameter_get_value(ParameterObject *self, int remote) {
	if (self->param.array_size > 1 && self->param.type != PARAM_TYPE_STRING)
		return _pycsh_util_get_array(&self->param, remote, self->host, self->timeout, self->retries, self->paramver);
	return _pycsh_util_get_single(&self->param, INT_MIN, remote, self->host, self->timeout, self->retries, self->paramver);
}

static int Parameter_set_value(ParameterObject *self, PyObject *value, int remote) {

	if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the value attribute");
        return -1;
    }

	if (self->param.array_size > 1 && self->param.type != PARAM_TYPE_STRING)  // Is array parameter
		return _pycsh_util_set_array(&self->param, value, self->host, self->timeout, self->retries, self->paramver);
	return _pycsh_util_set_single(&self->param, value, INT_MIN, self->host, self->timeout, self->retries, self->paramver, remote);  // Normal parameter
}

static PyObject * Parameter_get_remote_value(ParameterObject *self, void *closure) {
	return Parameter_get_value(self, 1);
}

static PyObject * Parameter_get_cached_value(ParameterObject *self, void *closure) {
	return Parameter_get_value(self, 0);
}

static int Parameter_set_remote_value(ParameterObject *self, PyObject *value, void *closure) {
	return Parameter_set_value(self, value, 1);
}

static int Parameter_set_cached_value(ParameterObject *self, PyObject *value, void *closure) {
	return Parameter_set_value(self, value, 0);
}

static PyObject * Parameter_is_vmem(ParameterObject *self, void *closure) {
	// I believe this is the most appropriate way to check for vmem parameters.
	PyObject * result = self->param.vmem == NULL ? Py_False : Py_True;
	Py_INCREF(result);
	return result;
}

static PyObject * Parameter_get_timeout(ParameterObject *self, void *closure) {
	return Py_BuildValue("i", self->timeout);
}

static int Parameter_set_timeout(ParameterObject *self, PyObject *value, void *closure) {

	if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the timeout attribute");
        return -1;
    }

	if (value == Py_None) {
		self->timeout = pycsh_dfl_timeout;
		return 0;
	}

	if(!PyLong_Check(value)) {
		PyErr_SetString(PyExc_TypeError,
                        "The timeout attribute must be set to an int or None");
        return -1;
	}

	int timeout = _PyLong_AsInt(value);

	if (PyErr_Occurred())
		return -1;  // 'Reraise' the current exception.

	self->timeout = timeout;

	return 0;
}

static PyObject * Parameter_get_retries(ParameterObject *self, void *closure) {
	return Py_BuildValue("i", self->retries);
}

static int Parameter_set_retries(ParameterObject *self, PyObject *value, void *closure) {

	if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the retries attribute");
        return -1;
    }

	if (value == Py_None) {
		self->retries = 1;
		return 0;
	}

	if(!PyLong_Check(value)) {
		PyErr_SetString(PyExc_TypeError,
                        "The retries attribute must be set to an int or None");
        return -1;
	}

	int retries = _PyLong_AsInt(value);

	if (PyErr_Occurred())
		return -1;  // 'Reraise' the current exception.

	self->retries = retries;

	return 0;
}

static PyMemberDef Parameter_members[] = {
    {"name", 	  T_STRING, 	offsetof(ParameterObject, param.name), 	 	READONLY, "The name of the wrapped param_t C struct"},
    {"unit", 	  T_STRING, 	offsetof(ParameterObject, param.unit), 	 	READONLY, "The unit of the wrapped param_t c struct as a string or None"},
    {"docstr", 	  T_STRING, 	offsetof(ParameterObject, param.docstr), 	READONLY, "The help-text of the wrapped param_t c struct as a string or None"},
    {"id", 		  T_SHORT, 		offsetof(ParameterObject, param.id), 	 	READONLY, "ID of the parameter"},
    {"type", 	  T_OBJECT_EX,  offsetof(ParameterObject, type), 		 	READONLY, "type of the parameter"},
    {"mask", 	  T_UINT, 		offsetof(ParameterObject, param.mask),   	READONLY, "mask of the parameter"},
    {"timestamp", T_UINT, 		offsetof(ParameterObject, param.timestamp), READONLY, "timestamp of the parameter"},
    {"node", 	  T_SHORT, 		offsetof(ParameterObject, param.node), 		READONLY, "node of the parameter"},
    {NULL, 0, 0, 0, NULL}  /* Sentinel */
};

/* 
The Python binding 'Parameter' class exposes most of its attributes through getters, 
as only its 'value', 'host' and 'node' are mutable, and even those are through setters.
*/
static PyGetSetDef Parameter_getsetters[] = {
#if 0
	{"node", (getter)Parameter_get_node, (setter)Parameter_set_node,
     "node of the parameter", NULL},
#endif
	{"host", (getter)Parameter_get_host, (setter)Parameter_set_host,
     "host of the parameter", NULL},
	{"remote_value", (getter)Parameter_get_remote_value, (setter)Parameter_set_remote_value,
     "get/set the remote (and cached) value of the parameter", NULL},
	{"cached_value", (getter)Parameter_get_cached_value, (setter)Parameter_set_cached_value,
     "get/set the cached value of the parameter", NULL},
	{"value", (getter)Parameter_get_oldvalue, (setter)Parameter_set_oldvalue,
     "value of the parameter", NULL},
	{"is_vmem", (getter)Parameter_is_vmem, NULL,
     "whether the parameter is a vmem parameter", NULL},
	{"timeout", (getter)Parameter_get_timeout, (setter)Parameter_set_timeout,
     "timeout of the parameter", NULL},
	{"retries", (getter)Parameter_get_retries, (setter)Parameter_set_retries,
     "available retries of the parameter", NULL},
    {NULL, NULL, NULL, NULL}  /* Sentinel */
};

PyTypeObject ParameterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pycsh.Parameter",
    .tp_doc = "Wrapper utility class for libparam parameters.",
    .tp_basicsize = sizeof(ParameterObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Parameter_new,
    .tp_dealloc = (destructor)Parameter_dealloc,
	.tp_getset = Parameter_getsetters,
	.tp_members = Parameter_members,
	// .tp_methods = Parameter_methods,
	.tp_str = (reprfunc)Parameter_str,
	.tp_richcompare = (richcmpfunc)Parameter_richcompare,
};
