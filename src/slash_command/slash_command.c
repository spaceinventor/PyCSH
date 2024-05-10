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

#include "../pycsh.h"
#include "../utils.h"

/* Maps param_t to its corresponding PythonParameter for use by C callbacks. */
PyDictObject * slash_command_dict = NULL;

/* 1 for success. Compares the wrapped param_t for parameters, otherwise 0. Assumes self to be a SlashCommandObject. */
static int SlashCommand_equal(PyObject *self, PyObject *other) {
	if (PyObject_TypeCheck(other, &SlashCommandType))
		return ((SlashCommandObject *)self)->command == ((SlashCommandObject *)other)->command;
	return 0;
#if 0
	if (PyObject_TypeCheck(other, &SlashCommandType) && (memcmp(&(((SlashCommandObject *)other)->command), &(((SlashCommandObject *)self)->command), sizeof(struct slash_command)) == 0))
		return 1;
	return 0;
#endif
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

static void SlashCommand_dealloc(SlashCommandObject *self) {
#if 0
	/* Whether or not we keep self->param in the list, it should not point to a freed 'self' */
	PyObject *key = PyLong_FromVoidPtr(self->param);
	if (PyDict_GetItem((PyObject*)slash_command_dict, key) != NULL)
		PyDict_DelItem((PyObject*)slash_command_dict, key);
	Py_DECREF(key);
#endif

	// C slash commands should not be removed from the list
	//slash_list_remove(self->command);

	// Get the type of 'self' in case the user has subclassed 'SlashCommand'.
	Py_TYPE(self)->tp_free((PyObject *) self);
}

__attribute__((malloc, malloc(SlashCommand_dealloc, 1)))
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

	SlashCommandObject *self = (SlashCommandObject *)type->tp_alloc(type, 0);
	if (self == NULL)
		return NULL;

	self->command = command;

	return (PyObject*)self;
#if 0
    return _pycsh_SlashCommand_from_slash_command(type, param, NULL, host, timeout, retries, paramver);
#endif
}

// TODO Kevin: _set_name
static PyObject * SlashCommand_get_name(SlashCommandObject *self, void *closure) {
	return Py_BuildValue("s", self->command->name);
}

static PyObject * SlashCommands_get_args(SlashCommandObject *self, void *closure) {
	return Py_BuildValue("s", self->command->args);
}


static long SlashCommand_hash(SlashCommandObject *self) {
	/* Use the ID of the parameter as the hash, as it is assumed unique. */
    return (long)self->command;
}

#if 0
static void py_print__str__(PyObject *obj) {
	// Get the string representation of the object
    PyObject* str_repr = PyObject_Str(obj);
    if (str_repr == NULL) {
		fprintf(stderr, "Failed to get string representation\n");
		return;
	}

	// Convert the string representation to a C string
	const char* c_str_repr = PyUnicode_AsUTF8(str_repr);
	if (c_str_repr != NULL) {
		// Print the string representation
		printf("String representation: %s\n", c_str_repr);
	} else {
		fprintf(stderr, "Failed to convert string representation to C string\n");
	}

	// Decrement the reference count of the string representation
	Py_DECREF(str_repr);
}
#endif

// Source https://chatgpt.com
static char* _tuple_to_slash_string(const char * command_name, PyObject* args) {
    // Check if args is a tuple
    if (!PyTuple_Check(args)) {
        PyErr_SetString(PyExc_TypeError, "Argument is not a tuple");
        return NULL;
    }

    // Get the length of the tuple
    Py_ssize_t size = PyTuple_Size(args);
    if (size < 0) {
        // Error occurred while getting tuple size
        return NULL;
    }

    // Create an empty Unicode object
    PyObject* unicode_result;
	if (command_name == NULL) {
		unicode_result = PyUnicode_New(0, 0);
	} else {
		unicode_result = PyUnicode_FromString(command_name);
	}

    if (unicode_result == NULL) {
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate memory");
        return NULL;
    }

    // Iterate over the tuple elements
    for (Py_ssize_t i = 0; i < size; ++i) {
        // Get the i-th element of the tuple
        PyObject* item = PyTuple_GetItem(args, i);
        if (item == NULL) {
            // Error occurred while getting tuple item
            Py_DECREF(unicode_result);
            return NULL;
        }

        // Convert the element to a string
        PyObject* str_obj = PyObject_Str(item);
        if (str_obj == NULL) {
            // Error occurred while converting to string
            Py_DECREF(unicode_result);
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
    }

	char *result = malloc(strlen(temp)+1);
	if (result == NULL) {
		PyErr_NoMemory();
		return NULL;
	}
	strcpy(result, temp);

    // Decrement the reference count of the Unicode object
    Py_DECREF(unicode_result);

    return result;
}

static PyObject * SlashCommand_call(SlashCommandObject *self, PyObject *args, PyObject *kwds) {

	if (kwds == NULL || (kwds != NULL && PyDict_Size(kwds))) {

		// Get the string representation of the object
		PyObject* str_repr = PyObject_Str(kwds);
		if (str_repr == NULL) {
			fprintf(stderr, "Failed to get string representation\n");
			PyErr_SetString(PyExc_TypeError, "Cannot call slash command with keyword arguments");
			return NULL;
		}

		// Convert the string representation to a C string
		const char* c_str_repr = PyUnicode_AsUTF8(str_repr);
		if (c_str_repr == NULL) {
			fprintf(stderr, "Failed to convert string representation to C string\n");
		}

		PyErr_Format(PyExc_TypeError, "Cannot call slash command with keyword arguments: %s", c_str_repr);

		// Decrement the reference count of the string representation
		Py_DECREF(str_repr);
	}

	char *line = _tuple_to_slash_string(self->command->name, args);

	if (line == NULL) {
		//PyErr_SetString(PyExc_ValueError, "Failed to convert arguments to slash string");
		return NULL;
	}

	struct slash slas = {0};
    #define LINE_SIZE		    512
    char line_buf[LINE_SIZE];
    #define HISTORY_SIZE		2048
    char hist_buf[HISTORY_SIZE];
    slash_create_static(&slas, line_buf, LINE_SIZE, hist_buf, HISTORY_SIZE);
	/* We could consider just running self->command_heap->func() ourselves,
		but slash slash_execute() is a lot more convenient. */
	PyObject *ret = Py_BuildValue("i", slash_execute(&slas, line));
	free(line);
	return ret;
}

/* 
The Python binding 'Parameter' class exposes most of its attributes through getters, 
as only its 'value', 'host' and 'node' are mutable, and even those are through setters.
*/
static PyGetSetDef Parameter_getsetters[] = {

	{"name", (getter)SlashCommand_get_name, NULL,
     "Returns the name of the wrapped slash_command.", NULL},
    {"unit", (getter)SlashCommands_get_args, NULL,
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
    .tp_dealloc = (destructor)SlashCommand_dealloc,
	.tp_getset = Parameter_getsetters,
	// .tp_members = Parameter_members,
	// .tp_methods = Parameter_methods,
	.tp_str = (reprfunc)SlashCommand_str,
	.tp_richcompare = (richcmpfunc)SlashCommand_richcompare,
	.tp_hash = (hashfunc)SlashCommand_hash,
	.tp_call = (PyCFunctionWithKeywords)SlashCommand_call,
};
