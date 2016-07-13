/* Copyright (c) 2016 Fabian Schuiki */
#include "lib-internal.h"
#include "util.h"


struct lib_table_template {
	char *name;
	lib_table_format_t *fmt;
};

static void cell_free(lib_cell_t*);
static void pin_free(lib_pin_t*);
static void timing_free(lib_timing_t*);
static void table_free(lib_table_t*);


// -----------------------------------------------------------------------------
//  Library
// -----------------------------------------------------------------------------


lib_t *
lib_new(const char *name) {
	assert(name);
	lib_t *lib = calloc(1, sizeof(*lib));
	lib->name = dupstr(name);
	lib->capacitance_unit = 1e-12; /* default to pF */
	lib->leakage_power_unit = 1e-9; /* default to nW */
	array_init(&lib->cells, sizeof(phx_cell_t*));
	array_init(&lib->templates, sizeof(struct lib_table_template));
	return lib;
}


void
lib_free(lib_t *lib) {
	assert(lib);
	free(lib->name);
	for (size_t z = 0; z < lib->cells.size; ++z) {
		cell_free(array_at(lib->cells, lib_cell_t*, z));
	}
	for (unsigned u = 0; u < lib->templates.size; ++u) {
		struct lib_table_template *tmpl = array_get(&lib->templates, u);
		free(tmpl->name);
		lib_table_format_dispose(tmpl->fmt);
	}
	array_dispose(&lib->cells);
	array_dispose(&lib->templates);
	free(lib);
}


static int
cmp_name_and_cell(const char *name, lib_cell_t **cell) {
	return strcmp(name, (*cell)->name);
}


static int
cmp_name_and_pin(const char *name, lib_pin_t **pin) {
	return strcmp(name, (*pin)->name);
}


static int
cmp_name_and_template(const char *name, struct lib_table_template *tmpl) {
	return strcmp(name, tmpl->name);
}


int
lib_add_cell(lib_t *lib, const char *name, lib_cell_t **out) {
	unsigned pos;
	assert(lib && name && out);

	// Find the location where the cell should be inserted. If the cell already
	// exists, return an error.
	if (array_bsearch(&lib->cells, name, (void*)cmp_name_and_cell, &pos))
		return LIB_ERR_CELL_EXISTS;

	// Create the new cell and insert it at the location found above.
	lib_cell_t *cell = calloc(1, sizeof(*cell));
	cell->lib = lib;
	cell->name = dupstr(name);
	array_init(&cell->pins, sizeof(lib_pin_t*));
	array_insert(&lib->cells, pos, &cell);
	*out = cell;
	return LIB_OK;
}


lib_cell_t *
lib_find_cell(lib_t *lib, const char *name) {
	assert(lib && name);
	return *(lib_cell_t**)array_bsearch(&lib->cells, name, (void*)cmp_name_and_cell, NULL);
}


unsigned
lib_get_num_cells(lib_t *lib) {
	assert(lib);
	return lib->cells.size;
}


lib_cell_t *
lib_get_cell(lib_t *lib, unsigned idx) {
	assert(lib && idx < lib->cells.size);
	return array_at(lib->cells, lib_cell_t*, idx);
}


void
lib_set_capacitance_unit(lib_t *lib, double u) {
	assert(lib && u > 0);
	lib->capacitance_unit = u;
}


double
lib_get_capacitance_unit(lib_t *lib) {
	assert(lib);
	return lib->capacitance_unit;
}


void
lib_set_leakage_power_unit(lib_t *lib, double u) {
	assert(lib && u > 0);
	lib->leakage_power_unit = u;
}


double
lib_get_leakage_power_unit(lib_t *lib) {
	assert(lib);
	return lib->leakage_power_unit;
}


int
lib_add_lut_template(lib_t *lib, const char *name, lib_table_format_t **out) {
	assert(lib && name && out);
	unsigned pos;

	// Find the location where the table format should be inserted. If it
	// already exists, return an error.
	if (array_bsearch(&lib->templates, name, (void*)cmp_name_and_template, &pos))
		return LIB_ERR_TEMPLATE_EXISTS;

	// Create the new table format and insert it at the location found above.
	struct lib_table_template *tmpl = array_insert(&lib->templates, pos, NULL);
	memset(tmpl, 0, sizeof(*tmpl));
	tmpl->name = dupstr(name);
	tmpl->fmt = calloc(1, sizeof(lib_table_format_t));
	lib_table_format_init(tmpl->fmt);
	*out = tmpl->fmt;
	return LIB_OK;
}


