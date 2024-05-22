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
PythonSlashCommandObject *python_wraps_slash_command(const struct slash_command * command) {
    if (command->func != SlashCommand_func)
        return NULL;  // This slash command is not wrapped by PythonSlashCommandObject
    return (PythonSlashCommandObject *)((char *)command - offsetof(PythonSlashCommandObject, command_heap));
}

/**
 * @brief Parse positional slash arguments to **args_out and named arguments to **kwargs_out
 * 
 * @return int 0 when arguments are successfully parsed
 */
int pycsh_parse_slash_args(const struct slash *slash, PyObject **args_out, PyObject **kwargs_out) {

    // Create a tuple and dit to hold *args and **kwargs.
    // TODO Kevin: Support args_out and kwargs_out already being partially filled i.e not NULL
    PyObject* args_tuple = PyTuple_New(slash->argc < 0 ? 0 : slash->argc);
    PyObject* kwargs_dict = PyDict_New();

    if (args_tuple == NULL || kwargs_dict == NULL) {
        // Handle memory allocation failure
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate memory for args list or kwargs dictionary");
        Py_XDECREF(args_tuple);
        Py_XDECREF(kwargs_dict);
        return -1;
    }

    int parsed_positional_args = 0;

    // Tokenize the input arguments
    for (int i = 1; i < slash->argc; ++i) {
        const char* arg = slash->argv[i];
        // Check if the argument is of the form "--key=value"
        if (strncmp(arg, "--", 2) != 0) {
            /* Token is not a keyword argument, simply add it as a positional argument to *args_out */
            PyObject* py_arg = PyUnicode_FromString(arg);
            if (py_arg == NULL) {
                // Handle memory allocation failure
                PyErr_SetString(PyExc_MemoryError, "Failed to allocate memory for positional argument");
                Py_DECREF(args_tuple);
                Py_DECREF(kwargs_dict);
                return -1;
            }
            PyTuple_SET_ITEM(args_tuple, parsed_positional_args++, py_arg);  // Add the argument to the args_out list
            continue; // Skip processing for keyword arguments
        }

        /* Token is a keyword argument, now try to split it into key and value */
        char* equal_sign = strchr(arg, '=');
        if (equal_sign == NULL) {
            // Invalid format for keyword argument
            PyErr_SetString(PyExc_ValueError, "Invalid format for keyword argument");
            Py_DECREF(args_tuple);
            Py_DECREF(kwargs_dict);
            return -1;
        }

        *equal_sign = '\0'; // Null-terminate the key
        const char* key = arg + 2; // Skip "--"
        const char* value = equal_sign + 1;
        
        // Create Python objects for key and value
        PyObject* py_key = PyUnicode_FromString(key);
        PyObject* py_value = PyUnicode_FromString(value);
        if (py_key == NULL || py_value == NULL) {
            // Handle memory allocation failure
            PyErr_SetString(PyExc_MemoryError, "Failed to allocate memory for key or value");
            Py_XDECREF(py_key);
            Py_XDECREF(py_value);
            Py_DECREF(args_tuple);
            Py_DECREF(kwargs_dict);
            return -1;
        }

        // Add the key-value pair to the kwargs_out dictionary
        if (PyDict_SetItem(kwargs_dict, py_key, py_value) != 0) {
            // Handle dictionary insertion failure
            PyErr_SetString(PyExc_RuntimeError, "Failed to add key-value pair to kwargs dictionary");
            Py_DECREF(args_tuple);
            Py_DECREF(kwargs_dict);
            return -1;
        }

        /* Reccnt should be 2 after we have added to the dict. */
        assert(py_key->ob_refcnt != 1);
        assert(py_value->ob_refcnt != 1);

        Py_XDECREF(py_key);
        Py_XDECREF(py_value);
    }

    // Resize tuple to actual length
    if (_PyTuple_Resize(&args_tuple, parsed_positional_args) < 0) {
        // Handle tuple resizing failure
        PyErr_SetString(PyExc_RuntimeError, "Failed to resize tuple for positional arguments");
        Py_DECREF(args_tuple);
        Py_DECREF(kwargs_dict);
        return -1;
    }

    // Assign the args and kwargs variables
    *args_out = args_tuple;
    *kwargs_out = kwargs_dict;

    return 0;
}

