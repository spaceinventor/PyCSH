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

/* Maps param_t to its corresponding PythonParameter for use by C callbacks. */
PyDictObject * param_callback_dict = NULL;

/* 1 for success. Compares the wrapped param_t for parameters, otherwise 0. Assumes self to be a ParameterObject. */
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
	sprintf(buf, "[id:%i|node:%i] %s | %s", self->param->id, *self->param->node, self->param->name, self->type->tp_name);
	return Py_BuildValue("s", buf);
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

static PyObject * Parameter_get_name(ParameterObject *self, void *closure) {
	return Py_BuildValue("s", self->param->name);
}

static PyObject * Parameter_get_unit(ParameterObject *self, void *closure) {
	return Py_BuildValue("s", self->param->unit);
}

static PyObject * Parameter_get_docstr(ParameterObject *self, void *closure) {
	return Py_BuildValue("s", self->param->docstr);
}

static PyObject * Parameter_get_id(ParameterObject *self, void *closure) {
	return Py_BuildValue("H", self->param->id);
}

static PyObject * Parameter_get_node(ParameterObject *self, void *closure) {
	return Py_BuildValue("H", *self->param->node);
}

static PyObject * Parameter_get_storage_type(ParameterObject *self, void *closure) {
	return Py_BuildValue("H", self->param->vmem->type);
}

