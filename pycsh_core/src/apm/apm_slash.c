/**
 * Override/extend the "apm load" slash command, such that it also supports Python APMs
 */

// It is recommended to always define PY_SSIZE_T_CLEAN before including Python.h
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdlib.h>
#include <string.h>
#include <slash/optparse.h>
#include <slash/completer.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include "../utils.h"
#include "../slash_command/python_slash_command.h"

#include "apm.h"


/* "py run" is diabled until further notice,
	due to pycsh_load_pymod() not requiring the specified init_function to be present (to remain compatible with CSH). */
#if 0

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
#endif


extern struct slash_command slash_cmd_apm_load;
//#define original_apm_load slash_cmd_apm_load
struct slash_command * original_apm_load = &slash_cmd_apm_load;

int py_apm_load_cmd(struct slash *slash) {

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

			/* Verify file has search string */
			if (search_str && !strstr(entry->d_name, search_str)) {
				continue;
			}
			if(strstr(entry->d_name, ".py")) {
				int fullpath_len = strnlen(path, WALKDIR_MAX_PATH_SIZE) + strnlen(entry->d_name, WALKDIR_MAX_PATH_SIZE);
				char fullpath[fullpath_len+1];
				strncpy(fullpath, path, fullpath_len);
				strncat(fullpath, entry->d_name, fullpath_len-strnlen(path, WALKDIR_MAX_PATH_SIZE));
				PyObject * pymod AUTO_DECREF = pycsh_load_pymod(fullpath, DEFAULT_INIT_FUNCTION, 1);
				if (pymod == NULL) {
					PyErr_Print();
					continue;
				}
				lib_count++;

				// TODO Kevin: Verbose argument?
				printf("\033[32mLoaded: %s\033[0m\n", fullpath);
			}
		}
	}

	optparse_del(parser);
	return SLASH_SUCCESS;
}
// __slash_command(slash_cmd_apm_load_py, "apm load", py_apm_load_cmd, NULL, "", "Load an APM (Python or C)");

