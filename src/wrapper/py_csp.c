/*
 * py_csp.c
 *
 * Wrappers for src/slash_csp.c
 *
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <csp/csp_cmp.h>

#include "../pycsh.h"

#include "py_csp.h"


PyObject * pycsh_slash_ping(PyObject * self, PyObject * args, PyObject * kwds) {

    if (!csp_initialized()) {
        PyErr_SetString(PyExc_RuntimeError,
            "Cannot perform operations before .init() has been called.");
        return NULL;
    }

    unsigned int node = pycsh_dfl_node;
    unsigned int timeout = pycsh_dfl_timeout;
    unsigned int size = 1;

    static char *kwlist[] = {"node", "timeout", "size", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|III", kwlist, &node, &timeout, &size)) {
        return NULL;  // TypeError is thrown
    }

    int result;
    Py_BEGIN_ALLOW_THREADS;
    printf("Ping node %u size %u timeout %u: ", node, size, timeout);

    result = csp_ping(node, timeout, size, CSP_O_CRC32);

    if (result >= 0) {
        printf("Reply in %d [ms]\n", result);
    } else {
        printf("No reply\n");
    }
    Py_END_ALLOW_THREADS;

    return Py_BuildValue("i", result);

}

PyObject * pycsh_slash_ident(PyObject * self, PyObject * args, PyObject * kwds) {

    if (!csp_initialized()) {
        PyErr_SetString(PyExc_RuntimeError,
            "Cannot perform operations before .init() has been called.");
        return NULL;
    }

    unsigned int node = pycsh_dfl_node;
    unsigned int timeout = pycsh_dfl_timeout;

    static char *kwlist[] = {"node", "timeout", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|II", kwlist, &node, &timeout)) {
        return NULL;  // TypeError is thrown
    }

    struct csp_cmp_message msg;
    msg.type = CSP_CMP_REQUEST;
    msg.code = CSP_CMP_IDENT;
    int size = sizeof(msg.type) + sizeof(msg.code) + sizeof(msg.ident);

    csp_conn_t * conn;
    csp_packet_t * packet;
    Py_BEGIN_ALLOW_THREADS;
    conn = csp_connect(CSP_PRIO_NORM, node, CSP_CMP, timeout, CSP_O_CRC32);
    if (conn == NULL) {
        return 0;
    }

    packet = csp_buffer_get(size);
    if (packet == NULL) {
        csp_close(conn);
        return 0;
    }

    /* Copy the request */
    memcpy(packet->data, &msg, size);
    packet->length = size;

    csp_send(conn, packet);
    Py_END_ALLOW_THREADS;

    PyObject * list_string = PyUnicode_New(0, 0);

    while((packet = csp_read(conn, timeout)) != NULL) {
        memcpy(&msg, packet->data, packet->length < size ? packet->length : size);
        if (msg.code == CSP_CMP_IDENT) {
            char buf[500];
            snprintf(buf, sizeof(buf), "\nIDENT %hu\n  %s\n  %s\n  %s\n  %s %s\n", packet->id.src, msg.ident.hostname, msg.ident.model, msg.ident.revision, msg.ident.date, msg.ident.time);
            printf("%s", buf);
            PyUnicode_AppendAndDel(&list_string, PyUnicode_FromString(buf));
        }
        csp_buffer_free(packet);
    }

    csp_close(conn);

    return list_string;
    
}

PyObject * pycsh_slash_reboot(PyObject * self, PyObject * args) {

    unsigned int node = pycsh_dfl_node;

    if (!PyArg_ParseTuple(args, "|I", &node))
        return NULL;  // Raises TypeError.

    Py_BEGIN_ALLOW_THREADS;
    csp_reboot(node);
    Py_END_ALLOW_THREADS;

    Py_RETURN_NONE;
}