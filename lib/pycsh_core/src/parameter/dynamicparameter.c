/*
 * dynamicparameter.c
 *
 * Contains the DynamicParameter Parameter subclass.
 *
 */

#include "dynamicparameter.h"

// It is recommended to always define PY_SSIZE_T_CLEAN before including Python.h
#define PY_SSIZE_T_CLEAN
#include <Python.h>

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

    PyObject *key AUTO_DECREF = PyLong_FromVoidPtr(param);
    DynamicParameterObject *python_param = (DynamicParameterObject*)PyDict_GetItem((PyObject*)param_callback_dict, key);

    /* This param_t uses the Python Parameter callback, but doesn't actually point to a Parameter.
        Perhaps it was deleted? Or perhaps it was never set correctly. */
    if (python_param == NULL) {
        assert(false);  // TODO Kevin: Is this situation worthy of an assert(), or should we just ignore it?
        return;
    }

    // DynamicParameterObject *python_param = (DynamicParameterObject *)((char *)param - offsetof(DynamicParameterObject, parameter_object.param));
    PyObject *python_callback = python_param->callback;

    /* This Parameter has no callback */
    /* Python_callback should not be NULL here when Parameter_wraps_param(), but we will allow it for now... */
    if (python_callback == NULL || python_callback == Py_None) {
        return;
    }

    assert(PyCallable_Check(python_callback));
    /* Create the arguments. */
    PyObject *pyoffset AUTO_DECREF = Py_BuildValue("i", offset);
    PyObject * args AUTO_DECREF = PyTuple_Pack(2, python_param, pyoffset);
    /* Call the user Python callback */
    PyObject *value AUTO_DECREF = PyObject_CallObject(python_callback, args);

    if (PyErr_Occurred()) {
        /* It may not be clear to the user, that the exception came from the callback,
            we therefore chain unto the existing exception, for better clarity. */
        /* _PyErr_FormatFromCause() seems simpler than PyException_SetCause() and PyException_SetContext() */
        // TODO Kevin: It seems exceptions raised in the CSP thread are ignored.
        _PyErr_FormatFromCause(PyExc_ParamCallbackError, "Error calling Python callback");
        #if PYCSH_HAVE_APM  // TODO Kevin: This is pretty ugly, but we can't let the error propagate when building for APM, as there is no one but us to catch it.
            /* It may not be clear to the user, that the exception came from the callback,
                we therefore chain unto the existing exception, for better clarity. */
            /* _PyErr_FormatFromCause() seems simpler than PyException_SetCause() and PyException_SetContext() */
            // TODO Kevin: It seems exceptions raised in the CSP thread are ignored.
            //_PyErr_FormatFromCause(PyExc_ParamCallbackError, "Error calling Python callback");
            PyErr_Print();
        #endif
    }
}

// Source: https://chat.openai.com
/**
 * @brief Check that the callback accepts exactly one Parameter and one integer,
 *  as specified by "void (*callback)(struct param_s * param, int offset)"
 * 
 * Currently also checks type-hints (if specified).
 * Additional optional arguments are also allowed,
 *  as these can be disregarded by the caller.
 * 
 * @param callback function to check
 * @param raise_exc Whether to set exception message when returning false.
 * @return true for success
 */
