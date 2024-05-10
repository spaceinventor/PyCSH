/*
 * pythonparameter.c
 *
 * Contains the PythonParameter Parameter subclass.
 *
 */

#include "python_slash_command.h"

#include "structmember.h"

#include <slash/slash.h>
#include <slash/completer.h>

#include "../pycsh.h"
#include "../utils.h"


int SlashCommand_func(struct slash *slash);

/**
 * @brief Check if this slash command is wrapped by a PythonSlashCommandObject.
 * 
 * @return borrowed reference to the wrapping PythonSlashCommandObject if wrapped, otherwise NULL.
 */
static PythonSlashCommandObject *python_wraps_slash_command(const struct slash_command * command) {
    if (command->func != SlashCommand_func)
        return NULL;  // This slash command is not wrapped by PythonSlashCommandObject
    return (PythonSlashCommandObject *)((char *)command - offsetof(PythonSlashCommandObject, command_heap));
}

/**
 * @brief Shared callback for all slash_commands wrapped by a Slash object instance.
 */
int SlashCommand_func(struct slash *slash) {

    char *args;

    /* We need to find our way back to the storage location is this called slash command */
    extern struct slash_command * slash_command_find(struct slash *slash, char *line, size_t linelen, char **args);
    struct slash_command *command = slash_command_find(slash, slash->buffer, strlen(slash->buffer), &args);
    assert(command != NULL);  // If slash was able to execute this function, then the command should very much exist
    /* If we find a command that doesn't use this function, then there's likely duplicate of multiple applicable commands in the list.
        We could probably manually iterate until we find what is hopefully our command, but that also has issues.*/

    PyGILState_STATE gstate = PyGILState_Ensure();  // TODO Kevin: Do we ever need to take the GIL here, we don't have a CSP thread that can run slash commands
    //assert(Parameter_wraps_param(param));  // TODO Kevin: It shouldn't be neccesarry to assert wrapped here, only PythonSlashCommands should use this function.
    assert(!PyErr_Occurred());  // Callback may raise an exception. But we don't want to override an existing one.

    PythonSlashCommandObject *python_slash_command = python_wraps_slash_command(command);
    assert(python_slash_command != NULL);  // Slash command must be wrapped by Python
    // PyObject *python_callback = python_slash_command->py_slash_func;
    PyObject *python_func = python_slash_command->py_slash_func;

    /* It probably doesn't make sense to have a slash command without a corresponding function,
        but we will allow it for now. */
    if (python_func == NULL || python_func == Py_None) {
        PyGILState_Release(gstate);
        return SLASH_ENOENT;
    }

    assert(PyCallable_Check(python_func));
    /* Create the arguments. */
    PyObject *py_args = PyTuple_New(slash->argc-1);
	for (int i = 0; i < slash->argc-1; i++) {
		PyObject *value = PyUnicode_FromString(slash->argv[i+1]);
		if (!value) {
			Py_DECREF(py_args);
			return SLASH_EINVAL;
		}
		/* pValue reference stolen here: */
		PyTuple_SetItem(py_args, i, value);
	}
    /* Call the user provided Python function */
    PyObject * value = PyObject_CallObject(python_func, py_args);

    /* Cleanup */
    Py_XDECREF(value);
    Py_DECREF(py_args);

    if (PyErr_Occurred()) {
        PyErr_Print();
        PyGILState_Release(gstate);
        return SLASH_EINVAL;
    }

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

    // Cleanup
    Py_DECREF(func_code);

    return true;
}

int PythonSlashCommand_set_func(PythonSlashCommandObject *self, PyObject *value, void *closure) {

    if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the function attribute");
        return -1;
    }

    if (!is_valid_slash_func(value, true)) {
        return -1;
    }

    if (value == self->py_slash_func)
        return 0;  // No work to do

    /* Changing the callback to None. */
    if (value == Py_None) {
        if (self->py_slash_func != Py_None) {
            /* We should not arrive here when the old value is Py_None, 
                but prevent Py_DECREF() on at all cost. */
            Py_XDECREF(self->py_slash_func);
        }
        self->py_slash_func = Py_None;
        return 0;
    }

    /* We now know that 'value' is a new callable. */

    /* When replacing a previous callable. */
    if (self->py_slash_func != Py_None) {
        Py_XDECREF(self->py_slash_func);
    }

    Py_INCREF(value);
    self->py_slash_func = value;

    return 0;
}

static PyObject * PythonSlashCommand_get_keep_alive(PythonSlashCommandObject *self, void *closure) {
    return self->keep_alive ? Py_True : Py_False;
}

