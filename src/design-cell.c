/* Copyright (c) 2016 Fabian Schuiki */
#include "design-internal.h"

/**
 * @file
 * @author Fabian Schuiki <fschuiki@student.ethz.ch>
 *
 * This file implements the phx_cell struct.
 */


void
phx_cell_invalidate(phx_cell_t *cell, uint8_t bits) {
	assert(cell);
	if (~cell->invalid & bits) {
		cell->invalid |= bits;
		for (unsigned u = 0; u < cell->uses.size; ++u) {
			phx_inst_invalidate(cell->uses.items[u], bits);
		}
	}
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
phx_cell_update_extents(phx_cell_t *cell) {
	assert(cell);
	cell->invalid &= ~PHX_EXTENTS;

	phx_geometry_update(&cell->geo, PHX_EXTENTS);
	phx_extents_reset(&cell->ext);
	phx_extents_include(&cell->ext, &cell->geo.ext);

	for (size_t z = 0; z < cell->insts.size; ++z) {
		phx_inst_t *inst = array_at(cell->insts, phx_inst_t*, z);
		phx_inst_update(inst, PHX_EXTENTS);
		phx_extents_include(&cell->ext, &inst->ext);
	}

	for (size_t z = 0; z < cell->pins.size; ++z) {
		phx_pin_t *pin = array_at(cell->pins, phx_pin_t*, z);
		phx_geometry_update(&pin->geo, PHX_EXTENTS);
		phx_extents_include(&cell->ext, &pin->geo.ext);
	}
}


static void
phx_cell_update_capacitances(phx_cell_t *cell) {
	assert(cell);
	cell->invalid &= ~PHX_CAPACITANCES;

	for (unsigned u = 0; u < cell->insts.size; ++u) {
		phx_inst_t *inst = array_at(cell->insts, phx_inst_t*, u);
		phx_cell_update(inst->cell, PHX_CAPACITANCES);
	}

	for (unsigned u = 0; u < cell->nets.size; ++u) {
		phx_net_t *net = array_at(cell->nets, phx_net_t*, u);
		double c = 0;
		for (unsigned u = 0; u < net->conns.size; ++u) {
			phx_terminal_t *conn = array_get(&net->conns, u);
			if (conn->inst != NULL)
				c += conn->pin->capacitance;
		}
		net->capacitance = c;
		for (unsigned u = 0; u < net->conns.size; ++u) {
			phx_terminal_t *conn = array_get(&net->conns, u);
			if (conn->inst == NULL)
				conn->pin->capacitance = c;
		}
	}
}


static void
phx_cell_update_timing(phx_cell_t *cell) {
	assert(cell);
	cell->invalid &= ~PHX_TIMING;

	for (unsigned u = 0; u < cell->insts.size; ++u) {
		phx_inst_t *inst = array_at(cell->insts, phx_inst_t*, u);
		phx_cell_update(inst->cell, PHX_TIMING);
	}

	for (unsigned u = 0; u < cell->nets.size; ++u) {
		phx_net_t *net = array_at(cell->nets, phx_net_t*, u);
		phx_net_update(net, PHX_TIMING);
	}
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
}


void
phx_cell_update(phx_cell_t *cell, uint8_t bits) {
	assert(cell);
	if (cell->invalid & bits & PHX_EXTENTS)
		phx_cell_update_extents(cell);
	if (cell->invalid & bits & PHX_CAPACITANCES)
		phx_cell_update_capacitances(cell);
	if (cell->invalid & bits & PHX_TIMING)
		phx_cell_update_timing(cell);
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


phx_inst_t *
phx_cell_find_inst(phx_cell_t *cell, const char *name) {
	assert(cell && name);
	/// @todo Use an index for this.
	for (unsigned u = 0; u < cell->insts.size; ++u) {
		phx_inst_t *inst = array_at(cell->insts, phx_inst_t*, u);
		if (strcmp(inst->name, name) == 0) {
			return inst;
		}
	}
	return NULL;
}


void
phx_cell_add_gds_text(phx_cell_t *cell, unsigned layer, unsigned type, vec2_t pos, const char *text) {
	assert(cell && text);
	size_t len = strlen(text)+1;
	phx_gds_text_t *txt = calloc(1, sizeof(*txt) + len);
	txt->layer = layer;
	txt->type = type;
	txt->pos = pos;
	txt->text = (void*)txt + sizeof(*txt);
	memcpy(txt->text, text, len);
	array_add(&cell->gds_text, &txt);
}
