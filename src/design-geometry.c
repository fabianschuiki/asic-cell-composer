/* Copyright (c) 2016 Fabian Schuiki */
#include "design-internal.h"


/**
 * @file
 * @author Fabian Schuiki <fschuiki@student.ethz.ch>
 *
 * This file implements the geometry representation in a design.
 */


void
phx_geometry_init(phx_geometry_t *geo, phx_cell_t *cell) {
	assert(geo);
	memset(geo, 0, sizeof(*geo));
	geo->invalid = PHX_INIT_INVALID;
	geo->cell = cell;
	array_init(&geo->layers, sizeof(phx_layer_t));
}


void
phx_geometry_dispose(phx_geometry_t *geo) {
	assert(geo);
	for (size_t z = 0; z < geo->layers.size; ++z)
		phx_layer_dispose(array_get(&geo->layers, z));
	array_dispose(&geo->layers);
}


/**
 * Flag bits of a geometry as needing recalculation.
 */
void
phx_geometry_invalidate(phx_geometry_t *geo, uint8_t bits) {
	assert(geo);
	geo->invalid |= bits;
	phx_cell_invalidate(geo->cell, bits);
}


static int
compare_layers(phx_layer_t *a, phx_layer_t *b) {
	if (a->tech < b->tech) return -1;
	if (a->tech > b->tech) return  1;
	return 0;
}


phx_layer_t *
phx_geometry_on_layer(phx_geometry_t *geo, phx_tech_layer_t *tech) {
	phx_layer_t *layer;
	assert(geo);

	// Try to find the layer.
	phx_layer_t key = { .tech = tech };
	unsigned pos;
	layer = array_bsearch(&geo->layers, &key, (void*)compare_layers, &pos);
	if (layer)
		return layer;

	// Create the layer since none was found.
	layer = array_add(&geo->layers, NULL);
	phx_layer_init(layer, geo, tech);
	return layer;
}


static void
phx_geometry_update_extents(phx_geometry_t *geo) {
	assert(geo);
	geo->invalid &= ~PHX_EXTENTS;
	phx_extents_reset(&geo->ext);

	for (size_t z = 0; z < geo->layers.size; ++z) {
		phx_layer_t *layer = array_get(&geo->layers, z);
		phx_layer_update(layer, PHX_EXTENTS);
		phx_extents_include(&geo->ext, &layer->ext);
	}
}


void
phx_geometry_update(phx_geometry_t *geo, uint8_t bits) {
	assert(geo);
	if (geo->invalid & bits & PHX_EXTENTS)
		phx_geometry_update_extents(geo);
}


unsigned
phx_geometry_get_num_layers(phx_geometry_t *geo) {
	assert(geo);
	return geo->layers.size;
}


phx_layer_t *
phx_geometry_get_layer(phx_geometry_t *geo, unsigned idx) {
	assert(geo && idx < geo->layers.size);
	return array_get(&geo->layers, idx);
}


void
phx_layer_init(phx_layer_t *layer, phx_geometry_t *geo, phx_tech_layer_t *tech) {
	assert(layer && tech);
	layer->invalid = PHX_INIT_INVALID;
	layer->geo = geo;
	layer->tech = tech;
	array_init(&layer->lines, sizeof(phx_line_t));
	array_init(&layer->shapes, sizeof(phx_shape_t));
}


void
phx_layer_dispose(phx_layer_t *layer) {
	assert(layer);
	for (size_t z = 0; z < layer->lines.size; ++z)
		free(array_at(layer->lines, phx_line_t*, z));
	for (size_t z = 0; z < layer->shapes.size; ++z)
		free(array_at(layer->shapes, phx_shape_t*, z));
	array_dispose(&layer->lines);
	array_dispose(&layer->shapes);
}


/**
 * Flag bits of a layer as needing recalculation.
 */
void
phx_layer_invalidate(phx_layer_t *layer, uint8_t bits) {
	assert(layer);
	layer->invalid |= bits;
	phx_geometry_invalidate(layer->geo, bits);
}


phx_line_t *
phx_layer_add_line(phx_layer_t *layer, double width, size_t num_pts, vec2_t *pts) {
	assert(layer && num_pts >= 2);
	size_t sz_pts = num_pts * sizeof(vec2_t);
	phx_line_t *line = calloc(1, sizeof(*line) + sz_pts);
	line->width = width;
	line->num_pts = num_pts;
	if (pts)
		memcpy(line->pts, pts, sz_pts);
	phx_layer_invalidate(layer, PHX_EXTENTS);
	array_add(&layer->lines, &line);
	return line;
}


phx_shape_t *
phx_layer_add_shape(phx_layer_t *layer, size_t num_pts, vec2_t *pts) {
	assert(layer && num_pts >= 3);
	size_t sz_pts = num_pts * sizeof(vec2_t);
	phx_shape_t *shape = calloc(1, sizeof(*shape) + sz_pts);
	shape->num_pts = num_pts;
	if (pts)
		memcpy(shape->pts, pts, sz_pts);
	phx_layer_invalidate(layer, PHX_EXTENTS);
	array_add(&layer->shapes, &shape);
	return shape;
}


size_t
phx_layer_get_num_lines(phx_layer_t *layer) {
	assert(layer);
	return layer->lines.size;
}


size_t
phx_layer_get_num_shapes(phx_layer_t *layer) {
	assert(layer);
	return layer->shapes.size;
}


phx_line_t *
phx_layer_get_line(phx_layer_t *layer, size_t idx) {
	assert(layer && idx < layer->lines.size);
	return array_at(layer->lines, phx_line_t*, idx);
}


phx_shape_t *
phx_layer_get_shape(phx_layer_t *layer, size_t idx) {
	assert(layer && idx < layer->shapes.size);
	return array_at(layer->shapes, phx_shape_t*, idx);
}


static void
phx_layer_update_extents(phx_layer_t *layer) {
	assert(layer);
	layer->invalid &= ~PHX_EXTENTS;
	phx_extents_reset(&layer->ext);

	// Lines
	for (size_t z = 0; z < layer->lines.size; ++z) {
		phx_line_t *line = array_at(layer->lines, phx_line_t*, z);
		double hw = line->width / 2;
		for (size_t z = 0; z < line->num_pts; ++z) {
			vec2_t pt = line->pts[z];
			phx_extents_add(&layer->ext, (vec2_t){ pt.x - hw, pt.y - hw });
			phx_extents_add(&layer->ext, (vec2_t){ pt.x + hw, pt.y + hw });
		}
	}

	// Shapes
	for (size_t z = 0; z < layer->shapes.size; ++z) {
		phx_shape_t *shape = array_at(layer->shapes, phx_shape_t*, z);
		for (size_t z = 0; z < shape->num_pts; ++z) {
			phx_extents_add(&layer->ext, shape->pts[z]);
		}
	}
}


void
phx_layer_update(phx_layer_t *layer, uint8_t bits) {
	assert(layer);
	if (layer->invalid & bits & PHX_EXTENTS)
		phx_layer_update_extents(layer);
}


phx_tech_layer_t *
phx_layer_get_tech(phx_layer_t *layer) {
	assert(layer);
	return layer->tech;
}
