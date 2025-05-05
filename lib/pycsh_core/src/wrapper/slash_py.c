#include "slash_py.h"
#include <slash/slash.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>


PyObject * pycsh_slash_execute(PyObject * self, PyObject * args, PyObject * kwds) {

    /* NOTE: We have no way of knowing if the slash command will require CSP.
        So while we don't call CSP_INIT_CHECK() here, segfaults may occur.
        But this isn't different from CSH, so it's probably okay. */

    char * command = NULL;

    static char *kwlist[] = {"command", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "s:slash_execute", kwlist, &command)) {
        return NULL;  // TypeError is thrown
    }

    struct slash slas = {0};
    #define LINE_SIZE		    512
    char line_buf[LINE_SIZE];
    #define HISTORY_SIZE		2048
    char hist_buf[HISTORY_SIZE];
    slash_create_static(&slas, line_buf, LINE_SIZE, hist_buf, HISTORY_SIZE);

    return Py_BuildValue("i", slash_execute(&slas, command));
}
