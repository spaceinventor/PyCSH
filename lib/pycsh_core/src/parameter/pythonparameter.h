/*
 * pythonparameter.h
 *
 * Contains the PythonParameter Parameter subclass.
 *
 */

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdbool.h>
#include <param/param.h>

#include "parameter.h"
#include "dynamicparameter.h"

typedef struct {
    DynamicParameterObject dyn_param;
	int keep_alive: 1;  // For the sake of reference counting, keep_alive should only be changed by Parameter_setkeep_alive()
} PythonParameterObject;

extern PyTypeObject PythonParameterType;

// Source: https://chat.openai.com
/**
 * @brief Check that the callback accepts exactly one Parameter and one integer,
 *  as specified by "void (*callback)(struct param_s * param, int offset)"
 * 
 * Currently also checks type-hints (if specified).
 * 
 * @param callback function to check
 * @param raise_exc Whether to set exception message when returning false.
 * @return true for success
 */
bool is_valid_callback(const PyObject *callback, bool raise_exc);
