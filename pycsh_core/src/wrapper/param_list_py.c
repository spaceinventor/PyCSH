/*
 * param_list_py.c
 *
 * Wrappers for lib/param/src/param/list/param_list_slash.c
 *
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <param/param_string.h>
#include <param/param_list.h>

#include "../pycsh.h"
#include <pycsh/utils.h>
#include <pycsh/parameter.h>
#include <pycsh/pythonparameter.h>

#include "param_list_py.h"

PyObject * pycsh_param_list(PyObject * self, PyObject * args, PyObject * kwds) {

    int node = pycsh_dfl_node;
    int verbosity = 1;
    PyObject * mask_obj = NULL;
    char * globstr = NULL;

    static char *kwlist[] = {"node", "verbose", "mask", "globstr", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|iiOz:list", kwlist, &node, &verbosity, &mask_obj, &globstr)) {
        return NULL;
    }

    /* Interpret maskstring */
    uint32_t mask = 0xFFFFFFFF;
    if (mask_obj != NULL) {
        if (pycsh_parse_param_mask(mask_obj, &mask) != 0) {
            return NULL;  // Exception message set by pycsh_parse_param_mask()
        }
    }

    param_list_print(mask, node, globstr, verbosity);

    return pycsh_util_parameter_list(mask, node, globstr);
}

PyObject * pycsh_param_list_download(PyObject * self, PyObject * args, PyObject * kwds) {

    CSP_INIT_CHECK()

    unsigned int node = pycsh_dfl_node;
    unsigned int timeout = pycsh_dfl_timeout;
    unsigned int version = 2;
    int include_remotes = 0;

    static char *kwlist[] = {"node", "timeout", "version", "remotes", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|IIII:list_download", kwlist, &node, &timeout, &version, &include_remotes))
        return NULL;  // TypeError is thrown

    {  /* Allow threads during list_download() */
        int list_download_res;
        Py_BEGIN_ALLOW_THREADS;
        list_download_res = param_list_download(node, timeout, version, include_remotes);
        Py_END_ALLOW_THREADS;
        // TODO Kevin: Downloading parameters with an incorrect version, can lead to a segmentation fault.
        //	Had it been easier to detect when an incorrect version is used, we would've raised an exception instead.
        if (list_download_res < 1) {  // We assume a connection error has occurred if we don't receive any parameters.
            PyErr_Format(PyExc_ConnectionError, "No response (node=%d, timeout=%d)", node, timeout);
            return NULL;
        }
    }

    return pycsh_util_parameter_list(0xFFFFFFFF, node, NULL);

}

PyObject * pycsh_param_list_add(PyObject * self, PyObject * args, PyObject * kwds) {
    unsigned int node;
    unsigned int length;
    unsigned int id;
    char * name;
    unsigned int type; 
    PyObject * mask_obj = NULL;
    char * helpstr = NULL;
    char * unitstr = NULL;

    static char *kwlist[] = {"node", "length", "id", "name", "type", "mask", "comment", "unit", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "IIIsI|Ozz:list_add", kwlist, &node, &length, &id, &name, &type, &mask_obj, &helpstr, &unitstr)) {
        return NULL;
    }

    switch (type) {
        case PARAM_TYPE_UINT8:
        case PARAM_TYPE_UINT16:
        case PARAM_TYPE_UINT32:
        case PARAM_TYPE_UINT64:
        case PARAM_TYPE_INT8:
        case PARAM_TYPE_INT16:
        case PARAM_TYPE_INT32:
        case PARAM_TYPE_INT64:
        case PARAM_TYPE_XINT8:
        case PARAM_TYPE_XINT16:
        case PARAM_TYPE_XINT32:
        case PARAM_TYPE_XINT64:
        case PARAM_TYPE_FLOAT:
        case PARAM_TYPE_DOUBLE:
        case PARAM_TYPE_STRING:
        case PARAM_TYPE_DATA:
        case PARAM_TYPE_INVALID:
            break;
        
        default:
            PyErr_SetString(PyExc_InvalidParameterTypeError, "An invalid parameter type was specified during creation of a new parameter");
            return NULL;
    }

    uint32_t mask = 0;
    if (mask_obj != NULL) {
        if (pycsh_parse_param_mask(mask_obj, &mask) != 0) {
            return NULL;  // Exception message set by pycsh_parse_param_mask()
        }
    }
    
    param_t * param = param_list_create_remote(id, node, type, mask, length, name, unitstr, helpstr, -1);
    

    PyObject * param_instance = _pycsh_Parameter_from_param(&ParameterType, param, NULL, INT_MIN, pycsh_dfl_timeout, 1, 2);

    if (param_instance == NULL) {
        PyErr_SetString(PyExc_ValueError, "Unable to create param");
        return NULL;
    }
    if (param_list_add(param) != 0) {
        param_list_destroy(param);
        Py_DECREF(param_instance);
        PyErr_SetString(PyExc_ValueError, "Failed to add parameter to list");
        return NULL;
    }   

    return param_instance;
}

/**
 * @brief Version of param_list_remove() that will not destroy param_t's referenced by a ParameterObject wrapper.
 */
static int param_list_remove_py(int node, uint8_t verbose) {

	int count = 0;

	param_list_iterator i = {};
	param_t * iter_param = param_list_iterate(&i);

	while (iter_param) {

		param_t * param = iter_param;  // Free the current parameter after we have used it to iterate.
		iter_param = param_list_iterate(&i);

		if (i.phase == 0)  // Protection against removing static parameters
			continue;

		uint8_t match = node < 0;  // -1 means all nodes (except for 0)

		if (node > 0)
			match = *param->node == node;

		if (match) {
            ParameterObject *python_parameter = Parameter_wraps_param(param);
            /* TODO Kevin: Perhaps we need a better way to distinguish between param_t that are wrapped by Parameter()s,
                and those that are not. Currently we do this by reimplementing param_list_remove() here.
                We need to do this, because we must not free() param_t's referenced by Parameter()s,
                this must be done in their .tp_dealloc() instead.
                On the contrary, param_t's that are not wrapped by a Parameter() should be free()d now,
                before a Parameter() manages to grab a reference to it. */ 
            if (python_parameter) {
                param_list_remove_specific(param, verbose, 0);
                Py_DECREF(python_parameter);  // The parameter list no longer holds a reference to the Parameter
            } else {
                param_list_remove_specific(param, verbose, 1);
            }
			count++;
		}
	}

	return count;
}

PyObject * pycsh_param_list_forget(PyObject * self, PyObject * args, PyObject * kwds) {

    int node = pycsh_dfl_node;
    int verbose = 1;

    static char *kwlist[] = {"node", "verbose", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ii:list_forget", kwlist, &node, &verbose))
        return NULL;  // TypeError is thrown

    const int count_removed = param_list_remove_py(node, verbose);

    if (verbose >= 1) {
        printf("Removed %i parameters\n", count_removed);
    }
    return Py_BuildValue("i", count_removed);
}

PyObject * pycsh_param_list_save(PyObject * self, PyObject * args, PyObject * kwds) {

    char * filename = NULL;
    int node = pycsh_dfl_node;
    int skip_node = false;  // Make node optional, as to support adding to env node

    static char *kwlist[] = {"filename", "node", "skip_node", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|zip:list_save", kwlist, &filename, &node, &skip_node)) {
        return NULL;  // TypeError is thrown
    }

    param_list_save(filename, node, skip_node);

    Py_RETURN_NONE;
}
