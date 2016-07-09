/* Copyright (c) 2016 Fabian Schuiki */
#pragma once
#include "cell.h"

void phx_cell_invalidate(phx_cell_t*, uint8_t);
void phx_inst_invalidate(phx_inst_t*, uint8_t);
void phx_geometry_invalidate(phx_geometry_t*, uint8_t);
void phx_layer_invalidate(phx_layer_t*, uint8_t);