// Source: https://chat.openai.com
// Function to print the signature of a provided Python function
void print_function_signature(PyObject* function) {
    assert(function != NULL);

    if (!PyCallable_Check(function)) {
        PyErr_SetString(PyExc_TypeError, "Argument is not callable");
        return;
    }

    PyObject* inspect_module AUTO_DECREF = PyImport_ImportModule("inspect");
    if (!inspect_module) {
        return;
    }

    PyObject* signature_func AUTO_DECREF = PyObject_GetAttrString(inspect_module, "signature");
    if (!signature_func) {
        return;
    }

    PyObject* signature AUTO_DECREF = PyObject_CallFunctionObjArgs(signature_func, function, NULL);
    if (!signature) {
        return;
    }

    PyObject* str_signature AUTO_DECREF = PyObject_Str(signature);
    if (!str_signature) {
        return;
    }

    PyObject* qualname_attr AUTO_DECREF = PyObject_GetAttrString(function, "__qualname__");
    PyObject* name_attr AUTO_DECREF = PyObject_GetAttrString(function, "__name__");

    const char *qualname = qualname_attr ? PyUnicode_AsUTF8(qualname_attr) : NULL;
    const char *name = name_attr ? PyUnicode_AsUTF8(name_attr) : NULL;
    const char *func_name = qualname ? qualname : (name ? name : "Unknown");

    const char *signature_cstr = PyUnicode_AsUTF8(str_signature);
    if (!signature_cstr) {
        return;
    }

    printf("def %s%s\n", func_name, signature_cstr);
}

#undef CLEANUP


/**
 * @brief Shared callback for all slash_commands wrapped by a Slash object instance.
 */
int SlashCommand_func(struct slash *slash) {

    char *args;

    /* We need to find our way back to the storage location is this called slash command */
    extern struct slash_command * slash_command_find(struct slash *slash, char *line, size_t linelen, char **args);
    struct slash_command *command = slash_command_find(slash, slash->buffer, strlen(slash->buffer), &args);
    assert(command != NULL);  // If slash was able to execute this function, then the command should very much exist
    /* If we find a command that doesn't use this function, then there's likely duplicate or multiple applicable commands in the list.
        We could probably manually iterate until we find what is hopefully our command, but that also has issues.*/

    PyGILState_STATE CLEANUP_GIL gstate = PyGILState_Ensure();  // TODO Kevin: Do we ever need to take the GIL here, we don't have a CSP thread that can run slash commands
    if(PyErr_Occurred()) {  // Callback may raise an exception. But we don't want to override an existing one.
        PyErr_Print();  // Provide context by printing before we self-destruct
        assert(false);
    }

    PythonSlashCommandObject *self = python_wraps_slash_command(command);
    assert(self != NULL);  // Slash command must be wrapped by Python
    // PyObject *python_callback = self->py_slash_func;
    PyObject *python_func = self->py_slash_func;

    /* It probably doesn't make sense to have a slash command without a corresponding function,
        but we will allow it for now. */
    if (python_func == NULL || python_func == Py_None) {
        return SLASH_ENOENT;
    }

    assert(PyCallable_Check(python_func));

    // TODO Kevin: Support printing help with "help <command>", would probably require print_function_signature() to return char*
    // Check if we should just print help.
    for (int i = 1; i < slash->argc; i++) {
        const char *arg = slash->argv[i];
        if (strncmp(arg, "-h", 3) == 0 || strncmp(arg, "--help", 7) == 0) {

            if (self->command_heap.args == NULL) {
                print_function_signature(python_func);
            } else {
                void slash_command_usage(struct slash *slash, struct slash_command *command);
                slash_command_usage(slash, command);
            }

            if (PyErr_Occurred()) {
                PyErr_Print();
                return SLASH_ENOENT;
            }
            return SLASH_SUCCESS;
        }
    }

    /* Create the arguments. */
    PyObject *py_args = NULL;
    PyObject *py_kwargs = NULL;
    if (pycsh_parse_slash_args(slash, &py_args, &py_kwargs) != 0) {
        PyErr_Print();
        return SLASH_EINVAL;
    }
    
    /* Call the user provided Python function */
    PyObject * value = PyObject_Call(python_func, py_args, py_kwargs);

    /* Cleanup */
    Py_XDECREF(value);
    Py_DECREF(py_args);

    if (PyErr_Occurred()) {
        PyErr_Print();
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
    return SLASH_SUCCESS;//?
}

#if 0
PythonSlashCommandObject * find_existing_PythonSlashCommand(const struct slash_command *command) {
	/* TODO Kevin: If it ever becomes possible to assert() the held state of the GIL,
		we would definitely want to do it here. We don't want to use PyGILState_Ensure()
		because the GIL should still be held after returning. */
    assert(command != NULL);

    if (command->func != (slash_func_t)SlashCommand_func) {
        return NULL;
    }

	return (PythonSlashCommandObject *)((char *)command - offsetof(PythonSlashCommandObject, command_heap));
}
#endif

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
        PyErr_Format(PyExc_ValueError, "Command with name \"%s\" already exists", name);
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
    // TODO Kevin: Maybe PythonSlashCommandObject should have getsetters for 'name' and 'args'
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