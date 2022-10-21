/*
 * wrapper.c
 *
 * Contains CSH commands converted to Python functions.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

#include "wrapper.h"

#include <csp/csp_cmp.h>

#include <param/param.h>
#include <param/param_client.h>

#include <vmem/vmem_server.h>
#include <vmem/vmem_client.h>

#include "pycsh.h"
#include "utils.h"


PyObject * pycsh_param_get(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!csp_initialized()) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before .init() has been called.");
		return NULL;
	}

	PyObject * param_identifier;  // Raw argument object/type passed. Identify its type when needed.
	int host = INT_MIN;
	int node = default_node;
	int offset = INT_MIN;  // Using INT_MIN as the least likely value as Parameter arrays should be backwards subscriptable like lists.
	int timeout = PYCSH_DFL_TIMEOUT;
	int retries = 1;

	static char *kwlist[] = {"param_identifier", "host", "node", "offset", "timeout", "retries", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|iiiii", kwlist, &param_identifier, &host, &node, &offset, &timeout, &retries))
		return NULL;  // TypeError is thrown

	param_t *param = _pycsh_util_find_param_t(param_identifier, node);

	if (param == NULL)  // Did not find a match.
		return NULL;  // Raises TypeError or ValueError.

	// _pycsh_util_get_single() and _pycsh_util_get_array() will return NULL for exceptions, which is fine with us.
	if (param->array_size > 1 && param->type != PARAM_TYPE_STRING)
		return _pycsh_util_get_array(param, autosend, host, timeout, retries);
	return _pycsh_util_get_single(param, offset, autosend, host, timeout, retries);
}

PyObject * pycsh_param_set(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!csp_initialized()) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before .init() has been called.");
		return NULL;
	}

	PyObject * param_identifier;  // Raw argument object/type passed. Identify its type when needed.
	PyObject * value;
	int host = INT_MIN;
	int node = default_node;
	int offset = INT_MIN;  // Using INT_MIN as the least likely value as Parameter arrays should be backwards subscriptable like lists.
	int timeout = PYCSH_DFL_TIMEOUT;
	int retries = 1;

	static char *kwlist[] = {"param_identifier", "value", "host", "node", "offset", "timeout", "retries", NULL};
	
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO|iiiii", kwlist, &param_identifier, &value, &host, &node, &offset, &timeout, &retries)) {
		return NULL;  // TypeError is thrown
	}

	param_t *param = _pycsh_util_find_param_t(param_identifier, node);

	if (param == NULL)  // Did not find a match.
		return NULL;  // Raises TypeError or ValueError.

	if((PyIter_Check(value) || PySequence_Check(value)) && !PyObject_TypeCheck(value, &PyUnicode_Type)) {
		if (_pycsh_util_set_array(param, value, host, timeout, retries))
			return NULL;  // Raises one of many possible exceptions.
	} else {
		param_queue_t *usequeue = autosend ? NULL : &param_queue_set;
		if (_pycsh_util_set_single(param, value, offset, host, timeout, retries, usequeue))
			return NULL;  // Raises one of many possible exceptions.
		param_print(param, -1, NULL, 0, 2);
	}

	Py_RETURN_NONE;
}

PyObject * pycsh_param_pull(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!csp_initialized()) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before .init() has been called.");
		return NULL;
	}

	unsigned int host = 0;
	unsigned int timeout = PYCSH_DFL_TIMEOUT;
	uint32_t include_mask = 0xFFFFFFFF;
	uint32_t exclude_mask = PM_REMOTE | PM_HWREG;

	char * _str_include_mask = NULL;
	char * _str_exclude_mask = NULL;

	static char *kwlist[] = {"host", "include_mask", "exclude_mask", "timeout", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|ssI", kwlist, &host, &_str_include_mask, &_str_exclude_mask, &timeout)) {
		return NULL;
	}

	if (_str_include_mask != NULL)
		include_mask = param_maskstr_to_mask(_str_include_mask);
	if (_str_exclude_mask != NULL)
		exclude_mask = param_maskstr_to_mask(_str_exclude_mask);

	int result = -1;
	if (param_queue_get.used == 0) {
		result = param_pull_all(1, host, include_mask, exclude_mask, timeout, paramver);
	} else {
		result = param_pull_queue(&param_queue_get, 1, host, timeout);
	}

	if (result) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		return 0;
	}

	Py_RETURN_NONE;
}

PyObject * pycsh_param_push(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!csp_initialized()) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before .init() has been called.");
		return NULL;
	}

	unsigned int node = 0;
	unsigned int timeout = PYCSH_DFL_TIMEOUT;
	uint32_t hwid = 0;

	static char *kwlist[] = {"node", "timeout", "hwid", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|II", kwlist, &node, &timeout, &hwid)) {
		return NULL;  // TypeError is thrown
	}

	if (param_push_queue(&param_queue_set, 1, node, timeout, hwid) < 0) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		return 0;
	}

	Py_RETURN_NONE;
}

PyObject * pycsh_param_clear(PyObject * self, PyObject * args) {

	param_queue_get.used = 0;
	param_queue_set.used = 0;
    param_queue_get.version = paramver;
    param_queue_set.version = paramver;
	printf("Queue cleared\n");
	Py_RETURN_NONE;

}

PyObject * pycsh_param_node(PyObject * self, PyObject * args) {

	int node = -1;

	if (!PyArg_ParseTuple(args, "|i", &node)) {
		return NULL;  // TypeError is thrown
	}

	if (node == -1)
		printf("Default node = %d\n", default_node);
	else {
		default_node = node;
		printf("Set default node to %d\n", default_node);
	}

	return Py_BuildValue("i", default_node);
}

PyObject * pycsh_param_paramver(PyObject * self, PyObject * args) {

	// Not sure if the static paramver would be set to NULL, when nothing is passed.
	int _paramver = -1;  

	if (!PyArg_ParseTuple(args, "|i", &_paramver)) {
		return NULL;  // TypeError is thrown
	}

	if (_paramver == -1)
		printf("Parameter client version = %d\n", paramver);
	else {
		paramver = _paramver;
		printf("Set parameter client version to %d\n", paramver);
	}

	return Py_BuildValue("i", paramver);
}

PyObject * pycsh_param_autosend(PyObject * self, PyObject * args) {

	int _autosend = -1;

	if (!PyArg_ParseTuple(args, "|i", &_autosend)) {
		return NULL;  // TypeError is thrown
	}

	if (_autosend == -1)
		printf("autosend = %d\n", autosend);
	else {
		autosend = _autosend;
		printf("Set autosend to %d\n", autosend);
	}

	return Py_BuildValue("i", autosend);
}

PyObject * pycsh_param_queue(PyObject * self, PyObject * args) {

	if ( (param_queue_get.used == 0) && (param_queue_set.used == 0) ) {
		printf("Nothing queued\n");
	}
	if (param_queue_get.used > 0) {
		printf("Get Queue\n");
		param_queue_print(&param_queue_get);
	}
	if (param_queue_set.used > 0) {
		printf("Set Queue\n");
		param_queue_print(&param_queue_set);
	}

	Py_RETURN_NONE;

}


PyObject * pycsh_param_list(PyObject * self, PyObject * args, PyObject * kwds) {

	int node = default_node;
    int verbosity = 1;
    char * maskstr = NULL;
	char * globstr = NULL;

	static char *kwlist[] = {"node", "verbose", "mask", "globstr", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|iiss", kwlist, &node, &verbosity, &maskstr, &globstr)) {
		return NULL;
	}

	/* Interpret maskstring */
    uint32_t mask = 0xFFFFFFFF;
    if (maskstr != NULL) {
        mask = param_maskstr_to_mask(maskstr);
    }

	param_list_print(mask, node, globstr, verbosity);

	return pycsh_util_parameter_list();
}

