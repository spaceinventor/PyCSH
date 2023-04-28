/*
 * vmem_client_py.c
 *
 * Wrappers for lib/param/src/vmem/vmem_client_slash.c
 *
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <vmem/vmem_server.h>

#include "vmem_client_py.h"

#include "../pycsh.h"

PyObject * pycsh_vmem(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!csp_initialized()) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before .init() has been called.");
		return NULL;
	}

	unsigned int node = pycsh_dfl_node;
	unsigned int timeout = pycsh_dfl_timeout;
	unsigned int version = 2;

	static char *kwlist[] = {"node", "timeout", "version", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|iii", kwlist, &node, &timeout, &version))
		return NULL;  // Raises TypeError.

	printf("Requesting vmem list from node %u timeout %u version %d\n", node, timeout, version);

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_NONE);
	if (conn == NULL) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		return NULL;
	}

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	if (packet == NULL) {
		PyErr_SetString(PyExc_MemoryError, "Failed to get CSP buffer");
		return NULL;
	}

	vmem_request_t * request = (void *) packet->data;
	request->version = version;
	request->type = VMEM_SERVER_LIST;
	packet->length = sizeof(vmem_request_t);

	csp_send(conn, packet);

	/* Wait for response */
	packet = csp_read(conn, timeout);
	if (packet == NULL) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		csp_close(conn);
		return NULL;
	}

	PyObject * list_string = PyUnicode_New(0, 0);

	if (request->version == 2) {
		for (vmem_list2_t * vmem = (void *) packet->data; (intptr_t) vmem < (intptr_t) packet->data + packet->length; vmem++) {
			char buf[500];
			snprintf(buf, sizeof(buf), " %u: %-5.5s 0x%lX - %u typ %u\r\n", vmem->vmem_id, vmem->name, be64toh(vmem->vaddr), (unsigned int) be32toh(vmem->size), vmem->type);
			printf("%s", buf);
			PyUnicode_AppendAndDel(&list_string, PyUnicode_FromString(buf));
		}
	} else {
		for (vmem_list_t * vmem = (void *) packet->data; (intptr_t) vmem < (intptr_t) packet->data + packet->length; vmem++) {
			char buf[500];
			snprintf(buf, sizeof(buf), " %u: %-5.5s 0x%08X - %u typ %u\r\n", vmem->vmem_id, vmem->name, (unsigned int) be32toh(vmem->vaddr), (unsigned int) be32toh(vmem->size), vmem->type);
			printf("%s", buf);
			PyUnicode_AppendAndDel(&list_string, PyUnicode_FromString(buf));
		}
	}

	csp_buffer_free(packet);
	csp_close(conn);

	return list_string;
}