/*
 * pycsh.h
 *
 * Setup and initialization for the PyCSH module.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

#pragma once

// It is recommended to always define PY_SSIZE_T_CLEAN before including Python.h
#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "pycshconfig.h"

#include <param/param_queue.h>

#define CSP_INIT_CHECK() \
	if (!csp_initialized()) {\
		PyErr_SetString(PyExc_RuntimeError,\
			"Cannot perform operations before .init() has been called.");\
		return NULL;\
	}

uint8_t csp_initialized();
extern PyMODINIT_FUNC PyInit_pycsh(void);

#ifdef PYCSH_HAVE_SLASH
#include <slash/dflopt.h>
#define pycsh_dfl_node slash_dfl_node
#define pycsh_dfl_timeout slash_dfl_timeout  // In milliseconds
#else
extern unsigned int pycsh_dfl_timeout;  // In milliseconds
extern unsigned int pycsh_dfl_node;
#endif
extern unsigned int pycsh_dfl_verbose;