bool is_valid_callback(const PyObject *callback, bool raise_exc) {

    /*We currently allow both NULL and Py_None,
            as those are valid to have on DynamicParameterObject */
    if (callback == NULL || callback == Py_None)
        return true;

    // Suppress the incompatible pointer type warning when AUTO_DECREF is used on subclasses of PyObject*
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wincompatible-pointer-types"

    // Get the __code__ attribute of the function, and check that it is a PyCodeObject
    // TODO Kevin: Hopefully it's safe to assume that PyObject_GetAttrString() won't mutate callback
    PyCodeObject *func_code AUTO_DECREF = (PyCodeObject*)PyObject_GetAttrString((PyObject*)callback, "__code__");
    if (!func_code || !PyCode_Check(func_code)) {
        if (raise_exc)
            PyErr_SetString(PyExc_TypeError, "Provided callback must be callable");
        return false;
    }

    int accepted_pos_args = pycsh_get_num_accepted_pos_args(callback, raise_exc);
    if (accepted_pos_args < 2) {
        if (raise_exc)
            PyErr_SetString(PyExc_TypeError, "Provided callback must accept at least 2 positional arguments");
        return false;
    }

    // Check for too many required arguments
    int num_non_default_pos_args = pycsh_get_num_required_args(callback, raise_exc);
    if (num_non_default_pos_args > 2) {
        if (raise_exc)
            PyErr_SetString(PyExc_TypeError, "Provided callback must not require more than 2 positional arguments");
        return false;
    }

    // Get the __annotations__ attribute of the function
    // TODO Kevin: Hopefully it's safe to assume that PyObject_GetAttrString() won't mutate callback
    PyDictObject *func_annotations AUTO_DECREF = (PyDictObject *)PyObject_GetAttrString((PyObject*)callback, "__annotations__");

    // Re-enable the warning
    #pragma GCC diagnostic pop

    assert(PyDict_Check(func_annotations));
    if (!func_annotations) {
        return true;  // It's okay to not specify type-hints for the callback.
    }

    // Get the parameters annotation
    // PyCode_GetVarnames() exists and should be exposed, but it doesn't appear to be in any visible header.
    PyObject *param_names AUTO_DECREF = PyObject_GetAttrString((PyObject*)func_code, "co_varnames");// PyCode_GetVarnames(func_code);
    if (!param_names) {
        return true;  // Function parameters have not been annotated, this is probably okay.
    }

    // Check if it's a tuple
    if (!PyTuple_Check(param_names)) {
        // TODO Kevin: Not sure what exception to set here.
        if (raise_exc)
            PyErr_Format(PyExc_TypeError, "param_names type \"%s\" %p", param_names->ob_type->tp_name, param_names);
        return false;  // Not sure when this fails, but it's probably bad if it does.
    }

    PyObject *typing_module_name AUTO_DECREF = PyUnicode_FromString("typing");
    if (!typing_module_name) {
        return false;
    }

    PyObject *typing_module AUTO_DECREF = PyImport_Import(typing_module_name);
    if (!typing_module) {
        if (raise_exc)
            PyErr_SetString(PyExc_ImportError, "Failed to import typing module");
        return false;
    }

#if 1
    PyObject *get_type_hints AUTO_DECREF = PyObject_GetAttrString(typing_module, "get_type_hints");
    if (!get_type_hints) {
        if (raise_exc)
            PyErr_SetString(PyExc_ImportError, "Failed to get 'get_type_hints()' function");
        return false;
    }
    assert(PyCallable_Check(get_type_hints));


    PyObject *type_hint_dict AUTO_DECREF = PyObject_CallFunctionObjArgs(get_type_hints, callback, NULL);

#else

    PyObject *get_type_hints_name AUTO_DECREF = PyUnicode_FromString("get_type_hints");
    if (!get_type_hints_name) {
        return false;
    }

    PyObject *type_hint_dict AUTO_DECREF = PyObject_CallMethodObjArgs(typing_module, get_type_hints_name, callback, NULL);
    if (!type_hint_dict) {
        if (raise_exc)
            PyErr_SetString(PyExc_ImportError, "Failed to get type hints of callback");
        return false;
    }
#endif

    // TODO Kevin: Perhaps issue warnings for type-hint errors, instead of errors.
    {   // Checking first parameter type-hint

        // co_varnames may be too short for our index, if the signature has *args, but that's okay.
        if (PyTuple_Size(param_names)-1 <= 0) {
            return true;
        }

        PyObject *param_name = PyTuple_GetItem(param_names, 0);
        if (!param_name) {
            if (raise_exc)
                PyErr_SetString(PyExc_IndexError, "Could not get first parameter name");
            return false;
        }

        PyObject *param_annotation = PyDict_GetItem(type_hint_dict, param_name);
        if (param_annotation != NULL && param_annotation != Py_None) {
            if (!PyType_Check(param_annotation)) {
                if (raise_exc)
                    PyErr_Format(PyExc_TypeError, "First parameter annotation is %s, which is not a type", param_annotation->ob_type->tp_name);
                return false;
            }
            if (!PyObject_IsSubclass(param_annotation, (PyObject *)&ParameterType)) {
                if (raise_exc)
                    PyErr_Format(PyExc_TypeError, "First callback parameter should be type-hinted as Parameter (or subclass). (not %s)", param_annotation->ob_type->tp_name);
                return false;
            }
        }
    }

    {   // Checking second parameter type-hint

        // co_varnames may be too short for our index, if the signature has *args, but that's okay.
        if (PyTuple_Size(param_names)-1 <= 1) {
            return true;
        }

        PyObject *param_name = PyTuple_GetItem(param_names, 1);
        if (!param_name) {
            if (raise_exc)
                PyErr_SetString(PyExc_IndexError, "Could not get first parameter name");
            return false;
        }

        PyObject *param_annotation = PyDict_GetItem(type_hint_dict, param_name);
        if (param_annotation != NULL && param_annotation != Py_None) {
            if (!PyType_Check(param_annotation)) {
                if (raise_exc)
                    PyErr_Format(PyExc_TypeError, "Second parameter annotation is %s, which is not a type", param_annotation->ob_type->tp_name);
                return false;
            }
            if (!PyObject_IsSubclass(param_annotation, (PyObject *)&PyLong_Type)) {
                if (raise_exc)
                    PyErr_Format(PyExc_TypeError, "Second callback parameter should be type-hinted as int offset. (not %s)", param_annotation->ob_type->tp_name);
                return false;
            }
        }
    }

    return true;
}

