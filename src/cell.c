/* Copyright (c) 2016 Fabian Schuiki */
#include "cell.h"


static pin_t *new_pin(cell_t *cell, const char *name);
static void free_pin(pin_t *pin);


void
extents_reset(extents_t *ext) {
	assert(ext);
	ext->min.x =  INFINITY;
	ext->min.y =  INFINITY;
	ext->max.x = -INFINITY;
	ext->max.y = -INFINITY;
}

void
extents_include(extents_t *ext, extents_t *other) {
	if (other->min.x < ext->min.x) ext->min.x = other->min.x;
	if (other->min.y < ext->min.y) ext->min.y = other->min.y;
	if (other->max.x > ext->max.x) ext->max.x = other->max.x;
	if (other->max.y > ext->max.y) ext->max.y = other->max.y;
}

void
extents_add(extents_t *ext, vec2_t v) {
	if (v.x < ext->min.x) ext->min.x = v.x;
	if (v.y < ext->min.y) ext->min.y = v.y;
	if (v.x > ext->max.x) ext->max.x = v.x;
	if (v.y > ext->max.y) ext->max.y = v.y;
}



library_t *
new_library() {
	library_t *lib = calloc(1, sizeof(*lib));
	array_init(&lib->cells, sizeof(cell_t*));
	return lib;
}

void
free_library(library_t *lib) {
	size_t z;
	assert(lib);
	for (z = 0; z < lib->cells.size; z++) {
		free_cell(array_at(lib->cells, cell_t*, z));
	}
	free(lib);
}

/**
 * @return A pointer to the cell with the given name, or `NULL` if no such cell
 *         exists.
 */
cell_t *
get_cell(library_t *lib, const char *name) {
	assert(lib && name);
	/// @todo Keep a sorted lookup table to increase the speed of this.
	for (size_t z = 0; z < lib->cells.size; ++z) {
		cell_t *cell = array_at(lib->cells, cell_t*, z);
		if (strcmp(cell->name, name) == 0){
			return cell;
		}
	}
	return NULL;
}



cell_t *
new_cell(library_t *lib, const char *name) {
	assert(lib && name);
	cell_t *cell = calloc(1, sizeof(*cell));
	cell->lib = lib;
	cell->name = dupstr(name);
	array_init(&cell->insts, sizeof(inst_t*));
	array_init(&cell->pins, sizeof(pin_t*));
	array_init(&cell->nets, sizeof(net_t*));
	geometry_init(&cell->geo);
	array_add(&lib->cells, &cell);
	return cell;
}

void
free_cell(cell_t *cell) {
	assert(cell);
	for (size_t z = 0; z < cell->insts.size; ++z) {
		free_inst(array_at(cell->insts, inst_t*, z));
	}
	for (size_t z = 0; z < cell->pins.size; ++z) {
		free_pin(array_at(cell->pins, pin_t*, z));
	}
	free(cell->name);
	geometry_dispose(&cell->geo);
	array_dispose(&cell->insts);
	array_dispose(&cell->pins);
	array_dispose(&cell->nets);
	free(cell);
}

const char *
cell_get_name(cell_t *cell) {
	assert(cell);
	return cell->name;
}

void
cell_set_origin(cell_t *cell, vec2_t o) {
	assert(cell);
	cell->origin = o;
	/// @todo update_extents(cell);
}

void
cell_set_size(cell_t *cell, vec2_t sz) {
	assert(cell);
	cell->size = sz;
	/// @todo update_extents(cell);
}

vec2_t
cell_get_origin(cell_t *cell) {
	assert(cell);
	return cell->origin;
}

vec2_t
cell_get_size(cell_t *cell) {
	assert(cell);
	return cell->size;
}

size_t
cell_get_num_insts(cell_t *cell) {
	assert(cell);
	return cell->insts.size;
}

inst_t *
cell_get_inst(cell_t *cell, size_t idx) {
	assert(cell && idx < cell->insts.size);
	return array_at(cell->insts, inst_t*, idx);
}

geometry_t *
cell_get_geometry(cell_t *cell) {
	assert(cell);
	return &cell->geo;
}

void
cell_update_extents(cell_t *cell) {
	assert(cell);
	geometry_update_extents(&cell->geo);
	extents_reset(&cell->ext);
	extents_include(&cell->ext, &cell->geo.ext);
	for (size_t z = 0; z < cell->insts.size; ++z) {
		inst_t *inst = array_at(cell->insts, inst_t*, z);
		inst_update_extents(inst);
		extents_include(&cell->ext, &inst->ext);
	}
	for (size_t z = 0; z < cell->pins.size; ++z) {
		pin_t *pin = array_at(cell->pins, pin_t*, z);
		geometry_update_extents(&pin->geo);
		extents_include(&cell->ext, &pin->geo.ext);
	}
}

