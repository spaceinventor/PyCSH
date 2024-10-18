/*
 * parameter.h
 *
 * Contains the Parameter base class.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <slash/slash.h>


typedef struct {
    PyObject_HEAD

	struct slash_command * command;

} SlashCommandObject;

extern PyTypeObject SlashCommandType;
