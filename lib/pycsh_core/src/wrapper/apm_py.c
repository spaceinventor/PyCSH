/*
 * dflopt_py.c
 *
 * Wrappers for lib/slash/src/dflopt.c
 *
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "apm_py.h"

#include <pwd.h>

#include "../apm/apm.h"
#include "../utils.h"

#include "../pycsh.h"

#undef NDEBUG
#include <assert.h>


PyObject * pycsh_apm_load(PyObject * self, PyObject * args, PyObject * kwds) {

	char * path = NULL;
    char * filename = NULL;  // TODO Kevin: Actually use the provided filename
    bool stop_on_error = false;
	int verbose = pycsh_dfl_verbose;

    static char *kwlist[] = {"path", "filename", "stop_on_error", "verbose", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|zzpi:apm_load", kwlist, &path, &filename, &stop_on_error, &verbose))
		return NULL;  // TypeError is thrown

	// TODO Kevin: This wrapper should probably ALSO load C APMs. But we can clean up the C APM API a bit before that.

#if 0
	PyEval_RestoreThread(main_thread_state);
	PyThreadState *state __attribute__((cleanup(state_release_GIL))) = main_thread_state;
	if (main_thread_state == NULL) {
		fprintf(stderr, "main_thread_state is NULL\n");
		return NULL;
	}
#endif

	PyObject *return_dict AUTO_DECREF = PyDict_New();
	if (return_dict == NULL) {
		return NULL;  // Hopefully PyDict_New() has set an exception here
	}

	if (path == NULL) {
        char *home_dir = getenv("HOME");
        if (home_dir == NULL) {
            home_dir = getpwuid(getuid())->pw_dir;
            if (home_dir == NULL) {
				PyErr_SetString(PyExc_IOError, "No home folder found");
                return NULL;
            }
        }
        path = (char *)malloc(WALKDIR_MAX_PATH_SIZE);  // TODO Kevin: Memory leak here
        strcpy(path, home_dir);
        strcat(path, PYAPMS_DIR);
    }
	// Function to search for python files in specified path
	int lib_count = 0;
	{
		DIR *dir CLEANUP_DIR = opendir(path);
		if (dir == NULL) {
			perror("opendir");
			PyErr_Format(PyExc_IOError, "Failed to open directory '%s'", path);
			return NULL;
		}
		
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL)  {

			/* Build full path. */
			int fullpath_len = strnlen(path, WALKDIR_MAX_PATH_SIZE) + strnlen(entry->d_name, WALKDIR_MAX_PATH_SIZE);
			char fullpath[fullpath_len+1];
			strncpy(fullpath, path, fullpath_len);
			strncat(fullpath, entry->d_name, fullpath_len-strnlen(path, WALKDIR_MAX_PATH_SIZE));

			/* Actually load the module. */
			assert(!PyErr_Occurred());
			PyObject * pymod AUTO_DECREF = pycsh_load_pymod(fullpath, DEFAULT_INIT_FUNCTION, verbose);
			if (pymod == NULL && stop_on_error) {
				if (!PyErr_Occurred()) {
					PyErr_Format(PyExc_ImportError, "Failed to import module: '%s'", fullpath);
				}
				return NULL;  
			}

			PyObject *value = (pymod != NULL) ? pymod : PyErr_Occurred();
			
			if (value == NULL) {
				/* No exception was raised, assert() that this 'module' was skipped deliberately. */
				continue;
			}

			lib_count++;

			PyErr_Clear();
			assert(!stop_on_error);

			if (PyDict_SetItemString(return_dict, fullpath, value) != 0) {
				return NULL;
			}

			if (pymod != NULL && verbose >= 1) {
				printf("\033[32mLoaded: %s\033[0m\n", fullpath);
			}
		}
	}

	return Py_NewRef(return_dict);
}
