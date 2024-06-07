
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
#include "utils.h"
#include "slash_command/python_slash_command.h"

#define WALKDIR_MAX_PATH_SIZE 256

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


int search_python_files(const char *path, const char *search_str, char ***files) {
    printf("Searching in path: %s\n", path);
    
    DIR *dir;
    struct dirent *entry;
    int count = 0;
    int capacity = 10; // Initial capacity for the files array

    *files = (char **)malloc(capacity * sizeof(char *));
    if (*files == NULL) {
        perror("malloc");
        return -1;
    }

    dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        free(*files);
        *files = NULL;
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) { // If the entry is a regular file
            char *dot = strrchr(entry->d_name, '.');

            if (dot && strcmp(dot, ".py") == 0) { // Check for .py extension
                if (search_str == NULL || strstr(entry->d_name, search_str) != NULL) {
                    if (count >= capacity) {
                        capacity *= 2;
                        char **new_files = (char **)realloc(*files, capacity * sizeof(char *));
                        if (new_files == NULL) {
                            perror("realloc");
                            closedir(dir);
                            for (int i = 0; i < count; i++) {
                                free((*files)[i]);
                            }
                            free(*files);
                            *files = NULL;
                            return -1;
                        }
                        *files = new_files;
                    }
                    // Allocate memory for the filename without ".py"
                    size_t len = dot - entry->d_name; // Length of the filename without the extension
                    (*files)[count] = (char *)malloc(len + 1); // +1 for null terminator
                    if ((*files)[count] == NULL) {
                        perror("malloc");
                        closedir(dir);
                        for (int i = 0; i < count; i++) {
                            free((*files)[i]);
                        }
                        free(*files);
                        *files = NULL;
                        return -1;
                    }
                    strncpy((*files)[count], entry->d_name, len);
                    (*files)[count][len] = '\0'; // Null-terminate the string
                    count++;
                }
            }
        }
    }
    closedir(dir);

    return count;
}

const char *get_python_script_path(char **files, int index) {
    return files[index];
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
        strcat(path, "/Desktop/Coding/python_apm");
    }
	// Function to search for python files in specified path
	char **files = NULL;
	int lib_count = search_python_files(path, search_str, &files);

	if (lib_count == 0) {
		printf("No Python scripts found in %s\n", path);
		free(path);
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	PyEval_RestoreThread(main_thread_state);
	PyThreadState *state __attribute__((cleanup(state_release_GIL))) = main_thread_state;

	for (int i = 0; i < lib_count; i++) {
		const char *script_path = get_python_script_path(files, i);
		printf("Loading script: %s\n", script_path);

		if (main_thread_state == NULL) {
			fprintf(stderr, "main_thread_state is NULL\n");
			continue;
		}
		
		PyObject *pName = PyUnicode_DecodeFSDefault(script_path);
		if (pName == NULL) {
			PyErr_Print();
			fprintf(stderr, "Failed to decode script name: %s\n", script_path);
			continue;
		}

		PyObject *pModule = PyImport_Import(pName);
		Py_DECREF(pName);

		if (pModule == NULL) {
			PyErr_Print();
			fprintf(stderr, "Failed to load module: %s\n", script_path);
			continue;
		}

		PyObject *pFunc = PyObject_GetAttrString(pModule, "main");
		if (!pFunc || !PyCallable_Check(pFunc)) {
			if (PyErr_Occurred()) {
				PyErr_Print();
			}
			fprintf(stderr, "Cannot find function \"main\" in %s\n", script_path);
			Py_DECREF(pModule);
			continue;
		}

		printf("Preparing arguments for script: %s\n", script_path);

		PyObject *pArgs = PyTuple_New(0);
		if (pArgs == NULL) {
			PyErr_Print();
			Py_DECREF(pFunc);
			Py_DECREF(pModule);
			fprintf(stderr, "Failed to create arguments tuple\n");
			continue;
		}


		printf("Calling main function of script: %s\n", script_path);

		PyObject *pValue = PyObject_CallObject(pFunc, pArgs);
		Py_DECREF(pArgs);

		if (pValue == NULL) {
			PyErr_Print();
			Py_DECREF(pFunc);
			Py_DECREF(pModule);
			fprintf(stderr, "Call failed for script: %s\n", script_path);
			continue;
		}

		printf("Script executed successfully: %s\n", script_path);

		Py_DECREF(pFunc);
		Py_DECREF(pValue);
		Py_DECREF(pModule);
	}

	for (int i = 0; i < lib_count; i++) {
		free(files[i]);
	}
	free(files);
	free(path);

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