#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdint.h>
#include <slash/dflopt.h>
#include "../src/csh/known_hosts.h"

PyObject * pycsh_node_add(PyObject * self, PyObject * args, PyObject * kwds){
    int node = slash_dfl_node;
    int pdu;
    uint8_t channel;
    uint32_t serial;
    char * name;


    static char *kwlist[] = {"node", "name", "pdu", "channel", "serial", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|iihIs", kwlist, &node, &name, &pdu, &channel, &serial)) {
        return NULL;
    }

    if (node == 0) {
        PyErr_SetString(PyExc_ValueError, "Refusing to add hostname for node 0");
        return NULL;
    }

    if (known_hosts_add(node, name, true) == NULL) {
        PyErr_SetString(PyExc_MemoryError, "No more memory, failed to add host");
        return NULL;  // We have already checked for node 0, so this can currently only be a memory error.
    }


    return Py_None;
}