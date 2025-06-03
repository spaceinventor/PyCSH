
#include "vmem.h"

// It is recommended to always define PY_SSIZE_T_CLEAN before including Python.h
#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "structmember.h"

#include "../pycsh.h"
#include <pycsh/utils.h>



csp_packet_t * pycsh_vmem_client_list_get(int node, int timeout, int version) {

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_CRC32);
	if (conn == NULL)
		return NULL;

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	if (packet == NULL)
		return NULL;

	vmem_request_t * request = (void *) packet->data;
	request->version = version;
	request->type = VMEM_SERVER_LIST;
	packet->length = sizeof(vmem_request_t);

	csp_send(conn, packet);

	csp_packet_t *resp = NULL;

	if (version == 3) {
		/* Allocate the maximum packet length to hold the response for the caller */
		resp = csp_buffer_get(CSP_BUFFER_SIZE);
		if (!resp) {
			printf("Could not allocate CSP buffer for VMEM response.\n");
			csp_close(conn);
			return NULL;
		}

		resp->length = 0;
		/* Keep receiving until we got everything or we got a timeout */
		while ((packet = csp_read(conn, timeout)) != NULL) {
			if (packet->data[0] & 0b01000000) {
				/* First packet */
				resp->length = 0;
			}

			/* Collect the response in the response packet */
			memcpy(&resp->data[resp->length], &packet->data[1], (packet->length - 1));
			resp->length += (packet->length - 1);

			if (packet->data[0] & 0b10000000) {
				/* Last packet, break the loop */
				csp_buffer_free(packet);
				break;
			}

			csp_buffer_free(packet);
		}

		if (packet == NULL) {
			printf("No response to VMEM list request\n");
		}
	} else {
		/* Wait for response */
		packet = csp_read(conn, timeout);
		if (packet == NULL) {
			printf("No response to VMEM list request\n");
		}
		resp = packet;
	}

	csp_close(conn);

	return resp;
}

static PyObject * Vmem_str(VmemObject *self) {
	const vmem_list3_t * const vmem = &self->vmem;
	return PyUnicode_FromFormat(
        " %2u: %-16.16s 0x%016"PRIX64" - %"PRIu64" typ %u\r\n",
		vmem->vmem_id, vmem->name, be64toh(vmem->vaddr), be64toh(vmem->size), vmem->type
    );
}

