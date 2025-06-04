#include <param/param_server.h>
#include <param/param_queue.h>
#include <apm/csh_api.h>

static uint32_t _serial0;
#define PARAMID_SERIAL0                      31

PARAM_DEFINE_STATIC_RAM(PARAMID_SERIAL0, serial0, PARAM_TYPE_INT32, -1, 0, PM_HWREG, NULL, "", &_serial0, NULL);

unsigned int slash_dfl_node = 0;
unsigned int slash_dfl_timeout = 1000;

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
