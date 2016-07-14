/* Copyright (c) 2016 Fabian Schuiki */
#pragma once
#include "common.h"
#include "lef.h"
#include "lib.h"

void dump_cell_nets(phx_cell_t *cell, FILE *out);
void dump_timing_arcs(phx_cell_t *cell);

void load_lef(phx_library_t *into, lef_t *lef, phx_tech_t *tech);
void load_lib(phx_library_t *into, lib_t *lib, phx_tech_t *tech);
void load_gds(phx_library_t *into, gds_lib_t *lib, phx_tech_t *tech);
void load_tech_layer_map(phx_tech_t *tech, const char *filename);
void plot_cell_as_pdf(phx_cell_t *cell, const char *filename);
void dump_cell_nets(phx_cell_t *cell, FILE *out);
int phx_net_connects_to(phx_net_t *net, phx_pin_t *pin, phx_inst_t *inst);
void connect(phx_cell_t *cell, phx_pin_t *pin_a, phx_inst_t *inst_a, phx_pin_t *pin_b, phx_inst_t *inst_b);
gds_struct_t *cell_to_gds(phx_cell_t *cell, gds_lib_t *target);

enum route_dir {
	ROUTE_X,
	ROUTE_Y
};

struct route_segment {
	enum route_dir dir;
	double pos;
	unsigned layer;
};

void umc65_route(phx_cell_t *cell, phx_tech_t *tech, vec2_t start_pos, unsigned start_layer, unsigned end_layer, unsigned num_segments, struct route_segment *segments);
