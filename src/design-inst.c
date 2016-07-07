/* Copyright (c) 2016 Fabian Schuiki */
#include "design-internal.h"

/**
 * @file
 * @author Fabian Schuiki <fschuiki@student.ethz.ch>
 *
 * This file implements the phx_inst struct.
 */


void
phx_inst_invalidate(phx_inst_t *inst, uint8_t mask) {
	assert(inst);
	inst->invalid |= mask;
	phx_cell_invalidate(inst->cell, mask);
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
