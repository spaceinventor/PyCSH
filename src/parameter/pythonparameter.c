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

// Instantiated in our PyMODINIT_FUNC
PyObject * PyExc_ParamCallbackError;
PyObject * PyExc_InvalidParameterTypeError;

/**
 * @brief Shared callback for all param_t's wrapped by a Parameter instance.
 */
void Parameter_callback(param_t * param, int offset) {
    PyGILState_STATE gstate = PyGILState_Ensure();
    assert(Parameter_wraps_param(param));
    assert(!PyErr_Occurred());  // Callback may raise an exception. But we don't want to override an existing one.

    PyObject *key = PyLong_FromVoidPtr(param);
    PythonParameterObject *python_param = (PythonParameterObject*)PyDict_GetItem((PyObject*)param_callback_dict, key);
    Py_DECREF(key);

    /* This param_t uses the Python Parameter callback, but doesn't actually point to a Parameter.
        Perhaps it was deleted? Or perhaps it was never set correctly. */
    if (python_param == NULL) {
        assert(false);  // TODO Kevin: Is this situation worthy of an assert(), or should we just ignore it?
        PyGILState_Release(gstate);
        return;
    }

    // PythonParameterObject *python_param = (PythonParameterObject *)((char *)param - offsetof(PythonParameterObject, parameter_object.param));
    PyObject *python_callback = python_param->callback;

    /* This Parameter has no callback */
    /* Python_callback should not be NULL here when Parameter_wraps_param(), but we will allow it for now... */
    if (python_callback == NULL || python_callback == Py_None) {
        PyGILState_Release(gstate);
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
        // TODO Kevin: It seems exceptions raised in the CSP thread are ignored.
        _PyErr_FormatFromCause(PyExc_ParamCallbackError, "Error calling Python callback");
    }
    PyGILState_Release(gstate);
}

/* Internal API for creating a new PythonParameterObject. */
static PythonParameterObject * Parameter_create_new(PyTypeObject *type, uint16_t id, param_type_e param_type, uint32_t mask, char * name, char * unit, char * docstr, int array_size, const PyObject * callback, int host, int timeout, int retries, int paramver) {

    /* Check for valid parameter type. param_list_create_remote() should always return NULL for errors,
        but this allows us to raise a specific exception. */
    /* I'm not sure whether we can use (param_type > PARAM_TYPE_INVALID) to check for invalid parameters,
        so for now we will use a switch. This should also make GCC warn us when new types are added. */
    switch (param_type) {

        case PARAM_TYPE_UINT8:
        case PARAM_TYPE_UINT16:
        case PARAM_TYPE_UINT32:
        case PARAM_TYPE_UINT64:
        case PARAM_TYPE_INT8:
        case PARAM_TYPE_INT16:
        case PARAM_TYPE_INT32:
        case PARAM_TYPE_INT64:
        case PARAM_TYPE_XINT8:
        case PARAM_TYPE_XINT16:
        case PARAM_TYPE_XINT32:
        case PARAM_TYPE_XINT64:
        case PARAM_TYPE_FLOAT:
        case PARAM_TYPE_DOUBLE:
        case PARAM_TYPE_STRING:
        case PARAM_TYPE_DATA:
        case PARAM_TYPE_INVALID:
            break;
        
        default:
            PyErr_SetString(PyExc_InvalidParameterTypeError, "An invalid parameter type was specified during creation of a new parameter");
            return NULL;
    }

    param_t * new_param = param_list_create_remote(id, 0, param_type, mask, array_size, name, unit, docstr, -1);
    if (new_param == NULL) {
        return (PythonParameterObject*)PyErr_NoMemory();
    }

    PythonParameterObject * self = (PythonParameterObject *)_pycsh_Parameter_from_param(type, new_param, callback, host, timeout, retries, paramver);
    if (self == NULL) {
        /* This is likely a memory allocation error, in which case we expect .tp_alloc() to have raised an exception. */
        return NULL;
    }

    switch (param_list_add(new_param)) {
        case 0:
            break;  // All good
        case 1: {
            PyErr_SetString(PyExc_KeyError, "Local parameter with the specifed ID already exists");
            Py_DECREF(self);
            return NULL;
        }
        default: {
            Py_DECREF(self);
            assert(false);  // list_dynamic=false ?
            break;
        }
    }

    // ((ParameterObject *)self)->param->callback = Parameter_callback;  // NOTE: This assignment is performed in _pycsh_Parameter_from_param()
    self->keep_alive = 1;
    Py_INCREF(self);  // Parameter list holds a reference to the ParameterObject
    /* NOTE: If .keep_alive defaults to False, then we should remove this Py_INCREF() */

    /* NULL callback becomes None on a ParameterObject instance */
    if (callback == NULL)
        callback = Py_None;

    if (Parameter_set_callback(self, (PyObject *)callback, NULL) == -1) {
        Py_DECREF(self);
        return NULL;
    }

    ((ParameterObject*)self)->param->callback = Parameter_callback;

    return self;
}

static void PythonParameter_dealloc(PythonParameterObject *self) {
    if (self->callback != NULL && self->callback != Py_None) {
        Py_XDECREF(self->callback);
        self->callback = NULL;
    }
    param_list_remove_specific(((ParameterObject*)self)->param, 0, 1);
    // Get the type of 'self' in case the user has subclassed 'Parameter'.
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static PyObject * PythonParameter_new(PyTypeObject *type, PyObject * args, PyObject * kwds) {

    uint16_t id;
    // TODO Kevin: Flags should not be hard coded here.
    char * name;
    param_type_e param_type;
    uint32_t mask;
    char * unit = "";
    char * docstr = "";
    int array_size = 0;
    PyObject * callback = NULL;
    int host = INT_MIN;
    // TODO Kevin: What are these 2 doing here?
    int timeout = pycsh_dfl_timeout;
    int retries = 0;
    int paramver = 2;

    static char *kwlist[] = {"id", "name", "type", "mask", "unit", "docstr", "array_size", "callback", "host", "timeout", "retries", "paramver", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "HsiI|ssiOiiii", kwlist, &id, &name, &param_type, &mask, &unit, &docstr, &array_size, &callback, &host, &timeout, &retries, &paramver))
        return NULL;  // TypeError is thrown

    if (param_list_find_id(0, id) != NULL) {
        /* Run away as quickly as possible if this ID is already in use, we would otherwise get a segfault, which is driving me insane. */
        PyErr_Format(PyExc_ValueError, "Parameter with id %d already exists", id);
        return NULL;
    }

    if (array_size < 1)
        array_size = 1;

    PythonParameterObject * python_param = Parameter_create_new(type, id, param_type, mask, name, unit, docstr, array_size, callback, host, timeout, retries, paramver);
    if (python_param == NULL) {
        // Assume exception message to be set by Parameter_create_new()
        /* physaddr should be freed in dealloc() */
        return NULL;
    }

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