lib_table_format_t *
lib_find_lut_template(lib_t *lib, const char *name) {
	assert(lib && name);
	struct lib_table_template *tmpl = array_bsearch(&lib->templates, name, (void*)cmp_name_and_template, NULL);
	return tmpl ? tmpl->fmt : NULL;
}



// -----------------------------------------------------------------------------
//  Cell
// -----------------------------------------------------------------------------


static void
cell_free(lib_cell_t *cell) {
	assert(cell);
	free(cell->name);
	for (size_t z = 0; z < cell->pins.size; ++z) {
		pin_free(array_at(cell->pins, lib_pin_t*, z));
	}
	array_dispose(&cell->pins);
	free(cell);
}


int
lib_cell_add_pin(lib_cell_t *cell, const char *name, lib_pin_t **out) {
	unsigned pos;
	assert(cell && name && out);

	// Find the location where the pin should be inserted. If the pin already
	// exists, return an error.
	if (array_bsearch(&cell->pins, name, (void*)cmp_name_and_pin, &pos))
		return LIB_ERR_PIN_EXISTS;

	// Create the new pin and insert it at the location found above.
	lib_pin_t *pin = calloc(1, sizeof(*pin));
	array_init(&pin->timings, sizeof(lib_timing_t*));
	pin->cell = cell;
	pin->name = dupstr(name);
	array_insert(&cell->pins, pos, &pin);
	*out = pin;
	return LIB_OK;
}


lib_pin_t *
lib_cell_find_pin(lib_cell_t *cell, const char *name) {
	assert(cell && name);
	return *(lib_pin_t**)array_bsearch(&cell->pins, name, (void*)cmp_name_and_pin, NULL);
}


const char *
lib_cell_get_name(lib_cell_t *cell) {
	assert(cell);
	return cell->name;
}


unsigned
lib_cell_get_num_pins(lib_cell_t *cell) {
	return cell->pins.size;
}


lib_pin_t *
lib_cell_get_pin(lib_cell_t *cell, unsigned idx) {
	assert(cell && idx < cell->pins.size);
	return array_at(cell->pins, lib_pin_t*, idx);
}


void
lib_cell_set_leakage_power(lib_cell_t *cell, double pwr) {
	assert(cell);
	cell->leakage_power = pwr;
}


double
lib_cell_get_leakage_power(lib_cell_t *cell) {
	assert(cell);
	return cell->leakage_power;
}


// -----------------------------------------------------------------------------
//  PIN
// -----------------------------------------------------------------------------


static void
pin_free(lib_pin_t *pin) {
	assert(pin);
	for (unsigned u = 0; u < pin->timings.size; ++u)
		timing_free(array_at(pin->timings, lib_timing_t*, u));
	array_dispose(&pin->timings);
	free(pin->name);
	free(pin);
}


const char *
lib_pin_get_name(lib_pin_t *pin) {
	assert(pin);
	return pin->name;
}


void
lib_pin_set_capacitance(lib_pin_t *pin, double capacitance) {
	assert(pin);
	pin->capacitance = capacitance;
}


double
lib_pin_get_capacitance(lib_pin_t *pin) {
	assert(pin);
	return pin->capacitance;
}


lib_timing_t *
lib_pin_add_timing(lib_pin_t *pin) {
	assert(pin);
	lib_timing_t *tmg = calloc(1, sizeof(*tmg));
	tmg->pin = pin;
	array_init(&tmg->related_pins, sizeof(char*));
	array_add(&pin->timings, &tmg);
	return tmg;
}


unsigned
lib_pin_get_num_timings(lib_pin_t *pin) {
	assert(pin);
	return pin->timings.size;
}


lib_timing_t *
lib_pin_get_timing(lib_pin_t *pin, unsigned idx) {
	assert(pin && idx < pin->timings.size);
	return array_at(pin->timings, lib_timing_t*, idx);
}


// -----------------------------------------------------------------------------
//  TIMING
// -----------------------------------------------------------------------------


