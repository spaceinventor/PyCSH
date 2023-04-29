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

uint8_t csp_initialized();

extern unsigned int pycsh_dfl_node;
extern unsigned int pycsh_dfl_timeout;  // In milliseconds
