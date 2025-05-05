#include "slash_py.h"
#include <slash/slash.h>

#include <csh/url_utils.h>
#include <csh/param_sniffer.h>
#include <csh/victoria_metrics.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

extern int vm_running;

extern vm_args victoria_metrics_args;

PyObject * pycsh_vm_start(PyObject * self, PyObject * py_args, PyObject * kwds) {

    if (vm_running) return SLASH_SUCCESS;

    bool logfile = false;
    char * tmp_username = NULL;
    char * tmp_password = NULL;
    char * key_file = NULL;
    char * sec_key = NULL;
    char * api_root = NULL;
    vm_args * args = &victoria_metrics_args;

    static char *kwlist[] = {"api_root", "logfile", "username", "password", "auth_file", "ssl", "server_port", "skip_verify", "verbose", NULL};

    if (!PyArg_ParseTupleAndKeywords(py_args, kwds, "s|pzzzpIpp:vm_start", kwlist, &api_root, &logfile, &args->username, &args->password, &key_file, &args->use_ssl, &args->port, &args->skip_verify, &args->verbose)) {
        return NULL;  // TypeError is thrown
    }
    
    if(false == is_http_url(api_root)) {
        args->server_ip = strdup(api_root);
        args->api_root = NULL;
    } else {
        args->api_root = strdup(api_root);
        args->server_ip = NULL;
    }

    if(key_file) {

        char key_file_local[256];
        if (key_file[0] == '~') {
            strcpy(key_file_local, getenv("HOME"));
            strcpy(&key_file_local[strlen(key_file_local)], &key_file[1]);
        }
        else {
            strcpy(key_file_local, key_file);
        }

        FILE *file = fopen(key_file_local, "r");
        if(file == NULL) {
            PyErr_Format(PyExc_FileNotFoundError, "Could not open config %s", key_file_local);
            return NULL;
        }

        sec_key = malloc(1024);
        if (sec_key == NULL) {
            //fprintf(stderr, "Failed to allocate memory for secret key.\n");
            //PyErr_NoMemory();
            fclose(file);
            PyErr_SetString(PyExc_MemoryError, "Failed to allocate memory for secret key.");
            return NULL;
        }

        if (fgets(sec_key, 1024, file) == NULL) {
            free(sec_key);
            fclose(file);
            PyErr_SetString(PyExc_IOError, "Failed to read secret key from file.");
            return NULL;
        }
        fclose(file);
    }

    if (tmp_username) {
        if (!tmp_password && !sec_key) {
            PyErr_SetString(PyExc_ValueError, "No password or auth_file provided");
            return NULL;
        }
        args->username = strdup(tmp_username);
        args->password = strdup(sec_key ? sec_key : tmp_password);
        char * const newline = strchr(args->password, '\n');
        if (newline) {
            *newline = '\0';
        }
        if (!args->port) {
            args->port = SERVER_PORT_AUTH;
        }
    } else if (!args->port) {
        args->port = SERVER_PORT;
    }

    param_sniffer_init(logfile);
    pthread_create(&vm_push_thread, NULL, &vm_push, args);
    vm_running = 1;

    if (sec_key != NULL) {
        free(sec_key);
    }
    
    Py_RETURN_NONE;
}

PyObject * pycsh_vm_stop(PyObject * self, PyObject * py_args) {

    vm_args * args = &victoria_metrics_args;
    if (!vm_running) return SLASH_SUCCESS;

    if(args->username) {
        free(args->username);
        args->username = NULL;
    }
    if(args->password) {
         free(args->password);
         args->password = NULL;
    }
    if(args->api_root) {
         free(args->api_root);
         args->api_root = NULL;
    }
    if(args->server_ip) {
         free(args->server_ip);
         args->server_ip = NULL;
    }

    vm_running = 0;

    Py_RETURN_NONE;
}