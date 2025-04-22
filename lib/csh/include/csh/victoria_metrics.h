/*
 * victoria_metrics.h
 *
 *  Created on: May 8, 2023
 *      Author: Edvard
 */
#pragma once

#include <pthread.h>
#include <param/param.h>

#define SERVER_PORT      8428
#define SERVER_PORT_AUTH 8427
#define BUFFER_SIZE      10 * 1024 * 1024

typedef struct {
    int use_ssl;
    int port;
    int skip_verify;
    int verbose;
    char * api_root;
    char * username;
    char * password;
    char * server_ip;
} vm_args;

extern vm_args victoria_metrics_args;
extern pthread_t vm_push_thread;

void vm_add(char * metric_line);
void vm_add_param(param_t * param);
void * vm_push(void * arg);
