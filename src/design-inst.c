/* Copyright (c) 2016 Fabian Schuiki */
#include "design-internal.h"

/**
 * @file
 * @author Fabian Schuiki <fschuiki@student.ethz.ch>
 *
 * This file implements the phx_inst struct.
 */


/**
 * Flag bits of an instance as invalid.
 */
void
phx_inst_invalidate(phx_inst_t *inst, uint8_t bits) {
	assert(inst);
	if (~inst->invalid & bits) {
		inst->invalid |= bits;
		phx_cell_invalidate(inst->parent, bits);
	}
}


static void
phx_inst_update_extents(phx_inst_t *inst) {
	assert(inst);
	inst->invalid &= ~PHX_EXTENTS;

	phx_extents_t ext = inst->cell->ext;
	if (inst->orientation & PHX_MIRROR_X) {
		double tmp = ext.min.x;
		ext.min.x = -ext.max.x;
		ext.max.x = -tmp;
	}
	if (inst->orientation & PHX_MIRROR_Y) {
		double tmp = ext.min.y;
		ext.min.y = -ext.max.y;
		ext.max.y = -tmp;
	}
	if (inst->orientation & PHX_ROTATE_90) {
		double tmp;
		tmp = ext.min.x;
		ext.min.x = ext.min.y;
		ext.max.y = -tmp;
		tmp = ext.max.x;
		ext.max.x = ext.max.y;
		ext.min.y = -tmp;
	}

	vec2_t off = vec2_sub(inst->pos, inst->cell->origin);
	inst->ext.min = vec2_add(ext.min, off);
	inst->ext.max = vec2_add(ext.max, off);
}


/**
 * Update invalid bits of an instance.
 */
void
phx_inst_update(phx_inst_t *inst, uint8_t bits) {
	assert(inst);
	phx_cell_update(inst->cell, bits);
	if (inst->invalid & bits & PHX_EXTENTS)
		phx_inst_update_extents(inst);
}


/**
 * Set an instance's orientation.
 */
void
phx_inst_set_orientation(phx_inst_t *inst, phx_orientation_t orientation) {
	assert(inst);
	if (inst->orientation != orientation) {
		phx_inst_invalidate(inst, PHX_EXTENTS);
		inst->orientation = orientation;
	}
}


/**
 * Get an instance's orientation.
 */
phx_orientation_t
phx_inst_get_orientation(phx_inst_t *inst) {
	assert(inst);
	return inst->orientation;
}


/**
 * Translates a point from the parent's coordinate space to the instance's
 * coordinate space, accounting for origin and orientation.
 */
vec2_t
phx_inst_vec_from_parent(phx_inst_t *inst, vec2_t pt) {
	assert(inst);
	pt.x -= inst->pos.x - inst->cell->origin.x;
	pt.y -= inst->pos.y - inst->cell->origin.y;
	if (inst->orientation & PHX_ROTATE_90) {
		double tmp = pt.y;
		pt.y = pt.x;
		pt.x = -tmp;
	}
	if (inst->orientation & PHX_MIRROR_X) pt.x *= -1;
	if (inst->orientation & PHX_MIRROR_Y) pt.y *= -1;
	return pt;
}


/**
 * Translates a point from the instance's coordinate space to the parent's
 * coordinate space, accounting for origin and orientation.
 */
vec2_t
phx_inst_vec_to_parent(phx_inst_t *inst, vec2_t pt) {
	assert(inst);
	if (inst->orientation & PHX_MIRROR_X) pt.x *= -1;
	if (inst->orientation & PHX_MIRROR_Y) pt.y *= -1;
	if (inst->orientation & PHX_ROTATE_90) {
		double tmp = pt.x;
		pt.x = pt.y;
		pt.y = -tmp;
	}
	pt.x += inst->pos.x - inst->cell->origin.x;
	pt.y += inst->pos.y - inst->cell->origin.y;
	return pt;
}


/**
 * Copies the contents of one geometry into another, translating the coordinates
 * from the instance's to the parent's coordinate space. Useful e.g. to raise an
 * instance's pin to the parent.
 */
void
phx_inst_copy_geometry_to_parent(phx_inst_t *inst, phx_geometry_t *src, phx_geometry_t *dst) {
	assert(inst && src && dst);
	for (size_t z = 0; z < src->layers.size; ++z) {
		phx_layer_t *layer_src = array_get(&src->layers, z);
		phx_layer_t *layer_dst = phx_geometry_on_layer(dst, layer_src->tech);

		// Lines
		for (size_t z = 0; z < layer_src->lines.size; ++z) {
			phx_line_t *line_src = array_at(layer_src->lines, phx_line_t*, z);
			phx_line_t *line_dst = phx_layer_add_line(layer_dst, line_src->width, line_src->num_pts, line_src->pts);
			for (size_t z = 0; z < line_src->num_pts; ++z) {
				line_dst->pts[z] = phx_inst_vec_to_parent(inst, line_src->pts[z]);
			}
		}

		// Shapes
		for (size_t z = 0; z < layer_src->shapes.size; ++z) {
			phx_shape_t *shape_src = array_at(layer_src->shapes, phx_shape_t*, z);
			phx_shape_t *shape_dst = phx_layer_add_shape(layer_dst, shape_src->num_pts, shape_src->pts);
			for (size_t z = 0; z < shape_src->num_pts; ++z) {
				shape_dst->pts[z] = phx_inst_vec_to_parent(inst, shape_src->pts[z]);
			}
		}
	}
}