PyObject * pycsh_param_list_download(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!csp_initialized()) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before .init() has been called.");
		return NULL;
	}

	unsigned int node;
    unsigned int timeout = PYCSH_DFL_TIMEOUT;
    unsigned int version = 2;

	static char *kwlist[] = {"node", "timeout", "version", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|II", kwlist, &node, &timeout, &version))
		return NULL;  // TypeError is thrown

	// TODO Kevin: Downloading parameters with an incorrect version, can lead to a segmentation version.
	//	Had it been easier to detect when a incorrect version is used, we would've raised an exception instead.
	if (param_list_download(node, timeout, version) < 1) {  // We assume a connection error has occurred if we don't receive any parameters.
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		return NULL;
	}

	return pycsh_util_parameter_list();

}

PyObject * pycsh_param_list_forget(PyObject * self, PyObject * args, PyObject * kwds) {

	int node = default_node;
    int verbose = 1;

	static char *kwlist[] = {"node", "verbose", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ii", kwlist, &node, &verbose))
		return NULL;  // TypeError is thrown

	int res = param_list_remove(node, verbose);
	printf("Removed %i parameters\n", res);
	return Py_BuildValue("i", res);;
}


PyObject * pycsh_csh_ping(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!csp_initialized()) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before .init() has been called.");
		return NULL;
	}

	unsigned int node;
	unsigned int timeout = PYCSH_DFL_TIMEOUT;
	unsigned int size = 1;

	static char *kwlist[] = {"node", "timeout", "size", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|II", kwlist, &node, &timeout, &size)) {
		return NULL;  // TypeError is thrown
	}

	printf("Ping node %u size %u timeout %u: ", node, size, timeout);

	int result = csp_ping(node, timeout, size, CSP_O_CRC32);

	if (result >= 0) {
		printf("Reply in %d [ms]\n", result);
	} else {
		printf("No reply\n");
	}

	return Py_BuildValue("i", result);

}

