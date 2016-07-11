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


/**
 * Set the GDS structure associated with this cell.
 */
void
phx_cell_set_gds(phx_cell_t *cell, gds_struct_t *gds) {
	assert(cell);
	if (cell->gds != gds) {
		if (cell->gds) gds_struct_unref(cell->gds);
		if (gds) gds_struct_ref(gds);
		cell->gds = gds;
	}
}


/**
 * Get the GDS structure associated with this cell.
 */
gds_struct_t *
phx_cell_get_gds(phx_cell_t *cell) {
	assert(cell);
	return cell->gds;
}


unsigned
phx_cell_get_num_pins(phx_cell_t *cell) {
	assert(cell);
	return cell->pins.size;
}


phx_pin_t *
phx_cell_get_pin(phx_cell_t *cell, unsigned idx) {
	assert(cell && idx < cell->pins.size);
	return array_at(cell->pins, phx_pin_t*, idx);
}


const char *
phx_pin_get_name(phx_pin_t *pin) {
	assert(pin);
	return pin->name;
}


phx_geometry_t *
phx_pin_get_geometry(phx_pin_t *pin) {
	assert(pin);
	return &pin->geo;
}