static void DynamicParameter_dealloc(DynamicParameterObject *self) {

    if (self->callback != NULL && self->callback != Py_None) {
        Py_XDECREF(self->callback);
        self->callback = NULL;
    }

    /* We defer deallocation to our Parameter baseclass,
        as it must also handle deallocation of param_t's that have been "list forget"en anyway. */
    param_list_remove_specific(((ParameterObject*)self)->param, 0, 0);

    PyTypeObject *baseclass = pycsh_get_base_dealloc_class(&DynamicParameterType);
    baseclass->tp_dealloc((PyObject*)self);
}

static int Parameter_set_callback(DynamicParameterObject *self, PyObject *value, void *closure) {

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
                but prevent Py_DECREF() on it at all cost. */
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

/* Internal API for creating a new DynamicParameterObject. */
__attribute__((malloc(DynamicParameter_dealloc, 1)))
DynamicParameterObject * Parameter_create_new(PyTypeObject *type, uint16_t id, uint16_t node, param_type_e param_type, uint32_t mask, char * name, char * unit, char * docstr, int array_size, const PyObject * callback, int host, int timeout, int retries, int paramver) {

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
        case PARAM_TYPE_INVALID:  // TODO Kevin: Is PARAM_TYPE_INVALID a valid type?
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

    param_t * new_param = param_list_create_remote(id, node, param_type, mask, array_size, name, unit, docstr, -1);
    if (new_param == NULL) {
        return (DynamicParameterObject*)PyErr_NoMemory();
    }

    DynamicParameterObject * self = (DynamicParameterObject *)_pycsh_Parameter_from_param(type, new_param, callback, host, timeout, retries, paramver);
    if (self == NULL) {
        /* This is likely a memory allocation error, in which case we expect .tp_alloc() to have raised an exception. */
        return NULL;
    }

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

static PyObject * DynamicParameter_new(PyTypeObject *type, PyObject * args, PyObject * kwds) {

    uint16_t id;
    char * name;
    param_type_e param_type;
    PyObject * mask_obj;
    char * unit = "";
    char * docstr = "";
    int array_size = 0;
    PyObject * callback = NULL;
    uint16_t node = 0;
    int host = INT_MIN;
    // TODO Kevin: What are these 2 doing here?
    int timeout = pycsh_dfl_timeout;
    int retries = 0;
    int paramver = 2;

    static char *kwlist[] = {"id", "name", "type", "mask", "unit", "docstr", "array_size", "callback", "node", "host", "timeout", "retries", "paramver", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "HsiO|zziOHiiii", kwlist, &id, &name, &param_type, &mask_obj, &unit, &docstr, &array_size, &callback, &node, &host, &timeout, &retries, &paramver))
        return NULL;  // TypeError is thrown

    uint32_t mask;
    if (pycsh_parse_param_mask(mask_obj, &mask) != 0) {
        return NULL;  // Exception message set by pycsh_parse_param_mask()
    }

    if (array_size < 1)
        array_size = 1;

    DynamicParameterObject * python_param = Parameter_create_new(type, id, node, param_type, mask, name, unit, docstr, array_size, callback, host, timeout, retries, paramver);
    if (python_param == NULL) {
        // Assume exception message to be set by Parameter_create_new()
        /* physaddr should be freed in dealloc() */
        return NULL;
    }

    /* return should steal the reference created by Parameter_create_new() */
    return (PyObject *)python_param;
}


static int _DynamicParameterObject_list_forget(DynamicParameterObject *self) {
    assert(self);
    param_list_remove_specific(self->parameter_object.param, pycsh_dfl_verbose, 0);
    Py_DECREF(self);  // Parameter list no longer holds a reference to self.
    return 0;
}


/* Pulls all Parameters in the list as a single request. */
static PyObject * DynamicParameter_list_add(DynamicParameterObject *self, PyObject *args, PyObject *kwds) {

	bool override = false;

	static char *kwlist[] = {"override", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|p", kwlist, &override))
		return NULL;  // TypeError is thrown

    param_t * self_param = self->parameter_object.param;
	param_t * existing = param_list_find_id(*self_param->node, self_param->id);

    assert(self_param);
    if (existing == self_param) {
        return Py_NewRef(self);
    }

    if (existing) {
        if (!override) {
            PyErr_Format(PyExc_ValueError, "Parameter with ID %d on node %d already exists in the list (with name %s)", existing->id, existing->node, existing->name);
            return NULL;
        }

        ParameterObject * existing_python = Parameter_wraps_param(existing);

        if (existing_python) {
            assert(PyObject_IsInstance((PyObject*)existing_python, (PyObject*)&DynamicParameterType));
            /* TODO Kevin: If existing is a DynamicParameter, we should handle reference counting when removing it from the list. */
            _DynamicParameterObject_list_forget((DynamicParameterObject*)existing_python);
        } else {
            /* We can't really know if the existing parameter should be destroyed, but doing it incorrectly  */
            param_list_remove_specific(existing, pycsh_dfl_verbose, 1);
        }
    }

    int res = param_list_add(self->parameter_object.param);
    assert(res == 0);
    Py_INCREF(self);  // Parameter list now holds a reference to self.

	Py_RETURN_NONE;
}

static PyObject * DynamicParameter_list_forget(PyObject * self, PyObject * args) {
    if (_DynamicParameterObject_list_forget((DynamicParameterObject*)self)) {
        Py_RETURN_FALSE;
    } else {
        Py_RETURN_TRUE;
    }
}

static PyMethodDef DynamicParameter_methods[] = {
    {"list_add", (PyCFunction)DynamicParameter_list_add, METH_VARARGS | METH_KEYWORDS,
     PyDoc_STR("Adds `self` to the global parameter list")},
	{"list_forget", (PyCFunction)DynamicParameter_list_forget, METH_NOARGS,
     PyDoc_STR("Removes `self` from the global parameter list")},
    {NULL, NULL, 0, NULL}
};

static PyObject * Parameter_get_callback(DynamicParameterObject *self, void *closure) {
    return Py_NewRef(self->callback);
}

static PyGetSetDef DynamicParameter_getsetters[] = {
    //{"keep_alive", (getter)Parameter_get_keep_alive, (setter)Parameter_set_keep_alive,
    // "Whether the Parameter should remain in the parameter list, when all Python references are lost. This makes it possible to recover the Parameter instance through list()", NULL},
    {"callback", (getter)Parameter_get_callback, (setter)Parameter_set_callback,
     "callback of the parameter", NULL},
    {NULL, NULL, NULL, NULL}  /* Sentinel */
};

PyTypeObject DynamicParameterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pycsh.DynamicParameter",
    .tp_doc = "Parameter created in Python.",
    .tp_basicsize = sizeof(DynamicParameterObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = DynamicParameter_new,
    .tp_dealloc = (destructor)DynamicParameter_dealloc,
    .tp_getset = DynamicParameter_getsetters,
    // .tp_str = (reprfunc)Parameter_str,
    // .tp_richcompare = (richcmpfunc)Parameter_richcompare,
    .tp_base = &ParameterType,
    .tp_methods = DynamicParameter_methods,
};
