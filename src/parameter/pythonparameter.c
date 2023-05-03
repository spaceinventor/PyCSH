/*
 * pythonparameter.c
 *
 * Contains the PythonParameter Parameter subclass.
 *
 */

#include "pythonparameter.h"

#include "structmember.h"

#include <param/param.h>

#include "../pycsh.h"
#include "../utils.h"

/**
 * @brief Shared callback for all param_t's wrapped by a Parameter instance.
 */
void Parameter_callback(param_t * param, int offset) {
	assert(Parameter_wraps_param(param));
	assert(!PyErr_Occurred());  // Callback may raise an exception. But we don't want to override an existing one.

	PythonParameterObject *python_param = (PythonParameterObject *)((char *)param - offsetof(PythonParameterObject, parameter_object.param));
	PyObject *python_callback = python_param->callback;

	/* This Parameter has no callback */
	/* Python_callback should not be NULL here when Parameter_wraps_param(), but we will allow it for now... */
	if (python_callback == NULL || python_callback == Py_None) {
		return;
    }

	assert(PyCallable_Check(python_callback));
	/* Create the arguments. */
	PyObject *pyoffset = Py_BuildValue("i", offset);
	PyObject * args = PyTuple_Pack(2, python_param, pyoffset);
	/* Call the user Python callback */
	PyObject_CallObject(python_callback, args);
	/* Cleanup */
	Py_DECREF(args);
	Py_DECREF(pyoffset);

	if (PyErr_Occurred()) {
		/* It may not be clear to the user, that the exception came from the callback,
			we therefore chain unto the existing exception, for better clarity. */
		/* _PyErr_FormatFromCause() seems simpler than PyException_SetCause() and PyException_SetContext() */
		// TODO Kevin: We could create a CallbackException class and raise here.
		_PyErr_FormatFromCause(PyExc_RuntimeError, "Error calling Python callback");
	}
}

int Parameter_wraps_param(param_t *param) {
	assert(param!= NULL);
	return param->callback == Parameter_callback;
}

static PythonParameterObject * Parameter_create_new(PyTypeObject *type, uint16_t id, param_type_e param_type, char * name, char * unit, char * docstr, void * addr, int array_size, const PyObject * callback, int host, int timeout, int retries, int paramver) {
	param_t param = {
		.id = id,
		.type = param_type,
		.name = name,
		.unit = unit,
		.docstr = docstr,
		.addr = addr,
		.array_size = array_size,
		.array_step = param_typesize(param_type),
	};
	PythonParameterObject * self = (PythonParameterObject *)_pycsh_Parameter_from_param(type, &param, callback, host, timeout, retries, paramver);
	param_list_add(&((ParameterObject *)self)->param);

    ((ParameterObject *)self)->param.callback = Parameter_callback;
    self->keep_alive = 1;

    /* NULL callback becomes None on a ParameterObject instance */
	if (callback == NULL)
		callback = Py_None;

	if (Parameter_set_callback(self, (PyObject *)callback, NULL) == -1) {
		Py_DECREF(self);
        return NULL;
    }

	return self;
}

static void PythonParameter_dealloc(PythonParameterObject *self) {
	if (self->callback != NULL && self->callback != Py_None) {
		Py_XDECREF(self->callback);
        self->callback = NULL;
    }
    /* We can't let param_list_remove_specific() free() the parameter, as it should be done by Python. */
	param_list_remove_specific(&((ParameterObject*)self)->param, 0, 0);
    /* ... But it is still our responsibility to free the .addr memory.
        It should also have been allocated in our __new__() method. */
    free((((ParameterObject*)self)->param.addr));
	// Get the type of 'self' in case the user has subclassed 'Parameter'.
	Py_TYPE(self)->tp_free((PyObject *) self);
}

