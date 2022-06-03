/*
 * parameter.c
 *
 * Contains the Parameter base class.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

#include "parameter.h"

#include <param/param.h>

#include "../pycsh.h"
#include "../utils.h"



/* 1 for success. Comapares the wrapped param_t for parameters, otherwise 0. Assumes self to be a ParameterObject. */
static int Parameter_equal(PyObject *self, PyObject *other) {
	if (PyObject_TypeCheck(other, &ParameterType) && ((ParameterObject *)other)->param == ((ParameterObject *)self)->param)
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
	sprintf(buf, "[id:%i|node:%i] %s | %s", self->param->id, self->param->node, self->param->name, self->type->tp_name);
	return Py_BuildValue("s", buf);
}

static void Parameter_dealloc(ParameterObject *self) {
	Py_XDECREF((PyObject*)self->type);
	Py_XDECREF(self->name);
	Py_XDECREF(self->unit);
	// Get the type of 'self' in case the user has subclassed 'Parameter'.
	// Not that this makes a lot of sense to do.
	Py_TYPE(self)->tp_free((PyObject *) self);
}

/* May perform black magic and return a ParameterArray instead of the specified type. */
static PyObject * Parameter_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {

	static char *kwlist[] = {"param_identifier", "node", "host", "timeout", "retries", NULL};

	PyObject * param_identifier;  // Raw argument object/type passed. Identify its type when needed.
	int node = default_node;
	int host = INT_MIN;
	int timeout = PYCSH_DFL_TIMEOUT;
	int retries = 1;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|iiii", kwlist, &param_identifier, &node, &host, &timeout, &retries)) {
		return NULL;  // TypeError is thrown
	}

	param_t * param = _pycsh_util_find_param_t(param_identifier, node);

	if (param == NULL)  // Did not find a match.
		return NULL;  // Raises TypeError or ValueError.

    return _pycsh_Parameter_from_param(type, param, host, timeout, retries);
}

static PyObject * Parameter_getname(ParameterObject *self, void *closure) {
	Py_INCREF(self->name);
	return self->name;
}

static PyObject * Parameter_getunit(ParameterObject *self, void *closure) {
	Py_INCREF(self->unit);
	return self->unit;
}

static PyObject * Parameter_getdocstr(ParameterObject *self, void *closure) {
	Py_INCREF(self->docstr);
	return self->docstr;
}

static PyObject * Parameter_getid(ParameterObject *self, void *closure) {
	return Py_BuildValue("H", self->param->id);
}

static PyObject * Parameter_getnode(ParameterObject *self, void *closure) {
	return Py_BuildValue("H", self->param->node);
}

/* This will change self->param to be one by the same name at the specified node. */
static int Parameter_setnode(ParameterObject *self, PyObject *value, void *closure) {

	if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the node attribute");
        return -1;
    }

	if(!PyLong_Check(value)) {
		PyErr_SetString(PyExc_TypeError,
                        "The node attribute must be set to an int");
        return -1;
	}

	uint16_t node;

	// This is pretty stupid, but seems to be the easiest way to convert a long to short using Python.
	PyObject * value_tuple = PyTuple_Pack(1, value);
	if (!PyArg_ParseTuple(value_tuple, "H", &node)) {
		Py_DECREF(value_tuple);
		return -1;
	}
	Py_DECREF(value_tuple);

	param_t * param = _pycsh_util_find_param_t(self->name, node);

	if (param == NULL)  // Did not find a match.
		return -1;  // Raises either TypeError or ValueError.

	self->param = param;

	return 0;
}

static PyObject * Parameter_gethost(ParameterObject *self, void *closure) {
	if (self->host != INT_MIN)
		return Py_BuildValue("i", self->host);
	Py_RETURN_NONE;
}