static void
timing_free(lib_timing_t *tmg) {
	assert(tmg);
	for (unsigned u = 0; u < tmg->related_pins.size; ++u)
		free(array_at(tmg->related_pins, char*, u));
	for (unsigned u = 0; u < ASIZE(tmg->tables); ++u) {
		if (tmg->tables[u])
			table_free(tmg->tables[u]);
	}
	array_dispose(&tmg->related_pins);
	free(tmg);
}


int
lib_timing_add_table(lib_timing_t *tmg, unsigned param, lib_table_t **out) {
	unsigned idx = param & LIB_MODEL_INDEX_MASK;
	assert(tmg && idx < LIB_MODEL_NUM_PARAMS && out);

	// Check whether the table already exists. If it does, return an error.
	if (tmg->tables[idx]) {
		return LIB_ERR_TABLE_EXISTS;
	}

	// Create a new table in the appropriate slot.
	lib_table_t *tbl = calloc(1, sizeof(*tbl));
	tbl->tmg = tmg;
	tmg->tables[idx] = tbl;
	*out = tbl;
	return LIB_OK;
}


lib_table_t *
lib_timing_find_table(lib_timing_t *tmg, unsigned param) {
	unsigned idx = param & LIB_MODEL_INDEX_MASK;
	assert(tmg && idx < LIB_MODEL_NUM_PARAMS);
	return tmg->tables[idx];
}


unsigned
lib_timing_get_num_related_pins(lib_timing_t *tmg) {
	assert(tmg);
	return tmg->related_pins.size;
}


const char *
lib_timing_get_related_pin(lib_timing_t *tmg, unsigned idx) {
	assert(tmg && idx < tmg->related_pins.size);
	return array_at(tmg->related_pins, const char*, idx);
}


unsigned
lib_timing_get_type(lib_timing_t *tmg) {
	assert(tmg);
	return tmg->timing_type;
}


unsigned
lib_timing_get_sense(lib_timing_t *tmg) {
	assert(tmg);
	return tmg->timing_sense;
}


// -----------------------------------------------------------------------------
//  TABLE
// -----------------------------------------------------------------------------


static void
table_free(lib_table_t *tbl) {
	assert(tbl);
	if (tbl->values)
		free(tbl->values);
	free(tbl);
}


unsigned
lib_table_get_num_dims(lib_table_t *tbl) {
	assert(tbl);
	for (unsigned i = ASIZE(tbl->fmt.variables); i > 0; --i)
		if (tbl->fmt.variables[i-1] != LIB_VAR_NONE)
			return i;
	return 0;
}


unsigned
lib_table_get_variable(lib_table_t *tbl, unsigned idx) {
	assert(tbl && idx < ASIZE(tbl->fmt.variables));
	return tbl->fmt.variables[idx];
}


unsigned
lib_table_get_num_indices(lib_table_t *tbl, unsigned idx) {
	assert(tbl && idx < ASIZE(tbl->fmt.num_indices));
	return tbl->fmt.num_indices[idx];
}


double *
lib_table_get_indices(lib_table_t *tbl, unsigned idx) {
	assert(tbl && idx < ASIZE(tbl->fmt.indices));
	return tbl->fmt.indices[idx];
}


unsigned
lib_table_get_num_values(lib_table_t *tbl) {
	assert(tbl);
	return tbl->num_values;
}


double *
lib_table_get_values(lib_table_t *tbl) {
	assert(tbl);
	return tbl->values;
}


void
lib_table_format_init(lib_table_format_t *fmt) {
	assert(fmt);
	memset(fmt, 0, sizeof(*fmt));
}


void
lib_table_format_copy(lib_table_format_t *dst, lib_table_format_t *src) {
	assert(dst && src);
	memcpy(dst, src, sizeof(*src));
	for (unsigned u = 0; u < ASIZE(src->indices); ++u) {
		size_t sz = src->num_indices[u] * sizeof(double);
		dst->indices[u] = malloc(sz);
		memcpy(dst->indices[u], src->indices[u], sz);
	}
}


void
lib_table_format_dispose(lib_table_format_t *fmt) {
	assert(fmt);
	for (unsigned u = 0; u < ASIZE(fmt->indices); ++u) {
		if (fmt->indices[u])
			free(fmt->indices[u]);
	}
	memset(fmt, 0, sizeof(*fmt));
}
