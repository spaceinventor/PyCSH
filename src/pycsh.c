/*
 * pycsh.c
 *
 * Setup and initialization for the PyCSH module.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

// It is recommended to always define PY_SSIZE_T_CLEAN before including Python.h
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/utsname.h>

#include <param/param.h>
#include <param/param_collector.h>
#include <param/param_queue.h>
#include <param/param_list.h>
#include <param/param_server.h>
#include <param/param_client.h>

#include <vmem/vmem_server.h>
#include <vmem/vmem_client.h>
#include <vmem/vmem_ram.h>
#include <vmem/vmem_file.h>

#include <csp/csp.h>
#include <csp/csp_yaml.h>
#include <csp/csp_cmp.h>
#include <pthread.h>
#include <csp/interfaces/csp_if_can.h>
#include <csp/interfaces/csp_if_kiss.h>
#include <csp/interfaces/csp_if_udp.h>
#include <csp/interfaces/csp_if_zmqhub.h>
#include <csp/drivers/usart.h>
#include <csp/drivers/can_socketcan.h>

#include <sys/types.h>

#include "lib/param/src/param/param_string.h"
#include "csh/prometheus.h"
#include "csh/param_sniffer.h"

#include "parameter/parameter.h"
#include "parameter/parameterarray.h"
#include "parameter/parameterlist.h"
#include "wrapper.h"
#include "utils.h"


#define PARAMID_CSP_RTABLE					 12

// Here be forward declerations.
//static PyTypeObject ParameterType;
//static PyTypeObject ParameterArrayType;
//static PyTypeObject ParameterListType;

VMEM_DEFINE_FILE(csp, "csp", "cspcnf.vmem", 120);
VMEM_DEFINE_FILE(params, "param", "params.csv", 50000);
VMEM_DEFINE_FILE(col, "col", "colcnf.vmem", 120);
VMEM_DEFINE_FILE(dummy, "dummy", "dummy.txt", 1000000);

void * param_collector_task(void * param) {
	param_collector_loop(param);
	return NULL;
}

void * router_task(void * param) {
	while(1) {
		csp_route_work();
	}
}

void * vmem_server_task(void * param) {
	vmem_server_loop(param);
	return NULL;
}

PARAM_DEFINE_STATIC_VMEM(PARAMID_CSP_RTABLE,      csp_rtable,        PARAM_TYPE_STRING, 64, 0, PM_SYSCONF, NULL, "", csp, 0, NULL);


// We include this parameter when testing the behavior of arrays, as none would exist otherwise.
uint8_t _test_array[] = {1,2,3,4,5,6,7,8};
PARAM_DEFINE_STATIC_RAM(1001, test_array_param,          PARAM_TYPE_UINT8,  8, sizeof(uint8_t),  PM_DEBUG, NULL, "", _test_array, "Parameter to use when testing arrays.");

static char _test_str[80];
PARAM_DEFINE_STATIC_RAM(1002, test_str,          PARAM_TYPE_STRING,  80, 1,  PM_DEBUG, NULL, "", _test_str, "Parameter to use when testing strings");


// Keep track of whether init has been run,
// to prevent unexpected behavior from running relevant functions first.
static uint8_t _csp_initialized = 0;

uint8_t csp_initialized() {
	return _csp_initialized;
}

param_queue_t param_queue_set = { };
param_queue_t param_queue_get = { };
int default_node = -1;
int autosend = 1;
int paramver = 2;

// TODO Kevin: It's probably not safe to call this function consecutively with the same std_stream or stream_buf.
static int _handle_stream(PyObject * stream_identifier, FILE **std_stream, FILE *stream_buf) {
	if (!stream_identifier)
		return 0;

	if (PyLong_Check(stream_identifier)) {
		long int val = PyLong_AsLong(stream_identifier);
		switch (val) {
			case -2:  // STDOUT
				break;  // Default behavior is correct, don't do anything.
			case -3:  // DEVNULL
				// fclose(stdout);
				if ((stream_buf = fopen("/dev/null", "w")) == NULL) {
					char buf[150];
					snprintf(buf, 150, "Impossible error! Can't open /dev/null: %s\n", strerror(errno));
					PyErr_SetString(PyExc_IOError, buf);
					return -1;
				}
				*std_stream = stream_buf;
				break;
			default:
				PyErr_SetString(PyExc_ValueError, "Argument should be either -2 for subprocess.STDOUT, -3 for subprocess.DEVNULL or a string to a file.");
				return -2;
		}
	} else if (PyUnicode_Check(stream_identifier)) {
		const char *filename = PyUnicode_AsUTF8(stream_identifier);

		if (stream_buf != NULL)
			fclose(stream_buf);

		if ((stream_buf = freopen(filename, "w+", *std_stream)) == NULL) {
			char buf[30 + strlen(filename)];
			sprintf(buf, "Failed to open file: %s", filename);
			PyErr_SetString(PyExc_IOError, buf);
			return -3;
		}
		// std_stream = stream_buf;
	} else {
		PyErr_SetString(PyExc_TypeError, "Argument should be either -2 for subprocess.STDOUT, -3 for subprocess.DEVNULL or a string to a file.");
		return -4;
	}
	return 0;
}

static PyObject * pycsh_init(PyObject * self, PyObject * args, PyObject *kwds) {

	if (_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot initialize multiple instances of libparam bindings. Please use a previous binding.");
		return NULL;
	}

	static char *kwlist[] = {
		"csp_version", "csp_hostname", "csp_model", 
		"use_prometheus", "rtable", "yamlname", "dfl_addr", "stdout", "stderr", NULL,
	};

	static struct utsname info;
	uname(&info);

	csp_conf.hostname = "python_bindings";
	csp_conf.model = info.version;
	csp_conf.revision = info.release;
	csp_conf.version = 2;
	csp_conf.dedup = CSP_DEDUP_OFF;

	int use_prometheus = 0;
	char * rtable = NULL;
	char * yamlname = "csh.yaml";
	char * dirname = getenv("HOME");
	unsigned int dfl_addr = 0;
	
	PyObject *csh_stdout = NULL;
	PyObject *csh_stderr = NULL;
	

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|BssissIOO:init", kwlist,
		&csp_conf.version,  &csp_conf.hostname, 
		&csp_conf.model, &use_prometheus, &rtable,
		&yamlname, &dfl_addr, &csh_stdout, &csh_stderr)
	)
		return NULL;  // TypeError is thrown

	if (strcmp(yamlname, "csh.yaml"))
		dirname = "";

	// TODO Kevin: Support reassigning streams through module function or global.
	static FILE *temp_stdout_fp = NULL;
	static FILE *temp_stderr_fp = NULL;

	if (
		_handle_stream(csh_stdout, &stdout, temp_stdout_fp) != 0 ||
		_handle_stream(csh_stderr, &stderr, temp_stderr_fp) != 0
	) {
		return NULL;
	}

	srand(time(NULL));

	void serial_init(void);
	serial_init();

	/* Parameters */
	vmem_file_init(&vmem_params);
	param_list_store_vmem_load(&vmem_params);

	csp_init();

	if (strlen(dirname)) {
		char buildpath[100];
		snprintf(buildpath, 100, "%s/%s", dirname, yamlname);
		csp_yaml_init(buildpath, &dfl_addr);
	} else {
		csp_yaml_init(yamlname, &dfl_addr);

	}

	csp_rdp_set_opt(3, 10000, 5000, 1, 2000, 2);

