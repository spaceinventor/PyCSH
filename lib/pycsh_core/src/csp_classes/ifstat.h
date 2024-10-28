#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <csp/csp_cmp.h>
#include <csp/csp_types.h>


PyObject * Ifstat_new(PyTypeObject *type, PyObject *args, PyObject *kwds);

typedef struct {
    PyObject_HEAD

    struct csp_cmp_message message;

} IfstatObject;

extern PyTypeObject IfstatType;
