/*
 * parameter.c
 *
 * Contains the Parameter base class.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

#include "ident.h"

#include "structmember.h"

#include <csp/csp_cmp.h>
#include <csp/csp_types.h>

#include "../pycsh.h"
#include <pycsh/utils.h>
#include <apm/csh_api.h>



/* 1 for success. Compares the fields of two 'ident' replies, otherwise 0. Assumes self to be a IdentObject. */
static int Ident_equal(PyObject *self, PyObject *other) {
    if (PyObject_TypeCheck(other, &IdentType))
        return PyObject_Hash(self) == PyObject_Hash(other);
    return 0;
}

static PyObject * Ident_richcompare(PyObject *self, PyObject *other, int op) {

    PyObject *result = Py_NotImplemented;

    switch (op) {
        // case Py_LT:
        // 	break;
        // case Py_LE:
        // 	break;
        case Py_EQ:
            result = (Ident_equal(self, other)) ? Py_True : Py_False;
            break;
        case Py_NE:
            result = (Ident_equal(self, other)) ? Py_False : Py_True;
            break;
        // case Py_GT:
        // 	break;
        // case Py_GE:
        // 	break;
    }

    Py_XINCREF(result);
    return result;
}

static PyObject * Ident_str(IdentObject *self) {
    return PyUnicode_FromFormat(
        "\nIDENT %d\n  %U\n  %U\n  %U\n  %U %U\n",
        self->id.src,
        self->hostname,
        self->model,
        self->revision,
        self->date,
        self->time
    );
}

static PyObject * Ident_repr(IdentObject *self) {
    assert(self->model);
    return PyUnicode_FromFormat("%s@%d", PyUnicode_AsUTF8(self->hostname), self->id.src);
}

static void Ident_dealloc(IdentObject *self) {

    Py_XDECREF(self->hostname);
    Py_XDECREF(self->model);
    Py_XDECREF(self->revision);
    Py_XDECREF(self->date);
    Py_XDECREF(self->time);
    Py_XDECREF(self->datetime);

    // Get the type of 'self' in case the user has subclassed 'Ident'.
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static void csp_conn_cleanup(csp_conn_t ** conn) {
    csp_close(*conn);
}

static void csp_buffer_cleanup(csp_packet_t ** packet) {
    csp_buffer_free(*packet);
}

__attribute__((malloc(Ident_dealloc, 1)))
static PyObject * Ident_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {

    static char *kwlist[] = {"node", "timeout", "override", NULL};

    unsigned int node = pycsh_dfl_node;
    unsigned int timeout = pycsh_dfl_timeout;
    bool override = false;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|IIp", kwlist, &node, &timeout, &override)) {
        return NULL;  // TypeError is thrown
    }

    struct csp_cmp_message msg = {
		.type = CSP_CMP_REQUEST,
		.code = CSP_CMP_IDENT,
	};
	int size = sizeof(msg.type) + sizeof(msg.code) + sizeof(msg.ident);

    csp_conn_t * conn __attribute__((cleanup(csp_conn_cleanup))) = NULL;
    csp_packet_t * packet;

    Py_BEGIN_ALLOW_THREADS;
        conn = csp_connect(CSP_PRIO_NORM, node, CSP_CMP, timeout, CSP_O_CRC32);
    Py_END_ALLOW_THREADS;

    if (conn == NULL) {
        PyErr_SetString(PyExc_ConnectionError, "Failed to connect to node");
        return NULL;
    }

    packet = csp_buffer_get(size);
    if (packet == NULL) {
        PyErr_SetString(PyExc_MemoryError, "Failed to allocate CSP buffer");
        return NULL;
    }

    /* AUTO_DECREF used for exception handling, Py_NewRef() returned otherwise. */
    PyObject * reply_tuple AUTO_DECREF = PyTuple_New(0);

    if (!reply_tuple) {
        return NULL;  // Let's just assume that Python has set some a MemoryError exception here
    }

    /* Not really sure it's worth unlocking the GIL for `csp_send(...)`... */
    Py_BEGIN_ALLOW_THREADS;

        /* Copy the request */
        memcpy(packet->data, &msg, size);
        packet->length = size;

        csp_send(conn, packet);
    Py_END_ALLOW_THREADS;

    while(true) {

        /* Using __attribute__, so we don't forget to free */
        csp_packet_t *__packet __attribute__((cleanup(csp_buffer_cleanup))) = packet;

        Py_BEGIN_ALLOW_THREADS;
            packet = csp_read(conn, timeout);
        Py_END_ALLOW_THREADS;

        if (packet == NULL) {
            break;
        }

        memcpy(&msg, packet->data, packet->length < size ? packet->length : size);
        if (msg.code != CSP_CMP_IDENT) {
            continue;
        }

        const Py_ssize_t reply_index = PyTuple_GET_SIZE(reply_tuple);
        /* Resize tuple to fit reply, this could probably be done more efficiently. */
        if (_PyTuple_Resize(&reply_tuple, reply_index+1) < 0) {
            PyErr_SetString(PyExc_RuntimeError, "Failed to resize tuple for ident replies");
            return NULL;
        }

        IdentObject *self = (IdentObject *)type->tp_alloc(type, 0);
        if (self == NULL) {
            /* This is likely a memory allocation error, in which case we expect .tp_alloc() to have raised an exception. */
            return NULL;
        }

        memcpy(&self->id, &packet->id, sizeof(csp_id_t));

        /* ´PyUnicode_FromStringAndSize()´ will pad the string with \x00 up to the specified size.
            So we use strnlen() first to strip them, while respecting the maximum length from libcsp. */
        self->hostname = PyUnicode_FromStringAndSize(msg.ident.hostname, strnlen(msg.ident.hostname, CSP_HOSTNAME_LEN));
        self->model = PyUnicode_FromStringAndSize(msg.ident.model, strnlen(msg.ident.model, CSP_MODEL_LEN));
        self->revision = PyUnicode_FromStringAndSize(msg.ident.revision, strnlen(msg.ident.revision, CSP_CMP_IDENT_REV_LEN));
        self->date = PyUnicode_FromStringAndSize(msg.ident.date, strnlen(msg.ident.date, CSP_CMP_IDENT_DATE_LEN));
        self->time = PyUnicode_FromStringAndSize(msg.ident.time, strnlen(msg.ident.time, CSP_CMP_IDENT_TIME_LEN));

        if (!(self->hostname && self->model && self->revision && self->date && self->time)) {
            PyErr_SetString(PyExc_MemoryError, "Failed to allocate memory for ident strings");
            // TODO Kevin: Maybe we can return the objects we managed to create?
            return NULL;
        }

        self->datetime = pycsh_ident_time_to_datetime(msg.ident.date, msg.ident.time);  // Steal the reference from the returned datetime()
        if (!self->datetime) {
            return NULL;
        }

        PyTuple_SET_ITEM(reply_tuple, reply_index, (PyObject*)self);
    }

    return Py_NewRef(reply_tuple);
}


