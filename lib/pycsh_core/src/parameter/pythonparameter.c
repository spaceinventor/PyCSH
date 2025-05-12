/*
 * pythonparameter.c
 *
 * Contains the PythonParameter Parameter subclass.
 *
 */

#include "pythonparameter.h"

// It is recommended to always define PY_SSIZE_T_CLEAN before including Python.h
#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "structmember.h"

#include <param/param.h>

#include "../pycsh.h"
#include "../utils.h"
#include "dynamicparameter.h"


static PyObject * PythonParameter_new(PyTypeObject *type, PyObject * args, PyObject * kwds) {

    uint16_t id;
    char * name;
    param_type_e param_type;
    PyObject * mask_obj;
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

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "HsiO|zziOiiii", kwlist, &id, &name, &param_type, &mask_obj, &unit, &docstr, &array_size, &callback, &host, &timeout, &retries, &paramver))
        return NULL;  // TypeError is thrown

    uint32_t mask;
    if (pycsh_parse_param_mask(mask_obj, &mask) != 0) {
        return NULL;  // Exception message set by pycsh_parse_param_mask()
    }

    if (array_size < 1)
        array_size = 1;

#if 0
    static bool first = true;
    if (first) {
        first = false;
        fprintf(stderr, "`PythonParameter(...)` is deprecated, use `DynamicParameter(..., node=0, ...)` instead");
    }
#endif

    PyErr_WarnEx(PyExc_DeprecationWarning, "`PythonParameter(...)` is deprecated, use `DynamicParameter(..., node=0, ...)` instead", 1);

    assert(PyObject_IsSubclass((PyObject*)type, (PyObject*)&PythonParameterType));

    PythonParameterObject * python_param = (PythonParameterObject*)Parameter_create_new(type, id, 0, param_type, mask, name, unit, docstr, array_size, callback, host, timeout, retries, paramver);
    if (python_param == NULL) {
        // Assume exception message to be set by Parameter_create_new()
        /* physaddr should be freed in dealloc() */
        return NULL;
    }

    switch (param_list_add(python_param->dyn_param.parameter_object.param)) {
        case 0:
            break;  // All good
        case 1: {
            // It shouldn't be possible to arrive here, except perhaps from race conditions.
            PyErr_SetString(PyExc_KeyError, "Local parameter with the specifed ID already exists");
            Py_DECREF(python_param);
            return NULL;
        }
        default: {
            Py_DECREF(python_param);
            assert(false);  // list_dynamic=false ?
            break;
        }
    }

    python_param->keep_alive = 1;
    Py_INCREF(python_param);  // Parameter list holds a reference to the ParameterObject
    /* NOTE: If .keep_alive defaults to False, then we should remove this Py_INCREF() */

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


static PyGetSetDef PythonParameter_getsetters[] = {
    {"keep_alive", (getter)Parameter_get_keep_alive, (setter)Parameter_set_keep_alive,
     "Whether the Parameter should remain in the parameter list, when all Python references are lost. This makes it possible to recover the Parameter instance through list()", NULL},
    {NULL, NULL, NULL, NULL}  /* Sentinel */
};

PyTypeObject PythonParameterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pycsh.PythonParameter",
    .tp_doc = "Parameter created in Python.",
    .tp_basicsize = sizeof(PythonParameterObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = PythonParameter_new,
    .tp_getset = PythonParameter_getsetters,
    // .tp_str = (reprfunc)Parameter_str,
    // .tp_richcompare = (richcmpfunc)Parameter_richcompare,
    .tp_base = &DynamicParameterType,
};