static PyObject * PythonParameter_new(PyTypeObject *type, PyObject * args, PyObject * kwds) {

	uint16_t id;
	param_type_e param_type = PARAM_TYPE_INT32;  // TODO Kevin: type should not be hard coded here.
	char * name;
	char * unit = "";
	char * docstr = "";
	int array_size = 0;
	PyObject * callback = NULL;
	int host = INT_MIN;
	// TODO Kevin: What are these 2 doing here?
	int timeout = pycsh_dfl_timeout;
	int retries = 0;
	int paramver = 2;

	static char *kwlist[] = {"id", "name", "unit", "docstr", "array_size", "callback", "host", "timeout", "retries", "paramver", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "Hs|ssiOiiii", kwlist, &id, &name, &unit, &docstr, &array_size, &callback, &host, &timeout, &retries, &paramver))
		return NULL;  // TypeError is thrown

	if (array_size < 1)
		array_size = 1;

	void * physaddr = calloc(param_typesize(param_type), array_size);
	if (physaddr == NULL) {
		return PyErr_NoMemory();
	}
	PythonParameterObject * python_param = Parameter_create_new(type, id, param_type, name, unit, docstr, physaddr, array_size, callback, host, timeout, retries, paramver);
	if (python_param == NULL) {
		PyErr_SetString(PyExc_MemoryError, "Failed to allocate ParameterObject");
		free(physaddr);
		physaddr = NULL;
        return NULL;
    }

	Py_INCREF(python_param);  // Parameter list holds a reference to the ParameterObject
	/* NOTE: Unless .keep_alive defaults to False, then we should remove this Py_INCREF() */

	/* return should steal the reference created by Parameter_create_new() */
	return (PyObject *)python_param;
}

static PyObject * Parameter_get_keep_alive(PythonParameterObject *self, void *closure) {
	return self->keep_alive ? Py_True : Py_False;
}

static int Parameter_set_keep_alive(PythonParameterObject *self, PyObject *value, void *closure) {

	if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the keep_alive attribute");
        return -1;
    }

	if (value != Py_True && value != Py_False) {
		PyErr_SetString(PyExc_TypeError, "keep_alive must be True or False");
        return -1;
    }

	if (self->keep_alive && value == Py_False) {
		self->keep_alive = 0;
		Py_DECREF(self);
	} else if (!self->keep_alive && value == Py_True) {
		self->keep_alive = 1;
        Py_INCREF(self);
    }

	return 0;
}

static PyObject * Parameter_get_callback(PythonParameterObject *self, void *closure) {
	return Py_NewRef(self->callback);
}

int Parameter_set_callback(PythonParameterObject *self, PyObject *value, void *closure) {

	if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the callback attribute");
        return -1;
    }

	if (value != Py_None && !PyCallable_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "callback must be None or callable");
        return -1;
    }

	if (value == self->callback)
		return 0;  // No work to do

	/* Changing the callback to None. */
	if (value == Py_None) {
		if (self->callback != Py_None) {
			/* We should not arrive here when the old value is Py_None, 
				but prevent Py_DECREF() on at all cost. */
			Py_XDECREF(self->callback);
		}
		self->callback = Py_None;
        return 0;
    }

	/* We now know that 'value' is a new callable. */

	/* When replacing a previous callable. */
	if (self->callback != Py_None) {
		Py_XDECREF(self->callback);
	}

	Py_INCREF(value);
	self->callback = value;

	return 0;
}

static PyGetSetDef PythonParameter_getsetters[] = {
	{"keep_alive", (getter)Parameter_get_keep_alive, (setter)Parameter_set_keep_alive,
     "Whether the Parameter should remain in the parameter list, when all Python references are lost. This makes it possible to recover the Parameter instance through list()", NULL},
	{"callback", (getter)Parameter_get_callback, (setter)Parameter_set_callback,
     "callback of the parameter", NULL},
    {NULL, NULL, NULL, NULL}  /* Sentinel */
};

PyTypeObject PythonParameterType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "pycsh.PythonParameter",
	.tp_doc = "Parameter created in Python.",
	.tp_basicsize = sizeof(PythonParameterType),
	.tp_itemsize = 0,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_new = PythonParameter_new,
	.tp_dealloc = (destructor)PythonParameter_dealloc,
	.tp_getset = PythonParameter_getsetters,
	// .tp_str = (reprfunc)Parameter_str,
	// .tp_richcompare = (richcmpfunc)Parameter_richcompare,
	.tp_base = &ParameterType,
};