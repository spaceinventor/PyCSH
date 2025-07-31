
#include <param/param.h>
#include <param/param_server.h>
#include <param/param_queue.h>
#include <apm/csh_api.h>
#include <csp/csp_debug.h>

#include "pycshconfig.h"


#define PARAMID_SERIAL0                     31
#define PARAMID_CSP_DBG_BUFFER_OUT          51
#define PARAMID_CSP_DBG_CONN_OUT            52
#define PARAMID_CSP_DBG_CONN_OVF            53
#define PARAMID_CSP_DBG_CONN_NOROUTE        54
#define PARAMID_CSP_DBG_INVAL_REPLY         55
#define PARAMID_CSP_DBG_ERRNO               56
#define PARAMID_CSP_DBG_CAN_ERRNO           57
#define PARAMID_CSP_DBG_RDP_PRINT           58
#define PARAMID_CSP_DBG_PACKET_PRINT        59

static uint32_t _serial0;

PARAM_DEFINE_STATIC_RAM(PARAMID_SERIAL0, serial0, PARAM_TYPE_INT32, -1, 0, PM_HWREG, NULL, "", &_serial0, NULL);
PARAM_DEFINE_STATIC_RAM(PARAMID_CSP_DBG_BUFFER_OUT,   csp_buf_out,         PARAM_TYPE_UINT8,  0, 0, PM_DEBUG | PM_ERRCNT, NULL, "", &csp_dbg_buffer_out, "Number of buffer overruns");
PARAM_DEFINE_STATIC_RAM(PARAMID_CSP_DBG_CONN_OUT,     csp_conn_out,        PARAM_TYPE_UINT8,  0, 0, PM_DEBUG | PM_ERRCNT, NULL, "", &csp_dbg_conn_out, "Number of connection overruns");
PARAM_DEFINE_STATIC_RAM(PARAMID_CSP_DBG_CONN_OVF,     csp_conn_ovf,        PARAM_TYPE_UINT8,  0, 0, PM_DEBUG | PM_ERRCNT, NULL, "", &csp_dbg_conn_ovf, "Number of rx-queue overflows");
PARAM_DEFINE_STATIC_RAM(PARAMID_CSP_DBG_CONN_NOROUTE, csp_conn_noroute,    PARAM_TYPE_UINT8,  0, 0, PM_DEBUG | PM_ERRCNT, NULL, "", &csp_dbg_conn_noroute, "Numfer of packets dropped due to no-route");
PARAM_DEFINE_STATIC_RAM(PARAMID_CSP_DBG_INVAL_REPLY,  csp_inval_reply,     PARAM_TYPE_UINT8,  0, 0, PM_DEBUG | PM_ERRCNT, NULL, "", &csp_dbg_inval_reply, "Number of invalid replies from csp_transaction");
PARAM_DEFINE_STATIC_RAM(PARAMID_CSP_DBG_ERRNO,        csp_errno,           PARAM_TYPE_UINT8,  0, 0, PM_DEBUG, NULL, "", &csp_dbg_errno, "Global CSP errno, enum in csp_debug.h");
PARAM_DEFINE_STATIC_RAM(PARAMID_CSP_DBG_CAN_ERRNO,    csp_can_errno,       PARAM_TYPE_UINT8,  0, 0, PM_DEBUG, NULL, "", &csp_dbg_can_errno, "CAN driver specific errno, enum in csp_debug.h");
PARAM_DEFINE_STATIC_RAM(PARAMID_CSP_DBG_RDP_PRINT,    csp_print_rdp,       PARAM_TYPE_UINT8,  0, 0, PM_DEBUG, NULL, "", &csp_dbg_rdp_print, "Turn on csp_print of rdp information");
PARAM_DEFINE_STATIC_RAM(PARAMID_CSP_DBG_PACKET_PRINT, csp_print_packet,    PARAM_TYPE_UINT8,  0, 0, PM_DEBUG, NULL, "", &csp_dbg_packet_print, "Turn on csp_print of packet information");

static char queue_buf[PARAM_SERVER_MTU];
param_queue_t param_queue = { .buffer = queue_buf, .buffer_size = PARAM_SERVER_MTU, .type = PARAM_QUEUE_TYPE_EMPTY, .version = 2 };

#define BIN_PATH_MAX_ENTRIES 10
#define BIN_PATH_MAX_SIZE 256

struct bin_info_t {
    uint32_t addr_min;
    uint32_t addr_max;
    unsigned count;
    char entries[BIN_PATH_MAX_ENTRIES][BIN_PATH_MAX_SIZE];
};
struct bin_info_t bin_info;

#include "pycshconfig.h"
#ifdef PYCSH_HAVE_SLASH
#include <slash/slash.h>
__attribute__((constructor)) void _pycsh_init_slash(void) {
    slash_list_init();
}
#endif
