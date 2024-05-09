/*
 * pythonparameter.c
 *
 * Contains the PythonParameter Parameter subclass.
 *
 */

#include "python_slash_command.h"

#include "structmember.h"

#include <param/param.h>

#include "../pycsh.h"
#include "../utils.h"

// Instantiated in our PyMODINIT_FUNC
//PyObject * PyExc_SlashFunctionError;
//PyObject * PyExc_InvalidParameterTypeError;

/**
 * @brief Shared callback for all slash_commands wrapped by a Slash object instance.
 */
int SlashCommand_func(struct slash *slash) {
    PyGILState_STATE gstate = PyGILState_Ensure();  // TODO Kevin: Do we ever need to take the GIL here, we don't have a CSP thread that can run slash commands
    //assert(Parameter_wraps_param(param));  // TODO Kevin: It shouldn't be neccesarry to assert wrapped here, only PythonSlashCommands should use this function.
    assert(!PyErr_Occurred());  // Callback may raise an exception. But we don't want to override an existing one.

#if 0
    PyObject *key = PyLong_FromVoidPtr(param);
    PythonSlashCommandObject *python_slash_command = (PythonSlashCommandObject*)PyDict_GetItem((PyObject*)slash_command_dict, key);
    Py_DECREF(key);

    /* This param_t uses the Python Parameter callback, but doesn't actually point to a Parameter.
        Perhaps it was deleted? Or perhaps it was never set correctly. */
    if (python_slash_command == NULL) {
        assert(false);  // TODO Kevin: Is this situation worthy of an assert(), or should we just ignore it?
        PyGILState_Release(gstate);
        return;
    }
#endif

    PythonSlashCommandObject *python_slash_command = (PythonSlashCommandObject *)((char *)slash - offsetof(PythonSlashCommandObject, command_heap));
    // PyObject *python_callback = python_slash_command->slash_func;
    PyObject *slash_func = python_slash_command->slash_func;

    /* This Parameter has no callback */
    /* Python_callback should not be NULL here when Parameter_wraps_param(), but we will allow it for now... */
    if (slash_func == NULL || slash_func == Py_None) {
        PyGILState_Release(gstate);
        return SLASH_ENOENT;
    }

    assert(PyCallable_Check(slash_func));
    /* Create the arguments. */
    PyObject *args = PyTuple_New(slash->argc);
	for (int i = 0; i < slash->argc; i++) {
		PyObject *value = PyUnicode_FromString(slash->argv[i]);
		if (!value) {
			Py_DECREF(args);
			return SLASH_EINVAL;
		}
		/* pValue reference stolen here: */
		PyTuple_SetItem(args, i, value);
	}
    /* Call the user Python callback */
    PyObject_CallObject(slash_func, args);
    /* Cleanup */
    Py_DECREF(args);

#if 0  // It's probably best to just let any potential error propagate normally
    if (PyErr_Occurred()) {
        /* It may not be clear to the user, that the exception came from the callback,
            we therefore chain unto the existing exception, for better clarity. */
        /* _PyErr_FormatFromCause() seems simpler than PyException_SetCause() and PyException_SetContext() */
        // TODO Kevin: It seems exceptions raised in the CSP thread are ignored.
        _PyErr_FormatFromCause(PyExc_ParamCallbackError, "Error calling Python callback");
    }
#endif
    PyGILState_Release(gstate);
    return SLASH_SUCCESS;//?
}

PythonSlashCommandObject * find_existing_PythonSlashCommand(struct slash_command *command) {
	/* TODO Kevin: If it ever becomes possible to assert() the held state of the GIL,
		we would definitely want to do it here. We don't want to use PyGILState_Ensure()
		because the GIL should still be held after returning. */
    assert(command != NULL);

    if (command->func != (slash_func_t)SlashCommand_func) {
        return NULL;
    }

	return (PythonSlashCommandObject *)((char *)command - offsetof(PythonSlashCommandObject, command_heap));
}

