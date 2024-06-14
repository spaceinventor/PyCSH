/*
 * pythonparameter.c
 *
 * Contains the PythonParameter Parameter subclass.
 *
 */

#include "pythongetsetparameter.h"

#include "structmember.h"

#include <param/param.h>

#include "../pycsh.h"
#include "../utils.h"

#include "pycshconfig.h"


PythonGetSetParameterObject *python_wraps_vmem(const vmem_t * vmem);

static PyObject *_pycsh_val_to_pyobject(param_type_e type, const void * value) {
    switch (type) {
		case PARAM_TYPE_UINT8:
		case PARAM_TYPE_XINT8:
			return Py_BuildValue("B", *(uint8_t*)value);
		case PARAM_TYPE_UINT16:
		case PARAM_TYPE_XINT16:
			return Py_BuildValue("H", *(uint16_t*)value);
		case PARAM_TYPE_UINT32:
		case PARAM_TYPE_XINT32:
			return Py_BuildValue("I", *(uint32_t*)value);
		case PARAM_TYPE_UINT64:
		case PARAM_TYPE_XINT64:
			return Py_BuildValue("K", *(uint64_t*)value);
		case PARAM_TYPE_INT8:
			return Py_BuildValue("b", *(int8_t*)value);
		case PARAM_TYPE_INT16:
			return Py_BuildValue("h", *(int16_t*)value);
		case PARAM_TYPE_INT32:
			return Py_BuildValue("i", *(int32_t*)value);
		case PARAM_TYPE_INT64:
			return Py_BuildValue("k", *(int64_t*)value);
		case PARAM_TYPE_FLOAT:
			return Py_BuildValue("f", *(float*)value);
		case PARAM_TYPE_DOUBLE:
			return Py_BuildValue("d", *(double*)value);
		case PARAM_TYPE_STRING: {
			return Py_BuildValue("s", *(char*)value);
		}
		case PARAM_TYPE_DATA: {
			return Py_BuildValue("O&", *(char*)value);
		}
		default: {
			/* Default case to make the compiler happy. Set error and return */
			break;
		}
	}
	PyErr_SetString(PyExc_NotImplementedError, "Unsupported parameter type for get operation.");
	return NULL;
}

/**
 * @brief Convert an arbritrary value to a parameter value, based on parameter type.
 * 
 * @param type Parameter type, to determine desired output value type from.
 * @param value_in PyObject* value to parse.
 * @param dataout Buffer for retrieved value.
 * @param array_len Needed for array parameters
 * @return int 0 for success.
 */