/* This will change self->param to be one by the same name at the specified node. */
static int Parameter_set_node(ParameterObject *self, PyObject *value, void *closure) {

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
	PyObject * value_tuple AUTO_DECREF = PyTuple_Pack(1, value);
	if (!PyArg_ParseTuple(value_tuple, "H", &node)) {
		return -1;
	}

	param_t * param = param_list_find_id(node, self->param->id);

	if (param == NULL)  // Did not find a match.
		return -1;  // Raises either TypeError or ValueError.

	self->param = param;

	return 0;
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

static PyObject * Parameter_gettype(ParameterObject *self, void *closure) {
	Py_INCREF(self->type);
	return (PyObject *)self->type;
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
	if (self->param->array_size > 1 && self->param->type != PARAM_TYPE_STRING)
		return _pycsh_util_get_array(self->param, remote, self->host, self->timeout, self->retries, self->paramver, pycsh_dfl_verbose);
	return _pycsh_util_get_single(self->param, INT_MIN, remote, self->host, self->timeout, self->retries, self->paramver, pycsh_dfl_verbose);
}

static int Parameter_set_value(ParameterObject *self, PyObject *value, int remote) {

	if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the value attribute");
        return -1;
    }

	if (self->param->array_size > 1 && self->param->type != PARAM_TYPE_STRING && self->param->type != PARAM_TYPE_DATA)  // Is array parameter
		return _pycsh_util_set_array(self->param, value, self->host, self->timeout, self->retries, self->paramver, pycsh_dfl_verbose);
	return _pycsh_util_set_single(self->param, value, INT_MIN, self->host, self->timeout, self->retries, self->paramver, remote, pycsh_dfl_verbose);  // Normal parameter
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
	PyObject * result = self->param->vmem == NULL ? Py_False : Py_True;
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

static PyObject * Parameter_getmask(ParameterObject *self, void *closure) {
	return Py_BuildValue("I", self->param->mask);
}

static PyObject * Parameter_gettimestamp(ParameterObject *self, void *closure) {
	return Py_BuildValue("I", *(self->param->timestamp));
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

static long Parameter_hash(ParameterObject *self) {
	/* Use the ID of the parameter as the hash, as it is assumed unique. */
    return self->param->id;
}

static void Parameter_dealloc(ParameterObject *self) {

	{   /* Remove ourselves from the callback/lookup dictionary */
        PyObject *key AUTO_DECREF = PyLong_FromVoidPtr(self->param);
        assert(key != NULL);
        if (PyDict_GetItem((PyObject*)param_callback_dict, key) != NULL) {
            PyDict_DelItem((PyObject*)param_callback_dict, key);
        }
    }

	/* Somehow we hold a reference to a parameter that is not in the list,
		this should only be possible if it was "list forget"en, after we wrapped it.
		It should therefore follow that we are now responsible for its memory (<-- TODO Kevin: Is this true? It has probably already been freed).
		We must therefore free() it, now that we are being deallocated.
		We check that (self->param != NULL), just in case we allow that to raise exceptions in the future. */
	if (param_list_find_id(*self->param->node, self->param->id) != self->param && self->param != NULL) {
		param_list_destroy(self->param);
	}

	PyTypeObject *baseclass = pycsh_get_base_dealloc_class(&ParameterType);
    baseclass->tp_dealloc((PyObject*)self);
}


static PyObject * ParameterArray_GetItem(ParameterObject *self, PyObject *item) {

	if (!PyLong_Check(item)) {
		PyErr_SetString(PyExc_TypeError, "Index must be an integer.");
        return NULL;
	}

	// _PyLong_AsInt is dependant on the Py_LIMITED_API macro, hence the underscore prefix.
	int index = _PyLong_AsInt(item);

	// We can't use the fact that _PyLong_AsInt() returns -1 for error,
	// because it may be our desired index. So we check for an exception instead.
	if (PyErr_Occurred())
		return NULL;  // 'Reraise' the current exception.

	/* TODO Kevin: How should we handle remote/cached when using indexed access? */
	return _pycsh_util_get_single(self->param, index, 1, self->host, self->timeout, self->retries, self->paramver, -1);
}

static int ParameterArray_SetItem(ParameterObject *self, PyObject* item, PyObject* value) {

	if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete parameter array indexes.");
        return -1;
    }

	if (!PyLong_Check(item)) {
		PyErr_SetString(PyExc_TypeError, "Index must be an integer.");
        return -2;
	}

	// _PyLong_AsInt is dependant on the Py_LIMITED_API macro, hence the underscore prefix.
	int index = _PyLong_AsInt(item);

	// We can't use the fact that _PyLong_AsInt() returns -1 for error,
	// because it may be our desired index. So we check for an exception instead.
	if (PyErr_Occurred())
		return -3;  // 'Reraise' the current exception.

	// _pycsh_util_set_single() uses negative numbers for exceptions,
	// so we just return its return value.
#if 0  /* TODO Kevin: When should we use queues with the new cmd system? */
	param_queue_t *usequeue = autosend ? NULL : &param_queue_set;
#endif
	return _pycsh_util_set_single(self->param, value, index, self->host, self->timeout, self->retries, self->paramver, 1, pycsh_dfl_verbose);
}

static Py_ssize_t ParameterArray_length(ParameterObject *self) {
	#if 0
	// We currently raise an exception when getting len() of non-array type parameters.
	// This stops PyCharm (Perhaps other IDE's) from showing their length as 0. ¯\_(ツ)_/¯
	if (self->param->array_size <= 1) {
		PyErr_SetString(PyExc_AttributeError, "Non-array type parameter is not subscriptable");
		return -1;
	}
	#endif
	return self->param->array_size;
}

static PyMappingMethods ParameterArray_as_mapping = {
    (lenfunc)ParameterArray_length,
    (binaryfunc)ParameterArray_GetItem,
    (objobjargproc)ParameterArray_SetItem
};

/* 
The Python binding 'Parameter' class exposes most of its attributes through getters, 
as only its 'value', 'host' and 'node' are mutable, and even those are through setters.
*/
static PyGetSetDef Parameter_getsetters[] = {

#if 1  // param_t getsetters
	{"name", (getter)Parameter_get_name, NULL,
     "Returns the name of the wrapped param_t C struct.", NULL},
    {"unit", (getter)Parameter_get_unit, NULL,
     "The unit of the wrapped param_t c struct as a string or None.", NULL},
	{"docstr", (getter)Parameter_get_docstr, NULL,
     "The help-text of the wrapped param_t c struct as a string or None.", NULL},
	{"id", (getter)Parameter_get_id, NULL,
     "id of the parameter", NULL},
	{"type", (getter)Parameter_gettype, NULL,
     "type of the parameter", NULL},
	{"mask", (getter)Parameter_getmask, NULL,
     "mask of the parameter", NULL},
	{"timestamp", (getter)Parameter_gettimestamp, NULL,
     "timestamp of the parameter", NULL},
	{"node", (getter)Parameter_get_node, (setter)Parameter_set_node,
     "node of the parameter", NULL},
#endif

#if 1  // Parameter getsetters
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
	{"storage_type", (getter)Parameter_get_storage_type, NULL,
     "storage type of the parameter", NULL},
	{"timeout", (getter)Parameter_get_timeout, (setter)Parameter_set_timeout,
     "timeout of the parameter", NULL},
	{"retries", (getter)Parameter_get_retries, (setter)Parameter_set_retries,
     "available retries of the parameter", NULL},
#endif
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
	.tp_as_mapping = &ParameterArray_as_mapping,
	// .tp_members = Parameter_members,
	// .tp_methods = Parameter_methods,
	.tp_str = (reprfunc)Parameter_str,
	.tp_richcompare = (richcmpfunc)Parameter_richcompare,
	.tp_hash = (hashfunc)Parameter_hash,
};