static int PythonSlashCommand_set_keep_alive(PythonSlashCommandObject *self, PyObject *value, void *closure) {

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

static char *safe_strdup(const char *s) {
    if (s == NULL) {
        return NULL;
    }
    return strdup(s);
}

/* Internal API for creating a new PythonSlashCommandObject. */
static PythonSlashCommandObject * SlashCommand_create_new(PyTypeObject *type, char * name, const char * args, const PyObject * py_slash_func) {

/* NOTE: Overriding an existing PythonSlashCommand here will most likely cause a memory leak. SLIST_REMOVE() will not Py_DECREF() */
#if 0  /* It's okay if a command with this name already exists, Overriding it is an intended feature. */
    if (slash_list_find_name(0, name)) {
        PyErr_Format(PyExc_ValueError, "Parameter with name \"%s\" already exists", name);
        return NULL;
    }
#endif

    if (!is_valid_slash_func(py_slash_func, true)) {
        return NULL;  // Exception message set by is_valid_slash_func();
    }

    // TODO Kevin: We almost certainly have to strcpy() 'name' and 'args'
    PythonSlashCommandObject *self = (PythonSlashCommandObject *)type->tp_alloc(type, 0);
    if (self == NULL) {
        /* This is likely a memory allocation error, in which case we expect .tp_alloc() to have raised an exception. */
        return NULL;
    }

    self->keep_alive = 1;
    Py_INCREF(self);  // Slash command list now holds a reference to this PythonSlashCommandObject
    /* NOTE: If .keep_alive defaults to False, then we should remove this Py_INCREF() */

    /* NULL callback becomes None on a SlashCommandObject instance */
    if (py_slash_func == NULL)
        py_slash_func = Py_None;

    if (PythonSlashCommand_set_func(self, (PyObject *)py_slash_func, NULL) == -1) {
        Py_DECREF(self);
        return NULL;
    }

    const struct slash_command temp_command = {
        .args = safe_strdup(args),
        .name = safe_strdup(name),
        .func = SlashCommand_func,
        .completer = slash_path_completer,  // TODO Kevin: It should probably be possible for the user to change the completer.
    };

    /* NOTE: This memcpy() seems to be the best way to deal with the fact that self->command->func is const?  */
    memcpy(&self->command_heap, &temp_command, sizeof(struct slash_command));
    self->slash_command_object.command = &self->command_heap;

    struct slash_command * existing = slash_list_find_name(self->command_heap.name);

    int res = slash_list_add(&self->command_heap);
    if (res < 0) {
        fprintf(stderr, "Failed to add slash command \"%s\" while loading APM (return status: %d)\n", self->command_heap.name, res);
        Py_DECREF(self);
        return NULL;
    } else if (res > 0) {
        printf("Slash command '%s' is overriding an existing command\n", self->command_heap.name);
        PythonSlashCommandObject *py_slash_command = python_wraps_slash_command(existing);
        if (py_slash_command != NULL) {
            PythonSlashCommand_set_keep_alive(py_slash_command, Py_False, NULL);
        }
    }

    return self;
}

__attribute__((destructor(151))) static void remove_python_slash_commands(void) {
    struct slash_command * cmd;
    slash_list_iterator i = {};
    while ((cmd = slash_list_iterate(&i)) != NULL) {
        PythonSlashCommandObject *py_slash_command = python_wraps_slash_command(cmd);
        if (py_slash_command != NULL) {
            PythonSlashCommand_set_keep_alive(py_slash_command, Py_False, NULL);
        }
    }
}

static void PythonSlashCommand_dealloc(PythonSlashCommandObject *self) {

    if (self->py_slash_func != NULL && self->py_slash_func != Py_None) {
        Py_XDECREF(self->py_slash_func);
        self->py_slash_func = NULL;
    }

    struct slash_command * existing = slash_list_find_name(self->command_heap.name);
    //PythonSlashCommandObject *py_slash_command = python_wraps_slash_command(existing);

    /* This check will fail if a new slash command is added to the list, before we are garbage collected.
        This is actually quite likely to happen if the same Python 'APM' is loaded multiple times. */
    if (existing == &self->command_heap) {
        slash_list_remove(((SlashCommandObject*)self)->command);
    }

    free(self->command_heap.name);
    free((char*)self->command_heap.args);
    //self->command_heap.name = NULL;
    //((char*)self->command_heap.args) = NULL;
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

static PyObject * PythonSlashCommand_get_function(PythonSlashCommandObject *self, void *closure) {
    return Py_NewRef(self->py_slash_func);
}

static PyObject * PythonSlashCommand_call(PythonSlashCommandObject *self, PyObject *args, PyObject *kwds) {
    assert(self->py_slash_func);
    assert(PyCallable_Check(self->py_slash_func));
    /* Call the user provided Python function. Return whatever it returns, and let errors propagate normally. */
    return PyObject_Call(self->py_slash_func, args, kwds);
}

static PyGetSetDef PythonParameter_getsetters[] = {
    {"keep_alive", (getter)PythonSlashCommand_get_keep_alive, (setter)PythonSlashCommand_set_keep_alive,
     "Whether the slash command should remain in the command list, when all Python references are lost", NULL},
    {"function", (getter)PythonSlashCommand_get_function, (setter)PythonSlashCommand_set_func,
     "function invoked by the slash command", NULL},
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
    .tp_call = (PyCFunctionWithKeywords)PythonSlashCommand_call,
};
