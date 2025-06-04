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

#include "pycsh.h"
#include "pycshconfig.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/utsname.h>

#include <param/param.h>
#include <slash/slash.h>

#include <csp/csp.h>
#include <csp/csp_yaml.h>
#include <csp/csp_cmp.h>
#include <csp/arch/csp_time.h>
#include <csp/csp_hooks.h>
#include <pthread.h>
#include <csp/interfaces/csp_if_can.h>
#include <csp/interfaces/csp_if_kiss.h>
#include <csp/interfaces/csp_if_udp.h>
#include <csp/interfaces/csp_if_zmqhub.h>
#include <csp/drivers/usart.h>
#include <csp/drivers/can_socketcan.h>

#include <sys/types.h>

#include <pycsh/utils.h>

#include <pycsh/parameter.h>
#include <pycsh/pythonparameter.h>
#include "parameter/parameterarray.h"
#include "parameter/pythonarrayparameter.h"
#include "parameter/pythongetsetparameter.h"
#include "parameter/pythongetsetarrayparameter.h"
#include "parameter/parameterlist.h"

#include "csp_classes/ident.h"
#include "csp_classes/ifstat.h"
#include "csp_classes/vmem.h"

#include "slash_command/slash_command.h"
#include "slash_command/python_slash_command.h"

#include "wrapper/py_csp.h"
#include "wrapper/param_py.h"
#include "wrapper/slash_py.h"
#include "wrapper/dflopt_py.h"
#include "wrapper/spaceboot_py.h"
#include "wrapper/csp_init_py.h"
#include "wrapper/param_list_py.h"
#include "wrapper/vmem_client_py.h"
#include "wrapper/victoria_metrics_py.h"

extern const char *pycsh_version_string;

/* Assertions used when parsing Python arguments, i.e int -> uint32_t */
static_assert(sizeof(unsigned int) == sizeof(uint32_t));

// We include this parameter when testing the behavior of arrays, as none would exist otherwise.
uint8_t _test_array[] = {1,2,3,4,5,6,7,8};
PARAM_DEFINE_STATIC_RAM(1001, test_array_param,          PARAM_TYPE_UINT8,  8, sizeof(uint8_t),  PM_DEBUG, NULL, "", _test_array, "Parameter to use when testing arrays.");

static char _test_str[80];
PARAM_DEFINE_STATIC_RAM(1002, test_str,          PARAM_TYPE_STRING,  80, 1,  PM_DEBUG, NULL, "", _test_str, "Parameter to use when testing strings");




unsigned int pycsh_dfl_verbose = -1;

void * onehz_task(void * param) {
	while(1) {
#ifdef PARAM_HAVE_SCHEDULER
		csp_timestamp_t scheduler_time = {};
        csp_clock_get_time(&scheduler_time);
        param_schedule_server_update(scheduler_time.tv_sec * 1E9 + scheduler_time.tv_nsec);
#endif
		// Py_BEGIN_ALLOW_THREADS;
		sleep(1);
		// Py_END_ALLOW_THREADS;
	}
	return NULL;
}