static int _pycsh_param_pyval_to_cval(param_type_e type, PyObject * value_in, void * dataout, size_t array_len) {

    if (value_in == NULL) {
        return -6;
    }

    /* Error check switch */
	switch (type) {
		case PARAM_TYPE_UINT8:
		case PARAM_TYPE_XINT8:
		case PARAM_TYPE_UINT16:
		case PARAM_TYPE_XINT16:
		case PARAM_TYPE_UINT32:
		case PARAM_TYPE_XINT32:
		case PARAM_TYPE_UINT64:
		case PARAM_TYPE_XINT64:
		case PARAM_TYPE_INT8:
		case PARAM_TYPE_INT16:
		case PARAM_TYPE_INT32:
		case PARAM_TYPE_INT64: {
			if (!PyLong_Check(value_in)) {
                PyErr_Format(PyExc_TypeError, "Cannot set int parameter as %s", value_in->ob_type->tp_name);
                return -1;
            }
			break;
		}
		case PARAM_TYPE_FLOAT:
		case PARAM_TYPE_DOUBLE: {
			if (!PyFloat_Check(value_in)) {
                PyErr_Format(PyExc_TypeError, "Cannot set float parameter as %s", value_in->ob_type->tp_name);
                return -2;
            }
			break;
		}
		case PARAM_TYPE_STRING: {
			if (!PyUnicode_Check(value_in)) {
                PyErr_Format(PyExc_TypeError, "Cannot set string parameter as %s", value_in->ob_type->tp_name);
                return -3;
            }
			break;
		}
		case PARAM_TYPE_DATA:  // TODO Kevin: No idea how data will work
		default:  // Raise NotImplementedError when param_type remains NULL.
			PyErr_SetString(PyExc_NotImplementedError, "Unsupported parameter type.");
            return -4;
			break;
	}

    // TODO Kevin: No overflow checking! But I think this is most similar to libparam

    /* If we have not returned yet, then type should be valid. */
	switch (type) {
		case PARAM_TYPE_UINT8:
		case PARAM_TYPE_XINT8:
			*(uint8_t*)dataout = (uint8_t)PyLong_AsUnsignedLong(value_in);
            break;
		case PARAM_TYPE_UINT16:
		case PARAM_TYPE_XINT16:
			*(uint16_t*)dataout = (uint16_t)PyLong_AsUnsignedLong(value_in);
            break;
		case PARAM_TYPE_UINT32:
		case PARAM_TYPE_XINT32:
			*(uint32_t*)dataout = (uint32_t)PyLong_AsUnsignedLong(value_in);
            break;
		case PARAM_TYPE_UINT64:
		case PARAM_TYPE_XINT64:
			*(uint64_t*)dataout = (uint64_t)PyLong_AsUnsignedLong(value_in);
            break;
		case PARAM_TYPE_INT8:
			*(int8_t*)dataout = (int8_t)PyLong_AsLong(value_in);
            break;
		case PARAM_TYPE_INT16:
			*(int16_t*)dataout = (int16_t)PyLong_AsLong(value_in);
            break;
		case PARAM_TYPE_INT32:
			*(int32_t*)dataout = (int32_t)PyLong_AsLong(value_in);
            break;
		case PARAM_TYPE_INT64:
			*(int64_t*)dataout = (int64_t)PyLong_AsLong(value_in);
		case PARAM_TYPE_FLOAT:
			*(float*)dataout = (float)PyFloat_AsDouble(value_in);
            break;
		case PARAM_TYPE_DOUBLE:
			*(double*)dataout = PyFloat_AsDouble(value_in);
            break;
        case PARAM_TYPE_DATA:
		case PARAM_TYPE_STRING: {
            strncpy((char*)dataout, PyUnicode_AsUTF8(value_in), array_len);
            break;
		}
		default: {
			/* Default case to make the compiler happy. Set error and return */
			break;
		}
	}

    if (PyErr_Occurred()) {
        return -5;  // Probably OverflowError
    }

    return 0;
}

/**
 * @brief Shared getter for all param_t's wrapped by a Parameter instance.
 */
void Parameter_getter(vmem_t * vmem, uint32_t addr, void * dataout, uint32_t len) {

    PyGILState_STATE CLEANUP_GIL gstate = PyGILState_Ensure();
    assert(!PyErr_Occurred());  // Callback may raise an exception. But we don't want to override an existing one.

    PythonGetSetParameterObject *python_param = python_wraps_vmem(vmem);

    /* This param_t uses the Python Parameter callback, but doesn't actually point to a Parameter.
        Perhaps it was deleted? Or perhaps it was never set correctly. */
    assert(python_param != NULL);  // TODO Kevin: Is this situation worthy of an assert(), or should we just ignore it?

    // PythonParameterObject *python_param = (PythonParameterObject *)((char *)param - offsetof(PythonParameterObject, parameter_object.param));
    PyObject *python_getter = python_param->getter_func;

    /* This Parameter has no callback */
    /* Python_callback should not be NULL here when Parameter_wraps_param(), but we will allow it for now... */
    if (python_getter == NULL || python_getter == Py_None) {
        return;
    }

    // Undo VMEM addr conversion to find parameter index.
    const param_t *param = python_param->parameter_object.parameter_object.param;
    const int offset = (addr-(intptr_t)param->addr)/param->array_step;

    assert(PyCallable_Check(python_getter));
    /* Create the arguments. */
    PyObject *pyoffset AUTO_DECREF = Py_BuildValue("i", offset);
    PyObject * args AUTO_DECREF = PyTuple_Pack(2, python_param, pyoffset);
    /* Call the user Python getter */
    PyObject *value AUTO_DECREF = PyObject_CallObject(python_getter, args);

    _pycsh_param_pyval_to_cval(param->type, value, dataout, param->array_size-offset);

#if PYCSH_HAVE_APM  // TODO Kevin: This is pretty ugly, but we can't let the error propagate when building for APM, as there is no one but us to catch it.
    if (PyErr_Occurred()) {
        /* It may not be clear to the user, that the exception came from the callback,
            we therefore chain unto the existing exception, for better clarity. */
        /* _PyErr_FormatFromCause() seems simpler than PyException_SetCause() and PyException_SetContext() */
        // TODO Kevin: It seems exceptions raised in the CSP thread are ignored.
        //_PyErr_FormatFromCause(PyExc_ParamCallbackError, "Error calling Python callback");
        PyErr_Print();
    }
#endif
}

