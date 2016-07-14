/* Copyright (c) 2016 Fabian Schuiki */
#include "design-internal.h"
#include "table.h"


static phx_pin_t *new_pin(phx_cell_t *cell, const char *name);
static void free_pin(phx_pin_t *pin);
static void timing_arc_dispose(phx_timing_arc_t *arc);


void
phx_extents_reset(phx_extents_t *ext) {
	assert(ext);
	ext->min.x =  INFINITY;
	ext->min.y =  INFINITY;
	ext->max.x = -INFINITY;
	ext->max.y = -INFINITY;
}

void
phx_extents_include(phx_extents_t *ext, phx_extents_t *other) {
	if (other->min.x < ext->min.x) ext->min.x = other->min.x;
	if (other->min.y < ext->min.y) ext->min.y = other->min.y;
	if (other->max.x > ext->max.x) ext->max.x = other->max.x;
	if (other->max.y > ext->max.y) ext->max.y = other->max.y;
}

void
phx_extents_add(phx_extents_t *ext, vec2_t v) {
	if (v.x < ext->min.x) ext->min.x = v.x;
	if (v.y < ext->min.y) ext->min.y = v.y;
	if (v.x > ext->max.x) ext->max.x = v.x;
	if (v.y > ext->max.y) ext->max.y = v.y;
}



phx_library_t *
phx_library_create(phx_tech_t *tech) {
	phx_library_t *lib = calloc(1, sizeof(*lib));
	lib->tech = tech;
	array_init(&lib->cells, sizeof(phx_cell_t*));
	return lib;
}

void
phx_library_destroy(phx_library_t *lib) {
	assert(lib);
	for (size_t z = 0; z < lib->cells.size; z++)
		free_cell(array_at(lib->cells, phx_cell_t*, z));
	free(lib);
}

/**
 * @return A pointer to the cell with the given name, or `NULL` if no such cell
 *         exists.
 */
phx_cell_t *
phx_library_find_cell(phx_library_t *lib, const char *name, bool create) {
	phx_cell_t *cell;
	assert(lib && name);
	/// @todo Keep a sorted lookup table to increase the speed of this.
	for (size_t z = 0; z < lib->cells.size; ++z) {
		cell = array_at(lib->cells, phx_cell_t*, z);
		if (strcmp(cell->name, name) == 0)
			return cell;
	}
	if (create) {
		cell = new_cell(lib, name);
		return cell;
	} else {
		return NULL;
	}
}



phx_cell_t *
new_cell(phx_library_t *lib, const char *name) {
	assert(lib && name);
	phx_cell_t *cell = calloc(1, sizeof(*cell));
	cell->lib = lib;
	cell->name = dupstr(name);
	cell->invalid = PHX_INIT_INVALID;
	array_init(&cell->insts, sizeof(phx_inst_t*));
	array_init(&cell->pins, sizeof(phx_pin_t*));
	array_init(&cell->nets, sizeof(phx_net_t*));
	array_init(&cell->arcs, sizeof(phx_timing_arc_t));
	array_init(&cell->gds_text, sizeof(phx_gds_text_t*));
	ptrset_init(&cell->uses);
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
	for (unsigned u = 0; u < cell->gds_text.size; ++u) {
		free(array_at(cell->gds_text, phx_gds_text_t*, u));
	}
	free(cell->name);
	phx_geometry_dispose(&cell->geo);
	array_dispose(&cell->insts);
	array_dispose(&cell->pins);
	array_dispose(&cell->nets);
	array_dispose(&cell->arcs);
	array_dispose(&cell->gds_text);
	ptrset_dispose(&cell->uses);
	if (cell->gds)
		gds_struct_unref(cell->gds);
	free(cell);
}

const char *
phx_cell_get_name(phx_cell_t *cell) {
	assert(cell);
	return cell->name;
}

void
phx_cell_set_origin(phx_cell_t *cell, vec2_t o) {
	assert(cell);
	cell->origin = o;
	phx_cell_invalidate(cell, PHX_EXTENTS);
}

void
phx_cell_set_size(phx_cell_t *cell, vec2_t sz) {
	assert(cell);
	cell->size = sz;
	phx_cell_invalidate(cell, PHX_EXTENTS);
}

