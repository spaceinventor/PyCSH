/*
 * parameter.h
 *
 * Contains miscellaneous utilities used by PyCSH.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <param/param.h>
#include <param/param_queue.h>
#include "parameter/pythonparameter.h"

void cleanup_str(char ** obj);
void cleanup_free(void ** obj);
void cleanup_GIL(PyGILState_STATE * gstate);
void cleanup_pyobject(PyObject **obj);

void state_release_GIL(PyThreadState ** state);

#define CLEANUP_STR __attribute__((cleanup(cleanup_str)))
#define CLEANUP_FREE __attribute__((cleanup(cleanup_free)))
#define CLEANUP_GIL __attribute__((cleanup(cleanup_GIL)))
#define AUTO_DECREF __attribute__((cleanup(cleanup_pyobject)))

__attribute__((malloc, malloc(free, 1)))
char *safe_strdup(const char *s);

/* Source: https://pythonextensionpatterns.readthedocs.io/en/latest/super_call.html */
PyObject * call_super_pyname_lookup(PyObject *self, PyObject *func_name, PyObject *args, PyObject *kwargs);


/* Retrieves a param_t from either its name, id or wrapper object.
   May raise TypeError or ValueError, returned value will be NULL in either case. */
param_t * _pycsh_util_find_param_t(PyObject * param_identifier, int node);


/* Public interface for '_pycsh_misc_param_t_type()'
   Increments the reference count of the found type before returning. */
PyObject * pycsh_util_get_type(PyObject * self, PyObject * args);


/* Create a Python Parameter object from a param_t pointer directly. */
PyObject * _pycsh_Parameter_from_param(PyTypeObject *type, param_t * param, const PyObject * callback, int host, int timeout, int retries, int paramver);


/* Constructs a list of Python Parameters of all known param_t returned by param_list_iterate. */
PyObject * pycsh_util_parameter_list(void);

/* Private interface for getting the value of single parameter
   Increases the reference count of the returned item before returning.
   Use INT_MIN for offset as no offset. */
PyObject * _pycsh_util_get_single(param_t *param, int offset, int autopull, int host, int timeout, int retries, int paramver);

/* Private interface for getting the value of an array parameter
   Increases the reference count of the returned tuple before returning.  */
PyObject * _pycsh_util_get_array(param_t *param, int autopull, int host, int timeout, int retries, int paramver);


/* Private interface for setting the value of a normal parameter. 
   Use INT_MIN as no offset. */
int _pycsh_util_set_single(param_t *param, PyObject *value, int offset, int host, int timeout, int retries, int paramver, int remote);

/* Private interface for setting the value of an array parameter. */
int _pycsh_util_set_array(param_t *param, PyObject *value, int host, int timeout, int retries, int paramver);

/**
 * @brief Check if this param_t is wrapped by a ParameterObject.
 * 
 * @return borrowed reference to the wrapping PythonParameterObject if wrapped, otherwise NULL.
 */
PythonParameterObject * Parameter_wraps_param(param_t *param);

/**
 * @brief Convert a python str og int parameter mask to the uint32_t C equivalent.
 * 
 * @param mask_in Python mask to convert
 * @param mask_out Returned parsed parameter mask.
 * @return int 0 on success
 */
int pycsh_parse_param_mask(PyObject * mask_in, uint32_t * mask_out);
