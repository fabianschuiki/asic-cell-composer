/* Copyright (c) 2016 Fabian Schuiki */
#include "common.h"
#include "lib.h"
#include "cell.h"
#include "table.h"


/**
 * @file
 * @author Fabian Schuiki <fschuiki@student.ethz.ch>
 *
 * This file implements the conversion from and to LIB files.
 */


static void
phx_make_lib_table(phx_table_t *src_tbl, lib_timing_t *tmg, unsigned param) {
	assert(src_tbl && tmg);

	if (!src_tbl->fmt) {
		lib_timing_set_scalar(tmg, param, src_tbl->data[0]);
	} else {
		lib_table_t *dst_tbl;
		lib_timing_add_table(tmg, param, &dst_tbl);

		for (unsigned u = 0; u < src_tbl->fmt->num_axes; ++u) {
			phx_table_axis_t *axis = src_tbl->fmt->axes + u;
			unsigned var = LIB_VAR_NONE;
			switch (axis->id) {
				case PHX_TABLE_IN_TRANS: var = LIB_VAR_IN_TRAN; break;
				case PHX_TABLE_OUT_CAP:  var = LIB_VAR_OUT_CAP_TOTAL; break;
			}
			assert(var != LIB_VAR_NONE);
			lib_table_set_variable(dst_tbl, u, var);
			lib_table_set_indices(dst_tbl, u, axis->num_indices, (double*)axis->indices);
			lib_table_set_stride(dst_tbl, u, axis->stride);
		}

		lib_table_set_values(dst_tbl, src_tbl->fmt->num_values, src_tbl->data);
	}
}


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

		// Timing arcs.
		for (unsigned u = 0; u < src_cell->arcs.size; ++u) {
			phx_timing_arc_t *arc = array_get(&src_cell->arcs, u);
			if (arc->pin != src_pin)
				continue;

			lib_timing_t *tmg = lib_pin_add_timing(dst_pin);
			lib_timing_set_type(tmg, LIB_TMG_TYPE_COMB | LIB_TMG_EDGE_BOTH);
			lib_timing_set_sense(tmg, LIB_TMG_NON_UNATE);
			lib_timing_add_related_pin(tmg, phx_pin_get_name(arc->related_pin));
			if (arc->delay) {
				phx_make_lib_table(arc->delay, tmg, LIB_MODEL_CELL_RISE);
			}
			if (arc->transition) {
				phx_make_lib_table(arc->transition, tmg, LIB_MODEL_TRANSITION_RISE);
			}
		}
	}
}
