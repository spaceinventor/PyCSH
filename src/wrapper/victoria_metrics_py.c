/*
 * param_list_py.c
 *
 * Wrappers for lib/param/src/param/list/param_list_slash.c
 *
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <param/param_string.h>
#include <curl/curl.h>
#include <csp/csp.h>
#include "../pycsh.h"
#include "../utils.h"
#include "../parameter/parameter.h"
#include "../parameter/pythonparameter.h"
#include "../src/csh/victoria_metrics.h"

#include "victoria_metrics_py.h"

static pthread_t vm_push_thread;
extern int vm_running;

static int pycsh_parse_vm_args(PyObject * args_in, vm_args * args_out) {
	assert(args_in != NULL);  

    // Get all the values from the python dict. Everything is a string when it comes from python.
    PyObject * py_use_ssl = PyDict_GetItemString(args_in, "use_ssl");
    PyObject * py_port = PyDict_GetItemString(args_in, "port");
    PyObject * py_skip_verify = PyDict_GetItemString(args_in, "skip_verify");
    PyObject * py_verbose = PyDict_GetItemString(args_in, "verbose");
    PyObject * py_server_ip = PyDict_GetItemString(args_in, "server_ip");
    
    // Check if long characters are larger than INT_MAX since we actually want them to be ints and not longs.
    // These values are not required, so if the input is NULL we initialize them to 0.
    long use_ssl;
    if(py_use_ssl == NULL){
        use_ssl = 0;
    } else if (PyLong_Check(py_use_ssl)){
        use_ssl = PyLong_AsLong(py_use_ssl);
        if(use_ssl > INT_MAX){
            PyErr_SetString(PyExc_TypeError, "use_ssl value greater than INT_MAX");
            return -1;
        }
    } else {
        PyErr_SetString(PyExc_TypeError, "TypeError.");
        return -1;
    }

    long port;
    if(py_port == NULL){
        port = 0;
    } else if (PyLong_Check(py_port)){
        port = PyLong_AsLong(py_port);
        if(port > INT_MAX){
            PyErr_SetString(PyExc_TypeError, "port value greater than INT_MAX");
            return -1;
        }
    } else {
        PyErr_SetString(PyExc_TypeError, "TypeError.");
        return -1;
    }

    long skip_verify;
    if(py_skip_verify == NULL){
        skip_verify = 0;
    } else if (PyLong_Check(py_skip_verify)){
        skip_verify = PyLong_AsLong(py_skip_verify);
        if(skip_verify > INT_MAX){
            PyErr_SetString(PyExc_TypeError, "skip_verify value greater than INT_MAX");
            return -1;
        }
    } else {
        PyErr_SetString(PyExc_TypeError, "TypeError.");
        return -1;
    }

    long verbose;
    if(py_verbose == NULL){
        verbose = 0;
    } else if (PyLong_Check(py_verbose)){
        verbose = PyLong_AsLong(py_verbose);
        if(verbose > INT_MAX){
            PyErr_SetString(PyExc_TypeError, "verbose value greater than INT_MAX");
            return -1;
        }
    } else {
        PyErr_SetString(PyExc_TypeError, "TypeError.");
        return -1;
    }
        
    // Server_ip is required, so we make sure it exists.
    if(!py_server_ip){
        PyErr_SetString(PyExc_KeyError, "server_ip field missing.");
        return -1;
    }
    if(!PyUnicode_Check(py_server_ip)){
        PyErr_SetString(PyExc_TypeError, "TypeError in field server_ip.");
        return -1;
    }
    char * server_ip = strdup(PyUnicode_AsUTF8(py_server_ip));

    // Set values in the vm_args struct.
    args_out->use_ssl = (int)use_ssl;
    args_out->port = (int)port;
    args_out->skip_verify = (int)skip_verify;
    args_out->verbose = (int)verbose;

    args_out->server_ip = server_ip;
	    
	return 0;

}

PyObject * pycsh_vm_start(PyObject * self, PyObject * args, PyObject * kwds) {
    if(vm_running){
        printf("Thread already running.\n");
        return Py_None;
    } 

    int logfile = 0;
    char * tmp_username = NULL;
    char * tmp_password = NULL;
    PyObject * pyargs = NULL;

    static char *kwlist[] = {"logfile", "username", "password", "args", NULL};
    
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|issO", kwlist, &logfile, &tmp_username, &tmp_password, &pyargs)) {
        return NULL;
    }

    vm_args * vmargs = calloc(1, sizeof(vm_args));

    if(pycsh_parse_vm_args(pyargs, vmargs) < 0){
        PyErr_SetString(PyExc_ConnectionError, "Invalid arguments");
        return NULL;
    }

    if (tmp_username) {
        if (!tmp_password) {
            PyErr_SetString(PyExc_ConnectionError, "Provide password");
            return NULL;
        }
        vmargs->username = strdup(tmp_username);
        vmargs->password = strdup(tmp_password);
        if (!vmargs->port) {
            vmargs->port = SERVER_PORT_AUTH;
        }
    } else if (!vmargs->port) {
        vmargs->port = SERVER_PORT;
    }

    printf("\n%d\n", vmargs->port);
    pthread_create(&vm_push_thread, NULL, &vm_push, vmargs);
    vm_running = 1;

    return Py_None;

}

PyObject * pycsh_vm_stop(PyObject * self, PyObject * args, PyObject * kwds) {
    if(!vm_running) return Py_None;
    vm_running = 0;
    return Py_None;
}
