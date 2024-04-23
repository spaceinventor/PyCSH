#include "slash_py.h"
#include <slash/slash.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>


PyObject * pycsh_slash_execute(PyObject * self, PyObject * args, PyObject * kwds) {

    char * command = NULL;

    static char *kwlist[] = {"command", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s", kwlist, &command)) {
        return NULL;  // TypeError is thrown
    }

    struct slash slas = {0};
    #define LINE_SIZE 1024
    char line_buf[LINE_SIZE];
    #define HISTORY_SIZE 1024
    char hist_buf[HISTORY_SIZE];
    slash_create_static(&slas, line_buf, LINE_SIZE, hist_buf, HISTORY_SIZE);

    return Py_BuildValue("i", slash_execute(&slas, command));
}
