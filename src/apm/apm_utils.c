/**
 * Utils for loading Python APMs
 */

// It is recommended to always define PY_SSIZE_T_CLEAN before including Python.h
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include "../utils.h"


static void _dlclose_cleanup(void *const* handle) {
	if (*handle == NULL || handle == NULL) {
		return;
	}

	dlclose(*handle);
}

// Source: Mostly https://chatgpt.com
static PyObject * pycsh_integrate_pymod(const char * const _filepath) {
	void *handle __attribute__((cleanup(_dlclose_cleanup))) = dlopen(_filepath, RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        fprintf(stderr, "Error loading shared object file: %s\n", dlerror());
        return NULL;
    }

    // Construct the initialization function name: PyInit_<module_name>
    char * const filepath CLEANUP_STR = safe_strdup(_filepath);
	char *filename = strrchr(filepath, '/');
    if (!filename) {
        filename = filepath;
    } else {
        filename++;
    }
    char *dot = strchr(filename, '.');
    if (dot) {
        *dot = '\0';
    }

    size_t init_func_name_len = strlen("PyInit_") + strlen(filename) + 1;
    char init_func_name[init_func_name_len];
    snprintf(init_func_name, init_func_name_len, "PyInit_%s", filename);

    typedef PyObject* (*PyInitFunc)(void);
    PyInitFunc init_func = (PyInitFunc)dlsym(handle, init_func_name);

    if (!init_func) {
        fprintf(stderr, "Error finding initialization function: %s\n", dlerror());
        return NULL;
    }

    PyObject *module AUTO_DECREF = init_func();
    if (!module) {
        PyErr_Print();
        return NULL;
    }

#if 1
	/* TODO Kevin: Are we responsible for adding to sys.modules?
		Or will PyModule_Create() do that for us? */

    // Add the module to sys.modules
    if (PyDict_SetItemString(PyImport_GetModuleDict(), filename, module) < 0) {
        return NULL;
    }
#endif

	/* Currently init_func() will be called and executed successfully,
		but attempting to PyObject_GetAttrString() on the returned module causes a segmentation fault.
		For future reference, here is some reading material on how Python handles the import on Unixes:
		- https://stackoverflow.com/questions/25678174/how-does-module-loading-work-in-cpython */
    return Py_NewRef(module);
}

/**
 * @brief Handle loading of both .py and .so (APM) modules
 * 
 * TODO Kevin: Extract init_function call to separate function,
 * 	so it can be required by "py run" but optional by "apm load".
 * 
 * @param _filepath Specific Python APM to load/import.
 * @param init_function Optional init function to call after importing. Set to NULL to skip.
 * @param verbose Whether to printf() 
 * @return PyObject* new reference to the imported module
 */
PyObject * pycsh_load_pymod(const char * const _filepath, const char * const init_function, int verbose) {

	if (_filepath == NULL) {
		return NULL;
	}

	char * const filepath CLEANUP_STR = safe_strdup(_filepath);
	char *filename = strrchr(filepath, '/');

	/* Construct Python import string */
	if (filename == NULL) {  // No slashes in string, maybe good enough for Python
		filename = filepath;
	} else if (*(filename + 1) == '\0') {   // filename ends with a trailing slash  (i.e "dist-packeges/my_package/")
		*filename = '\0';  				    // Remove trailing slash..., (i.e "dist-packeges/my_package")
		filename = strrchr(filepath, '/');  // and attempt to find a slash before it. (i.e "/my_package")
		if (filename == NULL) {			    // Use full name if no other slashes are found.
			filename = filepath;
		} else {						    // Otherwise skip the slash. (i.e "my_package")
			filename += 1;
		}
	} else {
		filename += 1;
	}

	if (strcmp(filename, "__pycache__") == 0) {
		return NULL;
	}
	if (*filename == '.') {  // Exclude hidden files, . and ..
		return NULL;
	}
	
	char *dot = strchr(filename, '.');
	if (dot != NULL) {
		*dot = '\0';
	}

	// We probably shouldn't load ourselves, although a const string approach likely isn't the best guard.
	if (strcmp("libcsh_pycsh", filename) == 0) {
		return NULL;
	}

	{	/* Python code goes here */
		//printf("Loading script: %s\n", filename);

		PyObject *pName AUTO_DECREF = PyUnicode_DecodeFSDefault(filename);
		if (pName == NULL) {
			PyErr_Print();
			fprintf(stderr, "Failed to decode script name: %s\n", filename);
			return NULL;
		}

		PyObject *pModule AUTO_DECREF;

		/* Check for .so extension */
		int filepath_len = strlen(_filepath);
		if (filepath_len >= 3 && strcmp(_filepath + filepath_len - 3, ".so") == 0) {
			/* Assume this .so file is a Python module that should link with us.
				If it isn't a Python module, then this call will fail,
				after which the original "apm load" will attempt to import it.
				If it is an independent module, that should link with us,
				then is should've been imported with a normal Python "import" ya dummy :) */
			pModule = pycsh_integrate_pymod(_filepath);
		} else {
			pModule = PyImport_Import(pName);
		}

		if (pModule == NULL) {
			PyErr_Print();
			//fprintf(stderr, "Failed to load module: %s\n", filename);
			return NULL;
		}

		if (init_function == NULL) {
			if (verbose) {
				printf("Skipping init function for module '%s'", filename);
			}
			return Py_NewRef(pModule);
		}

		// TODO Kevin: Consider the consequences of throwing away the module when failing to call init function.

		PyObject *pFunc AUTO_DECREF = PyObject_GetAttrString(pModule, init_function);
		if (!pFunc || !PyCallable_Check(pFunc)) {
			PyErr_Clear();
			//fprintf(stderr, "Cannot find function \"%s()\" in %s\n", init_function, filename); // This print is a good idea for debugging, but since the apm_init(main) is not required this print can be a bit confusing.
			return NULL;
		}

		//printf("Preparing arguments for script: %s\n", filename);

		PyObject *pArgs AUTO_DECREF = PyTuple_New(0);
		if (pArgs == NULL) {
			PyErr_Print();
			fprintf(stderr, "Failed to create arguments tuple\n");
			return NULL;
		}

		if (verbose)
			printf("Calling '%s.%s()'\n", filename, init_function);

		PyObject *pValue AUTO_DECREF = PyObject_CallObject(pFunc, pArgs);

		if (pValue == NULL) {
			PyErr_Print();
			fprintf(stderr, "Call failed for: %s.%s\n", filename, init_function);
			return NULL;
		}

		if (verbose)
			printf("Script executed successfully: %s.%s()\n", filename, init_function);

		return Py_NewRef(pModule);
	}
}
