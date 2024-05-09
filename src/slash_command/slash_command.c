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

	slash_list_remove(self->command);  // TODO Kevin: I'm not sure if we should remove our parameter from the list :/

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
		PyErr_Format(PyExc_ValueError, "Could not find a slash command called %s", name);
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
};
