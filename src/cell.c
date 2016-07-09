/* Copyright (c) 2016 Fabian Schuiki */
#include "cell.h"
#include "table.h"


static phx_pin_t *new_pin(phx_cell_t *cell, const char *name);
static void free_pin(phx_pin_t *pin);
static void timing_arc_dispose(phx_timing_arc_t *arc);
static void phx_net_update(phx_net_t*, uint8_t bits);


void
extents_reset(phx_extents_t *ext) {
	assert(ext);
	ext->min.x =  INFINITY;
	ext->min.y =  INFINITY;
	ext->max.x = -INFINITY;
	ext->max.y = -INFINITY;
}

void
extents_include(phx_extents_t *ext, phx_extents_t *other) {
	if (other->min.x < ext->min.x) ext->min.x = other->min.x;
	if (other->min.y < ext->min.y) ext->min.y = other->min.y;
	if (other->max.x > ext->max.x) ext->max.x = other->max.x;
	if (other->max.y > ext->max.y) ext->max.y = other->max.y;
}

void
extents_add(phx_extents_t *ext, vec2_t v) {
	if (v.x < ext->min.x) ext->min.x = v.x;
	if (v.y < ext->min.y) ext->min.y = v.y;
	if (v.x > ext->max.x) ext->max.x = v.x;
	if (v.y > ext->max.y) ext->max.y = v.y;
}



library_t *
new_library() {
	library_t *lib = calloc(1, sizeof(*lib));
	array_init(&lib->cells, sizeof(phx_cell_t*));
	return lib;
}

void
free_library(library_t *lib) {
	size_t z;
	assert(lib);
	for (z = 0; z < lib->cells.size; z++) {
		free_cell(array_at(lib->cells, phx_cell_t*, z));
	}
	free(lib);
}

/**
 * @return A pointer to the cell with the given name, or `NULL` if no such cell
 *         exists.
 */
phx_cell_t *
get_cell(library_t *lib, const char *name) {
	assert(lib && name);
	/// @todo Keep a sorted lookup table to increase the speed of this.
	for (size_t z = 0; z < lib->cells.size; ++z) {
		phx_cell_t *cell = array_at(lib->cells, phx_cell_t*, z);
		if (strcmp(cell->name, name) == 0){
			return cell;
		}
	}
	return NULL;
}

phx_cell_t *
find_cell(library_t *lib, const char *name) {
	assert(lib && name);
	phx_cell_t *cell = get_cell(lib, name);
	if (cell)
		return cell;
	else
		return new_cell(lib, name);
}



phx_cell_t *
new_cell(library_t *lib, const char *name) {
	assert(lib && name);
	phx_cell_t *cell = calloc(1, sizeof(*cell));
	cell->lib = lib;
	cell->name = dupstr(name);
	array_init(&cell->insts, sizeof(phx_inst_t*));
	array_init(&cell->pins, sizeof(phx_pin_t*));
	array_init(&cell->nets, sizeof(phx_net_t*));
	array_init(&cell->arcs, sizeof(phx_timing_arc_t));
	phx_geometry_init(&cell->geo, cell);
	array_add(&lib->cells, &cell);
	return cell;
}

void
free_cell(phx_cell_t *cell) {
	assert(cell);
	for (size_t z = 0; z < cell->insts.size; ++z) {
		free_inst(array_at(cell->insts, phx_inst_t*, z));
	}
	for (size_t z = 0; z < cell->pins.size; ++z) {
		free_pin(array_at(cell->pins, phx_pin_t*, z));
	}
	for (unsigned u = 0; u < cell->arcs.size; ++u) {
		timing_arc_dispose(array_get(&cell->arcs, u));
	}
	free(cell->name);
	phx_geometry_dispose(&cell->geo);
	array_dispose(&cell->insts);
	array_dispose(&cell->pins);
	array_dispose(&cell->nets);
	array_dispose(&cell->arcs);
	if (cell->gds)
		gds_struct_unref(cell->gds);
	free(cell);
}

const char *
cell_get_name(phx_cell_t *cell) {
	assert(cell);
	return cell->name;
}

void
cell_set_origin(phx_cell_t *cell, vec2_t o) {
	assert(cell);
	cell->origin = o;
	/// @todo update_extents(cell);
}

