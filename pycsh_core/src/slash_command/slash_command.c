/*
 * parameter.c
 *
 * Contains the Parameter base class.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

#include "slash_command.h"

#include "structmember.h"

#include <slash/slash.h>

#include "python_slash_command.h"
#include "../pycsh.h"
#include <pycsh/utils.h>


/* 1 for success. Compares the wrapped slash_command (struct) between two SlashCommands, otherwise 0. Assumes self to be a SlashCommandObject. */
static int SlashCommand_equal(PyObject *self, PyObject *other) {
	if (PyObject_TypeCheck(other, &SlashCommandType))
		return ((SlashCommandObject *)self)->command == ((SlashCommandObject *)other)->command;
	return 0;
}

static PyObject * SlashCommand_richcompare(PyObject *self, PyObject *other, int op) {

	PyObject *result = Py_NotImplemented;

	switch (op) {
		// case Py_LT:
		// 	break;
		// case Py_LE:
		// 	break;
		case Py_EQ:
			result = (SlashCommand_equal(self, other)) ? Py_True : Py_False;
			break;
		case Py_NE:
			result = (SlashCommand_equal(self, other)) ? Py_False : Py_True;
			break;
		// case Py_GT:
		// 	break;
		// case Py_GE:
		// 	break;
	}

    Py_XINCREF(result);
    return result;
}

static PyObject * SlashCommand_str(SlashCommandObject *self) {
	return PyUnicode_FromString(self->command->name);
}
#if 0
static void SlashCommand_dealloc(SlashCommandObject *self) {

	// Get the type of 'self' in case the user has subclassed 'SlashCommand'.
	Py_TYPE(self)->tp_free((PyObject *) self);
}
__attribute__((malloc(SlashCommand_dealloc, 1)))
#endif
static PyObject * SlashCommand_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {

	static char *kwlist[] = {"name", NULL};

	char *name;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|", kwlist, &name)) {
		return NULL;  // TypeError is thrown
	}

	struct slash_command * command = slash_list_find_name(name);

	if (command == NULL) {  // Did not find a match.
		PyErr_Format(PyExc_ValueError, "Could not find a slash command called '%s'", name);
		return NULL;
	}

	/* I don't really like that we require knowledge of a subclass here,
		but it's probably worth it if we can return a more accurate existing object. */
	const PythonSlashCommandObject *existing_wrapper = python_wraps_slash_command(command);
	if (existing_wrapper != NULL)
		return Py_NewRef(existing_wrapper);

	SlashCommandObject *self = (SlashCommandObject *)type->tp_alloc(type, 0);
	if (self == NULL)
		return NULL;

	self->command = command;

	return (PyObject*)self;
}

// TODO Kevin: _set_name
static PyObject * SlashCommand_get_name(SlashCommandObject *self, void *closure) {
	return Py_BuildValue("s", self->command->name);
}

static PyObject * SlashCommands_get_args(SlashCommandObject *self, void *closure) {
	return Py_BuildValue("s", self->command->args);
}


static long SlashCommand_hash(SlashCommandObject *self) {
	/* Use pointer command pointer as the hash,
		as that should only collide when multiple objects wrap the same command. */
    return (long)self->command;
}

