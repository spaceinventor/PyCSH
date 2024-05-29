
// It is recommended to always define PY_SSIZE_T_CLEAN before including Python.h
#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <slash/optparse.h>
#include <slash/completer.h>

#include "utils.h"
#include "slash_command/python_slash_command.h"


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

extern struct slash_command slash_cmd_apm_load;
//#define original_apm_load slash_cmd_apm_load
struct slash_command * original_apm_load = &slash_cmd_apm_load;

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

	/* TODO Kevin: Load .py Python APMs here
		Then run and return code from original implementation afterwards. */
	return original_apm_load->func(slash);
}

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

}