/* Copyright (c) 2016 Fabian Schuiki */
#include "common.h"
#include "lib.h"
#include "cell.h"


/**
 * @file
 * @author Fabian Schuiki <fschuiki@student.ethz.ch>
 *
 * This file implements the conversion from and to LIB files.
 */


void
phx_make_lib_cell(phx_cell_t *src_cell, lib_cell_t *dst_cell) {
	assert(src_cell && dst_cell);
	int err;
	const char *cell_name = phx_cell_get_name(src_cell);

	// Attributes
	lib_cell_set_leakage_power(dst_cell, phx_cell_get_leakage_power(src_cell));

	// Pins
	for (unsigned u = 0, un = phx_cell_get_num_pins(src_cell); u < un; ++u) {
		phx_pin_t *src_pin = phx_cell_get_pin(src_cell, u);
		const char *pin_name = phx_pin_get_name(src_pin);
		lib_pin_t *dst_pin;

		err = lib_cell_add_pin(dst_cell, pin_name, &dst_pin);
		if (err != LIB_OK) {
			fprintf(stderr, "Unable to add pin %s to cell %s, %s\n", pin_name, cell_name, lib_errstr(err));
			continue;
		}

		// Attributes
		lib_pin_set_capacitance(dst_pin, phx_pin_get_capacitance(src_pin));
	}
}
