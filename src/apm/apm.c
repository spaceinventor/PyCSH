
// It is recommended to always define PY_SSIZE_T_CLEAN before including Python.h
#define PY_SSIZE_T_CLEAN
#include <Python.h>

// Slash is conditionally included by pycsh.h
#include "../pycsh.h"
#include "../utils.h"
#include "pycshconfig.h"
#include "../slash_command/python_slash_command.h"


static int py_iter_sys_path(void) {

	// Import the sys module
    PyObject* sys_module AUTO_DECREF = PyImport_ImportModule("sys");
    if (sys_module == NULL) {
		fprintf(stderr, "Failed to import sys module\n");
		return -1;
	}
	// Get the sys.path attribute
	PyObject* sys_path AUTO_DECREF = PyObject_GetAttrString(sys_module, "path");
	if (sys_path == NULL) {
		fprintf(stderr, "Failed to get sys.path attribute\n");
		return -2;
	}

	// Check if sys.path is a list
	if (!PyList_Check(sys_path)) {
		// Decrement the refcounts
		fprintf(stderr, "sys.path is not a list\n");
		return -3;
	}

	// Iterate over the elements of sys.path
	Py_ssize_t path_length = PyList_Size(sys_path);
	for (Py_ssize_t i = 0; i < path_length; ++i) {
		// Get the i-th element of sys.path
		PyObject* path_entry = PyList_GetItem(sys_path, i);
		if (path_entry != NULL) {
			// Convert the path entry to a C string
			const char* c_path_entry = PyUnicode_AsUTF8(path_entry);
			if (c_path_entry != NULL) {
				// Print the path entry
				printf("sys.path[%zd]: %s\n", i, c_path_entry);
			} else {
				printf("Failed to convert path entry to C string\n");
			}
		}
	}

	return 0;
}

/**
 * @brief Append our preferred Python APM paths to sys.path so we can import them with "py run"
 * 
 * Thank ChatGPT for this function, I was too lazy to write it.
 * 
 * @return int 0 for success
 */
static int append_pyapm_paths(void) {
	// Import the sys module
    PyObject* sys_module AUTO_DECREF = PyImport_ImportModule("sys");
    if (sys_module == NULL) {
        printf("Failed to import sys module\n");
        return -1;
    }

    // Get the sys.path attribute
    PyObject* sys_path AUTO_DECREF = PyObject_GetAttrString(sys_module, "path");
    if (sys_path == NULL) {
        printf("Failed to get sys.path attribute\n");
        return -2;
    }

    // Append $HOME to sys.path
    const char* home = getenv("HOME");
    if (home == NULL) {
        printf("HOME environment variable not set\n");
    } else {
        PyObject* home_path AUTO_DECREF = PyUnicode_FromString(home);
        if (home_path == NULL) {
            printf("Failed to create Python object for HOME directory\n");
        } else {
            PyList_Append(sys_path, home_path);
        }
    }

    // Append $HOME/.local/lib/csh to sys.path
    const char* local_lib_csh = "/.local/lib/csh"; // assuming it's appended to $HOME
    PyObject* local_lib_csh_path AUTO_DECREF = PyUnicode_FromString(local_lib_csh);
    if (local_lib_csh_path == NULL) {
        printf("Failed to create Python object for $HOME/.local/lib/csh\n");
    } else {
        PyObject* full_path AUTO_DECREF = PyUnicode_FromFormat("%s%s", home, local_lib_csh);
        if (full_path == NULL) {
            printf("Failed to create Python object for $HOME/.local/lib/csh\n");
        } else {
            PyList_Append(sys_path, full_path);
        }
    }

    // Append CWD to sys.path
    PyObject* cwd_path AUTO_DECREF = PyUnicode_FromString(".");
    if (cwd_path == NULL) {
        printf("Failed to create Python object for current working directory\n");
    } else {
        PyList_Append(sys_path, cwd_path);
    }

    return 0;
}

void libinfo(void) {

	printf("Loading PyCSH as an APM, embeds a Python interpreter into CSH,\n");
	printf("which can then run Python scripts that import PyCSH linked with our symbols\n");
}
 
/* Source: https://chat.openai.com
	Function to raise an exception in all threads */
static void raise_exception_in_all_threads(PyObject *exception) {
    PyGILState_STATE gstate = PyGILState_Ensure();
    PyInterpreterState *interp = PyInterpreterState_Head();
    PyThreadState *tstate = PyInterpreterState_ThreadHead(interp);

    while (tstate != NULL) {
        if (tstate != PyThreadState_Get()) {  // Skip the current thread
            PyThreadState_SetAsyncExc(tstate->thread_id, exception);
        }
        tstate = PyThreadState_Next(tstate);
    }

    PyGILState_Release(gstate);
}

__attribute__((destructor(150))) static void finalize_python_interpreter(void) {
	/* TODO Kevin: Is seems that this deallocator will still be called,
		even if the shared library (PyCSH) isn't loaded correctly.
		Which will cause a segmentation fault when finalizing Python.
		So we should make a guard clause for that. */
	printf("Shutting down Python interpreter\n");
	PyThreadState* tstate = PyGILState_GetThisThreadState();
	if(tstate == NULL){
		fprintf(stderr, "Python interpreter not started.\n");
		return;
	}
	PyEval_RestoreThread(main_thread_state);  // Re-acquire the GIL so we can raise and exit \_(ツ)_/¯
	raise_exception_in_all_threads(PyExc_SystemExit);
	/* NOTE: It seems exceptions don't interrupt sleep() in threads,
		so Py_FinalizeEx() still has to wait for them to finish.
		I have tried calling PyEval_SaveThread(); and here PyEval_RestoreThread(main_thread_state); here,
		but that doesn't seem to solve the problem. */
	if (Py_FinalizeEx() < 0) {
		/* We probably don't have to die to unloading errors here,
			but maybe the user wants to know. */
        fprintf(stderr, "Error finalizing Python interpreter\n");
    }
}

int apm_init(void) {
	PyImport_AppendInittab("pycsh", &PyInit_pycsh);
	PyImport_AppendInittab("PyCSH", &PyInit_pycsh);
	/* Calling Py_Initialize() "has the side effect of locking the global interpreter lock.
		Once the function completes, you are responsible for releasing the lock."
		-- https://www.linuxjournal.com/article/3641 */
	Py_Initialize();
	printf("Python interpreter started\n");
	if (append_pyapm_paths()) {
		fprintf(stderr, "Failed to add Python APM paths\n");
	}
	printf("Checked Python APM paths:\n");
	py_iter_sys_path();

	/* If we are being loaded as an APM,
		then we have to assume that CSH has already initialized CSP. */
	extern uint8_t _csp_initialized;
    _csp_initialized = 1;

	// release GIL here
  	main_thread_state = PyEval_SaveThread();

	return 0;
}
