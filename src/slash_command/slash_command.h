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

#include <param/param.h>

extern PyDictObject * slash_command_dict;

typedef struct {
    PyObject_HEAD

	struct slash_command * command;

} SlashCommandObject;

extern PyTypeObject SlashCommandType;

#if 0
/**
 * @brief Takes a slash_command and creates and or returns the wrapping SlashCommandObject.
 * 
 * @param param param_t to find the ParameterObject for.
 * @return ParameterObject* The wrapping ParameterObject.
 */
ParameterObject * SlashCommandObject_from_slash_command(param_t * param, int host, int timeout, int retries);
#endif