void
cell_set_size(phx_cell_t *cell, vec2_t sz) {
	assert(cell);
	cell->size = sz;
	/// @todo update_extents(cell);
}

vec2_t
cell_get_origin(phx_cell_t *cell) {
	assert(cell);
	return cell->origin;
}

vec2_t
cell_get_size(phx_cell_t *cell) {
	assert(cell);
	return cell->size;
}

size_t
cell_get_num_insts(phx_cell_t *cell) {
	assert(cell);
	return cell->insts.size;
}

phx_inst_t *
cell_get_inst(phx_cell_t *cell, size_t idx) {
	assert(cell && idx < cell->insts.size);
	return array_at(cell->insts, phx_inst_t*, idx);
}

phx_geometry_t *
cell_get_geometry(phx_cell_t *cell) {
	assert(cell);
	return &cell->geo;
}

void
cell_update_extents(phx_cell_t *cell) {
	assert(cell);
	phx_geometry_update(&cell->geo, PHX_EXTENTS);
	extents_reset(&cell->ext);
	extents_include(&cell->ext, &cell->geo.ext);
	for (size_t z = 0; z < cell->insts.size; ++z) {
		phx_inst_t *inst = array_at(cell->insts, phx_inst_t*, z);
		inst_update_extents(inst);
		extents_include(&cell->ext, &inst->ext);
	}
	for (size_t z = 0; z < cell->pins.size; ++z) {
		phx_pin_t *pin = array_at(cell->pins, phx_pin_t*, z);
		phx_geometry_update(&pin->geo, PHX_EXTENTS);
		extents_include(&cell->ext, &pin->geo.ext);
	}
}

phx_pin_t *
cell_find_pin(phx_cell_t *cell, const char *name) {
	phx_pin_t *pin;
	assert(cell && name);
	for (size_t z = 0, zn = cell->pins.size; z < zn; ++z) {
		pin = array_at(cell->pins, phx_pin_t*, z);
		if (strcmp(pin->name, name) == 0)
			return pin;
	}

	pin = new_pin(cell, name);
	array_add(&cell->pins, &pin);
	return pin;
}

void
cell_update_capacitances(phx_cell_t *cell) {
	assert(cell);
	for (unsigned u = 0; u < cell->insts.size; ++u) {
		phx_inst_t *inst = array_at(cell->insts, phx_inst_t*, u);
		cell_update_capacitances(inst->cell);
	}
	for (unsigned u = 0; u < cell->nets.size; ++u) {
		phx_net_t *net = array_at(cell->nets, phx_net_t*, u);
		double c = 0;
		for (unsigned u = 0; u < net->conns.size; ++u) {
			phx_terminal_t *conn = array_get(&net->conns, u);
			c += conn->pin->capacitance;
		}
		net->capacitance = c;
	}
}


size_t
geometry_get_num_layers(phx_geometry_t *geo) {
	assert(geo);
	return geo->layers.size;
}

phx_layer_t *
geometry_get_layer(phx_geometry_t *geo, size_t idx) {
	assert(geo && idx < geo->layers.size);
	return array_get(&geo->layers, idx);
}



phx_inst_t *
new_inst(phx_cell_t *into, phx_cell_t *cell, const char *name) {
	assert(into && cell);
	phx_inst_t *inst = calloc(1, sizeof(*inst));
	inst->cell = cell;
	inst->parent = into;
	inst->name = dupstr(name);
	array_add(&into->insts, &inst);
	return inst;
}

void
free_inst(phx_inst_t *inst) {
	assert(inst);
	if (inst->name) free(inst->name);
	free(inst);
}

void
inst_set_pos(phx_inst_t *inst, vec2_t pos) {
	assert(inst);
	inst->pos = pos;
}

vec2_t
inst_get_pos(phx_inst_t *inst) {
	assert(inst);
	return inst->pos;
}

phx_cell_t *
inst_get_cell(phx_inst_t *inst) {
	assert(inst);
	return inst->cell;
}