#if (CSP_HAVE_STDIO)
	if (rtable && csp_rtable_check(rtable)) {
		int error = csp_rtable_load(rtable);
		if (error < 1) {
			printf("csp_rtable_load(%s) failed, error: %d\n", rtable, error);
		}
	}
#endif
	(void) rtable;

	csp_bind_callback(csp_service_handler, CSP_ANY);
	csp_bind_callback(param_serve, PARAM_PORT_SERVER);

	vmem_file_init(&vmem_dummy);

	/* Start a collector task */
	vmem_file_init(&vmem_col);

	static pthread_t param_collector_handle;
	pthread_create(&param_collector_handle, NULL, &param_collector_task, NULL);

	static pthread_t router_handle;
	pthread_create(&router_handle, NULL, &router_task, NULL);

	static pthread_t vmem_server_handle;
	pthread_create(&vmem_server_handle, NULL, &vmem_server_task, NULL);

	if (use_prometheus) {
		prometheus_init();
		param_sniffer_init();
	}
	
	_csp_initialized = 1;
	Py_RETURN_NONE;

}


static PyMethodDef methods[] = {

	/* Converted CSH commands from libparam/src/param/param_slash.c */
	{"get", 		(PyCFunction)pycsh_param_get, 	METH_VARARGS | METH_KEYWORDS, "Set the value of a parameter."},
	{"set", 		(PyCFunction)pycsh_param_set, 	METH_VARARGS | METH_KEYWORDS, "Get the value of a parameter."},
	{"push", 		(PyCFunction)pycsh_param_push,	METH_VARARGS | METH_KEYWORDS, "Push the current queue."},
	{"pull", 		(PyCFunction)pycsh_param_pull,	METH_VARARGS | METH_KEYWORDS, "Pull all or a specific set of parameters."},
	{"clear", 		pycsh_param_clear, 			  	METH_NOARGS, 				  "Clears the queue."},
	{"node", 		pycsh_param_node, 			  	METH_VARARGS, 				  "Used to get or change the default node."},
	{"paramver", 	pycsh_param_paramver, 		  	METH_VARARGS, 				  "Used to get or change the parameter version."},
	{"autosend", 	pycsh_param_autosend, 		  	METH_VARARGS, 				  "Used to get or change whether autosend is enabled."},
	{"queue", 		pycsh_param_queue,			  	METH_NOARGS, 				  "Print the current status of the queue."},

	/* Converted CSH commands from libparam/src/param/list/param_list_slash.c */
	{"list", 		pycsh_param_list,			  	METH_VARARGS, 				  "List all known parameters."},
	{"list_download", (PyCFunction)pycsh_param_list_download, METH_VARARGS | METH_KEYWORDS, "Download all parameters on the specified node."},
	{"list_forget", (PyCFunction)pycsh_param_list_forget, METH_VARARGS | METH_KEYWORDS, "Remove remote parameters, matching the provided arguments, from the global list."},
	{"list_save", 	pycsh_param_list_save, 		  	METH_VARARGS, 				  "Save a list of parameters to a file."},
	{"list_load", 	pycsh_param_list_load, 		  	METH_VARARGS, 				  "Load a list of parameters from a file."},

	/* Converted CSH commands from csh/src/slash_csp.c */
	{"ping", 		(PyCFunction)pycsh_csh_ping, 	METH_VARARGS | METH_KEYWORDS, "Ping the specified node."},
	{"ident", 		(PyCFunction)pycsh_csh_ident,	METH_VARARGS | METH_KEYWORDS, "Print the identity of the specified node."},
	{"reboot", 		pycsh_csh_reboot, 			 	METH_VARARGS, 				  "Reboot the specified node."},

	/* Utility functions */
	{"get_type", 	pycsh_util_get_type, 		  	METH_VARARGS, 				  "Gets the type of the specified parameter."},

	/* Converted vmem commands from libparam/src/vmem/vmem_client_slash.c */
	{"vmem_list", 	(PyCFunction)pycsh_vmem_list,   METH_VARARGS | METH_KEYWORDS, "Builds a string of the vmem at the specified node."},
	{"vmem_restore",(PyCFunction)pycsh_vmem_restore,METH_VARARGS | METH_KEYWORDS, "Restore the configuration on the specified node."},
	{"vmem_backup", (PyCFunction)pycsh_vmem_backup, METH_VARARGS | METH_KEYWORDS, "Back up the configuration on the specified node."},
	{"vmem_unlock", (PyCFunction)pycsh_vmem_unlock, METH_VARARGS | METH_KEYWORDS, "Unlock the vmem on the specified node, such that it may be changed by a backup (for example)."},

	/* Misc */
	{"init", (PyCFunction)pycsh_init, 				METH_VARARGS | METH_KEYWORDS, "Initializes the module, with the provided settings."},

	/* sentinel */
	{NULL, NULL, 0, NULL}};

