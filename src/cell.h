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
	array_t cells; /* cell_t* */
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

struct pin {
	cell_t *cell;
	char *name;
	geometry_t geo;
	double capacitance;
};

struct cell {
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
	array_t pins; /* pin_t* */
	/// The cell's nets.
	array_t nets; /* net_t* */
};

struct inst {
	/// The instantiated cell.
	cell_t *cell;
	/// The cell within which this instance is placed.
	cell_t *parent;
	/// The instance name.
	char *name;
	/// The position of the cell's origin.
	vec2_t pos;
	/// The instance's extents.
	extents_t ext;
};

struct net {
	char *name;
	array_t conns; /* net_conn_t */
	double capacitance;
};

struct net_conn {
	inst_t *inst;
	pin_t *pin;
};


void extents_reset(extents_t*);
void extents_include(extents_t*, extents_t*);
void extents_add(extents_t*, vec2_t);

library_t *new_library();
void free_library(library_t*);
cell_t *get_cell(library_t*, const char *name);
cell_t *find_cell(library_t*, const char *name);

cell_t *new_cell(library_t*, const char *name);
void free_cell(cell_t*);
const char *cell_get_name(cell_t*);
void cell_set_origin(cell_t*, vec2_t);
void cell_set_size(cell_t*, vec2_t);
vec2_t cell_get_origin(cell_t*);
vec2_t cell_get_size(cell_t*);
size_t cell_get_num_insts(cell_t*);
inst_t *cell_get_inst(cell_t*, size_t idx);
geometry_t *cell_get_geometry(cell_t*);
void cell_update_extents(cell_t*);
pin_t *cell_find_pin(cell_t*, const char *name);
void cell_update_capacitances(cell_t*);

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

inst_t *new_inst(cell_t *into, cell_t *cell, const char *name);
void free_inst(inst_t*);
void inst_set_pos(inst_t*, vec2_t);
vec2_t inst_get_pos(inst_t*);
cell_t *inst_get_cell(inst_t*);
void inst_update_extents(inst_t*);