pin_t *
cell_find_pin(cell_t *cell, const char *name) {
	pin_t *pin;
	assert(cell && name);
	for (size_t z = 0, zn = cell->pins.size; z < zn; ++z) {
		pin = array_at(cell->pins, pin_t*, z);
		if (strcmp(pin->name, name) == 0)
			return pin;
	}

	pin = new_pin(cell, name);
	array_add(&cell->pins, &pin);
	return pin;
}



void
geometry_init(geometry_t *geo) {
	assert(geo);
	array_init(&geo->layers, sizeof(layer_t));
}

void
geometry_dispose(geometry_t *geo) {
	assert(geo);
	array_dispose(&geo->layers);
}

layer_t *
geometry_find_layer(geometry_t *geo, const char *name) {
	layer_t *layer;
	assert(geo && name);
	for (size_t z = 0, zn = geo->layers.size; z < zn; ++z) {
		layer = array_get(&geo->layers, z);
		if (strcmp(layer->name, name) == 0)
			return layer;
	}

	layer = array_add(&geo->layers, NULL);
	layer_init(layer, name);
	return layer;
}

size_t
geometry_get_num_layers(geometry_t *geo) {
	assert(geo);
	return geo->layers.size;
}

layer_t *
geometry_get_layer(geometry_t *geo, size_t idx) {
	assert(geo && idx < geo->layers.size);
	return array_get(&geo->layers, idx);
}

void
geometry_update_extents(geometry_t *geo) {
	assert(geo);
	extents_reset(&geo->ext);
	for (size_t z = 0; z < geo->layers.size; ++z) {
		layer_t *layer = array_get(&geo->layers, z);
		layer_update_extents(layer);
		extents_include(&geo->ext, &layer->ext);
	}
}



void
layer_init(layer_t *layer, const char *name) {
	assert(layer && name);
	layer->name = dupstr(name);
	array_init(&layer->shapes, sizeof(shape_t));
	array_init(&layer->points, sizeof(vec2_t));
}

void
layer_dispose(layer_t *layer) {
	assert(layer);
	free(layer->name);
	array_dispose(&layer->shapes);
	array_dispose(&layer->points);
}

void
layer_add_shape(layer_t *layer, vec2_t *points, size_t num_points) {
	assert(layer && (!num_points || points));
	shape_t sh = {
		.pt_begin = layer->points.size,
		.pt_end   = layer->points.size + num_points,
	};
	array_add(&layer->shapes, &sh);
	array_add_many(&layer->points, points, num_points);
}

size_t
layer_get_num_shapes(layer_t *layer) {
	assert(layer);
	return layer->shapes.size;
}

shape_t *
layer_get_shape(layer_t *layer, size_t idx) {
	assert(layer && idx < layer->shapes.size);
	return array_get(&layer->shapes, idx);
}

vec2_t *
layer_get_points(layer_t *layer) {
	assert(layer);
	return layer->points.items;
}

void
layer_update_extents(layer_t *layer) {
	assert(layer);
	extents_reset(&layer->ext);
	for (size_t z = 0; z < layer->points.size; ++z) {
		extents_add(&layer->ext, array_at(layer->points, vec2_t, z));
	}
}



inst_t *
new_inst(cell_t *into, cell_t *cell, const char *name) {
	assert(into && cell);
	inst_t *inst = calloc(1, sizeof(*inst));
	inst->cell = cell;
	inst->parent = into;
	inst->name = dupstr(name);
	array_add(&into->insts, &inst);
	return inst;
}

void
free_inst(inst_t *inst) {
	assert(inst);
	if (inst->name) free(inst->name);
	free(inst);
}

void
inst_set_pos(inst_t *inst, vec2_t pos) {
	assert(inst);
	inst->pos = pos;
}

vec2_t
inst_get_pos(inst_t *inst) {
	assert(inst);
	return inst->pos;
}

cell_t *
inst_get_cell(inst_t *inst) {
	assert(inst);
	return inst->cell;
}

void
inst_update_extents(inst_t *inst) {
	assert(inst);
	vec2_t off = vec2_sub(inst->pos, inst->cell->origin);
	inst->ext.min = vec2_add(inst->cell->ext.min, off);
	inst->ext.max = vec2_add(inst->cell->ext.max, off);
}



static pin_t *
new_pin(cell_t *cell, const char *name) {
	assert(cell && name);
	pin_t *pin = calloc(1, sizeof(*cell));
	pin->cell = cell;
	pin->name = dupstr(name);
	geometry_init(&pin->geo);
	return pin;
}

static void
free_pin(pin_t *pin) {
	assert(pin);
	free(pin->name);
	geometry_dispose(&pin->geo);
	free(pin);
}
