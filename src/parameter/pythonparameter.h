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

    /* We need somewhere to store the strings the param_t fields point to. */
    /* These lengths are sat from libparam/src/param/list/param_list.h */
    char name[36];
    char unit[10];
    char help[150];
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