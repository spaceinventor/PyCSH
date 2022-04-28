/*
 * wrapper.h
 *
 * Contains CSH commands converted to Python functions.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

PyObject * pycsh_param_get(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * pycsh_param_set(PyObject * self, PyObject * args, PyObject * kwds);;

PyObject * pycsh_param_pull(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * pycsh_param_push(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * pycsh_param_clear(PyObject * self, PyObject * args);

PyObject * pycsh_param_node(PyObject * self, PyObject * args);

PyObject * pycsh_param_paramver(PyObject * self, PyObject * args);

PyObject * pycsh_param_autosend(PyObject * self, PyObject * args);

PyObject * pycsh_param_queue(PyObject * self, PyObject * args);


PyObject * pycsh_param_list(PyObject * self, PyObject * args);

PyObject * pycsh_param_list_download(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * pycsh_param_list_forget(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * pycsh_param_list_save(PyObject * self, PyObject * args);

PyObject * pycsh_param_list_load(PyObject * self, PyObject * args);


PyObject * pycsh_csh_ping(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * pycsh_csh_ident(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * pycsh_csh_reboot(PyObject * self, PyObject * args);


PyObject * pycsh_vmem_list(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * pycsh_vmem_restore(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * pycsh_vmem_backup(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * pycsh_vmem_unlock(PyObject * self, PyObject * args, PyObject * kwds);