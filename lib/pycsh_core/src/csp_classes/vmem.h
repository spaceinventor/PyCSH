#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <vmem/vmem_server.h>


csp_packet_t * pycsh_vmem_client_list_get(int node, int timeout, int version);

typedef struct {
    PyObject_HEAD

    /* Use largest vmem type, which we hope will remain "backwards compatible". */
    vmem_list3_t vmem;

} VmemObject;

extern PyTypeObject VmemType;
