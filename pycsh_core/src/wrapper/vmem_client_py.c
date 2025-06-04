/*
 * vmem_client_py.c
 *
 * Wrappers for lib/param/src/vmem/vmem_client_slash.c
 *
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <vmem/vmem_server.h>
#include <vmem/vmem_client.h>

#include "vmem_client_py.h"

#include <pycsh/utils.h>
#include "../csp_classes/vmem.h"

#include "../pycsh.h"

PyObject * pycsh_vmem_download(PyObject * self, PyObject * args, PyObject * kwds) {

	CSP_INIT_CHECK()

	unsigned int node = pycsh_dfl_node;
	unsigned int timeout = pycsh_dfl_timeout;
	unsigned int version = 2;

	/* RDPOPT */
	unsigned int window = 3;
	unsigned int conn_timeout = 10000;
	unsigned int packet_timeout = 5000;
	unsigned int ack_timeout = 2000;
	unsigned int ack_count = 2;
	unsigned int address = 0;
	unsigned int length = 0;

    static char *kwlist[] = {"address", "length", "node", "window", "conn_timeout", "packet_timeout", "ack_timeout", "ack_count", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "II|IIIIII:vmem_download", kwlist, &address, &length, &node, &window, &conn_timeout, &packet_timeout, &ack_timeout, &ack_count))
		return NULL;  // TypeError is thrown

	printf("Setting rdp options: %u %u %u %u %u\n", window, conn_timeout, packet_timeout, ack_timeout, ack_count);
	csp_rdp_set_opt(window, conn_timeout, packet_timeout, 1, ack_timeout, ack_count);

	printf("Downloading from: %08"PRIX32"\n", address);
	char *odata = malloc(length);

	vmem_download(node,timeout,address,length,odata,version,1);

	PyObject * vmem_data = PyBytes_FromStringAndSize(odata, length);

	free(odata);

	return vmem_data;

}

static int is_file_object(PyObject *obj) {

	assert(obj);

    PyObject *io_module AUTO_DECREF = PyImport_ImportModule("io");
    if (!io_module) {
        fprintf(stderr, "Failed to import standard Python module ´io´, assert(false)");
		assert(false);
    }

    PyObject *io_base AUTO_DECREF = PyObject_GetAttrString(io_module, "IOBase");
    if (!io_base) {
        fprintf(stderr, "Failed to import standard Python class ´io.IOBase´, assert(false)");
		assert(false);
    }

    int result = PyObject_IsInstance(obj, io_base);
    return result;
}

static PyObject* io_object_to_bytes(PyObject *file_obj) {
    
	assert(file_obj);

    PyObject *read_method AUTO_DECREF = PyObject_GetAttrString(file_obj, "read");
    if (!read_method) {
        PyErr_SetString(PyExc_TypeError, "Object does not have a read() method");
        return NULL;
    }

    PyObject *result = PyObject_CallObject(read_method, NULL);
    if (!result) {
        return NULL;  // Error occurred in calling read()
    }

    // Ensure the result is actually bytes
    if (!PyBytes_Check(result)) {
        Py_DECREF(result);
        PyErr_SetString(PyExc_TypeError, "read() did not return bytes");
        return NULL;
    }

    return result;  // Return new reference to bytes object (caller must Py_DECREF it)
}

