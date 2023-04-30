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

typedef struct {
    PyObject_HEAD
    /* Type-specific fields go here. */
	PyTypeObject *type;  // Best Python representation of the parameter type, i.e 'int' for uint32.
	//uint32_t mask;

	/* Store Python strings for name and unit, to lessen the overhead of converting them from C */
	// PyObject *name;
	// PyObject *unit;
	// PyObject *docstr;
	PyObject *callback;
	int keep_alive: 1;  // For the sake of reference counting, keep_alive should only be changed by Parameter_setkeep_alive()

	param_t param;
	int host;
	char valuebuf[128] __attribute__((aligned(16)));
	int timeout;
	int retries;
	int paramver;
} ParameterObject;

extern PyTypeObject ParameterType;
void Parameter_callback(param_t * param, int offset);

/**
 * @brief Takes a param_t and creates and or returns the wrapping ParameterObject.
 * 
 * @param param param_t to find the ParameterObject for.
 * @return ParameterObject* The wrapping ParameterObject.
 */
ParameterObject * ParameterObject_from_param(param_t * param, int host, int timeout, int retries);

/**
 * @brief Check if this param_t is wrapped by a ParameterObject.
 * 
 * @return int 1 for wrapped, 0 for not wrapped.
 */
int Parameter_wraps_param(param_t *param);

int Parameter_set_callback(ParameterObject *self, PyObject *value, void *closure);