/*
 * param_py.h
 *
 * Wrappers for lib/param/src/param/param_slash.c
 *
 */

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

//#define PYAPMS_DIR "/.local/lib/csh/pyapms/"
#define PYAPMS_DIR "/.local/lib/csh/"

#define WALKDIR_MAX_PATH_SIZE 256
#define DEFAULT_INIT_FUNCTION "apm_init"

/**
 * @brief Handle loading of both .py and .so (APM) modules
 * 
 * TODO Kevin: Extract init_function call to separate function,
 * 	so it can be required by "py run" but optional by "apm load".
 * 
 * @param _filepath Specific Python APM to load/import.
 * @param init_function Optional init function to call after importing. Set to NULL to skip.
 * @param verbose Whether to printf() 
 * @return PyObject* new reference to the imported module
 */
PyObject * pycsh_load_pymod(const char * const _filepath, const char * const init_function, int verbose);