PyObject * pycsh_vmem_upload(PyObject * self, PyObject * args, PyObject * kwds) {
	
	CSP_INIT_CHECK()

	unsigned int node = pycsh_dfl_node;
	unsigned int timeout = pycsh_dfl_timeout;
	unsigned int version = 2;

	/* RDPOPT */
	unsigned int window = 3;
	unsigned int conn_timeout = 10000;
	unsigned int packet_timeout = 5000;
	unsigned int ack_timeout = 2000;
	unsigned int ack_count = 2;
	unsigned int address = 0;
	PyObject * data_in AUTO_DECREF = NULL;

    static char *kwlist[] = {"address", "data_in", "node", "window", "conn_timeout", "packet_timeout", "ack_timeout", "ack_count", "version", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "IO|IIIIIII:vmem_upload", kwlist, &address, &data_in, &node, &window, &conn_timeout, &packet_timeout, &ack_timeout, &ack_count, &version))
		return NULL;  // TypeError is thrown

	printf("Setting rdp options: %u %u %u %u %u\n", window, conn_timeout, packet_timeout, ack_timeout, ack_count);
	csp_rdp_set_opt(window, conn_timeout, packet_timeout, 1, ack_timeout, ack_count);

	printf("Uploading from: %08"PRIX32"\n", address);
	char *idata = NULL;
	Py_ssize_t idata_len = 0;

	if (is_file_object(data_in)) {
		data_in = io_object_to_bytes(data_in);
		if (data_in == NULL) {
			return NULL;
		}
	} else {
		/* We must call Py_DECREF() on io.IOBase.read(), so ensure we can do it on the original argument too. */
		Py_INCREF(data_in);
	}

	/* `PyBytes_AsStringAndSize(...)` will raise TypeError if `data_in` is a non-bytes object. */
	if (PyBytes_AsStringAndSize(data_in, &idata, &idata_len) != 0) {
		return NULL;
	}
	assert(!PyErr_Occurred());

	if (idata_len == 0 || idata == NULL) {
		PyErr_SetString(PyExc_ValueError, "Nothing to upload");
	}
	
	if (vmem_upload(node, timeout, address, idata, idata_len, version) < 0) {
		PyErr_Format(PyExc_ConnectionError, "Upload failed (address=%d), (node=%d), (window=%d), (conn_timeout=%d), (packet_timeout=%d), (ack_timeout=%d), (ack_count=%d), (version=%d)", address, data_in, node, &window, conn_timeout, packet_timeout, ack_timeout, ack_count, version);
	}
	Py_RETURN_NONE;

}

PyObject * pycsh_param_vmem(PyObject * self, PyObject * args, PyObject * kwds) {

	CSP_INIT_CHECK()

	unsigned int node = pycsh_dfl_node;
	unsigned int timeout = pycsh_dfl_timeout;
	unsigned int version = 2;
	int verbose = pycsh_dfl_verbose;

	static char *kwlist[] = {"node", "timeout", "version", "verbose", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|IIIi:vmem", kwlist, &node, &timeout, &version, &verbose))
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

	PyObject * list_string = PyUnicode_New(0, 0);

	if (version == 3) {
		for (vmem_list3_t * vmem = (void *) packet->data; (intptr_t) vmem < (intptr_t) packet->data + packet->length; vmem++) {
			char buf[500];
			snprintf(buf, sizeof(buf), " %2u: %-16.16s 0x%016"PRIX64" - %"PRIu64" typ %u\r\n", vmem->vmem_id, vmem->name, be64toh(vmem->vaddr), be64toh(vmem->size), vmem->type);
			if (verbose >= 1) {
				printf("%s", buf);
			}
			PyUnicode_AppendAndDel(&list_string, PyUnicode_FromString(buf));
		}
	} else if (version == 2) {
		for (vmem_list2_t * vmem = (void *) packet->data; (intptr_t) vmem < (intptr_t) packet->data + packet->length; vmem++) {
			char buf[500];
			snprintf(buf, sizeof(buf), " %2u: %-5.5s 0x%016"PRIX64" - %"PRIu32" typ %u\r\n", vmem->vmem_id, vmem->name, be64toh(vmem->vaddr), be32toh(vmem->size), vmem->type);
			if (verbose >= 1) {
				printf("%s", buf);
			}
			PyUnicode_AppendAndDel(&list_string, PyUnicode_FromString(buf));
		}
	} else {
		for (vmem_list_t * vmem = (void *) packet->data; (intptr_t) vmem < (intptr_t) packet->data + packet->length; vmem++) {
			char buf[500];
			snprintf(buf, sizeof(buf), " %2u: %-5.5s 0x%08"PRIX32" - %"PRIu32" typ %u\r\n", vmem->vmem_id, vmem->name, be32toh(vmem->vaddr), be32toh(vmem->size), vmem->type);
			if (verbose >= 1) {
				printf("%s", buf);
			}
			PyUnicode_AppendAndDel(&list_string, PyUnicode_FromString(buf));
		}

	}

	csp_buffer_free(packet);

	return list_string;
}