/**
 * @brief Shared setter for all param_t's wrapped by a Parameter instance.
 */
void Parameter_setter(vmem_t * vmem, uint32_t addr, const void * datain, uint32_t len) {

    PyGILState_STATE CLEANUP_GIL gstate = PyGILState_Ensure();
    assert(!PyErr_Occurred());  // Callback may raise an exception. But we don't want to override an existing one.

    PythonGetSetParameterObject *python_param = python_wraps_vmem(vmem);

    /* This param_t uses the Python Parameter callback, but doesn't actually point to a Parameter.
        Perhaps it was deleted? Or perhaps it was never set correctly. */
    assert(python_param != NULL);  // TODO Kevin: Is this situation worthy of an assert(), or should we just ignore it?

    // PythonParameterObject *python_param = (PythonParameterObject *)((char *)param - offsetof(PythonParameterObject, parameter_object.param));
    PyObject *python_setter = python_param->setter_func;

    /* This Parameter has no callback */
    /* Python_callback should not be NULL here when Parameter_wraps_param(), but we will allow it for now... */
    if (python_setter == NULL || python_setter == Py_None) {
        return;
    }

    // Undo VMEM addr conversion to find parameter index.
    const param_t *param = python_param->parameter_object.parameter_object.param;
    const int offset = (addr-(intptr_t)param->addr)/param->array_step;

    assert(PyCallable_Check(python_setter));
    /* Create the arguments. */
    PyObject *pyoffset AUTO_DECREF = Py_BuildValue("i", offset);
    PyObject *pyval AUTO_DECREF = _pycsh_val_to_pyobject(python_param->parameter_object.parameter_object.param->type, datain);
    if (pyval == NULL) {
        return;
    }
    PyObject * args AUTO_DECREF = PyTuple_Pack(3, python_param, pyoffset, pyval);
    /* Call the user Python callback */
    PyObject_CallObject(python_setter, args);

#if 0  // TODO Kevin: Either propagate exception naturally, or set FromCause to custom getter exception.
    if (PyErr_Occurred()) {
        /* It may not be clear to the user, that the exception came from the callback,
            we therefore chain unto the existing exception, for better clarity. */
        /* _PyErr_FormatFromCause() seems simpler than PyException_SetCause() and PyException_SetContext() */
        // TODO Kevin: It seems exceptions raised in the CSP thread are ignored.
        _PyErr_FormatFromCause(PyExc_ParamCallbackError, "Error calling Python callback");
    }
#endif
}

/**
 * @brief Check if this vmem is wrapped by a PythonGetSetParameterObject.
 * 
 * @return borrowed reference to the wrapping PythonSlashCommandObject if wrapped, otherwise NULL.
 */
PythonGetSetParameterObject *python_wraps_vmem(const vmem_t * vmem) {
    if (vmem == NULL || (vmem->read != Parameter_getter && vmem->write != Parameter_setter))
        return NULL;  // This slash command is not wrapped by PythonSlashCommandObject
    // TODO Kevin: What are the consequences of allowing only getter and or setter?
    // assert(vmem->write == Parameter_setter);  // It should not be possible to have the correct internal .read(), but the incorrect internal .write()
    return (PythonGetSetParameterObject *)((char *)vmem - offsetof(PythonGetSetParameterObject, vmem_heap));
}

// Source: https://chat.openai.com
/**
 * @brief Check that the getter accepts exactly one Parameter and one (index) integer,
 *  as specified by "void (*callback)(struct param_s * param, int offset)"
 * 
 * Currently also checks type-hints (if specified).
 * 
 * @param callback function to check
 * @param raise_exc Whether to set exception message when returning false.
 * @return true for success
 */