// TODO Kevin: It's probably not safe to call this function consecutively with the same std_stream or stream_buf.
static int _handle_stream(PyObject * stream_identifier, FILE **std_stream, FILE *stream_buf) {
	if (stream_identifier == NULL)
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

	assert(self);  // Assert that Python has called PyInit_Pycsh()

	// Suppress the "comparison will always evaluate as ‘false’" warning
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Waddress"

	/* It seems that we (PyCSH) cannot control the symbol visibility of libparam (static libraries),
		at least using linking/compile options.
		This is unfortunate as libparams symbols are also considered part of the API that PyCSH exposes.
		It is therefore only possible to ensure the presence of library symbols by also using them in PyCSH itself,
		to prevent them from being optimized out.
		Many thanks to JB for helping sort this out! :) */
	/* param_command_rm() is not in a public header. But it's probably okay to declare it here,
		since we only care about the name, not the full signature. */
	int param_command_rm(int server, int verbose, char command_name[], int timeout);
	if (param_command_rm == NULL) {
		fprintf(stderr, "param_command_rm was optimized out\n");
		exit(1);
		assert(false);
		return NULL;
	}

	// Re-enable the warning
    #pragma GCC diagnostic pop

	// if (_csp_initialized) {
	// 	PyErr_SetString(PyExc_RuntimeError,
	// 		"Cannot initialize multiple instances of libparam bindings. Please use a previous binding.");
	// 	return NULL;
	// }

	static char *kwlist[] = {
		"quiet", "stdout", "stderr", NULL,
	};

	int quiet = 0;
	PyObject *csh_stdout = NULL;
	PyObject *csh_stderr = NULL;
	

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|iOO:init", kwlist, &quiet, &csh_stdout, &csh_stderr))
		return NULL;  // TypeError is thrown

	// TODO Kevin: Support reassigning streams through module function or global.
	static FILE * temp_stdout_fd = NULL;
	static FILE * temp_stderr_fd = NULL;

	if (quiet) {
		if ((temp_stdout_fd = fopen("/dev/null", "w")) == NULL) {
			fprintf(stderr, "Impossible error! Can't open /dev/null: %s\n", strerror(errno));
			exit(1);
		}
		stdout = temp_stdout_fd;
	}
	else if (
		_handle_stream(csh_stdout, &stdout, temp_stdout_fd) != 0 ||
		_handle_stream(csh_stderr, &stderr, temp_stderr_fd) != 0
	) {
		return NULL;
	}
	
	/* Return singleton here for now, eventually we would want to do something akin to:
	`return PyModule_Create(&moduledef);` */
	return Py_NewRef(self);
}


