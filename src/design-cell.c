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


static void
phx_cell_update_leakage_power(phx_cell_t *cell) {
	assert(cell);
	cell->invalid &= ~PHX_POWER_LKG;

	if (cell->insts.size == 0)
		return;

	double pwr = 0;
	for (unsigned u = 0; u < cell->insts.size; ++u) {
		phx_inst_t *inst = array_at(cell->insts, phx_inst_t*, u);
		phx_cell_update(inst->cell, PHX_POWER_LKG);
		pwr += inst->cell->leakage_power;
	}
	cell->leakage_power = pwr;

	printf("Updated %s leakage power to %g\n", cell->name, cell->leakage_power);
}


void
phx_cell_update(phx_cell_t *cell, uint8_t bits) {
	assert(cell);
	// if (cell->invalid & bits & PHX_EXTENTS)
	// 	phx_cell_update_extents(cell);
	if (cell->invalid & bits & PHX_POWER_LKG)
		phx_cell_update_leakage_power(cell);
}


double
phx_cell_get_leakage_power(phx_cell_t *cell) {
	assert(cell);
	return cell->leakage_power;
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


void
phx_pin_set_capacitance(phx_pin_t *pin, double capacitance) {
	assert(pin);
	pin->capacitance = capacitance;
}


double
phx_pin_get_capacitance(phx_pin_t *pin) {
	assert(pin);
	return pin->capacitance;
}
