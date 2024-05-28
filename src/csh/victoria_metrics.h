/*
 * victoria_metrics.h
 *
 *  Created on: May 8, 2023
 *      Author: Edvard
 */
#pragma once

#include <param/param.h>

void vm_add(char * metric_line);
void vm_add_param(param_t * param);

#define SERVER_PORT      8428
#define SERVER_PORT_AUTH 8427
#define BUFFER_SIZE      10 * 1024 * 1024

typedef struct {
    int use_ssl;
    int port;
    int skip_verify;
    int verbose;
    char * username;
    char * password;
    char * server_ip;
} vm_args;

void * vm_push(void * arg);