/* Copyright (c) 2016 Fabian Schuiki */
#include "design-internal.h"

/**
 * @file
 * @author Fabian Schuiki <fschuiki@student.ethz.ch>
 *
 * This file implements the phx_cell struct.
 */


void
phx_cell_invalidate(phx_cell_t *cell, uint8_t mask) {
	assert(cell);
	cell->invalid |= mask;
	/// @todo Invalidate all instantiations of this cell.
}