static struct PyModuleDef moduledef = {
	PyModuleDef_HEAD_INIT,
	"pycsh",
	"Bindings primarily dedicated to the CSH shell interface commands",
	-1,
	methods,
	NULL,
	NULL,
	NULL,
	NULL};

PyMODINIT_FUNC PyInit_pycsh(void) {

	if (PyType_Ready(&ParameterType) < 0)
        return NULL;

	if (PyType_Ready(&ParameterArrayType) < 0)
        return NULL;

	ParameterListType.tp_base = &PyList_Type;
	if (PyType_Ready(&ParameterListType) < 0)
		return NULL;

	PyObject * m = PyModule_Create(&moduledef);
	if (m == NULL)
		return NULL;

	Py_INCREF(&ParameterType);
	if (PyModule_AddObject(m, "Parameter", (PyObject *) &ParameterType) < 0) {
		Py_DECREF(&ParameterType);
        Py_DECREF(m);
        return NULL;
	}

	Py_INCREF(&ParameterArrayType);
	if (PyModule_AddObject(m, "ParameterArray", (PyObject *) &ParameterArrayType) < 0) {
		Py_DECREF(&ParameterArrayType);
        Py_DECREF(m);
        return NULL;
	}

	Py_INCREF(&ParameterListType);
	if (PyModule_AddObject(m, "ParameterList", (PyObject *)&ParameterListType) < 0) {
		Py_DECREF(&ParameterListType);
		Py_DECREF(m);
		return NULL;
	}

	return m;
}