void
inst_update_extents(phx_inst_t *inst) {
	assert(inst);

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



static phx_pin_t *
new_pin(phx_cell_t *cell, const char *name) {
	assert(cell && name);
	phx_pin_t *pin = calloc(1, sizeof(*cell));
	pin->cell = cell;
	pin->name = dupstr(name);
	phx_geometry_init(&pin->geo, pin->cell);
	return pin;
}

static void
free_pin(phx_pin_t *pin) {
	assert(pin);
	free(pin->name);
	phx_geometry_dispose(&pin->geo);
	free(pin);
}



static int
compare_timing_arcs(phx_timing_arc_t *a, phx_timing_arc_t *b) {
	if (a->pin < b->pin) return -1;
	if (a->pin > b->pin) return  1;
	if (a->related_pin < b->related_pin) return -1;
	if (a->related_pin > b->related_pin) return  1;
	return 0;
}


/**
 * Obtains a timing arc from a cell. Creates the arc if it does not yet exist.
 */
static phx_timing_arc_t *
cell_get_timing_arc(phx_cell_t *cell, phx_pin_t *pin, phx_pin_t *related_pin) {
	assert(cell && pin);
	unsigned idx;
	phx_timing_arc_t key = { .pin = pin, .related_pin = related_pin };
	phx_timing_arc_t *arc = array_bsearch(&cell->arcs, &key, (void*)compare_timing_arcs, &idx);
	if (!arc) {
		arc = array_insert(&cell->arcs, idx, NULL);
		memset(arc, 0, sizeof(*arc));
		arc->pin = pin;
		arc->related_pin = related_pin;
	}
	return arc;
}


void
phx_cell_set_timing_table(phx_cell_t *cell, phx_pin_t *pin, phx_pin_t *related_pin, phx_timing_type_t type, phx_table_t *table) {
	assert(cell && pin && table);
	phx_timing_arc_t *arc = cell_get_timing_arc(cell, pin, related_pin);
	phx_table_t **slot;
	switch (type) {
		case PHX_TIM_DELAY: slot = &arc->delay; break;
		case PHX_TIM_TRANS: slot = &arc->transition; break;
	}
	assert(slot);
	if (*slot != table) {
		if (*slot) phx_table_free(*slot);
		*slot = table;
	}
}


static void
timing_arc_dispose(phx_timing_arc_t *arc) {
	assert(arc);
	if (arc->delay) phx_table_free(arc->delay);
	if (arc->transition) phx_table_free(arc->transition);
}


static void
cell_invalidate_recursively(phx_cell_t *cell, uint32_t flag) {
	assert(cell);
	cell->flags |= flag;
	for (unsigned u = 0; u < cell->insts.size; ++u) {
		cell_invalidate_recursively(array_at(cell->insts, phx_inst_t*, u)->cell, flag);
	}
}

static void
cell_update_timing_arcs_inner(phx_cell_t *cell) {
	assert(cell);
	cell->flags &= ~PHX_TIMING;
	printf("Updating timing arcs of cell %s\n", cell->name);

	// Invalidate the intermediate results for each net.
	for (unsigned u = 0; u < cell->nets.size; ++u) {
		array_at(cell->nets, phx_net_t*, u)->invalid |= PHX_TIMING;
	}
	for (unsigned u = 0; u < cell->nets.size; ++u) {
		phx_net_update(array_at(cell->nets, phx_net_t*, u), PHX_TIMING);
	}
}

static void
cell_update_recursively(phx_cell_t *cell) {
	assert(cell);
	for (unsigned u = 0; u < cell->insts.size; ++u) {
		cell_update_recursively(array_at(cell->insts, phx_inst_t*, u)->cell);
	}
	if (cell->flags & PHX_TIMING) cell_update_timing_arcs_inner(cell);
}

void
cell_update_timing_arcs(phx_cell_t *cell) {
	assert(cell);
	cell_invalidate_recursively(cell, PHX_TIMING);
	cell_update_recursively(cell);
}


static void
table_lerpcpy_axis(double *old_values, double *new_values, uint8_t axis, phx_table_axis_t **old_axes, phx_table_axis_t **new_axes, phx_table_axis_t *lerp_axis, uint16_t lerp_idx0, uint16_t lerp_idx1, double lerp_f) {

	/// @todo Move this to table.c

	// printf("table_lerpcpy axis=%u\n", (unsigned)axis);
	phx_table_axis_t *old_axis = old_axes[axis];
	phx_table_axis_t *new_axis = new_axes[axis];

	if (axis == 0) {
		for (unsigned u = 0; u < new_axis->num_indices; ++u) {
			double v0 = old_values[u * old_axis->stride + lerp_idx0 * lerp_axis->stride];
			double v1 = old_values[u * old_axis->stride + lerp_idx1 * lerp_axis->stride];
			// printf("lerping v0=%g v1=%g f=%g\n", v0, v1, lerp_f);
			new_values[u * new_axis->stride] = v0*(1-lerp_f) + v1*lerp_f;
		}
	} else {
		for (unsigned u = 0; u < new_axis->num_indices; ++u) {
			table_lerpcpy_axis(
				old_values + u * old_axis->stride,
				new_values + u * new_axis->stride,
				axis - 1,
				old_axes,
				new_axes,
				lerp_axis,
				lerp_idx0,
				lerp_idx1,
				lerp_f
			);
		}
	}
}

static phx_table_t *
fix_output_capacitance(phx_table_t *tbl, double capacitance) {
	assert(tbl);

	/// @todo Move this to table.c

	// See whether the table has a output capacitance axis to be fixed. If there
	// is no such axis, the resulting table is simply the same as the input
	// table.
	phx_table_lerp_t lerp;
	phx_table_lerp_axes(tbl, 1, (phx_table_quantity_t[]){PHX_TABLE_OUT_CAP}, (phx_table_index_t[]){{capacitance}}, &lerp);
	if (!lerp.axis)
		return tbl;

	// printf("Found cap axis to fix at %g F, lerp %u-%u %g\n", capacitance, idx_start, idx_end, f);

	// Configure the new table layout without the output capacitance axis.
	phx_table_quantity_t new_quantities[tbl->num_axes-1];
	uint16_t new_num_indices[tbl->num_axes-1];
	for (unsigned ui = 0, uo = 0; ui < tbl->num_axes; ++ui) {
		if (tbl->axes+ui != lerp.axis) {
			new_quantities[uo] = tbl->axes[ui].quantity;
			new_num_indices[uo] = tbl->axes[ui].num_indices;
			++uo;
		}
	}

	phx_table_t *new_tbl = phx_table_new(tbl->num_axes-1, new_quantities, new_num_indices);
	for (unsigned u = 0; u < tbl->num_axes; ++u) {
		if (tbl->axes+u != lerp.axis) {
			phx_table_set_indices(new_tbl, tbl->axes[u].quantity, tbl->axes[u].indices);
		}
	}

	// Make a list of axis pairs.
	phx_table_axis_t *old_axes[new_tbl->num_axes];
	phx_table_axis_t *new_axes[new_tbl->num_axes];
	for (unsigned uo = 0, un = 0; uo < tbl->num_axes; ++uo) {
		if (tbl->axes[uo].quantity == new_tbl->axes[un].quantity) {
			old_axes[un] = tbl->axes + uo;
			new_axes[un] = new_tbl->axes + un;
			++un;
		}
	}

	// phx_table_dump(tbl, stdout);

	// Store the interpolated values.
	table_lerpcpy_axis(tbl->data, new_tbl->data, new_tbl->num_axes-1, old_axes, new_axes, lerp.axis, lerp.lower, lerp.upper, lerp.f);

	return new_tbl;
}

static void
resample_input_transition_lerpcpy(double *src_values, double *dst_values, uint8_t axis, phx_table_axis_t *src_axes, phx_table_axis_t *dst_axes, phx_table_lerp_t *lerp, uint8_t skip_axis) {

	/// @todo Move this to table.c

	// printf("table_lerpcpy axis=%u\n", (unsigned)axis);
	phx_table_axis_t *src_axis = src_axes + axis;
	phx_table_axis_t *dst_axis = dst_axes + axis;
	// printf("lerpcpy src_axis = {stride = %u} to dst_axis = {stride = %u}\n", src_axis->stride, dst_axis->stride);

	if (axis == skip_axis) {
		if (skip_axis == 0) {
			double v0 = src_values[lerp->lower * lerp->axis->stride];
			double v1 = src_values[lerp->upper * lerp->axis->stride];
			dst_values[0] = v0*(1-lerp->f) + v1*lerp->f;
			// printf("dst_values[0]: lerp v0=%g v1=%g f=%g\n", v0, v1, lerp->f);
		} else {
			resample_input_transition_lerpcpy(
				src_values,
				dst_values,
				axis - 1,
				src_axes,
				dst_axes,
				lerp,
				skip_axis
			);
		}
	} else if (axis == 0) {
		for (unsigned u = 0; u < dst_axis->num_indices; ++u) {
			double v0 = src_values[u * src_axis->stride + lerp->lower * lerp->axis->stride];
			double v1 = src_values[u * src_axis->stride + lerp->upper * lerp->axis->stride];
			// printf("dst_values[%u]: lerp v0=%g v1=%g f=%g\n", u * dst_axis->stride, v0, v1, lerp->f);
			dst_values[u * dst_axis->stride] = v0*(1-lerp->f) + v1*lerp->f;
		}
	} else {
		for (unsigned u = 0; u < dst_axis->num_indices; ++u) {
			resample_input_transition_lerpcpy(
				src_values + u * src_axis->stride,
				dst_values + u * dst_axis->stride,
				axis - 1,
				src_axes,
				dst_axes,
				lerp,
				skip_axis
			);
		}
	}
}

static phx_table_t *
resample_input_transition(phx_table_t *Tbase, phx_table_t *Tres) {
	assert(Tbase && Tres);
	assert(Tres->num_axes == 1);

	// printf("Resampling:\n");
	// phx_table_dump(Tbase, stdout);
	// printf("Using the following IN_TRANS values:\n");
	// phx_table_dump(Tres, stdout);

	// The resulting table's layout shall be exactly the same as the base table
	// layout, however the indices of the input transition axis shall be the
	// ones from the resampling table.
	phx_table_quantity_t quantities[Tbase->num_axes];
	uint16_t num_indices[Tbase->num_axes];
	int pivot_axis = -1;
	for (unsigned u = 0; u < Tbase->num_axes; ++u) {
		if (Tbase->axes[u].quantity == PHX_TABLE_IN_TRANS) {
			pivot_axis = u;
			num_indices[u] = Tres->axes[0].num_indices;
		} else {
			num_indices[u] = Tbase->axes[u].num_indices;
		}
		quantities[u] = Tbase->axes[u].quantity;
	}
	assert(pivot_axis != -1 && "base table does not contain an IN_TRANS axis");
	/// @todo Simply return the base table if this happens, since the table is
	/// still valid.

	phx_table_t *tbl = phx_table_new(Tbase->num_axes, quantities, num_indices);
	for (unsigned u = 0; u < Tbase->num_axes; ++u) {
		phx_table_set_indices(tbl, Tbase->axes[u].quantity, (int)u == pivot_axis ? Tres->axes[0].indices : Tbase->axes[u].indices);
	}
	// phx_table_dump(tbl, stdout);

	// Iterate over each value in the resampling table and fill the table with
	// linearly interpolated values for the given value.
	for (unsigned u = 0; u < Tres->axes[0].num_indices; ++u) {
		phx_table_lerp_t lerp;
		phx_table_lerp_axes(Tbase, 1, (phx_table_quantity_t[]){PHX_TABLE_IN_TRANS}, (phx_table_index_t[]){{Tres->data[u]}}, &lerp);
		// printf("Tres[%u]: lerp %g as %u to %u @ f = %f\n", u, Tres->data[u], lerp.lower, lerp.upper, lerp.f);

		resample_input_transition_lerpcpy(Tbase->data, tbl->data + u * tbl->axes[pivot_axis].stride, tbl->num_axes-1, Tbase->axes, tbl->axes, &lerp, pivot_axis);
	}

	// printf("Which leads to the table:\n");
	// phx_table_dump(tbl, stdout);

	return tbl;
}

static phx_table_t *
add_delay(phx_table_t *Ttail, phx_table_t *Thead) {
	assert(Ttail && Thead);

	// printf("Adding delays of table:\n");
	// phx_table_dump(Thead, stdout);
	// printf("To Table:\n");
	// phx_table_dump(Ttail, stdout);

	// Make a list of axes that is the union of the head's and tail's axes.
	unsigned num_axes_max = Ttail->num_axes + Thead->num_axes;
	unsigned num_axes = 0;
	phx_table_quantity_t quantities[num_axes_max];
	uint16_t num_indices[num_axes_max];
	phx_table_axis_t *src_axes[num_axes_max];

	unsigned uh, ut;
	for (uh = 0, ut = 0; uh < Thead->num_axes && ut < Ttail->num_axes;) {
		phx_table_axis_t *ah = Thead->axes + uh;
		phx_table_axis_t *at = Thead->axes + ut;
		if (ah->quantity <= at->quantity) {
			quantities[num_axes] = ah->quantity;
			num_indices[num_axes] = ah->num_indices;
			src_axes[num_axes] = ah;
			++num_axes;
		} else {
			quantities[num_axes] = at->quantity;
			num_indices[num_axes] = at->num_indices;
			src_axes[num_axes] = at;
			++num_axes;
		}
		if (ah->quantity <= at->quantity) ++uh;
		if (ah->quantity >= at->quantity) ++ut;
	}
	for (; uh < Thead->num_axes; ++uh, ++num_axes) {
		phx_table_axis_t *a = Thead->axes + uh;
		quantities[num_axes] = a->quantity;
		num_indices[num_axes] = a->num_indices;
		src_axes[num_axes] = a;
	}
	for (; ut < Ttail->num_axes; ++ut, ++num_axes) {
		phx_table_axis_t *a = Ttail->axes + ut;
		quantities[num_axes] = a->quantity;
		num_indices[num_axes] = a->num_indices;
		src_axes[num_axes] = a;
	}

	// Create the table and set the indices.
	phx_table_t *tbl = phx_table_new(num_axes, quantities, num_indices);
	for (unsigned u = 0; u < num_axes; ++u) {
		phx_table_set_indices(tbl, src_axes[u]->quantity, src_axes[u]->indices);
	}

	// Add the two tables and return the result.
	phx_table_add(tbl, Ttail, Thead);
	return tbl;
}

static void
combine_arcs(phx_net_t *net, phx_net_t *other_net, phx_timing_arc_t *arc, phx_timing_arc_t *other_arc, phx_table_t *delay, phx_table_t *transition) {

	// Join the other net's tables with the current net's tables using the input
	// transition axis.
	phx_table_t *comb_delay = delay;
	phx_table_t *comb_transition = transition;

	// The two tables are now relative to the other net's input transition. What
	// we want, however, is to have these tables relative to the cell's input
	// transition. Therefore we resample the transition and delay tables using
	// the other arc's transition table.
	if (other_arc->transition) {
		if (comb_transition) {
			// printf("Resampling transition table\n");
			comb_transition = resample_input_transition(comb_transition, other_arc->transition);
			// phx_table_dump(comb_transition, stdout);
		}
		if (comb_delay) {
			// printf("Resampling delay table\n");
			comb_delay = resample_input_transition(comb_delay, other_arc->transition);
			// phx_table_dump(comb_delay, stdout);
		}
	}

	if (comb_delay && other_arc->delay) {
		comb_delay = add_delay(comb_delay, other_arc->delay);
	}

	// if (comb_transition) {
	// 	printf("Transition table:\n");
	// 	phx_table_dump(comb_transition, stdout);
	// }
	// if (comb_delay) {
	// 	printf("Delay table:\n");
	// 	phx_table_dump(comb_delay, stdout);
	// }

	// Store the combined arc in the net.
	if (comb_transition || comb_delay) {
		phx_timing_arc_t *out_arc = array_add(&net->arcs, NULL);
		memset(out_arc, 0, sizeof(*out_arc));
		out_arc->related_pin = other_arc->related_pin;
		out_arc->delay = comb_delay;
		out_arc->transition = comb_transition;
	}

	// if (comb_delay && other_arc->delay)
	// 	comb_delay = table_join_along_in_trans(other_arc->delay, comb_delay);
	// if (comb_transition && other_arc->transition)
	// 	comb_transition = table_join_along_in_trans(other_arc->transition, comb_transition);
}

static void
phx_net_update_arc_forward(phx_net_t *net, phx_net_t *other_net, phx_timing_arc_t *arc) {
	assert(net && other_net && arc);
	printf("Update arc %s -> %s (arc %p)\n", other_net->name, net->name, arc);

	// If the net is not exposed to outside circuitry, the capacitive load is
	// known and the delay and transition tables can be reduced by fixing the
	// output capacitance.
	phx_table_t *delay = arc->delay;
	phx_table_t *transition = arc->transition;
	if (!net->is_exposed) {
		if (delay) delay = fix_output_capacitance(delay, net->capacitance);
		if (transition) transition = fix_output_capacitance(transition, net->capacitance);
	} else {
		/// @todo Account for the capacitance of the net (due to routing, etc.)
		/// by subtracting it from the output capacitance indices of the delay
		/// and transition tables. IMPORTANT: What happens if one of the values
		/// becomes negative?
	}

	// if (delay) {
	// 	printf("Delay:\n");
	// 	phx_table_dump(delay, stdout);
	// }
	// if (transition) {
	// 	printf("Transition:\n");
	// 	phx_table_dump(transition, stdout);
	// }


	// If the other net is attached to an input pin, form the initial timing arc
	// by associating the tables calculated above with the input pin and storing
	// the arc in the net struct.
	if (other_net->is_exposed) {
		printf("Using as initial arc\n");
		phx_timing_arc_t *out_arc = array_add(&net->arcs, NULL);
		memset(out_arc, 0, sizeof(*out_arc));
		for (unsigned u = 0; u < other_net->conns.size; ++u) {
			phx_terminal_t *term = array_get(&other_net->conns, u);
			if (!term->inst) {
				assert(!out_arc->related_pin && "Two input pins connected to the same net. What should I do now?");
				printf("  to pin %s\n", term->pin->name);
				out_arc->related_pin = term->pin;
			}
		}
		out_arc->delay = delay;
		out_arc->transition = transition;
	}


	// If the other net is not attached to an input pin, combine the timing arcs
	// already established for that net with the tables calculated above.
	else {
		printf("Combining with other net\n");
		for (unsigned u = 0; u < other_net->arcs.size; ++u) {
			phx_timing_arc_t *other_arc = array_get(&other_net->arcs, u);
			printf("  which already has arc to pin %s\n", other_arc->related_pin->name);
			combine_arcs(net, other_net, arc, other_arc, delay, transition);
		}
	}

}


static void
phx_net_update_timing_arcs(phx_net_t *net) {
	assert(net);
	net->invalid &= ~PHX_TIMING;
	// printf("Updating timing arcs of net %s.%s\n", net->cell->name, net->name);

	// Iterate over the terminals attached to this net.
	for (unsigned u = 0; u < net->conns.size; ++u) {
		phx_terminal_t *term = array_get(&net->conns, u);
		if (!term->inst) continue;
		// printf("  %s.%s\n", term->inst->name, term->pin->name);

		// Determine the timing arcs associated with the connected pin.
		for (unsigned u = 0; u < term->inst->cell->arcs.size; ++u) {
			phx_timing_arc_t *arc = array_get(&term->inst->cell->arcs, u);
			if (arc->pin != term->pin) continue;
			// printf("    has timing arc to %s.%s\n", term->inst->name, arc->related_pin->name);

			// Ensure the timing arcs are updated for the net connecting to this
			// pin.
			for (unsigned u = 0; u < net->cell->nets.size; ++u) {
				phx_net_t *other_net = array_at(net->cell->nets, phx_net_t*, u);
				for (unsigned u = 0; u < other_net->conns.size; ++u) {
					phx_terminal_t *other_term = array_get(&other_net->conns, u);
					if (other_term->pin == arc->related_pin && other_term->inst == term->inst) {
						// printf("    dependent on net %s (%p)\n", other_net->name, other_net);
						phx_net_update(other_net, PHX_TIMING);
						// printf("    ----------------\n");
					}
				}
			}

			for (unsigned u = 0; u < net->cell->nets.size; ++u) {
				phx_net_t *other_net = array_at(net->cell->nets, phx_net_t*, u);
				int is_related = 0;
				for (unsigned u = 0; u < other_net->conns.size; ++u) {
					phx_terminal_t *other_term = array_get(&other_net->conns, u);
					if (other_term->pin == arc->related_pin && other_term->inst == term->inst) {
						is_related = 1;
						break;
					}
				}
				if (is_related) {
					phx_net_update_arc_forward(net, other_net, arc);
				}
			}
		}
	}
}


static void
phx_net_update(phx_net_t *net, uint8_t bits) {
	assert(net);
	if (net->invalid & bits & PHX_TIMING) phx_net_update_timing_arcs(net);
}