PyObject * pycsh_csh_ident(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!csp_initialized()) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before .init() has been called.");
		return NULL;
	}

	unsigned int node;
	unsigned int timeout = PYCSH_DFL_TIMEOUT;

	static char *kwlist[] = {"node", "timeout", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|I", kwlist, &node, &timeout)) {
		return NULL;  // TypeError is thrown
	}

	struct csp_cmp_message msg;
	msg.type = CSP_CMP_REQUEST;
	msg.code = CSP_CMP_IDENT;
	int size = sizeof(msg.type) + sizeof(msg.code) + sizeof(msg.ident);

	csp_conn_t * conn = csp_connect(CSP_PRIO_NORM, node, CSP_CMP, timeout, CSP_O_CRC32);
	if (conn == NULL) {
		return 0;
	}

	csp_packet_t * packet = csp_buffer_get(size);
	if (packet == NULL) {
		csp_close(conn);
		return 0;
	}

	/* Copy the request */
	memcpy(packet->data, &msg, size);
	packet->length = size;

	csp_send(conn, packet);

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

PyObject * pycsh_csh_reboot(PyObject * self, PyObject * args) {

	unsigned int node;

	if (!PyArg_ParseTuple(args, "I", &node))
		return NULL;  // Raises TypeError.

	csp_reboot(node);

	Py_RETURN_NONE;
}


PyObject * pycsh_vmem_list(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!csp_initialized()) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before .init() has been called.");
		return NULL;
	}

	int node = 0;
	int timeout = 2000;
	int version = 1;

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

PyObject * pycsh_vmem_restore(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!csp_initialized()) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before .init() has been called.");
		return NULL;
	}

	// The node argument is required (even though it has a default)
	// because Python requires that keyword/optional arguments 
	// come after positional/required arguments (vmem_id).
	int node = 0;
	int vmem_id;
	int timeout = 2000;

	static char *kwlist[] = {"node", "vmem_id", "timeout", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ii|i", kwlist, &node, &vmem_id, &timeout))
		return NULL;  // Raises TypeError.

	printf("Restoring vmem %u on node %u\n", vmem_id, node);

	int result = vmem_client_backup(node, vmem_id, timeout, 0);

	if (result == -2) {
		PyErr_SetString(PyExc_ConnectionError, "No response");
		return NULL;
	} else {
		printf("Result: %d\n", result);
	}

	return Py_BuildValue("i", result);
}