static PyMethodDef methods[] = {

	/* Converted CSH commands from libparam/src/param/param_slash.c */
	{"get", 		(PyCFunction)pycsh_param_get, 	METH_VARARGS | METH_KEYWORDS, "Set the value of a parameter."},
	{"set", 		(PyCFunction)pycsh_param_set, 	METH_VARARGS | METH_KEYWORDS, "Get the value of a parameter."},
	// {"push", 		(PyCFunction)pycsh_param_push,	METH_VARARGS | METH_KEYWORDS, "Push the current queue."},
	{"pull", 		(PyCFunction)pycsh_param_pull,	METH_VARARGS | METH_KEYWORDS, "Pull all or a specific set of parameters."},
	{"cmd_done", 	pycsh_param_cmd_done, 			METH_NOARGS, 				  "Clears the queue."},
	{"cmd_new", 	(PyCFunction)pycsh_param_cmd_new,METH_VARARGS | METH_KEYWORDS,"Create a new command"},
	{"node", 		pycsh_slash_node, 			  	METH_VARARGS, 				  "Used to get or change the default node."},
	{"timeout", 	(PyCFunction)pycsh_slash_timeout,METH_VARARGS | METH_KEYWORDS,"Used to get or change the default timeout."},
	{"verbose", 	pycsh_slash_verbose, 			METH_VARARGS, 		  		  "Used to get or change the default parameter verbosity."},
	{"queue", 		pycsh_param_cmd,			  	METH_NOARGS, 				  "Print the current command."},

	/* Converted CSH commands from libparam/src/param/list/param_list_slash.c */
	{"list", 		(PyCFunction)pycsh_param_list,		      METH_VARARGS | METH_KEYWORDS, "List all known parameters."},
	{"list_download",(PyCFunction)pycsh_param_list_download,  METH_VARARGS | METH_KEYWORDS, "Download all parameters on the specified node."},
	{"list_forget", (PyCFunction)pycsh_param_list_forget, 	  METH_VARARGS | METH_KEYWORDS, "Remove remote parameters, matching the provided arguments, from the global list."},
	{"list_save", 	(PyCFunction)pycsh_param_list_save, 	  METH_VARARGS | METH_KEYWORDS, "Save a list of parameters to a file."},
	{"list_add", 	(PyCFunction)pycsh_param_list_add, 	      METH_VARARGS | METH_KEYWORDS, "Add a paramter to the global list."},

	// {"list_load", 	pycsh_param_list_load, 		  	METH_VARARGS, 				  "Load a list of parameters from a file."},

	/* Converted CSH commands from csh/src/slash_csp.c */
	{"ping", 		(PyCFunction)pycsh_slash_ping, 	METH_VARARGS | METH_KEYWORDS, "Ping the specified node."},
	{"ident", 		(PyCFunction)pycsh_slash_ident,	METH_VARARGS | METH_KEYWORDS, "Print the identity of the specified node."},
	{"uptime", 		(PyCFunction)pycsh_csp_cmp_uptime,	METH_VARARGS | METH_KEYWORDS, "Return uptime information of the specified node."},
	{"ifstat", 		(PyCFunction)pycsh_csp_cmp_ifstat,	METH_VARARGS | METH_KEYWORDS, "Return information about the specified interface."},
	{"reboot", 		pycsh_slash_reboot, 			 	METH_VARARGS, 				  "Reboot the specified node."},

	/* Utility functions */
	{"get_type", 	pycsh_util_get_type, 		  	METH_VARARGS, 				  "Gets the type of the specified parameter."},
	{"slash_execute", (PyCFunction)pycsh_slash_execute, 			METH_VARARGS | METH_KEYWORDS, "Execute string as a slash command. Used to run .csh scripts"},

	/* Converted vmem commands from libparam/src/vmem/vmem_client_slash.c */
	{"vmem", 	(PyCFunction)pycsh_param_vmem,   METH_VARARGS | METH_KEYWORDS, "Builds a string of the vmem at the specified node."},

	/* Converted vmem commands from libparam/src/vmem/vmem_client.c */
	{"vmem_download", (PyCFunction)pycsh_vmem_download,   METH_VARARGS | METH_KEYWORDS, "Download a vmem area."},
	{"vmem_upload", (PyCFunction)pycsh_vmem_upload,   METH_VARARGS | METH_KEYWORDS, "Upload data to a vmem area."},

	/* Converted program/reboot commands from csh/src/spaceboot_slash.c */
	{"switch", 	(PyCFunction)slash_csp_switch,   METH_VARARGS | METH_KEYWORDS, "Reboot into the specified firmware slot."},
	{"program", (PyCFunction)pycsh_csh_program,  METH_VARARGS | METH_KEYWORDS, "Upload new firmware to a module."},
	{"sps", 	(PyCFunction)slash_sps,   		 METH_VARARGS | METH_KEYWORDS, "Switch -> Program -> Switch"},

	/* Wrappers for src/csp_init_cmd.c */
	{"csp_init", 	(PyCFunction)pycsh_csh_csp_init,   METH_VARARGS | METH_KEYWORDS, "Initialize CSP"},
	{"csp_add_zmq", (PyCFunction)pycsh_csh_csp_ifadd_zmq,   METH_VARARGS | METH_KEYWORDS, "Add a new ZMQ interface"},
	{"csp_add_kiss",(PyCFunction)pycsh_csh_csp_ifadd_kiss,   METH_VARARGS | METH_KEYWORDS, "Add a new KISS/UART interface"},
#if (CSP_HAVE_LIBSOCKETCAN)
	{"csp_add_can", (PyCFunction)pycsh_csh_csp_ifadd_can,   METH_VARARGS | METH_KEYWORDS, "Add a new UDP interface"},
#endif
	{"csp_add_eth", (PyCFunction)pycsh_csh_csp_ifadd_eth,   METH_VARARGS | METH_KEYWORDS, "Add a new ethernet interface"},
	{"csp_add_udp", (PyCFunction)pycsh_csh_csp_ifadd_udp,   METH_VARARGS | METH_KEYWORDS, "Add a new UDP interface"},
	{"csp_add_tun", (PyCFunction)pycsh_csh_csp_ifadd_tun,   METH_VARARGS | METH_KEYWORDS, "Add a new TUN interface"},

	{"csp_add_route", (PyCFunction)pycsh_csh_csp_routeadd_cmd,   METH_VARARGS | METH_KEYWORDS, "Add a new route"},

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

	/* AUTO_DECREF for exception handling */
	PyObject * pycsh AUTO_DECREF = PyModule_Create(&moduledef);
	if (pycsh == NULL)
		return NULL;

    #define PyModule_AddObject_ErrCheck(_mod, _name, _obj)                                                      \
        if (NULL == _obj) {                                                                                     \
            if (PyErr_Occurred()) {                                                                             \
                _PyErr_FormatFromCause(PyExc_ImportError, "%s is NULL", _name);                                 \
            } else {                                                                                            \
                PyErr_Format(PyExc_ImportError, "%s is NULL", _name);                                           \
            }                                                                                                   \
            return NULL;                                                                                        \
        }                                                                                                       \
        if (PyModule_AddObject(_mod, _name, _obj) < 0) {                                                        \
            _PyErr_FormatFromCause(PyExc_ImportError, "Failed to add %s to %s", _name, _mod->ob_type->tp_name); \
            return NULL;                                                                                        \
        }

	{  /* Exceptions */
		PyExc_ProgramDiffError = PyErr_NewExceptionWithDoc("pycsh.ProgramDiffError", 
			"Raised when a difference is detected between uploaded/downloaded data after programming.\n"
			"Must be caught before ConnectionError() baseclass.",
			PyExc_ConnectionError, NULL);
		PyModule_AddObject_ErrCheck(pycsh, "ProgramDiffError", PyExc_ProgramDiffError);

		PyExc_ParamCallbackError = PyErr_NewExceptionWithDoc("pycsh.ParamCallbackError", 
			"Raised and chains unto exceptions raised in the callbacks of PythonParameters.\n"
			"Must be caught before RuntimeError() baseclass.",
			PyExc_RuntimeError, NULL);
		PyModule_AddObject_ErrCheck(pycsh, "ParamCallbackError", PyExc_ParamCallbackError);

		PyExc_InvalidParameterTypeError = PyErr_NewExceptionWithDoc("pycsh.InvalidParameterTypeError", 
			"Raised when attempting to create a new PythonParameter() with an invalid type.\n"
			"Must be caught before ValueError() baseclass.",
			PyExc_ValueError, NULL);
		PyModule_AddObject_ErrCheck(pycsh, "ParamCallbackError", PyExc_InvalidParameterTypeError);
	}

	if (PyModule_AddType(pycsh, &ParameterType) < 0) {
        return NULL;
	}

	if (PyModule_AddType(pycsh, &ParameterArrayType) < 0) {
        return NULL;
	}

	if (PyModule_AddType(pycsh, &PythonParameterType) < 0) {
        return NULL;
	}

	/* PythonArrayParameterType must be created dynamically after
		ParameterArrayType and PythonParameterType to support multiple inheritance. */
	if (create_pythonarrayparameter_type() == NULL) {
		return NULL;
	}
    if (PyModule_AddType(pycsh, PythonArrayParameterType) < 0) {
        return NULL;
	}

	if (PyModule_AddType(pycsh, &PythonGetSetParameterType) < 0) {
        return NULL;
	}

	/* PythonArrayParameterType must be created dynamically after
		ParameterArrayType and PythonParameterType to support multiple inheritance. */
	if (create_pythongetsetarrayparameter_type() == NULL) {
		return NULL;
	}
    if (PyModule_AddType(pycsh, PythonGetSetArrayParameterType) < 0) {
        return NULL;
	}

	//ParameterListType.tp_base = &PyList_Type;
	if (PyModule_AddType(pycsh, &ParameterListType) < 0) {
		return NULL;
	}


	if (PyModule_AddType(pycsh, &IdentType) < 0) {
        return NULL;
	}

	if (PyModule_AddType(pycsh, &IfstatType) < 0) {
        return NULL;
	}

	if (PyModule_AddType(pycsh, &VmemType) < 0) {
        return NULL;
	}

	if (PyModule_AddType(pycsh, &SlashCommandType) < 0) {
        return NULL;
	}

	if (PyModule_AddType(pycsh, &PythonSlashCommandType) < 0) {
        return NULL;
	}

	{ /* Constants */

		/* Version Control */
		PyModule_AddObject_ErrCheck(pycsh, "VERSION", PyUnicode_FromString(pycsh_version_string));
		PyModule_AddObject_ErrCheck(pycsh, "COMPILE_DATE", PyUnicode_FromString(__DATE__));
		PyModule_AddObject_ErrCheck(pycsh, "COMPILE_DATETIME", pycsh_ident_time_to_datetime(__DATE__, __TIME__));

		/* Param Type Enums */
		PyModule_AddObject_ErrCheck(pycsh, "PARAM_TYPE_UINT8", PyLong_FromLong(PARAM_TYPE_UINT8));
		PyModule_AddObject_ErrCheck(pycsh, "PARAM_TYPE_UINT16", PyLong_FromLong(PARAM_TYPE_UINT16));
		PyModule_AddObject_ErrCheck(pycsh, "PARAM_TYPE_UINT32", PyLong_FromLong(PARAM_TYPE_UINT32));
		PyModule_AddObject_ErrCheck(pycsh, "PARAM_TYPE_UINT64", PyLong_FromLong(PARAM_TYPE_UINT64));
		PyModule_AddObject_ErrCheck(pycsh, "PARAM_TYPE_INT8", PyLong_FromLong(PARAM_TYPE_INT8));
		PyModule_AddObject_ErrCheck(pycsh, "PARAM_TYPE_INT16", PyLong_FromLong(PARAM_TYPE_INT16));
		PyModule_AddObject_ErrCheck(pycsh, "PARAM_TYPE_INT32", PyLong_FromLong(PARAM_TYPE_INT32));
		PyModule_AddObject_ErrCheck(pycsh, "PARAM_TYPE_INT64", PyLong_FromLong(PARAM_TYPE_INT64));
		PyModule_AddObject_ErrCheck(pycsh, "PARAM_TYPE_XINT8", PyLong_FromLong(PARAM_TYPE_XINT8));
		PyModule_AddObject_ErrCheck(pycsh, "PARAM_TYPE_XINT16", PyLong_FromLong(PARAM_TYPE_XINT16));
		PyModule_AddObject_ErrCheck(pycsh, "PARAM_TYPE_XINT32", PyLong_FromLong(PARAM_TYPE_XINT32));
		PyModule_AddObject_ErrCheck(pycsh, "PARAM_TYPE_XINT64", PyLong_FromLong(PARAM_TYPE_XINT64));
		PyModule_AddObject_ErrCheck(pycsh, "PARAM_TYPE_FLOAT", PyLong_FromLong(PARAM_TYPE_FLOAT));
		PyModule_AddObject_ErrCheck(pycsh, "PARAM_TYPE_DOUBLE", PyLong_FromLong(PARAM_TYPE_DOUBLE));
		PyModule_AddObject_ErrCheck(pycsh, "PARAM_TYPE_STRING", PyLong_FromLong(PARAM_TYPE_STRING));
		PyModule_AddObject_ErrCheck(pycsh, "PARAM_TYPE_DATA", PyLong_FromLong(PARAM_TYPE_DATA));
		PyModule_AddObject_ErrCheck(pycsh, "PARAM_TYPE_INVALID", PyLong_FromLong(PARAM_TYPE_INVALID));

		/* Param Mask Enums */
		PyModule_AddObject_ErrCheck(pycsh, "PM_READONLY", PyLong_FromLong(PM_READONLY));
		PyModule_AddObject_ErrCheck(pycsh, "PM_REMOTE", PyLong_FromLong(PM_REMOTE));
		PyModule_AddObject_ErrCheck(pycsh, "PM_CONF", PyLong_FromLong(PM_CONF));
		PyModule_AddObject_ErrCheck(pycsh, "PM_TELEM", PyLong_FromLong(PM_TELEM));
		PyModule_AddObject_ErrCheck(pycsh, "PM_HWREG", PyLong_FromLong(PM_HWREG));
		PyModule_AddObject_ErrCheck(pycsh, "PM_ERRCNT", PyLong_FromLong(PM_ERRCNT));
		PyModule_AddObject_ErrCheck(pycsh, "PM_SYSINFO", PyLong_FromLong(PM_SYSINFO));
		PyModule_AddObject_ErrCheck(pycsh, "PM_SYSCONF", PyLong_FromLong(PM_SYSCONF));
		PyModule_AddObject_ErrCheck(pycsh, "PM_WDT", PyLong_FromLong(PM_WDT));
		PyModule_AddObject_ErrCheck(pycsh, "PM_DEBUG", PyLong_FromLong(PM_DEBUG));
		PyModule_AddObject_ErrCheck(pycsh, "PM_CALIB", PyLong_FromLong(PM_CALIB));
		PyModule_AddObject_ErrCheck(pycsh, "PM_ATOMIC_WRITE", PyLong_FromLong(PM_ATOMIC_WRITE));
		PyModule_AddObject_ErrCheck(pycsh, "PM_PRIO1", PyLong_FromLong(PM_PRIO1));
		PyModule_AddObject_ErrCheck(pycsh, "PM_PRIO2", PyLong_FromLong(PM_PRIO2));
		PyModule_AddObject_ErrCheck(pycsh, "PM_PRIO3", PyLong_FromLong(PM_PRIO3));
		PyModule_AddObject_ErrCheck(pycsh, "PM_PRIO_MASK", PyLong_FromLong(PM_PRIO_MASK));

		/* (Param) VMEM types */
		PyModule_AddObject_ErrCheck(pycsh, "VMEM_TYPE_RAM", PyLong_FromLong(VMEM_TYPE_RAM));
		PyModule_AddObject_ErrCheck(pycsh, "VMEM_TYPE_FRAM", PyLong_FromLong(VMEM_TYPE_FRAM));
		PyModule_AddObject_ErrCheck(pycsh, "VMEM_TYPE_FRAM_SECURE", PyLong_FromLong(VMEM_TYPE_FRAM_SECURE));
		PyModule_AddObject_ErrCheck(pycsh, "VMEM_TYPE_FLASH", PyLong_FromLong(VMEM_TYPE_FLASH));
		PyModule_AddObject_ErrCheck(pycsh, "VMEM_TYPE_DRIVER", PyLong_FromLong(VMEM_TYPE_DRIVER));
		PyModule_AddObject_ErrCheck(pycsh, "VMEM_TYPE_FLASH_QSPI", PyLong_FromLong(VMEM_TYPE_FLASH_QSPI));
		PyModule_AddObject_ErrCheck(pycsh, "VMEM_TYPE_FILE", PyLong_FromLong(VMEM_TYPE_FILE));
		PyModule_AddObject_ErrCheck(pycsh, "VMEM_TYPE_FRAM_CACHE", PyLong_FromLong(VMEM_TYPE_FRAM_CACHE));
		PyModule_AddObject_ErrCheck(pycsh, "VMEM_TYPE_NOR_FLASH", PyLong_FromLong(VMEM_TYPE_NOR_FLASH));
		PyModule_AddObject_ErrCheck(pycsh, "VMEM_TYPE_BLOCK", PyLong_FromLong(VMEM_TYPE_BLOCK));
		/* Users should probably be a bit careful comparing against VMEM_TYPE_UNKNOWN (-1),
			since parameters could in theory have unknown types other than -1. */
        PyModule_AddObject_ErrCheck(pycsh, "VMEM_TYPE_UNKNOWN", PyLong_FromLong(VMEM_TYPE_UNKNOWN));

        PyModule_AddObject_ErrCheck(pycsh, "CSP_NO_VIA_ADDRESS", PyLong_FromLong(CSP_NO_VIA_ADDRESS));
        
		// TODO Kevin: We should probably add constants for SLASH_SUCCESS and such
	}

    #undef PyModule_AddObject_ErrCheck

	param_callback_dict = (PyDictObject *)PyDict_New();

	{  /* Argumentless CSH init */
		#ifndef PYCSH_HAVE_APM
		#ifdef PYCSH_HAVE_SLASH
			slash_list_init();
		#endif
		#endif
	}

	return Py_NewRef(pycsh);  // `Py_NewRef()` needed because we use AUTO_DECREF for exception handling.
}
