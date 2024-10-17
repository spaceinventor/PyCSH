#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <csp/csp_cmp.h>
#include <csp/csp_types.h>


typedef struct {
    PyObject_HEAD

    csp_id_t id;

    // struct csp_cmp_message ident;
    /* We would've had to to copy the strings in csp_cmp_message to reuse that struct.
        So we might as well just store the string as PyObjects* instead. */
    PyObject *hostname;
    PyObject *model;
    PyObject *revision;
    PyObject *date;
    PyObject *time;

    PyObject *datetime;

} IdentObject;

extern PyTypeObject IdentType;