static long Ident_hash(IdentObject *self) {
    return self->id.src + PyObject_Hash(self->hostname) + PyObject_Hash(self->model) + PyObject_Hash(self->revision) + PyObject_Hash(self->date) + PyObject_Hash(self->time);
}

#if 0
static PyObject * Ident_get_node(IdentObject *self, void *closure) {
    return Py_BuildValue("i", self->id.src);
}

static PyObject * Ident_get_hostname(IdentObject *self, void *closure) {
    return Py_NewRef(self->hostname);
}

static PyObject * Ident_get_model(IdentObject *self, void *closure) {
    return Py_NewRef(self->model);
}

static PyObject * Ident_get_revision(IdentObject *self, void *closure) {
    return Py_NewRef(self->revision);
}

static PyObject * Ident_get_date(IdentObject *self, void *closure) {
    return Py_NewRef(self->date);
}

static PyObject * Ident_get_time(IdentObject *self, void *closure) {
    return Py_NewRef(self->time);
}

static PyObject * Ident_get_datetime(IdentObject *self, void *closure) {
    return Py_NewRef(self->datetime);
}

static PyGetSetDef Ident_getsetters[] = {

    {"node", (getter)Ident_get_node, NULL, "Returns the node of Ident reply", NULL},
    {"hostname", (getter)Ident_get_hostname, NULL, "Returns the hostname of Ident reply", NULL},
    {"model", (getter)Ident_get_model, NULL, "Returns the model of Ident reply", NULL},
    {"revision", (getter)Ident_get_revision, NULL, "Returns the revision of Ident reply", NULL},
    {"date", (getter)Ident_get_date, NULL, "Returns the date of Ident reply", NULL},
    {"time", (getter)Ident_get_time, NULL, "Returns the time of Ident reply", NULL},
    {"datetime", (getter)Ident_get_datetime, NULL, "Returns the datetime of Ident reply", NULL},
     
    {NULL, NULL, NULL, NULL}  /* Sentinel */
};
#else
static PyMemberDef Ident_members[] = {
    /* Using T_OBJECT in case we decide to detect empty strings in the ident reply in the future.
        We will then set the IdentObject fields to NULL/Py_None, so None can be returned to Python. */
    {"node", T_USHORT, offsetof(IdentObject, id.src), READONLY, "Node of Ident reply"},
    {"hostname", T_OBJECT, offsetof(IdentObject, hostname), READONLY, "Hostname of Ident reply"},
    {"model", T_OBJECT, offsetof(IdentObject, model), READONLY, "Model of Ident reply"},
    {"revision", T_OBJECT, offsetof(IdentObject, revision), READONLY, "Revision of Ident reply"},
    {"date", T_OBJECT, offsetof(IdentObject, date), READONLY, "Date of Ident reply"},
    {"time", T_OBJECT, offsetof(IdentObject, time), READONLY, "Time of Ident reply"},
    {"datetime", T_OBJECT, offsetof(IdentObject, datetime), READONLY, "Datetime of Ident reply"},
    {NULL}  /* Sentinel */
};
#endif

PyTypeObject IdentType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pycsh.Ident",
    .tp_doc = "Convenient wrapper class for 'ident' replies. Allows for easy iteration of multiple responses",
    .tp_basicsize = sizeof(IdentObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Ident_new,
    .tp_dealloc = (destructor)Ident_dealloc,
    // .tp_getset = Ident_getsetters,
    .tp_members = Ident_members,
    // .tp_methods = Parameter_methods,
    .tp_str = (reprfunc)Ident_str,
    .tp_repr = (reprfunc)Ident_repr,
    .tp_richcompare = (richcmpfunc)Ident_richcompare,
    .tp_hash = (hashfunc)Ident_hash,
};
