/*
 * csp_init_py.c
 *
 * Wrappers for src/csp_init_cmd.c
 *
 */

#include "csp_init_py.h"

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdio.h>
#include <stdlib.h>

#include <csp/csp.h>
#include <csp/csp_id.h>

#include <pthread.h>
#include <param/param_server.h>
#include <vmem/vmem_server.h>
#include <sys/utsname.h>
#include <csp/interfaces/csp_if_zmqhub.h>
#include <csp/interfaces/csp_if_can.h>
#include <csp/interfaces/csp_if_lo.h>
#include <csp/interfaces/csp_if_tun.h>
#include <csp/interfaces/csp_if_udp.h>
#include <csp/interfaces/csp_if_eth.h>
#include <csp/drivers/can_socketcan.h>
#include <csp/drivers/eth_linux.h>
#include <csp/drivers/usart.h>
#include <csp/csp_rtable.h>
#include <ifaddrs.h>

/* These symbols are not present when using PyCSH "standalone" (from a Python script executed by the interactive interpreter for example) */
__attribute__((weak)) void *(*router_task)(void *) = NULL;
__attribute__((weak)) void *(*vmem_server_task)(void *) = NULL;

/* Keep track of whether `csp init` has been run, to prevent crashes from using CSP beforehand. */
bool csp_initialized() {
	return NULL != router_task;
}

void * py_router_task(void * param) {
    Py_Initialize();  // We need to initialize the Python interpreter before CSP may call any PythonParameter callbacks.
	while(1) {
		csp_route_work();
	}
    Py_Finalize();
}

void * py_vmem_server_task(void * param) {
    // TODO Kevin: If vmem_server ever needs to access Python objects, we should call Py_Initialize() here.
	vmem_server_loop(param);
	return NULL;
}

PyObject * pycsh_csh_csp_init(PyObject * self, PyObject * args, PyObject * kwds) {

    if(NULL != router_task) {
        Py_RETURN_NONE;
    }

	char * hostname = NULL;
    char * model = NULL;
    char * revision = NULL;
	int version = 2;
	int dedup = 3;

	static char *kwlist[] = {"host", "model", "revision", "version", "dedup", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|zzzii:csp_init", kwlist, &hostname, &model, &revision, &version, &dedup))
        return NULL;  // TypeError is thrown

    static struct utsname info;
    uname(&info);

    if (hostname == NULL)
        hostname = info.nodename;

    if (model == NULL)
        model = info.version;

    if (revision == NULL)
        revision = info.release;

    printf("  Version %d\n", version);
    printf("  Hostname: %s\n", hostname);
    printf("  Model: %s\n", model);
    printf("  Revision: %s\n", revision);
    printf("  Deduplication: %d\n", dedup);

    csp_conf.hostname = hostname;
    csp_conf.model = model;
    csp_conf.revision = revision;
    csp_conf.version = version;
    csp_conf.dedup = dedup;
    csp_init();

    csp_bind_callback(csp_service_handler, CSP_ANY);
    csp_bind_callback(param_serve, PARAM_PORT_SERVER);


    static pthread_t router_handle;
    pthread_create(&router_handle, NULL, &py_router_task, NULL);
    static pthread_t vmem_server_handle;
    pthread_create(&vmem_server_handle, NULL, &py_vmem_server_task, NULL);
    router_task = py_router_task;

    csp_iflist_check_dfl();

    csp_rdp_set_opt(3, 10000, 5000, 1, 2000, 2);
    //csp_rdp_set_opt(5, 10000, 5000, 1, 2000, 4);
    //csp_rdp_set_opt(10, 10000, 5000, 1, 2000, 8);
    //csp_rdp_set_opt(25, 10000, 5000, 1, 2000, 20);
    //csp_rdp_set_opt(40, 3000, 1000, 1, 250, 35);

    Py_RETURN_NONE;
}

PyObject * pycsh_csh_csp_ifadd_zmq(PyObject * self, PyObject * args, PyObject * kwds) {

    static int ifidx = 0;

    char name[10];
    sprintf(name, "ZMQ%u", ifidx++);
    
    unsigned int addr;
    char * server;
    int promisc = 0;
    int mask = 8;
    int dfl = 0;
    int pub_port = 6000;
    int sub_port = 7000;
    char * sec_key = NULL;

    static char *kwlist[] = {"addr", "server", "promisc", "mask", "default", "pub_port", "sub_port", "sec_key", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "Is|iiiiiz:csp_add_zmq", kwlist, &addr, &server, &promisc, &mask, &dfl, &pub_port, &sub_port, &sec_key))
		return NULL;  // TypeError is thrown

    csp_iface_t * iface;
    // TODO: We need to figure out how to correctly align the interface with respect to pub and sub ports. They are swapped
    csp_zmqhub_init_filter2((const char *) name, server, addr, mask, promisc, &iface, sec_key, pub_port, sub_port);
    iface->is_default = dfl;
    iface->addr = addr;
	iface->netmask = mask;

    Py_RETURN_NONE;
}

