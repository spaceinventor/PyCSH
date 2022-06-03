/*
 * pycsh.h
 *
 * Setup and initialization for the PyCSH module.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

#pragma once

#include <param/param_queue.h>

#define PYCSH_DFL_TIMEOUT 1000  // In milliseconds

uint8_t csp_initialized();

extern param_queue_t param_queue_set;
extern param_queue_t param_queue_get;

extern int default_node;
extern int autosend;
extern int paramver;