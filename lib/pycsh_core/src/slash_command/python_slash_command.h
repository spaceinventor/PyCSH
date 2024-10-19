/*
 * pythonparameter.h
 *
 * Contains the PythonParameter Parameter subclass.
 *
 */

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <slash/slash.h>

#include "slash_command.h"


extern PyThreadState* main_thread_state;

typedef struct {
    SlashCommandObject slash_command_object;
    struct slash_command command_heap;  // The implementation of slash allows us control where to store our slash_command
    PyObject *py_slash_func;
    // TODO Kevin: We should also expose completer to be implemented by the user
	int keep_alive: 1;  // For the sake of reference counting, keep_alive should only be changed by SlashCommand_set_keep_alive()
} PythonSlashCommandObject;

extern PyTypeObject PythonSlashCommandType;

PythonSlashCommandObject *python_wraps_slash_command(const struct slash_command * command);
