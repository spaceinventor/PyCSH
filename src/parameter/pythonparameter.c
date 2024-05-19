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
    PyGILState_STATE CLEANUP_GIL gstate = PyGILState_Ensure();
    assert(Parameter_wraps_param(param));
    assert(!PyErr_Occurred());  // Callback may raise an exception. But we don't want to override an existing one.

    PyObject *key = PyLong_FromVoidPtr(param);
    PythonParameterObject *python_param = (PythonParameterObject*)PyDict_GetItem((PyObject*)param_callback_dict, key);
    Py_DECREF(key);

    /* This param_t uses the Python Parameter callback, but doesn't actually point to a Parameter.
        Perhaps it was deleted? Or perhaps it was never set correctly. */
    if (python_param == NULL) {
        assert(false);  // TODO Kevin: Is this situation worthy of an assert(), or should we just ignore it?
        return;
    }

    // PythonParameterObject *python_param = (PythonParameterObject *)((char *)param - offsetof(PythonParameterObject, parameter_object.param));
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
        // TODO Kevin: It seems exceptions raised in the CSP thread are ignored.
        _PyErr_FormatFromCause(PyExc_ParamCallbackError, "Error calling Python callback");
    }
}

// Source: https://chat.openai.com
/**
 * @brief Check that the callback accepts exactly one Parameter and one integer,
 *  as specified by "void (*callback)(struct param_s * param, int offset)"
 * 
 * Currently also checks type-hints (if specified).
 * 
 * @param callback function to check
 * @param raise_exc Whether to set exception message when returning false.
 * @return true for success
 */
static bool is_valid_callback(const PyObject *callback, bool raise_exc) {

    /*We currently allow both NULL and Py_None,
            as those are valid to have on PythonParameterObject */
    if (callback == NULL || callback == Py_None)
        return true;

    // Get the __code__ attribute of the function, and check that it is a PyCodeObject
    // TODO Kevin: Hopefully it's safe to assume that PyObject_GetAttrString() won't mutate callback
    PyCodeObject *func_code = (PyCodeObject*)PyObject_GetAttrString((PyObject*)callback, "__code__");
    if (!func_code || !PyCode_Check(func_code)) {
        if (raise_exc)
            PyErr_SetString(PyExc_TypeError, "Provided callback must be callable");
        return false;
    }

    // Check that number of parameters is exactly 2
    /* func_code->co_argcount doesn't account for *args,
        but that's okay as it should always be empty as we only supply 2 arguments. */
    if (func_code->co_argcount != 2) {
        if (raise_exc)
            PyErr_SetString(PyExc_TypeError, "Provided callback must accept exactly 2 arguments");
        Py_DECREF(func_code);
        return false;
    }

    // Get the __annotations__ attribute of the function
    // TODO Kevin: Hopefully it's safe to assume that PyObject_GetAttrString() won't mutate callback
    PyDictObject *func_annotations = (PyDictObject *)PyObject_GetAttrString((PyObject*)callback, "__annotations__");
    assert(PyDict_Check(func_annotations));
    if (!func_annotations) {
        Py_DECREF(func_code);
        return true;  // It's okay to not specify type-hints for the callback.
    }

    // Get the parameters annotation
    PyObject *param_names = func_code->co_varnames;
    if (!param_names) {
        Py_DECREF(func_code);
        Py_DECREF(func_annotations);
        return true;  // Function parameters have not been annotated, this is probably okay.
    }

    // Check if it's a tuple
    if (!PyTuple_Check(param_names)) {
        // TODO Kevin: Not sure what exception to set here.
        if (raise_exc)
            PyErr_Format(PyExc_TypeError, "param_names type \"%s\" %p", param_names->ob_type->tp_name, param_names);
        Py_DECREF(func_code);
        Py_DECREF(func_annotations);
        return false;  // Not sure when this fails, but it's probably bad if it does.
    }

    // TODO Kevin: Perhaps issue warnings for type-hint errors, instead of errors.
    {  // Checking first parameter type-hint
        const char *param_name = PyUnicode_AsUTF8(PyTuple_GetItem(param_names, 0));
        PyTypeObject *param_type = (PyTypeObject *)PyDict_GetItemString((PyObject*)func_annotations, param_name);
        PyTypeObject *expected_param_type = &ParameterType;
        // param_type will be NULL when not type_hinted
        if (param_type != NULL && !PyObject_IsSubclass((PyObject *)param_type, (PyObject *)expected_param_type)) {
            if (raise_exc)
                PyErr_Format(PyExc_TypeError, "First callback parameter should be type-hinted as Parameter (or subclass). (not %s)", param_type->tp_name);
            Py_DECREF(func_code);
            Py_DECREF(func_annotations);
            return false;
        }

    }

    {  // Checking second parameter type-hint
        const char *param_name = PyUnicode_AsUTF8(PyTuple_GetItem(param_names, 1));
        PyTypeObject *param_type = (PyTypeObject*)PyDict_GetItemString((PyObject*)func_annotations, param_name);
        // param_type will be NULL when not type_hinted
        if (param_type != NULL && !PyObject_IsSubclass((PyObject *)param_type, (PyObject *)&PyLong_Type)) {
            if (raise_exc)
                PyErr_Format(PyExc_TypeError, "Second callback parameter should be type-hinted as int offset. (not %s)", param_type->tp_name);
            Py_DECREF(func_code);
            Py_DECREF(func_annotations);
            return false;
        }
    }

    // Cleanup
    Py_DECREF(func_code);
    Py_DECREF(func_annotations);

    return true;
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

    if (param_list_find_id(0, id) != NULL) {
        /* Run away as quickly as possible if this ID is already in use, we would otherwise get a segfault, which is driving me insane. */
        PyErr_Format(PyExc_ValueError, "Parameter with id %d already exists", id);
        return NULL;
    }

    if (param_list_find_name(0, name)) {
        /* While it is perhaps technically acceptable, it's probably best if we don't allow duplicate names either. */
        PyErr_Format(PyExc_ValueError, "Parameter with name \"%s\" already exists", name);
        return NULL;
    }

    if (!is_valid_callback(callback, true)) {
        return NULL;  // Exception message set by is_valid_callback();
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
            // It shouldn't be possible to arrive here, except perhaps from race conditions.
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

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "HsiO|ssiOiiii", kwlist, &id, &name, &param_type, &mask_obj, &unit, &docstr, &array_size, &callback, &host, &timeout, &retries, &paramver))
        return NULL;  // TypeError is thrown

    uint32_t mask;
    if (pycsh_parse_param_mask(mask_obj, &mask) != 0) {
        return NULL;  // Exception message set by pycsh_parse_param_mask()
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

    if (!is_valid_callback(value, true)) {
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