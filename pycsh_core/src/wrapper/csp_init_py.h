/*
 * csp_init_py.h
 *
 * Wrappers for src/csp_init_cmd.c
 *
 */

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdbool.h>


PyObject * pycsh_csh_csp_init(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * pycsh_csh_csp_ifadd_zmq(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * pycsh_csh_csp_ifadd_kiss(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * pycsh_csh_csp_ifadd_can(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * pycsh_csh_csp_ifadd_eth(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * pycsh_csh_csp_ifadd_udp(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * pycsh_csh_csp_ifadd_tun(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * pycsh_csh_csp_routeadd_cmd(PyObject * self, PyObject * args, PyObject * kwds);

extern bool csp_initialized();
