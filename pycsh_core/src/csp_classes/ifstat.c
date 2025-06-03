
#include "ifstat.h"

#include "structmember.h"

#include <csp/csp_cmp.h>
#include <csp/csp_types.h>

#include "../pycsh.h"
#include <pycsh/utils.h>
#include <apm/csh_api.h>


static PyObject * Ifstat_str(IfstatObject *self) {
	return PyUnicode_FromFormat(
        "%-5s   tx: %05"PRIu32" rx: %05"PRIu32" txe: %05"PRIu32" rxe: %05"PRIu32"\n"
	       "        drop: %05"PRIu32" autherr: %05"PRIu32" frame: %05"PRIu32"\n"
	       "        txb: %"PRIu32" rxb: %"PRIu32"\n\n",
		self->message.if_stats.interface,
		self->message.if_stats.tx,
		self->message.if_stats.rx,
		self->message.if_stats.tx_error,
		self->message.if_stats.rx_error,
		self->message.if_stats.drop,
		self->message.if_stats.autherr,
		self->message.if_stats.frame,
		self->message.if_stats.txbytes,
		self->message.if_stats.rxbytes
    );
}

PyObject * Ifstat_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {

	CSP_INIT_CHECK()

    char * if_name = NULL;
    unsigned int node = pycsh_dfl_node;
    unsigned int timeout = pycsh_dfl_timeout;

    static char *kwlist[] = {"if_name", "node", "timeout", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s|II", kwlist, &if_name, &node, &timeout)) {
        return NULL;  // TypeError is thrown
    }

	IfstatObject *self = (IfstatObject *)type->tp_alloc(type, 0);
	if (self == NULL) {
		/* This is likely a memory allocation error, in which case we expect .tp_alloc() to have raised an exception. */
		return NULL;
	}

	strncpy(self->message.if_stats.interface, if_name, CSP_CMP_ROUTE_IFACE_LEN - 1);
	
	bool no_reply = false;
    Py_BEGIN_ALLOW_THREADS;
    if (csp_cmp_if_stats(node, timeout, &self->message) != CSP_ERR_NONE) {
		no_reply = true;
	}
    Py_END_ALLOW_THREADS;

	if (no_reply) {
		Py_DECREF((PyObject*)self);
		PyErr_Format(PyExc_ConnectionError, "No response (node=%d, timeout=%d)", node, timeout);
		return NULL;
	}

    self->message.if_stats.tx =       be32toh(self->message.if_stats.tx);
	self->message.if_stats.rx =       be32toh(self->message.if_stats.rx);
	self->message.if_stats.tx_error = be32toh(self->message.if_stats.tx_error);
	self->message.if_stats.rx_error = be32toh(self->message.if_stats.rx_error);
	self->message.if_stats.drop =     be32toh(self->message.if_stats.drop);
	self->message.if_stats.autherr =  be32toh(self->message.if_stats.autherr);
	self->message.if_stats.frame =    be32toh(self->message.if_stats.frame);
	self->message.if_stats.txbytes =  be32toh(self->message.if_stats.txbytes);
	self->message.if_stats.rxbytes =  be32toh(self->message.if_stats.rxbytes);
	self->message.if_stats.irq = 	 be32toh(self->message.if_stats.irq);

    return (PyObject*)self;
}

static_assert(sizeof(uint32_t) == sizeof(unsigned int));

static PyMemberDef Ifstat_members[] = {
    {"interface", T_STRING_INPLACE, offsetof(IfstatObject, message.if_stats.interface), READONLY, "interface of the ifstat reply"},
    {"tx", T_UINT, offsetof(IfstatObject, message.if_stats.tx), READONLY, "tx of the ifstat reply"},
    {"rx", T_UINT, offsetof(IfstatObject, message.if_stats.rx), READONLY, "rx of the ifstat reply"},
    {"tx_error", T_UINT, offsetof(IfstatObject, message.if_stats.tx_error), READONLY, "tx_error of the ifstat reply"},
    {"rx_error", T_UINT, offsetof(IfstatObject, message.if_stats.rx_error), READONLY, "rx_error of the ifstat reply"},
    {"drop", T_UINT, offsetof(IfstatObject, message.if_stats.drop), READONLY, "drop of the ifstat reply"},
    {"autherr", T_UINT, offsetof(IfstatObject, message.if_stats.autherr), READONLY, "autherr of the ifstat reply"},
    {"frame", T_UINT, offsetof(IfstatObject, message.if_stats.frame), READONLY, "frame of the ifstat reply"},
    {"txbytes", T_UINT, offsetof(IfstatObject, message.if_stats.txbytes), READONLY, "txbytes of the ifstat reply"},
    {"rxbytes", T_UINT, offsetof(IfstatObject, message.if_stats.rxbytes), READONLY, "rxbytes of the ifstat reply"},
    {NULL}  /* Sentinel */
};

PyTypeObject IfstatType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pycsh.Ifstat",
    .tp_doc = "Convenient wrapper class for 'ifstat' replies.",
    .tp_basicsize = sizeof(IfstatObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Ifstat_new,
	.tp_members = Ifstat_members,
	.tp_str = (reprfunc)Ifstat_str,
};
