/**
 * Naive, slow and simple storage of nodeid and hostname
 */

#include <csh/known_hosts.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

//#include "cshconfig.h"


/*Both of these may be modified by APMs  */
__attribute__((used, retain)) unsigned int known_host_storage_size = sizeof(host_t);
SLIST_HEAD(known_host_s, host_s) known_hosts = {};

void known_hosts_del(int host) {

    // SLIST_FOREACH(host_t host, &known_hosts, next) {
    for (host_t* element = SLIST_FIRST(&known_hosts); element != NULL; element = SLIST_NEXT(element, next)) {
        if (element->node == host) {
            SLIST_REMOVE(&known_hosts, element, host_s, next);  // Probably not the best time-complexity here, O(n)*2 perhaps?
        }
    }
}

host_t * known_hosts_add(int addr, const char * new_name, bool override_existing) {

    if (addr == 0) {
        return NULL;
    }

    if (override_existing) {
        known_hosts_del(addr);  // Ensure 'addr' is not in the list
    } else {
        
        for (host_t* host = SLIST_FIRST(&known_hosts); host != NULL; host = SLIST_NEXT(host, next)) {
            if (host->node == addr) {
                return host;  // This node is already in the linked list, and we are not allowed to override it.
            }
        }
        // This node was not found in the list. Let's add it now.
    }

    // TODO Kevin: Do we want to break the API, and let the caller supply "host"?
    host_t * host = calloc(1, known_host_storage_size);
    if (host == NULL) {
        return NULL;  // No more memory
    }
    host->node = addr;
    strncpy(host->name, new_name, HOSTNAME_MAXLEN-1);  // -1 to fit NULL byte

    SLIST_INSERT_HEAD(&known_hosts, host, next);

    return host;
}

int known_hosts_get_name(int find_host, char * name, int buflen) {

    for (host_t* host = SLIST_FIRST(&known_hosts); host != NULL; host = SLIST_NEXT(host, next)) {
        if (host->node == find_host) {
            strncpy(name, host->name, buflen);
            return 1;
        }
    }

    return 0;

}


int known_hosts_get_node(const char * find_name) {

    if (find_name == NULL)
        return 0;

    for (host_t* host = SLIST_FIRST(&known_hosts); host != NULL; host = SLIST_NEXT(host, next)) {
        if (strncmp(find_name, host->name, HOSTNAME_MAXLEN) == 0) {
            return host->node;
        }
    }

    return 0;

}
