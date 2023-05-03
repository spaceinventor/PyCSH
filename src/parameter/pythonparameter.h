/*
 * pythonparameter.h
 *
 * Contains the PythonParameter Parameter subclass.
 *
 */

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <param/param.h>

#include "parameter.h"

typedef struct {
    ParameterObject parameter_object;
    PyObject *callback;
	int keep_alive: 1;  // For the sake of reference counting, keep_alive should only be changed by Parameter_setkeep_alive()
} PythonParameterObject;

extern PyTypeObject PythonParameterType;

void Parameter_callback(param_t * param, int offset);

/**
 * @brief Check if this param_t is wrapped by a ParameterObject.
 * 
 * @return int 1 for wrapped, 0 for not wrapped.
 */
int Parameter_wraps_param(param_t *param);

int Parameter_set_callback(PythonParameterObject *self, PyObject *value, void *closure);