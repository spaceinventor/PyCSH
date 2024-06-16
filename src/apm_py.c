
// It is recommended to always define PY_SSIZE_T_CLEAN before including Python.h
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdlib.h>
#include <string.h>
#include <slash/optparse.h>
#include <slash/completer.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <dlfcn.h>
#include "utils.h"
#include "slash_command/python_slash_command.h"

#define WALKDIR_MAX_PATH_SIZE 256

#define DEFAULT_INIT_FUNCTION "apm_init"

static bool path_is_file(const char * path) {
	struct stat s;
	if (stat(path, &s) == 0 && s.st_mode & S_IFREG)
		return true;
	return false;
}

static void python_module_path_completer(struct slash * slash, char * token) {

	size_t len = strlen(token);

#if 0  // Using path_is_file() to get the length to restore, means we have to type the full path with filename.
	if (path_is_file(token)) {
		char * file_extension = strrchr(token, '.');
		if (file_extension != NULL) {
			len = file_extension - token;
		}
	}
#endif

	/* Restore path after previous Python package tab completion */
	for (size_t i = 0; i < len; i++) {
		if (token[i] == '.') {
			token[i] = '/';
		}
	}

	slash_path_completer(slash, token);
	len = strlen(token);

	if (path_is_file(token)) {
#if 0
		/* Strip file extension if token is file */
		char * file_extension = strrchr(token, '.');
		if (file_extension != NULL) {
			*file_extension = '\0';
			slash->cursor = slash->length = file_extension - token;
		}
#else
	/* Strip .py extension */
    if (len >= 3 && strcmp(token + len - 3, ".py") == 0) {
        token[len - 3] = '\0'; // Null-terminate the string at the position before ".py"
		len -= 3;
		slash->cursor = slash->length -= 3;
    }
#endif
	}
	
	for (size_t i = 0; i < len; i++) {
		if (token[i] == '/') {
			token[i] = '.';
		}
	}
}