PyObject * pycsh_csh_csp_ifadd_kiss(PyObject * self, PyObject * args, PyObject * kwds) {

    static int ifidx = 0;

    char name[10];
    sprintf(name, "KISS%u", ifidx++);

    unsigned int addr;
    int mask = 8;
    int dfl = 0;
    int baud = 1000000;
    char * device = "ttyUSB0";

    static char *kwlist[] = {"addr", "mask", "default", "baud", "uart", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|iiiz:csp_add_kiss", kwlist, &addr, &mask, &dfl, &baud, &device))
		return NULL;  // TypeError is thrown

    csp_usart_conf_t conf = {
        .device = device,
        .baudrate = baud,
        .databits = 8,
        .stopbits = 1,
        .paritysetting = 0
    };

    csp_iface_t * iface;

    int error = csp_usart_open_and_add_kiss_interface(&conf, name, addr, &iface);
    if (error != CSP_ERR_NONE) {
        PyErr_SetString(PyExc_SystemError, "Failed to add kiss interface");
        return NULL;
    }

    iface->is_default = dfl;
    iface->addr = addr;
	iface->netmask = mask;

    Py_RETURN_NONE;
}

#if (CSP_HAVE_LIBSOCKETCAN)

PyObject * pycsh_csh_csp_ifadd_can(PyObject * self, PyObject * args, PyObject * kwds) {

    static int ifidx = 0;

    char name[10];
    sprintf(name, "CAN%u", ifidx++);

    unsigned int addr;
    int promisc = 0;
    int mask = 8;
    int dfl = 0;
    int baud = 1000000;
    char * device = "can0";

    static char *kwlist[] = {"addr", "promisc", "mask", "default", "baud", "can", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|iiiiz:csp_add_can", kwlist, &addr, &promisc, &mask, &dfl, &baud, &device))
		return NULL;  // TypeError is thrown

   csp_iface_t * iface;
    
    int error = csp_can_socketcan_open_and_add_interface(device, name, addr, baud, promisc, &iface);
    if (error != CSP_ERR_NONE) {
        char errbuf[100];
        snprintf(errbuf, 100, "failed to add CAN interface [%s], error: %d", device, error);
        PyErr_SetString(PyExc_SystemError, errbuf);
        return NULL;
    }

    iface->is_default = dfl;
    iface->addr = addr;
	iface->netmask = mask;

    Py_RETURN_NONE;
}

#endif

static void eth_select_interface(const char ** device) {

    static char selected[20];
    selected[0] = 0;

    // Create link of interface adresses
    struct ifaddrs *addresses;
    if (getifaddrs(&addresses) == -1)  {
        printf("eth_select_interface: getifaddrs call failed\n");
    } else {
        // Search for match
        struct ifaddrs * address = addresses;

        for( ; address && (selected[0] == 0); address = address->ifa_next) {
            if (address->ifa_addr && strcmp("lo", address->ifa_name) != 0) {
                if (strncmp(*device, address->ifa_name, strlen(*device)) == 0) {
                    strncpy(selected, address->ifa_name, sizeof(selected)-1);
                }
            }
        }
        freeifaddrs(addresses);
    }

    if (selected[0] == 0) {
        printf("  Device prefix '%s' not found.\n", *device);
    }
    *device = selected;
}

PyObject * pycsh_csh_csp_ifadd_eth(PyObject * self, PyObject * args, PyObject * kwds) {

    static int ifidx = 0;
    char name[CSP_IFLIST_NAME_MAX + 1];
    sprintf(name, "ETH%u", ifidx++);
    const char * device = "e";

    unsigned int addr;
    int promisc = 0;
    int mask = 8;
    int dfl = 0;
    int mtu = 1200;

    static char *kwlist[] = {"addr", "device", "promisc", "mask", "default", "mtu", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|ziiii:csp_add_eth", kwlist, &addr, &device, &promisc, &mask, &dfl, &mtu))
		return NULL;  // TypeError is thrown

    eth_select_interface(&device);
    if (strlen(device) == 0) {
        unsigned int len = strlen("The specified ethernet interface () could not be found") + strlen(device) + 1;
        char buf[len];
        sprintf(buf, "The specified ethernet interface (%s) could not be found", device);
        PyErr_SetString(PyExc_ValueError, buf);
        return NULL;
    }

    csp_iface_t * iface = NULL;

    // const char * device, const char * ifname, int mtu, unsigned int node_id, csp_iface_t ** iface, bool promisc
    csp_eth_init(device, name, mtu, addr, promisc == 1, &iface);

    iface->is_default = dfl;
    iface->addr = addr;
	iface->netmask = mask;

    Py_RETURN_NONE;
}

PyObject * pycsh_csh_csp_ifadd_udp(PyObject * self, PyObject * args, PyObject * kwds) {

    static int ifidx = 0;

    char name[10];
    sprintf(name, "UDP%u", ifidx++);

    unsigned int addr;
    char * server;
    int promisc = 0;
    int mask = 8;
    int dfl = 0;
    int listen_port = 9220;
    int remote_port = 9220;

    static char *kwlist[] = {"addr", "server", "promisc", "mask", "default", "listen_port", "remote_port", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "Is|iiiii:csp_add_udp", kwlist, &addr, &server, &promisc, &mask, &dfl, &listen_port, &remote_port))
		return NULL;  // TypeError is thrown

    csp_iface_t * iface = malloc(sizeof(csp_iface_t));
    if (iface == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    memset(iface, 0, sizeof(csp_iface_t));
    csp_if_udp_conf_t * udp_conf = malloc(sizeof(csp_if_udp_conf_t));
    if (udp_conf == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    udp_conf->host = strdup(server);
    udp_conf->lport = listen_port;
    udp_conf->rport = remote_port;
    if (udp_conf->host == NULL) {
        PyErr_NoMemory();
        return NULL;
    }
    csp_if_udp_init(iface, udp_conf);

    iface->is_default = dfl;
    iface->addr = addr;
	iface->netmask = mask;

    Py_RETURN_NONE;
}

PyObject * pycsh_csh_csp_ifadd_tun(PyObject * self, PyObject * args, PyObject * kwds) {

    static int ifidx = 0;

    char name[10];
    sprintf(name, "TUN%u", ifidx++);

    unsigned int addr;
    unsigned int tun_src;
    unsigned int tun_dst;
    int promisc = 0;
    int mask = 8;
    int dfl = 0;

    static char *kwlist[] = {"addr", "tun_src", "tun_dst", "promisc", "mask", "default", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "III|iii:csp_add_tun", kwlist, &addr, &tun_src, &tun_dst, &promisc, &mask, &dfl))
		return NULL;  // TypeError is thrown

    csp_iface_t * iface;
    iface = malloc(sizeof(csp_iface_t));
    csp_if_tun_conf_t * ifconf = malloc(sizeof(csp_if_tun_conf_t));
    ifconf->tun_dst = tun_dst;
    ifconf->tun_src = tun_src;

    csp_if_tun_init(iface, ifconf);

    iface->is_default = dfl;
    iface->addr = addr;
	iface->netmask = mask;

    Py_RETURN_NONE;
}

PyObject * pycsh_csh_csp_routeadd_cmd(PyObject * self, PyObject * args, PyObject * kwds) {

    unsigned int addr;
    unsigned int mask;
    char * interface_name = NULL;  // TODO Kevin: Create and accept Interface wrapper class here
    unsigned int via = CSP_NO_VIA_ADDRESS;

    static char *kwlist[] = {"addr", "mask", "interface", "via", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "IIs|I:csp_add_route", kwlist, &addr, &mask, &interface_name, &via)) {
		return NULL;  // TypeError is thrown
    }

    /* TODO Kevin: Can't quite decide between exceptions and error-as-value here.
        We should let the user see the returned error number. But a exception string is also quite nice. */

    if (via > UINT16_MAX) {
        /* TODO Kevin: shouldn't this be UINT14_MAX ? */
        PyErr_SetString(PyExc_ValueError, "Via address cannot be larger than 65535");
        return NULL;
    }

    if (addr > csp_id_get_max_nodeid()) {
        csp_dbg_errno = CSP_DBG_ERR_INVALID_RTABLE_ENTRY;  // TODO Kevin: Should we really set errno here?
        PyErr_Format(PyExc_ValueError, "Address cannot be larger than %d", csp_id_get_max_nodeid());
        return NULL;
    }

    if (mask > (int)csp_id_get_host_bits()) {
        csp_dbg_errno = CSP_DBG_ERR_INVALID_RTABLE_ENTRY;  // TODO Kevin: Should we really set errno here?
        PyErr_Format(PyExc_ValueError, "Mask cannot be larger than %d", (int)csp_id_get_host_bits());
        return NULL;
    }

    {   /* Checking for misaligned network address */
        const unsigned int subnet_size = pow(2, csp_id_get_host_bits()-mask);
        const unsigned int address_error = addr%subnet_size;
        if (address_error != 0) {
            /* TODO Kevin: Should we automatically use the floored address instead of erroring? */
            const unsigned int floor_address = addr-address_error;
            const unsigned int ceil_address = floor_address+subnet_size;
            PyErr_Format(PyExc_ValueError, "Invalid network address for route (%d/%d). Nearest valid lower address: %d, Nearest valid upper address %d", addr, mask, floor_address, ceil_address);
            return NULL;
        }
    }

    csp_iface_t * ifc = csp_iflist_get_by_name(interface_name);
    if (NULL == ifc) {
        /* NOTE: csp_rtable_set() already checks for valid ifc,
            but an explicit exception is probably more user-friendly. */
        PyErr_Format(PyExc_ValueError, "Failed to find interface by name '%s'", interface_name);
        return NULL;
    }

    int res = csp_rtable_set(addr, mask, ifc, via);
    if (CSP_ERR_NONE != res) {
        PyErr_Format(PyExc_ValueError, "Error while adding route. Returned error: %d", res);
        return NULL;
    }

    Py_RETURN_NONE;
}