PyObject * Vmem_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {

	CSP_INIT_CHECK()

	unsigned int node = pycsh_dfl_node;
	unsigned int timeout = pycsh_dfl_timeout;
	unsigned int version = 2;
	int verbose = pycsh_dfl_verbose;

	static char *kwlist[] = {"node", "timeout", "version", "verbose", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|IIIi", kwlist, &node, &timeout, &version, &verbose))
		return NULL;  // Raises TypeError.

	if (verbose >= 2) {
		printf("Requesting vmem list from node %u timeout %u version %d\n", node, timeout, version);
	}
	
	csp_packet_t * packet = NULL;
	Py_BEGIN_ALLOW_THREADS;
		packet = pycsh_vmem_client_list_get(node, timeout, version);
	Py_END_ALLOW_THREADS;
	if (packet == NULL) {
		PyErr_Format(PyExc_ConnectionError, "No response (node=%d, timeout=%d)", node, timeout);
		return NULL;
	}

	size_t vmem_idx = 0;
	size_t vmem_count = 0;
	size_t namelen = 0;  // Length from  vmem_server.h
	if (version == 3) {
		vmem_count = packet->length / sizeof(vmem_list3_t);
		namelen = 16+1;
	} else if (version == 2) {
		vmem_count = packet->length / sizeof(vmem_list2_t);
		namelen = 5;
	} else {
		vmem_count = packet->length / sizeof(vmem_list_t);
		namelen = 5;
	}

	/* AUTO_DECREF used for exception handling, Py_NewRef() returned otherwise. */
    PyObject * vmem_tuple AUTO_DECREF = PyTuple_New(vmem_count);
    if (!vmem_tuple) {
        return NULL;  // Let's just assume that Python has set some a MemoryError exception here
    }

	if (version == 3) {
		for (vmem_list3_t * vmem = (void *) packet->data; (intptr_t) vmem < (intptr_t) packet->data + packet->length; vmem++) {

			VmemObject *self = (VmemObject *)type->tp_alloc(type, 0);
			if (self == NULL) {
				/* This is likely a memory allocation error, in which case we expect .tp_alloc() to have raised an exception. */
				return NULL;
			}

			self->vmem = (vmem_list3_t){
				.vmem_id = vmem->vmem_id,
				.vaddr = be64toh(vmem->vaddr),
				.size = be64toh(vmem->size),
				.type = vmem->type,
			};
			strncpy(self->vmem.name, vmem->name, namelen);
			
			if (verbose >= 1) {
				printf(" %2u: %-16.16s 0x%016"PRIX64" - %"PRIu64" typ %u\r\n", vmem->vmem_id, vmem->name, be64toh(vmem->vaddr), be64toh(vmem->size), vmem->type);
			}
			
			PyTuple_SET_ITEM(vmem_tuple, vmem_idx++, (PyObject*)self);
		}
	} else if (version == 2) {
		for (vmem_list2_t * vmem = (void *) packet->data; (intptr_t) vmem < (intptr_t) packet->data + packet->length; vmem++) {

			VmemObject *self = (VmemObject *)type->tp_alloc(type, 0);
			if (self == NULL) {
				/* This is likely a memory allocation error, in which case we expect .tp_alloc() to have raised an exception. */
				return NULL;
			}

			self->vmem = (vmem_list3_t){
				.vmem_id = vmem->vmem_id,
				.vaddr = be64toh(vmem->vaddr),
				.size = be32toh(vmem->size),
				.type = vmem->type,
			};
			strncpy(self->vmem.name, vmem->name, namelen);
			
			if (verbose >= 1) {
				printf(" %2u: %-5.5s 0x%016"PRIX64" - %"PRIu32" typ %u\r\n", vmem->vmem_id, vmem->name, be64toh(vmem->vaddr), be32toh(vmem->size), vmem->type);
			}
			
			PyTuple_SET_ITEM(vmem_tuple, vmem_idx++, (PyObject*)self);
		}
	} else {
		for (vmem_list_t * vmem = (void *) packet->data; (intptr_t) vmem < (intptr_t) packet->data + packet->length; vmem++) {

			VmemObject *self = (VmemObject *)type->tp_alloc(type, 0);
			if (self == NULL) {
				/* This is likely a memory allocation error, in which case we expect .tp_alloc() to have raised an exception. */
				return NULL;
			}

			self->vmem = (vmem_list3_t){
				.vmem_id = vmem->vmem_id,
				.vaddr = be32toh(vmem->vaddr),
				.size = be32toh(vmem->size),
				.type = vmem->type,
			};
			strncpy(self->vmem.name, vmem->name, namelen);
			
			if (verbose >= 1) {
				printf(" %2u: %-5.5s 0x%08"PRIX32" - %"PRIu32" typ %u\r\n", vmem->vmem_id, vmem->name, be32toh(vmem->vaddr), be32toh(vmem->size), vmem->type);
			}
			
			PyTuple_SET_ITEM(vmem_tuple, vmem_idx++, (PyObject*)self);
		}

	}

    return Py_NewRef(vmem_tuple);
}

static PyMemberDef Vmem_members[] = {
	{"vaddr", T_LONG, offsetof(VmemObject, vmem.vaddr), READONLY, "Starting address of the VMEM area. Used for upload and download I think"},
    {"size", T_LONG, offsetof(VmemObject, vmem.size), READONLY, "Size of the VMEM area in bytes"},
    {"vmem_id", T_BYTE, offsetof(VmemObject, vmem.vmem_id), READONLY, "ID of the VMEM area, used for certain commands"},
    {"type", T_BYTE, offsetof(VmemObject, vmem.type), READONLY, "int type of the VMEM area"},
    {"name", T_STRING_INPLACE, offsetof(VmemObject, vmem.name), READONLY, "Name of the VMEM area"},
    {NULL}  /* Sentinel */
};

PyTypeObject VmemType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pycsh.Vmem",
    .tp_doc = "Convenient wrapper class for 'vmem' replies.",
    .tp_basicsize = sizeof(VmemObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = Vmem_new,
	.tp_members = Vmem_members,
	.tp_str = (reprfunc)Vmem_str,
};
