#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>

#include <csh/known_hosts.h>

#include "known_hosts.h"


static int cmd_node_save(struct slash *slash)
{
    
    FILE * out = stdout;

    char * dirname = getenv("HOME");
    char path[100];

	if (strlen(dirname)) {
        snprintf(path, 100, "%s/csh_hosts", dirname);
    } else {
        snprintf(path, 100, "csh_hosts");
    }

    /* write to file */
	FILE * fd = fopen(path, "w");
    if (fd) {
        out = fd;
    }

    for (host_t* host = SLIST_FIRST(&known_hosts); host != NULL; host = SLIST_NEXT(host, next)) {
        assert(host->node != 0);  // Holdout from array-based known_hosts
        if (host->node != 0) {
            fprintf(out, "node add -n %d %s\n", host->node, host->name);
            printf("node add -n %d %s\n", host->node, host->name);
        }
    }

    if (fd) {
        fclose(fd);
    }


    return SLASH_SUCCESS;
}

slash_command_sub(node, save, cmd_node_save, NULL, NULL);


static int cmd_nodes(struct slash *slash)
{
    for (host_t* host = SLIST_FIRST(&known_hosts); host != NULL; host = SLIST_NEXT(host, next)) {
        assert(host->node != 0);  // Holdout from array-based known_hosts
        if (host->node != 0) {
            printf("node add -n %d %s\n", host->node, host->name);
        }
    }

    return SLASH_SUCCESS;
}
slash_command_sub(node, list, cmd_nodes, NULL, NULL);

static int cmd_hosts_add(struct slash *slash)
{

    int node = slash_dfl_node;

    optparse_t * parser = optparse_new("hosts add", "<name>");
    optparse_add_help(parser);
    optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    if (node == 0) {
        fprintf(stderr, "Refusing to add hostname for node 0");
        optparse_del(parser);
        return SLASH_EINVAL;
    }

	/* Check if name is present */
	if (++argi >= slash->argc) {
		printf("missing node hostname\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	char * name = slash->argv[argi];

    if (known_hosts_add(node, name, true) == NULL) {
        fprintf(stderr, "No more memory, failed to add host");
        optparse_del(parser);
        return SLASH_ENOMEM;  // We have already checked for node 0, so this can currently only be a memory error.
    }
    optparse_del(parser);
    return SLASH_SUCCESS;
}
slash_command_sub(node, add, cmd_hosts_add, NULL, NULL);