PyObject * pycsh_vmem_backup(PyObject * self, PyObject * args, PyObject * kwds) {
	
	if (!csp_initialized()) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before .init() has been called.");
		return NULL;
	}

	// The node argument is required (even though it has a default)
	// because Python requires that keyword/optional arguments 
	// come after positional/required arguments (vmem_id).
	int node = 0;
	int vmem_id;
	int timeout = 2000;

	static char *kwlist[] = {"node", "vmem_id", "timeout", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ii|i", kwlist, &node, &vmem_id, &timeout))
		return NULL;  // Raises TypeError.

	printf("Taking backup of vmem %u on node %u\n", vmem_id, node);

	int result = vmem_client_backup(node, vmem_id, timeout, 1);

	if (result == -2) {
		PyErr_SetString(PyExc_ConnectionError, "No response");
		return NULL;
	} else {
		printf("Result: %d\n", result);
	}
	
	return Py_BuildValue("i", result);
}

PyObject * pycsh_vmem_unlock(PyObject * self, PyObject * args, PyObject * kwds) {
	// TODO Kevin: This function is likely to be very short lived.
	//	As this way of unlocking the vmem is obsolete.

	if (!csp_initialized()) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before .init() has been called.");
		return NULL;
	}

	int node = 0;
	int timeout = 2000;

	static char *kwlist[] = {"node", "timeout", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i|i", kwlist, &node, &timeout))
		return NULL;  // Raises TypeError.

	/* Step 0: Prepare request */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_NONE);
	if (conn == NULL) {
		PyErr_SetString(PyExc_ConnectionError, "No response");
		return NULL;
	}

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	if (packet == NULL) {
		PyErr_SetString(PyExc_MemoryError, "Failed to get CSP buffer");
		return NULL;
	}

	vmem_request_t * request = (void *) packet->data;
	request->version = 1;
	request->type = VMEM_SERVER_UNLOCK;
	packet->length = sizeof(vmem_request_t);

	/* Step 1: Check initial unlock code */
	request->unlock.code = htobe32(0x28140360);

	csp_send(conn, packet);

	/* Step 2: Wait for verification sequence */
	if ((packet = csp_read(conn, timeout)) == NULL) {
		csp_close(conn);
		PyErr_SetString(PyExc_ConnectionError, "No response");
		return NULL;
	}

	request = (void *) packet->data;
	uint32_t sat_verification = be32toh(request->unlock.code);

	// We skip and simulate successful user input.
	// We still print the output, 
	// if for no other reason than to keep up appearances.

	printf("Verification code received: %x\n\n", (unsigned int) sat_verification);

	printf("************************************\n");
	printf("* WARNING WARNING WARNING WARNING! *\n");
	printf("* You are about to unlock the FRAM *\n");
	printf("* Please understand the risks      *\n");
	printf("* Abort now by typing CTRL + C     *\n");
	printf("************************************\n");

	/* Step 2a: Ask user to input sequence */
	printf("Type verification sequence (you have <30 seconds): \n");

	/* Step 2b: Ask for final confirmation */
	printf("Validation sequence accepted\n");

	printf("Are you sure [Y/N]?\n");

	/* Step 3: Send verification sequence */
	request->unlock.code = htobe32(sat_verification);

	csp_send(conn, packet);

	/* Step 4: Check for result */
	if ((packet = csp_read(conn, timeout)) == NULL) {
		csp_close(conn);
		PyErr_SetString(PyExc_ConnectionError, "No response");
		return NULL;
	}

	request = (void *) packet->data;
	uint32_t result = be32toh(request->unlock.code);
	printf("Result: %x\n", (unsigned int) result);

	csp_close(conn);
	return Py_BuildValue("i", result);
}
