
// It is recommended to always define PY_SSIZE_T_CLEAN before including Python.h
#define PY_SSIZE_T_CLEAN
#include <Python.h>

// Slash is conditionally included by pycsh.h
#include "pycsh.h"
#include "utils.h"
#include "pycshconfig.h"
#include "slash_command/python_slash_command.h"


static int py_iter_sys_path(void) {

	// Import the sys module
    PyObject* sys_module = PyImport_ImportModule("sys");
    if (sys_module == NULL) {
		fprintf(stderr, "Failed to import sys module\n");
		return -1;
	}
	// Get the sys.path attribute
	PyObject* sys_path = PyObject_GetAttrString(sys_module, "path");
	if (sys_path == NULL) {
		fprintf(stderr, "Failed to get sys.path attribute\n");
		Py_DECREF(sys_module);
		return -2;
	}

	// Check if sys.path is a list
	if (!PyList_Check(sys_path)) {
		// Decrement the refcounts
		Py_DECREF(sys_path);
		Py_DECREF(sys_module);
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

	// Decrement the reference count of sys.path
	Py_DECREF(sys_path);

	// Decrement the reference count of the sys module
	Py_DECREF(sys_module);

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
    PyObject* sys_module = PyImport_ImportModule("sys");
    if (sys_module == NULL) {
        printf("Failed to import sys module\n");
        return -1;
    }

    // Get the sys.path attribute
    PyObject* sys_path = PyObject_GetAttrString(sys_module, "path");
    if (sys_path == NULL) {
        printf("Failed to get sys.path attribute\n");
        Py_DECREF(sys_module);
        return -2;
    }

    // Append $HOME to sys.path
    const char* home = getenv("HOME");
    if (home == NULL) {
        printf("HOME environment variable not set\n");
    } else {
        PyObject* home_path = PyUnicode_FromString(home);
        if (home_path == NULL) {
            printf("Failed to create Python object for HOME directory\n");
        } else {
            PyList_Append(sys_path, home_path);
            Py_DECREF(home_path);
        }
    }

    // Append $HOME/.local/lib/csh to sys.path
    const char* local_lib_csh = "/.local/lib/csh"; // assuming it's appended to $HOME
    PyObject* local_lib_csh_path = PyUnicode_FromString(local_lib_csh);
    if (local_lib_csh_path == NULL) {
        printf("Failed to create Python object for $HOME/.local/lib/csh\n");
    } else {
        PyObject* full_path = PyUnicode_FromFormat("%s%s", home, local_lib_csh);
        if (full_path == NULL) {
            printf("Failed to create Python object for $HOME/.local/lib/csh\n");
            Py_DECREF(local_lib_csh_path);
        } else {
            PyList_Append(sys_path, full_path);
            Py_DECREF(full_path);
            Py_DECREF(local_lib_csh_path);
        }
    }

    // Append CWD to sys.path
    PyObject* cwd_path = PyUnicode_FromString(".");
    if (cwd_path == NULL) {
        printf("Failed to create Python object for current working directory\n");
    } else {
        PyList_Append(sys_path, cwd_path);
        Py_DECREF(cwd_path);
    }

    // Decrement the reference count of sys_path
    Py_DECREF(sys_path);
    // Decrement the reference count of the sys module
    Py_DECREF(sys_module);

    return 0;
}

void libinfo(void) {

	printf("This APM embeds a Python interpreter into CSH,\n");
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
	printf("Shutting down Python interpreter\n");
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

#if 0
static void py_print__str__(PyObject *obj) {
	// Get the string representation of the object
    PyObject* str_repr = PyObject_Str(obj);
    if (str_repr == NULL) {
		fprintf(stderr, "Failed to get string representation\n");
		return;
	}

	// Convert the string representation to a C string
	const char* c_str_repr = PyUnicode_AsUTF8(str_repr);
	if (c_str_repr != NULL) {
		// Print the string representation
		printf("String representation: %s\n", c_str_repr);
	} else {
		fprintf(stderr, "Failed to convert string representation to C string\n");
	}

	// Decrement the reference count of the string representation
	Py_DECREF(str_repr);
}
#endif

#ifdef PYCSH_HAVE_SLASH
#include <slash/completer.h>

static void python_module_path_completer(struct slash * slash, char * token) {
    slash_path_completer(slash, token);
	
	/* strip_py_extension */
	size_t len = strlen(slash->buffer);
    if (len >= 3 && strcmp(slash->buffer + len - 3, ".py") == 0) {
        slash->buffer[len - 3] = '\0'; // Null-terminate the string at the position before ".py"
		slash->cursor = slash->length = strlen(slash->buffer);
    }
}

/* NOTE: It doesn't make sense for PYCSH_HAVE_APM without PYCSH_HAVE_SLASH.
	But we allow it for now, instead of erroring. */

static int py_run_cmd(struct slash *slash) {

	char * func_name = "main";

    optparse_t * parser = optparse_new("py run", "<file> [arguments...]");
    optparse_add_help(parser);
    optparse_add_string(parser, 'f', "func", "FUNCNAME", &func_name, "Function to call in the specified file (default = 'main')");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {  // Must have filename
        optparse_del(parser);
	    return SLASH_EUSAGE;
    }

	/* Check if filename is present */
	if (++argi >= slash->argc) {
		printf("missing parameter filename\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	PyEval_RestoreThread(main_thread_state);
    PyThreadState * state __attribute__((cleanup(state_release_GIL))) = main_thread_state;

	PyObject *pName = PyUnicode_DecodeFSDefault(slash->argv[argi]);
    /* Error checking of pName left out */

    PyObject *pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (pModule == NULL) {
		PyErr_Print();
        fprintf(stderr, "Failed to load \"%s\"\n", slash->argv[argi]);
		optparse_del(parser);
        return SLASH_EINVAL;
	}

	PyObject *pFunc = PyObject_GetAttrString(pModule, func_name);
	/* pFunc is a new reference */

	if (!pFunc || !PyCallable_Check(pFunc)) {
		if (PyErr_Occurred())
			PyErr_Print();
		fprintf(stderr, "Cannot find function \"%s\"\n", func_name);
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	PyObject *pArgs = PyTuple_New((slash->argc - argi)-1);
	for (int i = argi+1; i < slash->argc; i++) {
		PyObject *pValue = PyUnicode_FromString(slash->argv[i]);
		if (!pValue) {
			Py_DECREF(pArgs);
			Py_DECREF(pModule);
			fprintf(stderr, "Cannot convert argument\n");
			optparse_del(parser);
			return SLASH_EINVAL;
		}
		/* pValue reference stolen here: */
		PyTuple_SetItem(pArgs, i-argi-1, pValue);
	}

	PyObject *pValue = PyObject_CallObject(pFunc, pArgs);
	Py_DECREF(pArgs);

	if (pValue == NULL) {
		Py_DECREF(pFunc);
		Py_DECREF(pModule);
		PyErr_Print();
		fprintf(stderr,"Call failed\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	Py_DECREF(pFunc);
	Py_DECREF(pValue);
	Py_DECREF(pModule);

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub_completer(py, run, py_run_cmd, python_module_path_completer, "<file> [arguments...]", "Run a Python script with an importable PyCSH from this APM");

#endif  // PYCSH_HAVE_SLASH