/* This will change self->param to be one by the same name at the specified node. */
static int Parameter_sethost(ParameterObject *self, PyObject *value, void *closure) {

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

static PyObject * Parameter_gettype(ParameterObject *self, void *closure) {
	Py_INCREF(self->type);
	return (PyObject *)self->type;
}

static PyObject * Parameter_getvalue(ParameterObject *self, void *closure) {
	if (self->param->array_size > 1 && self->param->type != PARAM_TYPE_STRING)
		return _pycsh_util_get_array(self->param, autosend, self->host, self->timeout, self->retries);
	return _pycsh_util_get_single(self->param, INT_MIN, autosend, self->host, self->timeout, self->retries);
}

static int Parameter_setvalue(ParameterObject *self, PyObject *value, void *closure) {

	if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the value attribute");
        return -1;
    }

	if (self->param->array_size > 1 && self->param->type != PARAM_TYPE_STRING)  // Is array parameter
		return _pycsh_util_set_array(self->param, value, self->host, self->timeout, self->retries);
	param_queue_t *usequeue = autosend ? NULL : &param_queue_set;
	return _pycsh_util_set_single(self->param, value, INT_MIN, self->host, self->timeout, self->retries, usequeue);  // Normal parameter
}

static PyObject * Parameter_is_array(ParameterObject *self, void *closure) {
	// I believe this is the most appropriate way to check whether the parameter is an array.
	PyObject * result = self->param->array_size > 1 ? Py_True : Py_False;
	Py_INCREF(result);
	return result;
}

static PyObject * Parameter_is_vmem(ParameterObject *self, void *closure) {
	// I believe this is the most appropriate way to check for vmem parameters.
	PyObject * result = self->param->vmem == NULL ? Py_False : Py_True;
	Py_INCREF(result);
	return result;
}

static PyObject * Parameter_getmask(ParameterObject *self, void *closure) {
	return Py_BuildValue("I", self->param->mask);
}

static PyObject * Parameter_gettimestamp(ParameterObject *self, void *closure) {
	return Py_BuildValue("I", self->param->timestamp);
}

static PyObject * Parameter_gettimeout(ParameterObject *self, void *closure) {
	return Py_BuildValue("i", self->timeout);
}

static int Parameter_settimeout(ParameterObject *self, PyObject *value, void *closure) {

	if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the timeout attribute");
        return -1;
    }

	if (value == Py_None) {
		self->timeout = PYCSH_DFL_TIMEOUT;
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

static PyObject * Parameter_getretries(ParameterObject *self, void *closure) {
	return Py_BuildValue("i", self->retries);
}

static int Parameter_setretries(ParameterObject *self, PyObject *value, void *closure) {

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

/* 
The Python binding 'Parameter' class exposes most of its attributes through getters, 
as only its 'value', 'host' and 'node' are mutable, and even those are through setters.
*/
static PyGetSetDef Parameter_getsetters[] = {
    {"name", (getter)Parameter_getname, NULL,
     "Returns the name of the wrapped param_t C struct.", NULL},
    {"unit", (getter)Parameter_getunit, NULL,
     "The unit of the wrapped param_t c struct as a string or None.", NULL},
	{"docstr", (getter)Parameter_getdocstr, NULL,
     "The help-text of the wrapped param_t c struct as a string or None.", NULL},
	{"id", (getter)Parameter_getid, NULL,
     "id of the parameter", NULL},
	{"node", (getter)Parameter_getnode, (setter)Parameter_setnode,
     "node of the parameter", NULL},
	{"host", (getter)Parameter_gethost, (setter)Parameter_sethost,
     "host of the parameter", NULL},
	{"type", (getter)Parameter_gettype, NULL,
     "type of the parameter", NULL},
	{"value", (getter)Parameter_getvalue, (setter)Parameter_setvalue,
     "value of the parameter", NULL},
	{"is_array", (getter)Parameter_is_array, NULL,
     "whether the parameter is an array", NULL},
	{"is_vmem", (getter)Parameter_is_vmem, NULL,
     "whether the parameter is a vmem parameter", NULL},
	{"mask", (getter)Parameter_getmask, NULL,
     "mask of the parameter", NULL},
	{"timestamp", (getter)Parameter_gettimestamp, NULL,
     "timestamp of the parameter", NULL},
	{"timeout", (getter)Parameter_gettimeout, (setter)Parameter_settimeout,
     "timeout of the parameter", NULL},
	{"retries", (getter)Parameter_getretries, (setter)Parameter_setretries,
     "available retries of the parameter", NULL},
    {NULL}  /* Sentinel */
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
	.tp_str = (reprfunc)Parameter_str,
	.tp_richcompare = (richcmpfunc)Parameter_richcompare,
};