static bool is_valid_setter(const PyObject *setter, bool raise_exc) {

    /*We currently allow both NULL and Py_None,
            as those are valid to have on PythonParameterObject */
    if (setter == NULL)
        return true;

    // Get the __code__ attribute of the function, and check that it is a PyCodeObject
    // TODO Kevin: Hopefully it's safe to assume that PyObject_GetAttrString() won't mutate callback
    PyCodeObject *func_code = (PyCodeObject*)PyObject_GetAttrString((PyObject*)setter, "__code__");
    if (!func_code || !PyCode_Check(func_code)) {
        if (raise_exc)
            PyErr_SetString(PyExc_TypeError, "Provided callback must be callable");
        return false;
    }

    // Check that number of parameters is exactly 2
    /* func_code->co_argcount doesn't account for *args,
        but that's okay as it should always be empty as we only supply 2 arguments. */
    if (func_code->co_argcount != 3) {
        if (raise_exc)
            PyErr_SetString(PyExc_TypeError, "Provided setter must accept exactly 3 arguments");
        Py_DECREF(func_code);
        return false;
    }

    // Get the __annotations__ attribute of the function
    // TODO Kevin: Hopefully it's safe to assume that PyObject_GetAttrString() won't mutate callback
    PyDictObject *func_annotations = (PyDictObject *)PyObject_GetAttrString((PyObject*)setter, "__annotations__");
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

static void PythonGetSetParameter_dealloc(PythonGetSetParameterObject *self) {

    if (self->getter_func != NULL && self->getter_func != Py_None) {
        Py_XDECREF(self->getter_func);
        self->getter_func = NULL;
    }

    if (self->setter_func != NULL && self->setter_func != Py_None) {
        Py_XDECREF(self->setter_func);
        self->setter_func = NULL;
    }


    // Get the type of 'self' in case the user has subclassed 'Parameter'.
    // TODO Kevin: Alternatively it might be better to always iterate from pycsh.PythonParameter.
    PyTypeObject *baseclass = Py_TYPE(self);

    // Keep iterating baseclasses until we find one that doesn't use this deallocator.
    while ((baseclass = baseclass->tp_base)->tp_dealloc == (destructor)PythonGetSetParameter_dealloc && baseclass != NULL);

    assert(baseclass->tp_dealloc != NULL);  // Assert that Python installs some deallocator to classes that don't specifically implement one (Whether pycsh.Parameter or object()).
    baseclass->tp_dealloc((PyObject*)self);
}

static PyObject * Parameter_get_getter(PythonGetSetParameterObject *self, void *closure) {
    return Py_NewRef(self->getter_func);
}

int Parameter_set_getter(PythonGetSetParameterObject *self, PyObject *value, void *closure) {

    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the getter attribute");
        return -1;
    }

    if (!is_valid_callback(value, true)) {
        return -1;
    }

    if (value == self->getter_func)
        return 0;  // No work to do

    /* Changing the getter to None. */
    if (value == Py_None) {
        if (self->setter_func == NULL || self->setter_func == Py_None) {
            PyErr_SetString(PyExc_TypeError, "setter and getter may not be None at the same time (for technical reasons)");
            return -1;
        }

        if (self->getter_func != Py_None) {
            /* We should not arrive here when the old value is Py_None, 
                but prevent Py_DECREF() on at all cost. */
            Py_XDECREF(self->getter_func);
        }
        self->getter_func = Py_None;
        return 0;
    }

    /* We now know that 'value' is a new callable. */

    /* When replacing a previous callable. */
    if (self->getter_func != Py_None) {
        Py_XDECREF(self->getter_func);
    }

    Py_INCREF(value);
    self->getter_func = value;
    self->vmem_heap.read = Parameter_getter;

    return 0;
}

static PyObject * Parameter_get_setter(PythonGetSetParameterObject *self, void *closure) {
    return Py_NewRef(self->setter_func);
}

int Parameter_set_setter(PythonGetSetParameterObject *self, PyObject *value, void *closure) {

    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the setter attribute");
        return -1;
    }

    if (!is_valid_setter(value, true)) {
        return -1;
    }

    if (value == self->setter_func)
        return 0;  // No work to do

    /* Changing the setter to None. */
    if (value == Py_None) {
        if (self->getter_func == NULL || self->getter_func == Py_None) {
            PyErr_SetString(PyExc_TypeError, "setter and getter may not be None at the same time (for technical reasons)");
            return -1;
        }

        if (self->setter_func != Py_None) {
            /* We should not arrive here when the old value is Py_None, 
                but prevent Py_DECREF() on at all cost. */
            Py_XDECREF(self->setter_func);
        }
        self->setter_func = Py_None;
        self->vmem_heap.write = NULL;
        return 0;
    }

    /* We now know that 'value' is a new callable. */

    /* When replacing a previous callable. */
    if (self->setter_func != Py_None) {
        Py_XDECREF(self->setter_func);
    }

    Py_INCREF(value);
    self->setter_func = value;
    self->vmem_heap.write = Parameter_setter;

    return 0;
}