// Source https://chatgpt.com
static char* _tuple_to_slash_string(const char * command_name, PyObject* args, PyObject* kwargs) {

	// Check if args is a tuple
    if (!PyTuple_Check(args)) {
        PyErr_SetString(PyExc_TypeError, "First argument is not a tuple");
        return NULL;
    }

    // Get the length of the tuple
    Py_ssize_t size = PyTuple_Size(args);
    if (size < 0) {
        // Error occurred while getting tuple size
        return NULL;
    }

    // Create an empty Unicode object
    PyObject* unicode_result AUTO_DECREF = NULL;
	if (command_name == NULL) {
		unicode_result = PyUnicode_New(0, 0);
	} else {
		unicode_result = PyUnicode_FromString(command_name);
	}

    if (unicode_result == NULL) {
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate memory");
        return NULL;
    }

    // Append keyword arguments to the Unicode object
    if (kwargs != NULL && PyDict_Check(kwargs)) {
        PyObject *key, *value;
        Py_ssize_t pos = 0;

        while (PyDict_Next(kwargs, &pos, &key, &value)) {
            // Format each key-value pair as named arguments to shell commands
            PyObject* str_key AUTO_DECREF = PyObject_Str(key);
            PyObject* str_value AUTO_DECREF = PyObject_Str(value);
            if (str_key == NULL || str_value == NULL) {
                PyErr_SetString(PyExc_RuntimeError, "Failed to convert key-value pair to string");
                return NULL;
            }

            // Append to the Unicode object
            PyUnicode_AppendAndDel(&unicode_result, PyUnicode_FromString(" --"));
            PyUnicode_AppendAndDel(&unicode_result, str_key);
            /* Assume that boolean arguments are just flags without values,
                i.e "--override" for ident */
            if (!PyBool_Check(value)) {
                PyUnicode_AppendAndDel(&unicode_result, PyUnicode_FromString("="));
                PyUnicode_AppendAndDel(&unicode_result, str_value);
            }
        }
    }

    // Append positional arguments to the Unicode object
    for (Py_ssize_t i = 0; i < size; ++i) {
        // Get the i-th element of the tuple
        PyObject* item = PyTuple_GetItem(args, i);
        if (item == NULL) {
            // Error occurred while getting tuple item
            return NULL;
        }

        // Convert the element to a string
        PyObject* str_obj = PyObject_Str(item);
        if (str_obj == NULL) {
            // Error occurred while converting to string
            return NULL;
        }

        // Separate each argument with space
        PyUnicode_AppendAndDel(&unicode_result, PyUnicode_FromString(" "));

        // Append the string to the Unicode object
        PyUnicode_AppendAndDel(&unicode_result, str_obj);
    }

    // Convert Unicode object to UTF-8 string
    const char* temp = PyUnicode_AsUTF8(unicode_result);
    if (temp == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to convert Unicode to UTF-8");
		return NULL;
    }

	char *result = malloc(strlen(temp)+1);
	if (result == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	strcpy(result, temp);

    return result;
}



static PyObject * SlashCommand_call(SlashCommandObject *self, PyObject *args, PyObject *kwds) {

	char *line CLEANUP_STR = _tuple_to_slash_string(self->command->name, args, kwds);

	if (line == NULL) {
		//PyErr_SetString(PyExc_ValueError, "Failed to convert arguments to slash string");
		return NULL;
	}

	/* Configuration */
	#define SLASH_ARG_MAX		16	/* Maximum number of arguments */
	#define SLASH_SHOW_MAX		25	/* Maximum number of commands to list */
    #define LINE_SIZE		    512
    #define HISTORY_SIZE		2048

	struct slash slas = {0};
    char hist_buf[HISTORY_SIZE];
    slash_create_static(&slas, line, LINE_SIZE, hist_buf, HISTORY_SIZE);
	
	assert(self->command);  // It is invalid for SlashCommandObject to exist without wrapping a slash command

	/* Implement this function to perform logging for example */
	slash_on_execute_hook(line);

	char *slash_args = line+strnlen(self->command->name, LINE_MAX), *argv[SLASH_ARG_MAX];
	int argc = 0;

	/* Build args */
	extern int slash_build_args(char *args, char **argv, int *argc);
	if (slash_build_args(slash_args, argv, &argc) < 0) {
		slash_printf(&slas, "Mismatched quotes\n");
		// TODO Kevin: Probably raise exception here?
		return Py_BuildValue("i", -EINVAL);
	}

	/* Reset state for slash_getopt */
	slas.optarg = 0;
	slas.optind = 1;
	slas.opterr = 1;
	slas.optopt = '?';
	slas.sp = 1;

	slas.argc = argc;
	slas.argv = argv;
	const int ret = self->command->func(&slas);

	extern void slash_command_usage(struct slash *slash, struct slash_command *command);
	if (ret == SLASH_EUSAGE)
		slash_command_usage(&slas, self->command);

	return Py_BuildValue("i", ret);
}

/* 
The Python binding 'Parameter' class exposes most of its attributes through getters, 
as only its 'value', 'host' and 'node' are mutable, and even those are through setters.
*/
static PyGetSetDef Parameter_getsetters[] = {

	{"name", (getter)SlashCommand_get_name, NULL,
     "Returns the name of the wrapped slash_command.", NULL},
    {"args", (getter)SlashCommands_get_args, NULL,
     "The args string of the slash_command.", NULL},
    {NULL, NULL, NULL, NULL}  /* Sentinel */
};

PyTypeObject SlashCommandType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pycsh.SlashCommand",
    .tp_doc = "Wrapper utility class for slash commands.",
    .tp_basicsize = sizeof(SlashCommandObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = SlashCommand_new,
    // .tp_dealloc = (destructor)SlashCommand_dealloc,
	.tp_getset = Parameter_getsetters,
	// .tp_members = Parameter_members,
	// .tp_methods = Parameter_methods,
	.tp_str = (reprfunc)SlashCommand_str,
	.tp_richcompare = (richcmpfunc)SlashCommand_richcompare,
	.tp_hash = (hashfunc)SlashCommand_hash,
	.tp_call = (PyCFunctionWithKeywords)SlashCommand_call,
};
