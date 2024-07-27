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
#include "../utils.h"
#include "../csh/known_hosts.h"


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
	return PyUnicode_FromFormat("\nIDENT %hu\n  %s\n  %s\n  %s\n  %s %s\n", self->id.src, PyUnicode_AsUTF8(self->hostname), PyUnicode_AsUTF8(self->model), PyUnicode_AsUTF8(self->revision), PyUnicode_AsUTF8(self->date), PyUnicode_AsUTF8(self->time));
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

__attribute__((malloc, malloc(Ident_dealloc, 1)))
static PyObject * Ident_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {

	static char *kwlist[] = {"node", "timeout", "override", NULL};

	unsigned int node = pycsh_dfl_node;
    unsigned int timeout = pycsh_dfl_timeout;
    bool override = false;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|IIp", kwlist, &node, &timeout, &override)) {
		return NULL;  // TypeError is thrown
	}

	struct csp_cmp_message msg;
    msg.type = CSP_CMP_REQUEST;
    msg.code = CSP_CMP_IDENT;
    const int size = sizeof(msg.type) + sizeof(msg.code) + sizeof(msg.ident);

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

	Py_BEGIN_ALLOW_THREADS;

		/* Copy the request */
		memcpy(packet->data, &msg, size);
		packet->length = size;

		csp_send(conn, packet);
    Py_END_ALLOW_THREADS;

    PyObject * reply_tuple = PyTuple_New(0);

	if (!reply_tuple) {
		return NULL;  // Let's just assume that Python has set some a MemoryError exception here
	}

    while((packet = csp_read(conn, timeout)) != NULL) {

		/* Using __attribute__, so we don't forget to free */
		csp_packet_t *__packet __attribute__((cleanup(csp_buffer_cleanup))) = packet;

        memcpy(&msg, packet->data, packet->length < size ? packet->length : size);
		if (msg.code != CSP_CMP_IDENT) {
			continue;
		}

		Py_ssize_t reply_index = PyTuple_GET_SIZE(reply_tuple);
		/* Resize tuple to fit reply, this could probably be done more efficiently. */
		if (_PyTuple_Resize(&reply_tuple, reply_index+1) < 0) {
			// Handle tuple resizing failure
			PyErr_SetString(PyExc_RuntimeError, "Failed to resize tuple for ident replies");
			return NULL;
		}

		IdentObject *self = (IdentObject *)type->tp_alloc(type, 0);
		if (self == NULL) {
			/* This is likely a memory allocation error, in which case we expect .tp_alloc() to have raised an exception. */
			return NULL;
		}

        memcpy(&self->id, &packet->id, sizeof(csp_id_t));

		self->hostname = PyUnicode_FromStringAndSize(msg.ident.hostname, CSP_HOSTNAME_LEN);
		self->model = PyUnicode_FromStringAndSize(msg.ident.model, CSP_MODEL_LEN);
		self->revision = PyUnicode_FromStringAndSize(msg.ident.revision, CSP_CMP_IDENT_REV_LEN);
		self->date = PyUnicode_FromStringAndSize(msg.ident.date, CSP_CMP_IDENT_DATE_LEN);
		self->time = PyUnicode_FromStringAndSize(msg.ident.time, CSP_CMP_IDENT_TIME_LEN);

		if (!(self->hostname && self->model && self->revision && self->date && self->time)) {
			PyErr_SetString(PyExc_MemoryError, "Failed to allocate memory for ident strings");
			// TODO Kevin: Maybe we can return the objects we managed to create?
			return NULL;
		}

		{  /* Build and assign self->datetime */
			PyObject *datetime_module AUTO_DECREF = PyImport_ImportModule("datetime");
			if (!datetime_module) {
				return NULL;
			}

			PyObject *datetime_class AUTO_DECREF = PyObject_GetAttrString(datetime_module, "datetime");
			if (!datetime_class) {
				return NULL;
			}

			PyObject *datetime_strptime AUTO_DECREF = PyObject_GetAttrString(datetime_class, "strptime");
			if (!datetime_strptime) {
				return NULL;
			}

			//PyObject *datetime_str AUTO_DECREF = PyUnicode_FromFormat("%U %U", self->date, self->time);
			PyObject *datetime_str AUTO_DECREF = PyUnicode_FromFormat("%s %s", msg.ident.date, msg.ident.time);
			if (!datetime_str) {
				return NULL;
			}

			PyObject *format_str AUTO_DECREF = PyUnicode_FromString("%b %d %Y %H:%M:%S");
			if (!format_str) {
				return NULL;
			}

			PyObject *datetime_args AUTO_DECREF = PyTuple_Pack(2, datetime_str, format_str);
			if (!datetime_args) {
				return NULL;
			}

			PyObject *datetime_obj AUTO_DECREF = PyObject_CallObject(datetime_strptime, datetime_args);
			if (!datetime_obj) {
				return NULL;
			}

			self->datetime = datetime_obj;  // Steal the reference from PyObject_CallObject()
		}

		PyTuple_SET_ITEM(reply_tuple, reply_index, (PyObject*)self);

		known_hosts_add(packet->id.src, msg.ident.hostname, override);
    }

    return reply_tuple;
}


static long Ident_hash(IdentObject *self) {
    return self->id.src + PyObject_Hash(self->hostname) + PyObject_Hash(self->model) + PyObject_Hash(self->revision) + PyObject_Hash(self->date) + PyObject_Hash(self->time);
}

#if 0
static PyObject * Ident_get_name(IdentObject *self, void *closure) {
	return Py_BuildValue("s", self->command->name);
}

static PyGetSetDef Ident_getsetters[] = {

	{"name", (getter)Ident_get_name, NULL,
     "Returns the name of the wrapped slash_command.", NULL},
    {"args", (getter)Idents_get_args, NULL,
     "The args string of the slash_command.", NULL},
    {NULL, NULL, NULL, NULL}  /* Sentinel */
};
#endif

static PyMemberDef Ident_members[] = {
	/* Using T_OBJECT in case we decide to detect empty strings in the ident reply in the future.
		We will then set the IdentObject fields to NULL/Py_None, so None can be returned to Python. */
    {"node", T_OBJECT, offsetof(IdentObject, id.src), READONLY},
    {"hostname", T_OBJECT, offsetof(IdentObject, id.src), READONLY},
    {"model", T_OBJECT, offsetof(IdentObject, id.src), READONLY},
    {"revision", T_OBJECT, offsetof(IdentObject, id.src), READONLY},
    {"date", T_OBJECT, offsetof(IdentObject, id.src), READONLY},
    {"time", T_OBJECT, offsetof(IdentObject, id.src), READONLY},
    {"datetime", T_OBJECT, offsetof(IdentObject, id.src), READONLY},
    {NULL}  /* Sentinel */
};

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
	.tp_richcompare = (richcmpfunc)Ident_richcompare,
	.tp_hash = (hashfunc)Ident_hash,
};