vec2_t
phx_cell_get_origin(phx_cell_t *cell) {
	assert(cell);
	return cell->origin;
}

vec2_t
phx_cell_get_size(phx_cell_t *cell) {
	assert(cell);
	return cell->size;
}

size_t
phx_cell_get_num_insts(phx_cell_t *cell) {
	assert(cell);
	return cell->insts.size;
}

phx_inst_t *
phx_cell_get_inst(phx_cell_t *cell, size_t idx) {
	assert(cell && idx < cell->insts.size);
	return array_at(cell->insts, phx_inst_t*, idx);
}

phx_geometry_t *
phx_cell_get_geometry(phx_cell_t *cell) {
	assert(cell);
	return &cell->geo;
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


phx_inst_t *
new_inst(phx_cell_t *into, phx_cell_t *cell, const char *name) {
	assert(into && cell);
	phx_inst_t *inst = calloc(1, sizeof(*inst));
	inst->cell = cell;
	inst->parent = into;
	inst->name = dupstr(name);
	inst->invalid = PHX_INIT_INVALID;
	ptrset_add(&cell->uses, inst);
	array_add(&into->insts, &inst);
	return inst;
}

void
free_inst(phx_inst_t *inst) {
	assert(inst);
	ptrset_remove(&inst->cell->uses, inst);
	if (inst->name) free(inst->name);
	free(inst);
}

void
inst_set_pos(phx_inst_t *inst, vec2_t pos) {
	assert(inst);
	inst->pos = pos;
	phx_inst_invalidate(inst, PHX_EXTENTS);
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
phx_cell_get_timing_arc(phx_cell_t *cell, phx_pin_t *pin, phx_pin_t *related_pin) {
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
	phx_timing_arc_t *arc = phx_cell_get_timing_arc(cell, pin, related_pin);
	phx_table_t **slot;
	switch (type) {
		case PHX_TIM_DELAY: slot = &arc->delay; break;
		case PHX_TIM_TRANS: slot = &arc->transition; break;
	}
	assert(slot);
	if (*slot != table) {
		if (*slot) phx_table_unref(*slot);
		if (table) phx_table_ref(table);
		*slot = table;
		phx_cell_invalidate(cell, PHX_TIMING);
	}
}


static void
timing_arc_dispose(phx_timing_arc_t *arc) {
	assert(arc);
	if (arc->delay) phx_table_unref(arc->delay);
	if (arc->transition) phx_table_unref(arc->transition);
}


// static void
// cell_invalidate_recursively(phx_cell_t *cell, uint32_t flag) {
// 	assert(cell);
// 	cell->flags |= flag;
// 	for (unsigned u = 0; u < cell->insts.size; ++u) {
// 		cell_invalidate_recursively(array_at(cell->insts, phx_inst_t*, u)->cell, flag);
// 	}
// }

// static void
// cell_update_timing_arcs_inner(phx_cell_t *cell) {
// 	assert(cell);
// 	cell->flags &= ~PHX_TIMING;
// 	// printf("Updating timing arcs of cell %s\n", cell->name);

// 	// Invalidate the intermediate results for each net.
// 	for (unsigned u = 0; u < cell->nets.size; ++u) {
// 		array_at(cell->nets, phx_net_t*, u)->invalid |= PHX_TIMING;
// 	}
// 	for (unsigned u = 0; u < cell->nets.size; ++u) {
// 		phx_net_update(array_at(cell->nets, phx_net_t*, u), PHX_TIMING);
// 	}
// }

// static void
// cell_update_recursively(phx_cell_t *cell) {
// 	assert(cell);
// 	for (unsigned u = 0; u < cell->insts.size; ++u) {
// 		cell_update_recursively(array_at(cell->insts, phx_inst_t*, u)->cell);
// 	}
// 	if (cell->flags & PHX_TIMING) cell_update_timing_arcs_inner(cell);
// }

// void
// cell_update_timing_arcs(phx_cell_t *cell) {
// 	assert(cell);
// 	cell_invalidate_recursively(cell, PHX_TIMING);
// 	cell_update_recursively(cell);
// }