static void _dlclose_cleanup(void ** handle) {
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
 * @param filepath 
 * @return PyObject* new reference to the imported module
 */
static PyObject * pycsh_load_pymod(const char * const _filepath, const char * const init_function, int verbose) {

	if (_filepath == NULL) {
		return NULL;
	}

	char * const filepath CLEANUP_STR = safe_strdup(_filepath);
	char *filename = strrchr(filepath, '/');

	/* Construct Python import string */
	if (filename == NULL) {  // No slashes in string, maybe good enough for Python
		filename = filepath;
	} else if (*(filename + 1) == '\0') {  		 // filename ends with a trailing slash  (i.e "dist-packeges/my_package/")
		*filename = '\0';  						 // Remove trailing slash..., (i.e "dist-packeges/my_package")
		filename = strrchr(filepath, '/');  	 // and attempt to find a slash before it. (i.e "/my_package")
		if (filename == NULL) {					 // Use full name if no other slashes are found.
			filename = filepath;
		} else {								 // Otherwise skip the slash. (i.e "my_package")
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
			//fprintf(stderr, "Cannot find function \"apm_init\" in %s\n", filename); // This print is a good idea for debugging, but since the apm_init(main) is not required this print can be a bit confusing.
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

static int py_run_cmd(struct slash *slash) {

	char * func_name = DEFAULT_INIT_FUNCTION;

    optparse_t * parser = optparse_new("py run", "<file> [arguments...]");
    optparse_add_help(parser);
    optparse_add_string(parser, 'f', "func", "FUNCNAME", &func_name, "Function to call in the specified file (default = '"DEFAULT_INIT_FUNCTION"')");

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

	Py_XDECREF(pycsh_load_pymod(slash->argv[argi], func_name, 1));

	optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub_completer(py, run, py_run_cmd, python_module_path_completer, "<file> [arguments...]", "Run a Python script with an importable PyCSH from this APM");


extern struct slash_command slash_cmd_apm_load;
//#define original_apm_load slash_cmd_apm_load
struct slash_command * original_apm_load = &slash_cmd_apm_load;


static void _close_dir(DIR ** dir) {
	if (dir == NULL || *dir == NULL) {
		return;
	}
	closedir(*dir);
	*dir = NULL;
}

static int py_apm_load_cmd(struct slash *slash) {

    char * path = NULL;
    char * search_str = NULL;

    optparse_t * parser = optparse_new("apm load", "-f <filename> -p <pathname>");
    optparse_add_help(parser);
    optparse_add_string(parser, 'p', "path", "PATHNAME", &path, "Search paths separated by ';'");
    optparse_add_string(parser, 'f', "file", "FILENAME", &search_str, "Search string on APM file name");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    if (path == NULL) {
        char *home_dir = getenv("HOME");
        if (home_dir == NULL) {
            home_dir = getpwuid(getuid())->pw_dir;
            if (home_dir == NULL) {
                optparse_del(parser);
                printf("No home folder found\n");
                return SLASH_EINVAL;
            }
        }
        path = (char *)malloc(WALKDIR_MAX_PATH_SIZE);
        strcpy(path, home_dir);
        strcat(path, "/.local/lib/csh/");
    }
	// Function to search for python files in specified path
	int lib_count = 0;
	{
		DIR *dir __attribute__((cleanup(_close_dir))) = opendir(path);
		if (dir == NULL) {
			perror("opendir");
			return SLASH_EINVAL;
		}

		PyEval_RestoreThread(main_thread_state);
		PyThreadState *state __attribute__((cleanup(state_release_GIL))) = main_thread_state;
		if (main_thread_state == NULL) {
			fprintf(stderr, "main_thread_state is NULL\n");
			return SLASH_EINVAL;
		}
		
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL)  {
			int fullpath_len = strnlen(path, WALKDIR_MAX_PATH_SIZE) + strnlen(entry->d_name, WALKDIR_MAX_PATH_SIZE);
			char fullpath[fullpath_len+1];
			strncpy(fullpath, path, fullpath_len);
			strncat(fullpath, entry->d_name, fullpath_len-strnlen(path, WALKDIR_MAX_PATH_SIZE)-1);
			PyObject * pymod AUTO_DECREF = pycsh_load_pymod(fullpath, DEFAULT_INIT_FUNCTION, 1);
			if (pymod != NULL) {
				lib_count++;
			}
		}
	}

	optparse_del(parser);
	return original_apm_load->func(slash);
}
__slash_command(slash_cmd_apm_load_py, "apm load", py_apm_load_cmd, NULL, "", "Load an APM (Python or C)");

#if 0  // No need for a constructor that finds the "apm load" command, when we can just declare it extern.
__attribute__((constructor(152))) static void store_original_apm_load(void) {

	char * args = NULL;

	extern struct slash_command * slash_command_find(struct slash *slash, char *line, size_t linelen, char **args);
	// NOTE: slash_command_find() currently doesn't need a 'slash' instance, so we don't bother to create one.
    original_apm_load = slash_command_find(NULL, "apm load", strlen("apm load"), &args);
    assert(original_apm_load != NULL);  // If slash was able to execute this function, then the command should very much exist

	printf("FFF %p %s\n", original_apm_load, slash_cmd_apm_load.name);
}
#endif

__attribute__((destructor(152))) static void restore_original_apm_load(void) {

	char * args = NULL;

	extern struct slash_command * slash_command_find(struct slash *slash, char *line, size_t linelen, char **args);
	struct slash_command *current_apm_load = slash_command_find(NULL, "apm load", strlen("apm load"), &args);
	assert(current_apm_load != NULL);

	/* Someone has replaced us, let's not restore anyway. */
	if (current_apm_load != &slash_cmd_apm_load_py) {
		return;
	}

	// We explicitly remove our command from the list, to avoid the print from slash_list_add()
	slash_list_remove(&slash_cmd_apm_load_py);

	slash_list_add(original_apm_load);
}