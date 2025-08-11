#include <param/param.h>
#include <param/param_client.h>

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <slash/slash.h>
#include <slash/optparse.h>
#include "param_sniffer.h"

static int adcs_node = 0;

#define Q_HAT_ID 305
#define ORBIT_POS 357

int sockfd;
struct sockaddr_in server_addr;

int vts_running = 0;

static double to_jd(uint64_t ts_s) {
	return 2440587.5 + ((ts_s) / 86400.0);
}

int check_vts(uint16_t node, uint16_t id){
    if(!vts_running)
        return 0;
    if(node != adcs_node)
        return 0;
    if(id == Q_HAT_ID || id == ORBIT_POS)
        return true;
    return 0;
}

static uint64_t last_q_hat_time = 0;
static uint64_t last_pos_time = 0;

void vts_add(double arr[4], uint16_t id, int count, uint64_t time_ms){
    char buf[1000];
    uint64_t timestamp = time_ms / 1000;
    double jd_cnes = to_jd(timestamp) - 2433282.5;
    sprintf(buf, "TIME %f 1\n", jd_cnes);
    //printf("%s", buf);
    if(send(sockfd, buf, strlen(buf), MSG_NOSIGNAL) == -1){
        printf("VTS send failed!\n");
    }

    if(id == Q_HAT_ID && count == 4 && timestamp > last_q_hat_time){
        sprintf(buf, "DATA %f orbit_sim_quat \"%f %f %f %f\"\n", jd_cnes, arr[3], arr[0], arr[1], arr[2]);
        //printf("%s", buf);
        if(send(sockfd, buf, strlen(buf), MSG_NOSIGNAL) == -1){
            printf("VTS send failed!\n");
        }
        last_q_hat_time = timestamp;
    }

    if(id == ORBIT_POS && count == 3 && timestamp > last_pos_time){
        sprintf(buf, "DATA %f orbit_prop_pos \"%f %f %f\"\n", jd_cnes, arr[0]/1000, arr[1]/1000, arr[2]/1000); // convert to km
        //printf("%s", buf);
        if(send(sockfd, buf, strlen(buf), MSG_NOSIGNAL) == -1){
            printf("VTS send failed!\n");
        }
        last_pos_time = timestamp;
    }
}
