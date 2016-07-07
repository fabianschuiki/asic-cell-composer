/* Copyright (c) 2016 Fabian Schuiki */
#pragma once
#include "common.h"
#include "util.h"


struct tech {
	array_t layers; /* tech_layer_t* */
};

struct tech_layer {
	tech_t *tech;
	char *name;
	double color[3];
};

struct extents {
	vec2_t min;
	vec2_t max;
};

struct library {
	array_t cells; /* phx_cell_t* */
};

struct geometry {
	array_t layers; /* layer_t */
	extents_t ext;
	/// @todo Change layers to a map indexed by the the technology layer id.
};

struct layer {
	char *name;
	array_t shapes; /* shape_t */
	array_t points; /* vec2_t */
	/// The layer's extents.
	extents_t ext;
};

struct shape {
	/// Index of the shape's first point.
	uint32_t pt_begin;
	/// Index of the shape's last point.
	uint32_t pt_end;
};

struct phx_pin {
	phx_cell_t *cell;
	char *name;
	geometry_t geo;
	double capacitance;
};

enum cell_flags {
	PHX_EXTENTS = 1 << 0,
	PHX_TIMING  = 1 << 1,
};

struct phx_cell {
	uint8_t flags;
	/// Invalid bits of the cell.
	uint8_t invalid;
	library_t *lib;
	char *name;
	/// The cell's origin, in meters.
	vec2_t origin;
	/// The cell's size, in meters.
	vec2_t size;
	/// Instances contained within this cell.
	array_t insts;
	/// The cell's extents.
	extents_t ext;
	/// The cell's geometry.
	geometry_t geo;
	/// The cell's pins.
	array_t pins; /* phx_pin_t* */
	/// The cell's nets.
	array_t nets; /* phx_net_t* */
	/// The cell's timing arcs.
	array_t arcs; /* phx_timing_arc_t */
};

enum phx_orientation {
	PHX_MIRROR_X   = 1 << 0,
	PHX_MIRROR_Y   = 1 << 1,
	PHX_ROTATE_90  = 1 << 2,
	PHX_ROTATE_180 = PHX_MIRROR_X | PHX_MIRROR_Y,
	PHX_ROTATE_270 = PHX_ROTATE_90 | PHX_ROTATE_180,
};

struct phx_inst {
	/// The instantiated cell.
	phx_cell_t *cell;
	/// The cell within which this instance is placed.
	phx_cell_t *parent;
	/// Various flags.
	uint8_t flags;
	/// Invalidated bits of the instance.
	uint8_t invalid;
	/// The instance's orientation.
	uint8_t orientation; /* enum phx_orientation */
	/// The instance name.
	char *name;
	/// The position of the cell's origin.
	vec2_t pos;
	/// The instance's extents.
	extents_t ext;
};

struct phx_net {
	uint8_t invalid;
	phx_cell_t *cell;
	char *name;
	array_t conns; /* phx_terminal_t */
	double capacitance;
	array_t arcs; /* phx_timing_arc_t */
	int is_exposed;
};

struct phx_terminal {
	phx_inst_t *inst;
	phx_pin_t *pin;
};

enum phx_timing_type {
	PHX_TIM_DELAY,
	PHX_TIM_TRANS,
};

struct phx_timing_arc {
	phx_pin_t *pin;
	phx_pin_t *related_pin;
	phx_table_t *delay;
	phx_table_t *transition;
};


void extents_reset(extents_t*);
void extents_include(extents_t*, extents_t*);
void extents_add(extents_t*, vec2_t);

library_t *new_library();
void free_library(library_t*);
phx_cell_t *get_cell(library_t*, const char *name);
phx_cell_t *find_cell(library_t*, const char *name);

phx_cell_t *new_cell(library_t*, const char *name);
void free_cell(phx_cell_t*);
const char *cell_get_name(phx_cell_t*);
void cell_set_origin(phx_cell_t*, vec2_t);
void cell_set_size(phx_cell_t*, vec2_t);
vec2_t cell_get_origin(phx_cell_t*);
vec2_t cell_get_size(phx_cell_t*);
size_t cell_get_num_insts(phx_cell_t*);
phx_inst_t *cell_get_inst(phx_cell_t*, size_t idx);
geometry_t *cell_get_geometry(phx_cell_t*);
void cell_update_extents(phx_cell_t*);
phx_pin_t *cell_find_pin(phx_cell_t*, const char *name);
void cell_update_capacitances(phx_cell_t*);
void cell_update_timing_arcs(phx_cell_t*);

void geometry_init(geometry_t *geo);
void geometry_dispose(geometry_t *geo);
layer_t *geometry_find_layer(geometry_t*, const char *name);
size_t geometry_get_num_layers(geometry_t*);
layer_t *geometry_get_layer(geometry_t*, size_t idx);
void geometry_update_extents(geometry_t*);

void layer_init(layer_t *layer, const char *);
void layer_dispose(layer_t *layer);
void layer_add_shape(layer_t*, vec2_t*, size_t);
size_t layer_get_num_shapes(layer_t*);
shape_t *layer_get_shape(layer_t*, size_t idx);
vec2_t *layer_get_points(layer_t*);
void layer_update_extents(layer_t*);

phx_inst_t *new_inst(phx_cell_t *into, phx_cell_t *cell, const char *name);
void free_inst(phx_inst_t*);
void inst_set_pos(phx_inst_t*, vec2_t);
vec2_t inst_get_pos(phx_inst_t*);
phx_cell_t *inst_get_cell(phx_inst_t*);
void inst_update_extents(phx_inst_t*);
void phx_inst_set_orientation(phx_inst_t*, phx_orientation_t);
phx_orientation_t phx_inst_get_orientation(phx_inst_t*);

void phx_cell_set_timing_table(phx_cell_t*, phx_pin_t*, phx_pin_t*, phx_timing_type_t, phx_table_t*);