// Source: https://chat.openai.com
/**
 * @brief Check that the function accepts exactly one Parameter and one integer,
 *  as specified by "void (*callback)(struct param_s * param, int offset)"
 * 
 * Currently also checks type-hints (if specified).
 * 
 * @param callback function to check
 * @param raise_exc Whether to set exception message when returning false.
 * @return true for success
 */
static bool is_valid_slash_func(const PyObject *function, bool raise_exc) {

    /*We currently allow both NULL and Py_None,
            as those are valid to have on PythonSlashCommandObject */
    if (function == NULL || function == Py_None)
        return true;

    // Get the __code__ attribute of the function, and check that it is a PyCodeObject
    // TODO Kevin: Hopefully it's safe to assume that PyObject_GetAttrString() won't mutate function
    PyCodeObject *func_code = (PyCodeObject*)PyObject_GetAttrString((PyObject*)function, "__code__");
    if (!func_code || !PyCode_Check(func_code)) {
        if (raise_exc)
            PyErr_SetString(PyExc_TypeError, "Provided function must be callable");
        return false;
    }

    // Check that number of parameters is exactly 2
    /* func_code->co_argcount doesn't account for *args,
        but that's okay as it should always be empty as we only supply 2 arguments. */
    if (func_code->co_argcount != 2) {
        if (raise_exc)
            PyErr_SetString(PyExc_TypeError, "Provided function must accept exactly 2 arguments");
        Py_DECREF(func_code);
        return false;
    }

    // Get the __annotations__ attribute of the function
    // TODO Kevin: Hopefully it's safe to assume that PyObject_GetAttrString() won't mutate function
    PyDictObject *func_annotations = (PyDictObject *)PyObject_GetAttrString((PyObject*)function, "__annotations__");
    assert(PyDict_Check(func_annotations));
    if (!func_annotations) {
        Py_DECREF(func_code);
        return true;  // It's okay to not specify type-hints for the function.
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
        PyTypeObject *expected_param_type = &SlashCommandType;
        // param_type will be NULL when not type_hinted
        if (param_type != NULL && !PyObject_IsSubclass((PyObject *)param_type, (PyObject *)expected_param_type)) {
            if (raise_exc)
                PyErr_Format(PyExc_TypeError, "First function parameter should be type-hinted as Parameter (or subclass). (not %s)", param_type->tp_name);
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
                PyErr_Format(PyExc_TypeError, "Second function parameter should be type-hinted as int offset. (not %s)", param_type->tp_name);
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

int PythonSlashCommand_set_func(PythonSlashCommandObject *self, PyObject *value, void *closure) {

    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the callback attribute");
        return -1;
    }

    if (!is_valid_slash_func(value, true)) {
        return -1;
    }

    if (value == self->slash_func)
        return 0;  // No work to do

    /* Changing the callback to None. */
    if (value == Py_None) {
        if (self->slash_func != Py_None) {
            /* We should not arrive here when the old value is Py_None, 
                but prevent Py_DECREF() on at all cost. */
            Py_XDECREF(self->slash_func);
        }
        self->slash_func = Py_None;
        return 0;
    }

    /* We now know that 'value' is a new callable. */

    /* When replacing a previous callable. */
    if (self->slash_func != Py_None) {
        Py_XDECREF(self->slash_func);
    }

    Py_INCREF(value);
    self->slash_func = value;

    return 0;
}

/* Internal API for creating a new PythonSlashCommandObject. */
static PythonSlashCommandObject * SlashCommand_create_new(PyTypeObject *type, char * name, const char * args, const PyObject * slash_func) {

/* NOTE: Overriding an existing PythonSlashCommand here will most likely cause a memory leak. SLIST_REMOVE() will not Py_DECREF() */
#if 0  /* It's okay if a command with this name already exists, Overriding it is an intended feature. */
    if (slash_list_find_name(0, name)) {
        PyErr_Format(PyExc_ValueError, "Parameter with name \"%s\" already exists", name);
        return NULL;
    }
#endif

    if (!is_valid_slash_func(slash_func, true)) {
        return NULL;  // Exception message set by is_valid_slash_func();
    }

    // TODO Kevin: We almost certainly have to strcpy() 'name' and 'args'
    PythonSlashCommandObject *self = (PythonSlashCommandObject *)type->tp_alloc(type, 0);
    if (self == NULL) {
        /* This is likely a memory allocation error, in which case we expect .tp_alloc() to have raised an exception. */
        return NULL;
    }

    self->command_heap.name = name;
    self->command_heap.args = args;

    self->keep_alive = 1;
    Py_INCREF(self);  // Slash command list now holds a reference to this PythonSlashCommandObject
    /* NOTE: If .keep_alive defaults to False, then we should remove this Py_INCREF() */

    /* NULL callback becomes None on a SlashCommandObject instance */
    if (slash_func == NULL)
        slash_func = Py_None;

    if (PythonSlashCommand_set_func(self, (PyObject *)slash_func, NULL) == -1) {
        Py_DECREF(self);
        return NULL;
    }

    /* TODO Kevin: How should we deal with the fact that self->command->func is const? This is just a workaround. */
    slash_func_t *func_ptr = (slash_func_t *)&(((SlashCommandObject*)self)->command->func);
    // Assign the desired function to the const function pointer
    *func_ptr = SlashCommand_func;

    int res = slash_list_add(&(self->command_heap));
    if (res < 0) {
        fprintf(stderr, "Failed to add slash command \"%s\" while loading APM (return status: %d)\n", self->command_heap.name, res);
        Py_DECREF(self);
        return NULL;
    } else if (res > 0) {
        printf("Slash command '%s' is overriding an existing command\n", self->command_heap.name);
    }

    return self;
}

static void PythonSlashCommand_dealloc(PythonSlashCommandObject *self) {
    if (self->slash_func != NULL && self->slash_func != Py_None) {
        Py_XDECREF(self->slash_func);
        self->slash_func = NULL;
    }
    slash_list_remove(((SlashCommandObject*)self)->command);
    // Get the type of 'self' in case the user has subclassed 'Parameter'.
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static PyObject * PythonSlashCommand_new(PyTypeObject *type, PyObject * args, PyObject * kwds) {

    char * name;
    PyObject * function;
    char * slash_args = NULL;

    static char *kwlist[] = {"name", "function", "args", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "sO|s", kwlist, &name, &function, &slash_args))
        return NULL;  // TypeError is thrown

    PythonSlashCommandObject * python_slash_command = SlashCommand_create_new(type, name, slash_args, function);
    if (python_slash_command == NULL) {
        // Assume exception message to be set by SlashCommand_create_new()
        /* physaddr should be freed in dealloc() */
        return NULL;
    }

    /* return should steal the reference created by SlashCommand_create_new() */
    return (PyObject *)python_slash_command;
}

static PyObject * Parameter_get_keep_alive(PythonSlashCommandObject *self, void *closure) {
    return self->keep_alive ? Py_True : Py_False;
}

static int Parameter_set_keep_alive(PythonSlashCommandObject *self, PyObject *value, void *closure) {

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

static PyObject * Parameter_get_callback(PythonSlashCommandObject *self, void *closure) {
    return Py_NewRef(self->slash_func);
}

static PyGetSetDef PythonParameter_getsetters[] = {
    {"keep_alive", (getter)Parameter_get_keep_alive, (setter)Parameter_set_keep_alive,
     "Whether the Parameter should remain in the parameter list, when all Python references are lost. This makes it possible to recover the Parameter instance through list()", NULL},
    {"callback", (getter)Parameter_get_callback, (setter)PythonSlashCommand_set_func,
     "callback of the parameter", NULL},
    {NULL, NULL, NULL, NULL}  /* Sentinel */
};

PyTypeObject PythonSlashCommandType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pycsh.PythonSlashCommand",
    .tp_doc = "Slash command created in Python.",
    .tp_basicsize = sizeof(PythonSlashCommandType),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = PythonSlashCommand_new,
    .tp_dealloc = (destructor)PythonSlashCommand_dealloc,
    .tp_getset = PythonParameter_getsetters,
    // .tp_str = (reprfunc)SlashCommand_str,
    // .tp_richcompare = (richcmpfunc)SlashCommand_richcompare,
    .tp_base = &SlashCommandType,
};