__attribute__((malloc, malloc(PythonGetSetParameter_dealloc, 1)))
static PyObject * PythonGetSetParameter_new(PyTypeObject *type, PyObject * args, PyObject * kwds) {

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
    PyObject *getter_func = NULL;
    PyObject *setter_func = NULL;

    static char *kwlist[] = {"id", "name", "type", "mask", "unit", "docstr", "array_size", "callback", "host", "timeout", "retries", "paramver", "getter", "setter", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "HsiO|ssiOiiiiOO", kwlist, &id, &name, &param_type, &mask_obj, &unit, &docstr, &array_size, &callback, &host, &timeout, &retries, &paramver, &getter_func, &setter_func))
        return NULL;  // TypeError is thrown

    if (getter_func == NULL && setter_func == NULL) {
        PyErr_SetString(PyExc_TypeError, "PythonGetSetParameter must have at least a getter or setter (for technical reasons)");
        return NULL;
    }

    // .getter and .callback currently share the same signature.
    if (getter_func != NULL && !is_valid_callback(getter_func, true)) {
        return NULL;
    }

    if (setter_func != NULL && !is_valid_setter(setter_func, true)) {
        return NULL;
    }

    uint32_t mask;
    if (pycsh_parse_param_mask(mask_obj, &mask) != 0) {
        return NULL;  // Exception message set by pycsh_parse_param_mask()
    }

    if (array_size < 1)
        array_size = 1;

    // TODO Kevin: Call super with correct *args and **kwargs, instead of reimplementing PythonParameter.__new__()
    PythonGetSetParameterObject * self = (PythonGetSetParameterObject*)Parameter_create_new(type, id, param_type, mask, name, unit, docstr, array_size, callback, host, timeout, retries, paramver);
    if (self == NULL) {
        // Assume exception message to be set by Parameter_create_new()
        /* physaddr should be freed in dealloc() */
        return NULL;
    }

    {   /* Initialize the remaining fields, as would've been done by param_list_create_remote() */
        self->vmem_heap.ack_with_pull = false;
        self->vmem_heap.driver = NULL;
        self->vmem_heap.name = "GETSET";
        self->vmem_heap.size = array_size*param_typesize(self->parameter_object.parameter_object.param->type);
        self->vmem_heap.type = -1;  // TODO Kevin: Maybe expose vmem_types, instead of just setting unspecified.
        self->vmem_heap.vaddr = NULL;
        self->vmem_heap.backup = NULL;
        self->vmem_heap.big_endian = false;
        self->vmem_heap.restore = NULL;
        
        if (getter_func != NULL && getter_func != Py_None) {
            self->getter_func = Py_NewRef(getter_func);
            self->vmem_heap.read = Parameter_getter;
        }
        if (setter_func != NULL && setter_func != Py_None) {
            self->setter_func = Py_NewRef(setter_func);
            self->vmem_heap.write = Parameter_setter;
        }
        assert(self->vmem_heap.read != NULL || self->vmem_heap.write != NULL);
    }
    // Point our parameter to use our newly initialized get/set VMEM
    self->parameter_object.parameter_object.param->vmem = &self->vmem_heap;

    /* return should steal the reference created by Parameter_create_new() */
    return (PyObject *)self;
}


static PyGetSetDef PythonParameter_getsetters[] = {
    {"getter", (getter)Parameter_get_getter, (setter)Parameter_set_getter,
     "getter of the parameter", NULL},
    {"setter", (getter)Parameter_get_setter, (setter)Parameter_set_setter,
     "setter of the parameter", NULL},
    {NULL, NULL, NULL, NULL}  /* Sentinel */
};

PyTypeObject PythonGetSetParameterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pycsh.PythonGetSetParameter",
    .tp_doc = "Parameter, with getter and or setter, created in Python.",
    .tp_basicsize = sizeof(PythonGetSetParameterObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = PythonGetSetParameter_new,
    .tp_dealloc = (destructor)PythonGetSetParameter_dealloc,
    .tp_getset = PythonParameter_getsetters,
    // .tp_str = (reprfunc)Parameter_str,
    // .tp_richcompare = (richcmpfunc)Parameter_richcompare,
    .tp_base = &PythonParameterType,
};
